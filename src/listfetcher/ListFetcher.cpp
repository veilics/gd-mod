// Ported from GD-Roulette (SpaghettDev, AGPL-3.0) src/listfetcher/ListFetcher.cpp
// to the Geode v5 web API. Same endpoints, same request bodies, same parsing.
#include "ListFetcher.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <matjson.hpp>
#include <rtrp/rtrp.hpp>

#include "../managers/RouletteManager.hpp"
#include "../utils/utils.hpp"
#include "../utils/constants.hpp"
#include "../SorLevels.hpp"

using namespace geode::prelude;

namespace
{
constexpr std::string_view GJ_SECRET = "Wmfd2893gb7";
constexpr std::string_view GJ_LEVELS_URL = "https://www.boomlings.com/database/getGJLevels21.php";
constexpr std::string_view GJ_LISTS_URL = "https://www.boomlings.com/database/getGJLevelLists.php";
constexpr std::string_view GJ_LEN_QUERY = "0,1,2,3,4";
constexpr std::string_view DEMONLIST_URL = "https://pointercrate.com/api/v2/demons/listed";
constexpr std::string_view CHALLENGELIST_URL = "https://challengelist.gd/api/v1/demons";

// Upstream getDifficultyQuery: rated faces map to diff=<n>; demons are
// diff=-2 plus demonFilter (0 any, 1 easy .. 5 extreme).
std::string difficultyQuery(GJDifficulty difficulty)
{
	if (difficulty < GJDifficulty::Demon && static_cast<int>(difficulty) != -2)
		return fmt::format("&diff={}", static_cast<int>(difficulty));
	return fmt::format("&diff=-2&demonFilter={}",
		rl::constants::list_fetcher::DEMON_DIFF_TO_FILTER.at(difficulty));
}

// Sorrow rarity filter -> RobTop's own 2.2 search flags.
std::string rarityQuery(const std::string& r)
{
	if (r == "featured") return "&featured=1";
	if (r == "epic") return "&epic=1";
	if (r == "legendary") return "&legendary=1";
	if (r == "mythic") return "&mythic=1";
	return "";
}

using TextCb = std::function<void(geode::Result<std::string>)>;

// RobTop POST: form body + EMPTY User-Agent (RobTop's Cloudflare rejects
// unknown agents - the same trick GD-Roulette and the game itself use).
void robtopPost(std::string_view url, std::string body, TextCb cb)
{
	web::WebRequest req;
	req.userAgent("");
	req.header("Content-Type", "application/x-www-form-urlencoded");
	req.bodyString(std::move(body));
	geode::async::spawn(
		[req = std::move(req), url = std::string(url)]() mutable -> web::WebFuture { return req.post(url); },
		[cb = std::move(cb)](web::WebResponse res) {
			auto str = res.string();
			geode::Result<std::string> out = str.isOk()
				? geode::Result<std::string>(geode::Ok(str.unwrap()))
				: geode::Result<std::string>(geode::Err(fmt::format("HTTP {}", res.code())));
			geode::queueInMainThread([cb, out = std::move(out)]() mutable { cb(std::move(out)); });
		});
}

void jsonGet(std::string url, std::function<void(geode::Result<matjson::Value>)> cb)
{
	web::WebRequest req;
	geode::async::spawn(
		[req = std::move(req), url = std::move(url)]() mutable -> web::WebFuture { return req.get(url); },
		[cb = std::move(cb)](web::WebResponse res) {
			auto j = res.json();
			geode::Result<matjson::Value> out = j.isOk()
				? geode::Result<matjson::Value>(geode::Ok(j.unwrap()))
				: geode::Result<matjson::Value>(geode::Err(fmt::format("HTTP {}", res.code())));
			geode::queueInMainThread([cb, out = std::move(out)]() mutable { cb(std::move(out)); });
		});
}

std::string serverError(const std::string& resp, const char* where)
{
	if (resp.starts_with("error code"))
		return fmt::format("Server returned error code {}. Try again later. ({})", resp.substr(std::min<size_t>(12, resp.size())), where);
	return fmt::format("Error parsing response from servers. Try again later. ({})", where);
}

// Pick a random entry with a numeric level_id from a Pointercrate-style array.
int randomLevelIdFrom(const matjson::Value& arr)
{
	auto a = arr.asArray();
	if (!a || (*a).empty()) return -1;
	const auto& v = *a;
	for (int iter = 0; iter < 100; ++iter) {
		const auto& e = v[rl::utils::randomNumber(0ull, static_cast<unsigned long long>(v.size() - 1))];
		if (auto id = e["level_id"].asInt()) return static_cast<int>(id.unwrap());
	}
	return -1;
}
} // namespace

