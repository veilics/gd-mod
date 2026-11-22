#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <Geode/Result.hpp>
#include <Geode/Enums.hpp>
#include <rtrp/objects/LevelObject.hpp>
#include <rtrp/objects/CreatorObject.hpp>

#include "SingletonBase.hpp"

// Level fetching exactly like the original GD-Roulette (SpaghettDev): the
// game talks to RobTop's servers directly (getGJLevels21.php /
// getGJLevelLists.php, empty User-Agent, public secret) and to the
// Pointercrate / Challenge List APIs - no GDBrowser, no bot round-trip.
class ListFetcher : public SingletonBase<ListFetcher>
{
public:
	using level_pair_t = std::pair<rtrp::objects::LevelObject, rtrp::objects::CreatorObject>;

	void getRandomNormalListLevel(GJDifficulty, geode::Result<level_pair_t>&);
	void getRandomDemonListLevel(geode::Result<level_pair_t>&);
	void getRandomChallengeListLevel(geode::Result<level_pair_t>&);
	void getRandomGDListLevel(int, geode::Result<level_pair_t>&);
	void getLevelInfo(int, geode::Result<level_pair_t>&);

	void setFinishedFetchingCallback(std::function<void()>&& cb);

	std::atomic_bool is_fetching;

protected:
	ListFetcher() : is_fetching(false) {}

	void finish();

	std::function<void()> m_finished_fetching_cb;
	std::map<std::string, int> m_max_pages;          // per search query: page count
	int m_cached_gd_list_id = -1;
	std::vector<std::string> m_cached_gd_list_level_ids;
};