void ListFetcher::setFinishedFetchingCallback(std::function<void()>&& cb)
{
	m_finished_fetching_cb = std::move(cb);
}

void ListFetcher::finish()
{
	is_fetching = false;
	if (m_finished_fetching_cb) m_finished_fetching_cb();
}

// ---------------------------------------------------------------- normal --
// Upstream: secret, type=0 (search), star=1, len=0..4, diff[/demonFilter],
// random page within the real page count for that query.
void ListFetcher::getRandomNormalListLevel(GJDifficulty difficulty, geode::Result<level_pair_t>& result)
{
	is_fetching = true;
	const auto base = fmt::format("secret={}&type=0&star=1&len={}{}{}",
		GJ_SECRET, GJ_LEN_QUERY, difficultyQuery(difficulty), rarityQuery(RouletteManager::get().sorRarity));

	auto fetchPage = [this, &result, base, difficulty](int maxPages) {
		const int page = rl::utils::randomNumber(0, std::max(0, maxPages - 1));
		robtopPost(GJ_LEVELS_URL, fmt::format("{}&page={}", base, page), [this, &result, difficulty](geode::Result<std::string> r) {
			if (r.isErr()) {
				result = geode::Err(fmt::format("Servers returned an invalid response ({}). Try again later. (getGJLevels21.php, 2)", r.unwrapErr()));
				return this->finish();
			}
			const auto resp = r.unwrap();
			if (resp.empty() || resp == "-1") {
				result = geode::Err("No levels found for this filter. Try another difficulty or rarity.");
				return this->finish();
			}
			auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp);
			if (parsed.isErr()) { result = geode::Err(serverError(resp, "getGJLevels21.php, 2")); return this->finish(); }
			auto response = parsed.unwrap();

			// "this game is stupid" (upstream): 1-star auto levels show up under Easy
			if (difficulty == GJDifficulty::Easy) {
				response.levels.erase(std::remove_if(response.levels.begin(), response.levels.end(),
					[](const auto& level) { return level.stars == 1; }), response.levels.end());
			}
			if (response.levels.empty()) { result = geode::Err("No levels found on this page. Try again."); return this->finish(); }

			const auto idx = rl::utils::randomNumber(0ull, static_cast<unsigned long long>(response.levels.size() - 1));
			sorrow::levels::storeFromResponse(resp, response.levels[idx].levelID);
			result = geode::Ok(level_pair_t{
				response.levels[idx],
				rl::utils::getCreatorFromLevelResponse(response.creators, response.levels[idx])
			});
			this->finish();
		});
	};

	// Page count per query is learned once (upstream caches it per difficulty).
	if (auto it = m_max_pages.find(base); it != m_max_pages.end())
		return fetchPage(it->second);

	robtopPost(GJ_LEVELS_URL, fmt::format("{}&page=0", base), [this, &result, base, fetchPage](geode::Result<std::string> r) {
		int maxPages = 1;
		if (r.isOk() && !r.unwrap().empty() && r.unwrap() != "-1") {
			if (auto parsed = rtrp::RtResponseParser::parseLevelResponse(r.unwrap()); parsed.isOk()) {
				const auto& pg = parsed.unwrap().page;
				if (pg.countPerPage > 0) maxPages = std::max(1, static_cast<int>(std::ceil(pg.pages / static_cast<double>(pg.countPerPage))));
			}
		} else if (r.isErr()) {
			result = geode::Err(fmt::format("Servers returned an invalid response ({}). Try again later. (getGJLevels21.php, 1)", r.unwrapErr()));
			return this->finish();
		}
		m_max_pages[base] = std::min(maxPages, 1000); // RobTop stops serving very deep pages
		fetchPage(m_max_pages[base]);
	});
}

// ------------------------------------------------------------- demonlist --
// Upstream: Pointercrate listed demons, random entry, then getLevelInfo.
// Sorrow keeps it to the main list (top 75) like the bot's demonlist source.
void ListFetcher::getRandomDemonListLevel(geode::Result<level_pair_t>& result)
{
	is_fetching = true;
	jsonGet(fmt::format("{}/?limit=75&after=0", DEMONLIST_URL), [this, &result](geode::Result<matjson::Value> r) {
		if (r.isErr()) { result = geode::Err(fmt::format("Pointercrate API returned an invalid response ({}). Try again later.", r.unwrapErr())); return this->finish(); }
		const int id = randomLevelIdFrom(r.unwrap());
		if (id <= 0) { result = geode::Err("Pointercrate API returned no usable level. Try again later."); return this->finish(); }
		this->getLevelInfo(id, result);
	});
}

// --------------------------------------------------------- challenge list --
void ListFetcher::getRandomChallengeListLevel(geode::Result<level_pair_t>& result)
{
	is_fetching = true;
	jsonGet(fmt::format("{}/?limit=100&after={}", CHALLENGELIST_URL,
		rl::utils::randomNumber(0, static_cast<int>(rl::constants::list_fetcher::CHALLENGE_LIST_MAX_PAGE))),
		[this, &result](geode::Result<matjson::Value> r) {
			if (r.isErr()) { result = geode::Err(fmt::format("Challenge List API returned an invalid response ({}). Try again later.", r.unwrapErr())); return this->finish(); }
			const int id = randomLevelIdFrom(r.unwrap());
			if (id <= 0) { result = geode::Err("Challenge List API returned no usable level. Try again later."); return this->finish(); }
			this->getLevelInfo(id, result);
		});
}

// ---------------------------------------------------------------- GD list --
void ListFetcher::getRandomGDListLevel(int listID, geode::Result<level_pair_t>& result)
{
	is_fetching = true;
	if (listID <= 0) { result = geode::Err("Set a GD List ID in the roulette options first."); return this->finish(); }

	if (m_cached_gd_list_id == listID && !m_cached_gd_list_level_ids.empty()) {
		const auto& ids = m_cached_gd_list_level_ids;
		int id = 0;
		try { id = std::stoi(ids[rl::utils::randomNumber(0ull, static_cast<unsigned long long>(ids.size() - 1))]); } catch (...) {}
		return this->getLevelInfo(id, result);
	}

	robtopPost(GJ_LISTS_URL, fmt::format("secret={}&type=0&str={}", GJ_SECRET, listID), [this, &result, listID](geode::Result<std::string> r) {
		if (r.isErr()) { result = geode::Err(fmt::format("Servers returned an invalid response ({}). Try again later. (getGJLevelLists.php)", r.unwrapErr())); return this->finish(); }
		const auto resp = r.unwrap();
		if (resp.empty() || resp == "-1") { result = geode::Err("Invalid List ID. (getGJLevelLists.php)"); return this->finish(); }
		auto parsed = rtrp::RtResponseParser::parseListResponse(resp);
		if (parsed.isErr() || parsed.unwrap().lists.empty() || parsed.unwrap().lists[0].levelIDs.empty()) {
			result = geode::Err(serverError(resp, "getGJLevelLists.php"));
			return this->finish();
		}
		m_cached_gd_list_id = listID;
		m_cached_gd_list_level_ids = parsed.unwrap().lists[0].levelIDs;
		const auto& ids = m_cached_gd_list_level_ids;
		int id = 0;
		try { id = std::stoi(ids[rl::utils::randomNumber(0ull, static_cast<unsigned long long>(ids.size() - 1))]); } catch (...) {}
		this->getLevelInfo(id, result);
	});
}

// ------------------------------------------------------------- level info --
// Upstream: getGJLevels21.php secret, type=0, str=<levelID>.
void ListFetcher::getLevelInfo(int levelID, geode::Result<level_pair_t>& result)
{
	is_fetching = true;
	if (levelID <= 0) { result = geode::Err("Invalid Level ID."); return this->finish(); }
	robtopPost(GJ_LEVELS_URL, fmt::format("secret={}&type=0&str={}", GJ_SECRET, levelID), [this, &result](geode::Result<std::string> r) {
		if (r.isErr()) { result = geode::Err(fmt::format("Servers returned an invalid response ({}). Try again later. (getGJLevels21.php)", r.unwrapErr())); return this->finish(); }
		const auto resp = r.unwrap();
		if (resp.empty() || resp == "-1") { result = geode::Err("Invalid Level ID."); return this->finish(); }
		auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp);
		if (parsed.isErr() || parsed.unwrap().levels.empty()) { result = geode::Err(serverError(resp, "getGJLevels21.php, 1")); return this->finish(); }
		auto response = parsed.unwrap();
		sorrow::levels::storeFromResponse(resp, response.levels[0].levelID);
		result = geode::Ok(level_pair_t{
			response.levels[0],
			rl::utils::getCreatorFromLevelResponse(response.creators, response.levels[0])
		});
		this->finish();
	});
}
