// Sorrow Integration - multiplayer roulette rooms (bot-authoritative, Globed-synced).
//
// The bot owns the game (gd.js rooms/*): a room plays `rounds` rounds; each
// round it rolls a level + target %, and the FIRST player whose death or
// completion reaches the target wins the round (progressive rooms then raise
// the target to the winner's % + 1). Most round wins takes the pot. Bets go
// in before the game starts.
//
// The mod side:
//   * rooms::state()      - mirror of the bot room (applyRoomJson)
//   * RoomDirector        - polls the room every 2.5 s WHEREVER you are (menus
//                           or in a level), announces round wins / new rounds /
//                           the game result, and moves you to each new level
//   * Globed              - SorrowRoomEvent: instant re-poll for room-mates on
//                           every change, plus invites when a room is opened
//   * reportProgress      - every death / completion on the round level
#pragma once

#include "SorUi.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <matjson.hpp>
#include "SorConst.hpp"
#include "Bridge.hpp"
#include "GlobedLink.hpp"
#include "Glimmer.hpp"
#include "Bet.hpp"   // presentGlimmerWin
#include "SorLevels.hpp"
#include <Geode/utils/web.hpp>
#include <rtrp/rtrp.hpp>
#include <random>
#include <set>

namespace sorrow::rooms
{

// Alias: the bridge functions live in the parent namespace.
namespace Bridge = ::sorrow;

struct BrowseItem
{
    std::string code;
    std::string host;
    std::string hostId;
    std::string state = "lobby";
    int players = 0;
    int maxPlayers = 0;
    int rounds = 1;
    int round = 0;
    std::int64_t pot = 0;
    std::string globedRoom;
    std::string source;
    std::string rarity;
    std::string difficulty;
    bool progressive = true;
};

struct LastRound
{
    int round = 0;
    std::string winnerId;
    std::string winnerName;
    int percent = 0;
    int target = 0;
    std::string levelName;
};

struct ActiveRoom
{
    std::string host;
    std::string hostId;
    bool active = false;
    bool isHost = false;
    std::string code;
    std::string globedRoom;
    int levelId = 0;
    std::string levelName;
    std::string levelAuthor;
    int target = 1;
    int startTarget = 0;
    int rounds = 1;
    int round = 0;
    bool progressive = true;
    std::string source = "regular";
    std::string rarity;
    std::string difficulty = "any";
    bool demons = false;
    int maxPlayers = 0;
    std::string state = "lobby";   // lobby | playing | finished
    std::int64_t pot = 0;
    std::int64_t myBet = 0;
    bool imReady = false;
    LastRound lastRound;
    std::string winnerId;
    std::string winnerName;
    std::vector<std::string> playerIds;
    std::vector<std::string> playerNames;
    std::vector<int> playerPercents;
    std::vector<int> playerScores;
    std::vector<bool> playerReady;
    std::vector<std::int64_t> playerBets;
    std::vector<int> playerIconIds;
    std::vector<int> playerCol1;
    std::vector<int> playerCol2;
    std::vector<int> playerGlow;
    // modes / teams / elimination / history
    std::string mode = "classic";     // classic | teams | elimination
    int teamCount = 0;
    struct Team { int id = 0; std::string name; int score = 0; int members = 0; };
    std::vector<Team> teams;
    std::vector<int> playerTeams;
    std::vector<bool> playerEliminated;
    std::vector<bool> playerCleared;
    std::vector<int> playerAccounts;   // GD account ids (Globed filter)
    std::int64_t roundLeftMs = 0;
    int roundSeconds = 0;
    std::vector<std::string> winners;
    struct HistoryRow { int round = 0; std::string kind, levelName, winnerName, eliminated; int percent = 0, target = 0, team = -1; };
    std::vector<HistoryRow> history;
    struct GameRow { std::string mode, winnerName; int rounds = 0; std::int64_t pot = 0; };
    std::vector<GameRow> games;
    struct Event { int seq = 0; std::string kind, text, playerId; };
    std::vector<Event> events;

    int indexOf(const std::string& id) const {
        for (size_t i = 0; i < playerIds.size(); ++i) if (playerIds[i] == id) return static_cast<int>(i);
        return -1;
    }
};

inline ActiveRoom& state() { static ActiveRoom s; return s; }
inline void clearState() { state() = ActiveRoom{}; }

inline std::int64_t jsonId(const matjson::Value& v) {
    if (auto n = v.asInt()) return n.unwrap();
    if (auto d = v.asDouble()) return static_cast<std::int64_t>(d.unwrap());
    if (auto sv = v.asString()) { try { return std::stoll(sv.unwrap()); } catch (...) {} }
    return 0;
}

inline void applyRoomJson(const matjson::Value& r, ActiveRoom& out) {
    out = ActiveRoom{};
    out.active = true;
    out.code = r["code"].asString().unwrapOr("");
    out.hostId = r["hostId"].asString().unwrapOr("");
    out.host = r["hostName"].asString().unwrapOr("");
    out.globedRoom = r["globedRoom"].asString().unwrapOr("");
    out.isHost = !out.hostId.empty() && out.hostId == discordId();
    out.state = r["state"].asString().unwrapOr("lobby");
    out.pot = static_cast<std::int64_t>(r["pot"].asInt().unwrapOr(0));
    out.progressive = r["progressive"].asBool().unwrapOr(true);
    out.source = r["source"].asString().unwrapOr("regular");
    out.rarity = r["rarity"].asString().unwrapOr("");
    out.difficulty = r["difficulty"].asString().unwrapOr("any");
    out.demons = r["demons"].asBool().unwrapOr(false);
    out.maxPlayers = static_cast<int>(r["maxPlayers"].asInt().unwrapOr(0));
    out.rounds = static_cast<int>(r["rounds"].asInt().unwrapOr(1));
    out.round = static_cast<int>(r["round"].asInt().unwrapOr(0));
    out.target = static_cast<int>(r["target"].asInt().unwrapOr(0));
    out.startTarget = static_cast<int>(r["startTarget"].asInt().unwrapOr(0));
    out.winnerId = r["winnerId"].asString().unwrapOr("");
    out.winnerName = r["winnerName"].asString().unwrapOr("");
    out.mode = r["mode"].asString().unwrapOr("classic");
    out.teamCount = static_cast<int>(r["teamCount"].asInt().unwrapOr(0));
    out.roundSeconds = static_cast<int>(r["roundSeconds"].asInt().unwrapOr(0));
    out.roundLeftMs = static_cast<std::int64_t>(r["roundLeftMs"].asInt().unwrapOr(0));
    if (auto arr = r["teams"].asArray())
        for (const auto& t : *arr)
            out.teams.push_back({ static_cast<int>(t["id"].asInt().unwrapOr(0)), t["name"].asString().unwrapOr(""),
                static_cast<int>(t["score"].asInt().unwrapOr(0)), static_cast<int>(t["members"].asInt().unwrapOr(0)) });
    if (auto arr = r["winners"].asArray())
        for (const auto& w : *arr) out.winners.push_back(w.asString().unwrapOr(""));
    if (auto arr = r["history"].asArray())
        for (const auto& h : *arr) {
            ActiveRoom::HistoryRow row;
            row.round = static_cast<int>(h["round"].asInt().unwrapOr(0));
            row.kind = h["kind"].asString().unwrapOr("win");
            row.levelName = h["levelName"].asString().unwrapOr("");
            row.winnerName = h["winnerName"].asString().unwrapOr("");
            row.percent = static_cast<int>(h["percent"].asInt().unwrapOr(0));
            row.target = static_cast<int>(h["target"].asInt().unwrapOr(0));
            row.team = h["team"].isNumber() ? static_cast<int>(h["team"].asInt().unwrapOr(-1)) : -1;
            if (auto el = h["eliminated"].asArray()) {
                for (const auto& e : *el) { if (!row.eliminated.empty()) row.eliminated += ", "; row.eliminated += e.asString().unwrapOr(""); }
            }
            out.history.push_back(std::move(row));
        }
    if (auto arr = r["games"].asArray())
        for (const auto& g : *arr)
            out.games.push_back({ g["mode"].asString().unwrapOr("classic"), g["winnerName"].asString().unwrapOr(""),
                static_cast<int>(g["rounds"].asInt().unwrapOr(0)), static_cast<std::int64_t>(g["pot"].asInt().unwrapOr(0)) });
    if (auto arr = r["events"].asArray())
        for (const auto& e : *arr)
            out.events.push_back({ static_cast<int>(e["seq"].asInt().unwrapOr(0)), e["kind"].asString().unwrapOr(""),
                e["text"].asString().unwrapOr(""), e["playerId"].asString().unwrapOr(e["winnerId"].asString().unwrapOr("")) });

    const auto& lvl = out.state == "playing" ? r["level"] : r["lastLevel"];
    if (lvl.isObject()) {
        out.levelId = static_cast<int>(jsonId(lvl["id"]));
        out.levelName = lvl["name"].asString().unwrapOr("");
        out.levelAuthor = lvl["author"].asString().unwrapOr("");
    }
    if (r["lastRound"].isObject()) {
        const auto& lr = r["lastRound"];
        out.lastRound.round = static_cast<int>(lr["round"].asInt().unwrapOr(0));
        out.lastRound.winnerId = lr["winnerId"].asString().unwrapOr("");
        out.lastRound.winnerName = lr["winnerName"].asString().unwrapOr("");
        out.lastRound.percent = static_cast<int>(lr["percent"].asInt().unwrapOr(0));
        out.lastRound.target = static_cast<int>(lr["target"].asInt().unwrapOr(0));
        out.lastRound.levelName = lr["levelName"].asString().unwrapOr("");
    }

    if (auto arr = r["players"].asArray()) {
        for (const auto& p : *arr) {
            const auto id = p["discordId"].asString().unwrapOr("?");
            auto name = p["name"].asString().unwrapOr("");
            if (name.empty()) name = "Player";
            out.playerIds.push_back(id);
            out.playerNames.push_back(name);
            out.playerPercents.push_back(static_cast<int>(p["percent"].asInt().unwrapOr(0)));
            out.playerScores.push_back(static_cast<int>(p["score"].asInt().unwrapOr(0)));
            out.playerReady.push_back(p["ready"].asBool().unwrapOr(false));
            out.playerBets.push_back(static_cast<std::int64_t>(p["bet"].asInt().unwrapOr(0)));
            out.playerIconIds.push_back(static_cast<int>(p["iconId"].asInt().unwrapOr(1)));
            out.playerCol1.push_back(static_cast<int>(p["color1"].asInt().unwrapOr(1)));
            out.playerCol2.push_back(static_cast<int>(p["color2"].asInt().unwrapOr(3)));
            out.playerGlow.push_back(static_cast<int>(p["glow"].asInt().unwrapOr(0)));
            out.playerTeams.push_back(static_cast<int>(p["team"].asInt().unwrapOr(0)));
            out.playerEliminated.push_back(p["eliminated"].asBool().unwrapOr(false));
            out.playerCleared.push_back(p["cleared"].asBool().unwrapOr(false));
            {
                int acc = 0;
                try { acc = std::stoi(p["gdAccountId"].asString().unwrapOr("0")); } catch (...) {}
                out.playerAccounts.push_back(acc);
            }
            if (id == out.hostId && out.host.empty()) out.host = name;
        }
    }
    const std::string me = discordId();
    for (size_t i = 0; i < out.playerIds.size(); ++i)
        if (out.playerIds[i] == me) { out.imReady = out.playerReady[i]; out.myBet = out.playerBets[i]; break; }
}

inline matjson::Value idBody() {
    matjson::Value b = matjson::Value::object();
    b.set("discordId", discordId());
    b.set("gdAccountId", gdAccountId());
    return b;
}

inline void addIcon(matjson::Value& b) {
    if (auto gm = GameManager::get()) {
        b.set("iconId", static_cast<int>(gm->m_playerFrame));
        b.set("color1", static_cast<int>(gm->m_playerColor));
        b.set("color2", static_cast<int>(gm->m_playerColor2));
        b.set("glow", gm->m_playerGlow ? static_cast<int>(gm->m_playerGlowColor) : 0);
    }
}


// ------------------------------------------------------- level candidates --
//
// The bot host can be blocked by RobTop (and GDBrowser now 403s), so the
// player's game rolls the room levels itself, the same way GD-Roulette does:
// POST getGJLevels21.php (type=0 search, star=1, len=0..4, diff/demonFilter,
// empty User-Agent), random page within the real page count. One rolled
// level is kept ready and sent with start / ready / skip / progress; the bot
// uses it for the next round (or rolls on its own if none came).
struct Candidate {
    std::string id, name, author, difficulty, length;
    int stars = 0, coins = 0;
    bool demon = false, demonlist = false;
};
inline std::optional<Candidate>& candidate() { static std::optional<Candidate> c; return c; }
inline std::string& candidateKey() { static std::string k; return k; }   // room filters it was rolled for
inline bool& candidateRolling() { static bool b = false; return b; }

inline void attachCandidate(matjson::Value& b) {
    const auto& c = candidate();
    if (!c) return;
    matjson::Value j = matjson::Value::object();
    j.set("id", c->id); j.set("name", c->name); j.set("author", c->author);
    j.set("difficulty", c->difficulty); j.set("length", c->length);
    j.set("stars", c->stars); j.set("coins", c->coins);
    j.set("demon", c->demon); j.set("demonlist", c->demonlist);
    b.set("candidate", j);
}

namespace roll {
inline constexpr std::string_view LEVELS_URL = "https://www.boomlings.com/database/getGJLevels21.php";
inline constexpr std::string_view SECRET = "Wmfd2893gb7";

inline int rnd(int a, int b) {
    static std::mt19937 rng(std::random_device{}());
    return std::uniform_int_distribution<int>(a, std::max(a, b))(rng);
}

inline void post(std::string body, std::function<void(std::string)> cb) {
    web::WebRequest req;
    req.userAgent("");
    req.header("Content-Type", "application/x-www-form-urlencoded");
    req.bodyString(std::move(body));
    geode::async::spawn(
        [req = std::move(req)]() mutable -> web::WebFuture { return req.post(std::string(LEVELS_URL)); },
        [cb = std::move(cb)](web::WebResponse res) {
            std::string out = res.ok() ? res.string().unwrapOr("") : std::string();
            geode::queueInMainThread([cb, out = std::move(out)] { cb(out); });
        });
}

// bot DIFF_PARAMS keys -> RobTop query (same table the bot uses)
inline std::string diffQuery(const std::string& key, bool demons) {
    static const std::map<std::string, std::string> m = {
        { "auto", "&diff=-3" }, { "easy", "&diff=1" }, { "normal", "&diff=2" }, { "hard", "&diff=3" },
        { "harder", "&diff=4" }, { "insane", "&diff=5" },
        { "easy_demon", "&diff=-2&demonFilter=1" }, { "medium_demon", "&diff=-2&demonFilter=2" },
        { "hard_demon", "&diff=-2&demonFilter=3" }, { "insane_demon", "&diff=-2&demonFilter=4" },
        { "extreme_demon", "&diff=-2&demonFilter=5" } };
    if (auto it = m.find(key); it != m.end()) return it->second;
    return demons ? "" : "&diff=1,2,3,4,5";
}
inline std::string rarityQuery(const std::string& r) {
    if (r == "featured") return "&featured=1";
    if (r == "epic") return "&epic=1";
    if (r == "legendary") return "&legendary=1";
    if (r == "mythic") return "&mythic=1";
    return "";
}
inline std::string diffLabel(const rtrp::objects::LevelObject& l) {
    if (l.isDemon) {
        switch (l.demonDifficulty) { case 3: return "Easy Demon"; case 4: return "Medium Demon"; case 5: return "Insane Demon"; case 6: return "Extreme Demon"; default: return "Hard Demon"; }
    }
    if (l.isAuto) return "Auto";
    const int face = l.difficultyDenominator > 0 ? l.difficultyNumerator / l.difficultyDenominator : 0;
    static const char* faces[] = { "N/A", "Easy", "Normal", "Hard", "Harder", "Insane" };
    return faces[std::clamp(face, 0, 5)];
}
inline Candidate toCandidate(const rtrp::responses::LevelResponse& r, size_t i) {
    const auto& l = r.levels[i];
    static const char* lens[] = { "Tiny", "Short", "Medium", "Long", "XL" };
    Candidate c;
    c.id = std::to_string(l.levelID);
    c.name = l.name;
    for (const auto& cr : r.creators) if (cr.userID == l.creatorUserID) { c.author = cr.name; break; }
    c.difficulty = diffLabel(l);
    c.length = lens[std::clamp(l.length, 0, 4)];
    c.stars = l.stars;
    c.coins = l.coins;
    c.demon = l.isDemon;
    return c;
}
inline std::map<std::string, int>& pageCounts() { static std::map<std::string, int> m; return m; }

inline void finish(const std::string& key, std::optional<Candidate> c) {
    candidateRolling() = false;
    if (c && state().active && key == candidateKey()) candidate() = std::move(c);
}

inline void pickFromPage(const std::string& key, const std::string& base, int pages) {
    post(fmt::format("{}&page={}", base, rnd(0, pages - 1)), [key](std::string resp) {
        if (resp.empty() || resp == "-1") return finish(key, std::nullopt);
        auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp);
        if (parsed.isErr()) return finish(key, std::nullopt);
        auto r = parsed.unwrap();
        std::vector<size_t> ok;
        for (size_t i = 0; i < r.levels.size(); ++i) {
            const auto& l = r.levels[i];
            if (static_cast<int>(l.levelID) == state().levelId) continue;
            if (l.length > 4) continue; // platformer
            ok.push_back(i);
        }
        if (ok.empty()) return finish(key, std::nullopt);
        const auto pick = ok[rnd(0, static_cast<int>(ok.size()) - 1)];
        levels::storeFromResponse(resp, r.levels[pick].levelID); // songs + full level for the round
        finish(key, toCandidate(r, pick));
    });
}
} // namespace roll

// Roll a fresh candidate for the room's filters (no-op while one is ready).
inline void rollCandidate() {
    const auto& st = state();
    if (!st.active || candidateRolling()) return;
    const auto key = fmt::format("{}|{}|{}|{}", st.source, st.difficulty, st.demons, st.rarity);
    if (candidate() && candidateKey() == key && std::to_string(st.levelId) != candidate()->id) return;
    candidate().reset();
    candidateKey() = key;
    candidateRolling() = true;

    if (st.source == "demonlist") {
        // Pointercrate main list -> RobTop level info (GD-Roulette demon list flow)
        web::WebRequest req;
        geode::async::spawn(
            [req = std::move(req)]() mutable -> web::WebFuture { return req.get("https://pointercrate.com/api/v2/demons/listed/?limit=75&after=0"); },
            [key](web::WebResponse res) {
                auto j = res.json();
                std::vector<int> ids;
                if (j.isOk()) if (auto arr = j.unwrap().asArray())
                    for (const auto& d : *arr) if (auto id = d["level_id"].asInt()) ids.push_back(static_cast<int>(id.unwrap()));
                geode::queueInMainThread([key, ids] {
                    if (ids.empty()) return roll::finish(key, std::nullopt);
                    const int id = ids[roll::rnd(0, static_cast<int>(ids.size()) - 1)];
                    roll::post(fmt::format("secret={}&type=0&str={}", roll::SECRET, id), [key](std::string resp) {
                        auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp);
                        if (resp.empty() || resp == "-1" || parsed.isErr() || parsed.unwrap().levels.empty())
                            return roll::finish(key, std::nullopt);
                        levels::storeFromResponse(resp, parsed.unwrap().levels[0].levelID);
                        auto c = roll::toCandidate(parsed.unwrap(), 0);
                        c.demonlist = true;
                        roll::finish(key, c);
                    });
                });
            });
        return;
    }

    const std::string type = st.source == "trending" ? "3" : "0";
    const auto base = fmt::format("secret={}&type={}&star=1&len=0,1,2,3,4{}{}", roll::SECRET, type,
        roll::diffQuery(st.difficulty, st.demons), roll::rarityQuery(st.rarity));
    if (auto it = roll::pageCounts().find(base); it != roll::pageCounts().end())
        return roll::pickFromPage(key, base, it->second);
    roll::post(base + "&page=0", [key, base, type](std::string resp) {
        int pages = 1;
        if (!resp.empty() && resp != "-1") {
            if (auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp); parsed.isOk()) {
                const auto& pg = parsed.unwrap().page;
                if (pg.countPerPage > 0) pages = std::max(1, static_cast<int>(std::ceil(pg.pages / static_cast<double>(pg.countPerPage))));
            }
        }
        pages = std::min(pages, type == "3" ? 20 : 1000);
        roll::pageCounts()[base] = pages;
        roll::pickFromPage(key, base, pages);
    });
}

inline std::string errOf(const matjson::Value& body, const char* fallback) {
    return body.isObject() ? body["error"].asString().unwrapOr(fallback) : std::string(fallback);
}

// Tell room-mates on Globed that the room changed (they re-poll at once).
inline void pingRoomMates() {
    const auto& s = state();
    if (s.active) broadcastRoom(2, s.code, s.host);
}

inline void fetchList(const std::function<void(bool, std::vector<BrowseItem>)>& cb) {
    Bridge::get("rooms", [cb](bool ok, int, matjson::Value body) {
        std::vector<BrowseItem> items;
        if (ok && body.isObject()) {
            if (auto arr = body["rooms"].asArray()) {
                for (const auto& r : *arr) {
                    BrowseItem it;
                    it.code = r["code"].asString().unwrapOr("");
                    it.hostId = r["hostId"].asString().unwrapOr("");
                    it.host = r["hostName"].asString().unwrapOr("");
                    it.state = r["state"].asString().unwrapOr("lobby");
                    it.maxPlayers = static_cast<int>(r["maxPlayers"].asInt().unwrapOr(0));
                    it.rounds = static_cast<int>(r["rounds"].asInt().unwrapOr(1));
                    it.round = static_cast<int>(r["round"].asInt().unwrapOr(0));
                    it.pot = static_cast<std::int64_t>(r["pot"].asInt().unwrapOr(0));
                    it.globedRoom = r["globedRoom"].asString().unwrapOr("");
                    it.source = r["source"].asString().unwrapOr("regular");
                    it.rarity = r["rarity"].asString().unwrapOr("");
                    it.difficulty = r["difficulty"].asString().unwrapOr("any");
                    it.progressive = r["progressive"].asBool().unwrapOr(true);
                    if (auto pl = r["players"].asArray()) {
                        it.players = static_cast<int>((*pl).size());
                        if (it.host.empty())
                            for (const auto& p : *pl)
                                if (p["discordId"].asString().unwrapOr("") == it.hostId)
                                    it.host = p["name"].asString().unwrapOr("");
                    }
                    if (it.host.empty()) it.host = "Room";
                    if (!it.code.empty()) items.push_back(std::move(it));
                }
            }
        }
        // Rooms opened from your current Globed room float to the top.
        const auto myRoom = currentRoomId();
        std::stable_sort(items.begin(), items.end(), [&](const BrowseItem& a, const BrowseItem& b) {
            const bool ga = myRoom != "0" && a.globedRoom == myRoom;
            const bool gb = myRoom != "0" && b.globedRoom == myRoom;
            return ga && !gb;
        });
        cb(ok, std::move(items));
    });
}

using RoomCb = std::function<void(bool, std::string)>;

inline void handleRoomReply(bool ok, const matjson::Value& body, const char* fallback, const RoomCb& cb) {
    if (ok && body.isObject() && body["room"].isObject()) {
        applyRoomJson(body["room"], state());
        if (cb) cb(true, "");
    } else if (cb) {
        cb(false, errOf(body, fallback));
    }
}

inline void join(const std::string& code, const RoomCb& cb) {
    auto b = idBody();
    b.set("code", code);
    b.set("name", playerName());
    addIcon(b);
    if (isInRoom()) b.set("globedRoom", currentRoomId());
    Bridge::post("rooms/join", b, [cb](bool ok, int, matjson::Value body) {
        handleRoomReply(ok, body, "Join failed", [cb](bool ok2, std::string err) {
            if (ok2) { registerGlobedEvents(); pingRoomMates(); }
            if (cb) cb(ok2, err);
        });
    });
}

struct CreateOptions {
    std::string source = "regular";
    std::string difficulty = "any";
    std::string rarity = "any";
    bool demons = false;
    bool progressive = true;
    int maxPlayers = 8;
    int rounds = 3;
    int startTarget = 0; // 0 = the bot's default for the level
    std::string mode = "classic";
    int teamCount = 2;
    int roundSeconds = 180;
};

inline void create(const CreateOptions& o, const RoomCb& cb) {
    auto b = idBody();
    b.set("name", playerName());
    addIcon(b);
    b.set("globedRoom", isInRoom() ? currentRoomId() : "");
    b.set("progressive", o.progressive);
    b.set("source", o.source);
    b.set("rarity", o.rarity);
    b.set("difficulty", o.difficulty);
    b.set("demons", o.demons);
    b.set("maxPlayers", o.maxPlayers);
    b.set("rounds", o.rounds);
    b.set("startTarget", o.startTarget);
    b.set("mode", o.mode);
    b.set("teamCount", o.teamCount);
    b.set("roundSeconds", o.roundSeconds);
    Bridge::post("rooms/create", b, [cb](bool ok, int, matjson::Value body) {
        handleRoomReply(ok, body, "Create failed", [cb](bool ok2, std::string err) {
            if (ok2) {
                registerGlobedEvents();
                broadcastRoom(1, state().code, playerName()); // Globed invite
            }
            if (cb) cb(ok2, err);
        });
    });
}

inline void leave(const std::function<void(bool)>& cb = nullptr) {
    if (!state().active) { if (cb) cb(true); return; }
    auto b = idBody();
    b.set("code", state().code);
    const auto code = state().code;
    Bridge::post("rooms/leave", b, [cb, code](bool ok, int, matjson::Value) {
        broadcastRoom(2, code, "");
        clearState();
        refreshBalance(); // a lobby bet comes back
        if (cb) cb(ok);
    });
}

inline void fetchRoom(const std::string& code, const std::function<void(bool, matjson::Value)>& cb) {
    Bridge::get("rooms", [code, cb](bool ok, int, matjson::Value body) {
        if (!ok || !body.isObject()) { cb(false, {}); return; }
        if (auto arr = body["rooms"].asArray()) {
            for (const auto& r : *arr)
                if (r["code"].asString().unwrapOr("") == code) { cb(true, r); return; }
        }
        cb(true, matjson::Value()); // reachable, but the room is gone
    });
}

inline void ready(bool isReady, const RoomCb& cb) {
    auto b = idBody();
    b.set("code", state().code);
    b.set("ready", isReady);
    attachCandidate(b);
    Bridge::post("rooms/ready", b, [cb](bool ok, int, matjson::Value body) {
        handleRoomReply(ok, body, "Ready failed", [cb, body](bool ok2, std::string err) {
            if (ok2) pingRoomMates();
            if (cb) cb(ok2, err);
        });
    });
}

inline void switchTeam(const RoomCb& cb) {
    auto b = idBody();
    b.set("code", state().code);
    Bridge::post("rooms/team", b, [cb](bool ok, int, matjson::Value body) {
        handleRoomReply(ok, body, "Team switch failed", [cb](bool ok2, std::string err) {
            if (ok2) pingRoomMates();
            if (cb) cb(ok2, err);
        });
    });
}

inline void startGame(const RoomCb& cb) {
    auto b = idBody();
    b.set("code", state().code);
    attachCandidate(b);
    Bridge::post("rooms/start", b, [cb](bool ok, int, matjson::Value body) {
        handleRoomReply(ok, body, "Start failed", [cb](bool ok2, std::string err) {
            if (ok2) pingRoomMates();
            if (cb) cb(ok2, err);
        });
    });
}

inline void skip(const std::function<void(bool, int, int, std::string)>& cb) {
    auto b = idBody();
    b.set("code", state().code);
    attachCandidate(b);
    Bridge::post("rooms/skip", b, [cb](bool ok, int, matjson::Value body) {
        if (ok && body.isObject()) {
            if (body["room"].isObject()) applyRoomJson(body["room"], state());
            if (body["skipped"].asBool().unwrapOr(false)) pingRoomMates();
            cb(true, static_cast<int>(body["votes"].asInt().unwrapOr(0)),
               static_cast<int>(body["needed"].asInt().unwrapOr(0)), "");
        } else {
            cb(false, 0, 0, errOf(body, "Skip failed"));
        }
    });
}

inline void bet(std::int64_t amount, const std::function<void(bool, std::int64_t, std::string)>& cb) {
    auto b = idBody();
    b.set("code", state().code);
    b.set("amount", amount);
    Bridge::post("rooms/bet", b, [cb](bool ok, int, matjson::Value body) {
        std::int64_t bal = -1;
        if (ok && body.isObject()) {
            if (body["room"].isObject()) applyRoomJson(body["room"], state());
            bal = static_cast<std::int64_t>(body["balance"].asInt().unwrapOr(-1));
            if (bal >= 0) setGlimmer(bal, 0);
            pingRoomMates();
            cb(true, bal, "");
        } else {
            cb(false, bal, errOf(body, "Bet failed"));
        }
    });
}

// Game-result presentation guard (the reporter's reply and the director's
// poll can both see the same finish).
inline std::string& presentedFinishKey() { static std::string s; return s; }

// Every death / completion on the CURRENT round level goes to the bot; the
// bot decides whether it wins the round.
inline void reportProgress(int levelId, const std::string& levelName, int percent, bool completed) {
    auto& s = state();
    if (!s.active || s.state != "playing" || levelId != s.levelId) return;
    auto b = idBody();
    b.set("code", s.code);
    b.set("levelId", std::to_string(levelId));
    b.set("levelName", levelName);
    b.set("percent", percent);
    b.set("completed", completed);
    attachCandidate(b);
    const auto potBefore = s.pot;
    Bridge::post("rooms/progress", b, [potBefore](bool ok, int, matjson::Value body) {
        if (!ok || !body.isObject()) return;
        const int roundBefore = state().round;
        if (body["room"].isObject()) applyRoomJson(body["room"], state());
        const bool roundWon = body["roundWon"].asBool().unwrapOr(false);
        const bool gameOver = body["gameOver"].asBool().unwrapOr(false);
        const auto potWon = static_cast<std::int64_t>(body["potWon"].asInt().unwrapOr(0));
        if (roundWon) {
            pingRoomMates();
            Notification::create(fmt::format("You won round {}!", roundBefore).c_str(),
                NotificationIcon::Success, 2.f)->show();
        }
        if (body["cleared"].asBool().unwrapOr(false)) {
            pingRoomMates();
            Notification::create(fmt::format("Cleared {}% - you survive round {}", state().target, roundBefore).c_str(),
                NotificationIcon::Success, 2.f)->show();
        }
        if (gameOver && potWon > 0) {
            presentedFinishKey() = state().code + "#" + state().winnerId + "#" + std::to_string(state().lastRound.round);
            syncBalance([potWon](bool ok2, int, matjson::Value bb) {
                if (ok2 && bb.isObject()) setGlimmer(bb["balance"].asInt().unwrapOr(-1), 0);
                presentGlimmerWin(potWon, 0, nullptr);
            });
        }
        (void)potBefore;
    });
}

// ------------------------------------------------------------- director --
//
// Lives for the whole session (not tied to a layer). While you are in a
// room it polls every 2.5 s, diffs the room and reacts:
//   * someone won a round       -> notification ("Bob won round 2 at 47%")
//   * a new round / first round -> "Round 3/5: <level> - reach 48%", then
//                                  takes you to that level (from a level you
//                                  are playing: a short countdown first)
//   * game finished             -> result notification, pot animation if
//                                  it is yours, balance refresh
//   * room vanished             -> state cleared
class RoomDirector : public cocos2d::CCObject {
public:
    static RoomDirector* get() {
        static RoomDirector* s = [] {
            auto* d = new RoomDirector();
            cocos2d::CCDirector::get()->getScheduler()->scheduleSelector(
                schedule_selector(RoomDirector::tick), d, 0.5f, false);
            return d;
        }();
        return s;
    }

    // Kick an immediate poll (Globed ping, popup actions).
    void pokeNow() { m_wait = 0.f; }

    // Whether a pending auto-travel is counting down (HUD shows it).
    float travelCountdown() const { return m_travelIn; }

    void travelNow() { m_travelIn = 0.01f; }

    void tick(float dt) {
        if (m_travelIn > 0.f) {
            m_travelIn -= dt;
            if (m_travelIn <= 0.f) { m_travelIn = 0.f; this->goToRoundLevel(); }
        }
        if (m_returnIn > 0.f) {
            m_returnIn -= dt;
            if (m_returnIn <= 0.f) { m_returnIn = 0.f; this->returnToRoomScreen(); }
        }
        if (!state().active) return;
        rollCandidate();   // keep one RobTop-rolled level ready for the next round
        if (m_busy) return;
        m_wait -= dt;
        if (m_wait > 0.f) return;
        m_wait = 2.5f;
        m_busy = true;
        const auto code = state().code;
        fetchRoom(code, [this, code](bool ok, matjson::Value r) {
            m_busy = false;
            if (!ok || !state().active || state().code != code) return;
            if (!r.isObject()) {
                clearState();
                Notification::create("The roulette room closed", NotificationIcon::Info, 2.f)->show();
                return;
            }
            const ActiveRoom before = state();
            applyRoomJson(r, state());
            this->diff(before, state());
        });
    }

    // Called right after each poll.
    void diff(const ActiveRoom& a, const ActiveRoom& b) {
        const std::string me = discordId();
        // 1) the room's event feed -> notifications (skip our own + noise)
        if (m_resetEvents) {
            m_resetEvents = false;
            m_seenEvent = 0;
            for (const auto& e : b.events) m_seenEvent = std::max(m_seenEvent, e.seq);
        }
        int maxSeq = m_seenEvent;
        for (const auto& e : b.events) {
            if (e.seq <= m_seenEvent) continue;
            maxSeq = std::max(maxSeq, e.seq);
            if (!a.active) continue;                       // first poll after joining: no backlog spam
            if (e.kind == "round" || e.kind == "join" || e.kind == "open") continue; // shown elsewhere / noise
            if (e.playerId == me && (e.kind == "round-won" || e.kind == "cleared")) continue; // our reply already said it
            const bool aboutMe = e.playerId == me;
            Notification::create(e.text.c_str(),
                e.kind == "out" ? (aboutMe ? NotificationIcon::Error : NotificationIcon::Warning) : NotificationIcon::Info,
                2.2f)->show();
        }
        m_seenEvent = maxSeq;

        // 2) a new round (or the first one) -> banner + travel
        if (b.state == "playing" && b.levelId > 0 &&
            (b.levelId != m_seenLevel || b.round != m_seenRound)) {
            m_seenLevel = b.levelId;
            m_seenRound = b.round;
            const int idx = b.indexOf(me);
            const bool out = idx >= 0 && b.playerEliminated[idx];
            Notification::create(out ? fmt::format("Round {}: you are out - spectating", b.round).c_str()
                : fmt::format("Round {}{}: {} - reach {}%", b.round,
                    b.mode == "elimination" ? std::string("") : fmt::format("/{}", b.rounds),
                    b.levelName, b.target).c_str(), NotificationIcon::Info, 2.5f)->show();
            if (!out) this->scheduleTravel();
        }

        // 3) game over -> pot animation if it is ours, then back to the room screen
        if (b.state == "finished" && a.state == "playing") {
            const auto key = b.code + "#" + b.winnerId + "#" + std::to_string(b.lastRound.round);
            const bool won = std::find(b.winners.begin(), b.winners.end(), me) != b.winners.end();
            if (won && presentedFinishKey() != key) {
                presentedFinishKey() = key;
                const auto n = std::max<std::int64_t>(1, static_cast<std::int64_t>(b.winners.size()));
                const auto amount = a.pot / n + a.myBet; // room rule: your share + your own bet again
                syncBalance([amount](bool ok, int, matjson::Value bb) {
                    if (ok && bb.isObject()) setGlimmer(bb["balance"].asInt().unwrapOr(-1), 0);
                    if (amount > 0) presentGlimmerWin(amount, 0, nullptr);
                });
            } else if (!won) {
                refreshBalance();
            }
            m_seenLevel = 0;
            m_travelIn = 0.f;
            m_returnIn = won ? 3.5f : 2.f; // let the result/animation play, then go back
        }
    }

    // Set by the roulette screen: open the rooms popup on it (returns false
    // when the roulette screen is not the running scene).
    static std::function<bool()>& openRoomsHook() { static std::function<bool()> f; return f; }

    void resetSeen() { m_seenLevel = 0; m_seenRound = 0; m_seenRoundWin = 0; m_seenEvent = 1 << 30; m_resetEvents = true; }

private:
    void scheduleTravel() {
        // Already on the round level? nothing to do.
        if (auto* pl = PlayLayer::get()) {
            if (pl->m_level && static_cast<int>(pl->m_level->m_levelID.value()) == state().levelId) return;
            m_travelIn = 4.f; // let the player read the round banner first
        } else {
            m_travelIn = 1.2f;
        }
    }

    // After the last round: out of the level, off the level page, back to the
    // roulette screen with the room popup (lobby) open.
    void returnToRoomScreen() {
        if (auto* pl = PlayLayer::get()) {
            if (!m_quitRequested) { m_quitRequested = true; pl->onQuit(); }
            m_returnIn = 1.1f;
            return;
        }
        m_quitRequested = false;
        auto* dir = cocos2d::CCDirector::get();
        auto* running = dir->getRunningScene();
        if (running && running->getChildByType<LevelInfoLayer>(0)) {
            dir->popSceneWithTransition(.5f, PopTransition::kPopTransitionFade);
            m_returnIn = 0.8f;
            return;
        }
        if (auto& hook = openRoomsHook(); hook) hook();
    }

    void goToRoundLevel() {
        const auto& st = state();
        if (!st.active || st.state != "playing" || st.levelId <= 0) return;
        if (auto* pl = PlayLayer::get()) {
            if (pl->m_level && static_cast<int>(pl->m_level->m_levelID.value()) == st.levelId) return;
            if (m_quitRequested) { m_travelIn = 0.4f; return; } // still fading out of the old level
            m_quitRequested = true;
            pl->onQuit();          // back to the menu that opened the level
            m_travelIn = 1.1f;     // then open the round level once that transition ends
            return;
        }
        m_quitRequested = false;
        // Full GD-parsed level (song info included) - fetched from RobTop if
        // this game did not roll it itself.
        if (m_opening) return;
        m_opening = true;
        const int id = st.levelId;
        const auto name = st.levelName;
        levels::fetchById(id, [this, id, name](GJGameLevel* lvl) {
            m_opening = false;
            if (!state().active || state().levelId != id || PlayLayer::get()) return;
            levels::openLevelPage(lvl ? lvl : levels::bareLevel(id, name));
        });
    }

    float m_wait = 0.f;
    float m_travelIn = 0.f;
    bool m_busy = false;
    bool m_quitRequested = false;
    bool m_resetEvents = true;
    float m_returnIn = 0.f;
    int m_seenEvent = 0;
    bool m_opening = false;
    int m_seenLevel = 0, m_seenRound = 0, m_seenRoundWin = 0;
};


// ------------------------------------------------------- in-level room hud --
//
// Top-left, under the bet badge, while you are in a room:
//   ROOM K7QX   ROUND 2/3   TARGET 42%
//   1  Bob        2 pts   41%
//   2  You        1 pts   12%
//   Next level in 3...            (when the round moved on)
class SorRoomHud : public cocos2d::CCNode {
public:
    static SorRoomHud* create(int levelId) {
        auto* n = new SorRoomHud();
        n->m_levelId = levelId;
        n->autorelease();
        n->init();
        return n;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setID("sor-room-hud"_spr);
        this->setAnchorPoint({ 0.f, 1.f });
        m_bg = ui::panel(168.f, 40.f, { 0, 0, 0 }, 110);
        m_bg->setAnchorPoint({ 0.f, 0.f });
        this->addChild(m_bg);
        m_head = CCLabelBMFont::create("", "goldFont.fnt");
        m_head->setAnchorPoint({ 0.f, .5f });
        this->addChild(m_head);
        for (int i = 0; i < 4; ++i) {
            auto* n = CCLabelBMFont::create("", "bigFont.fnt");
            n->setAnchorPoint({ 0.f, .5f });
            this->addChild(n);
            m_rows.push_back(n);
            auto* r = CCLabelBMFont::create("", "bigFont.fnt");
            r->setAnchorPoint({ 1.f, .5f });
            this->addChild(r);
            m_rowsR.push_back(r);
        }
        m_foot = CCLabelBMFont::create("", "bigFont.fnt");
        m_foot->setAnchorPoint({ 0.f, .5f });
        m_foot->setColor({ 255, 220, 120 });
        this->addChild(m_foot);
        this->schedule(schedule_selector(SorRoomHud::refresh), 0.25f);
        this->refresh(0.f);
        return true;
    }

    void refresh(float) {
        const auto& st = state();
        if (auto* pl = typeinfo_cast<PlayLayer*>(this->getParent()); pl && pl->m_isPracticeMode) {
            // still shown in practice, dimmed: practice runs never count
            this->setOpacity(150);
        }
        // Only on the room's round level (or while moving you to the next one).
        const bool onRound = st.active && st.state == "playing" && st.levelId == m_levelId;
        const bool moving = st.active && RoomDirector::get()->travelCountdown() > 0.f;
        if (!onRound && !moving) { this->setVisible(false); return; }
        this->setVisible(true);

        const float W = 168.f, pad = 6.f, rowH = 11.f;
        std::string head;
        if (st.state == "playing" && st.mode == "elimination") {
            const int sec = static_cast<int>(st.roundLeftMs / 1000);
            head = fmt::format("ROOM {}  R{}  {}%  {}:{:02}", st.code, st.round, st.target, sec / 60, sec % 60);
        } else if (st.state == "playing" && st.mode == "teams") {
            std::string t;
            for (const auto& tm : st.teams) { if (!t.empty()) t += "-"; t += std::to_string(tm.score); }
            head = fmt::format("ROOM {}  R{}/{}  {}%  ({})", st.code, st.round, st.rounds, st.target, t);
        } else if (st.state == "playing")
            head = fmt::format("ROOM {}  R{}/{}  {}%", st.code, st.round, st.rounds, st.target);
        else if (st.state == "finished")
            head = fmt::format("ROOM {}  FINISHED", st.code);
        else
            head = fmt::format("ROOM {}  LOBBY", st.code);
        m_head->setString(head.c_str());
        m_head->limitLabelWidth(W - 2 * pad, .42f, .1f);

        // standings: score, then best % this round
        std::vector<size_t> order(st.playerIds.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            if (st.playerEliminated[a] != st.playerEliminated[b]) return !st.playerEliminated[a];
            if (st.playerScores[a] != st.playerScores[b]) return st.playerScores[a] > st.playerScores[b];
            return st.playerPercents[a] > st.playerPercents[b];
        });
        const std::string me = discordId();
        int shown = 0;
        for (int i = 0; i < 4; ++i) {
            const bool on = i < static_cast<int>(order.size());
            m_rows[i]->setVisible(on);
            m_rowsR[i]->setVisible(on);
            if (!on) continue;
            const auto idx = order[i];
            const bool mine = st.playerIds[idx] == me;
            m_rows[i]->setString(fmt::format("{} {}", i + 1, mine ? std::string("You") : st.playerNames[idx]).c_str());
            m_rows[i]->limitLabelWidth(92.f, .3f, .1f);
            const bool out = st.playerEliminated[idx];
            const std::string right = out ? std::string("OUT")
                : (st.mode == "elimination" && st.playerCleared[idx]) ? std::string("SAFE")
                : st.mode == "elimination" ? fmt::format("{}%", st.playerPercents[idx])
                : fmt::format("{} pts  {}%", st.playerScores[idx], st.playerPercents[idx]);
            m_rowsR[i]->setString(right.c_str());
            m_rowsR[i]->limitLabelWidth(64.f, .3f, .1f);
            static const cocos2d::ccColor3B teamCols[] = { { 255, 90, 90 }, { 90, 160, 255 }, { 100, 230, 120 }, { 255, 215, 80 } };
            cocos2d::ccColor3B c = mine ? cocos2d::ccColor3B { 120, 255, 140 } : cocos2d::ccColor3B { 255, 255, 255 };
            if (st.mode == "teams") c = teamCols[std::clamp(st.playerTeams[idx], 0, 3)];
            if (out) c = { 130, 130, 130 };
            m_rows[i]->setColor(c);
            m_rowsR[i]->setColor(c);
            ++shown;
        }

        std::string foot;
        const float t = RoomDirector::get()->travelCountdown();
        if (t > 0.f) foot = fmt::format("Next level in {}...", static_cast<int>(std::ceil(t)));

        m_foot->setString(foot.c_str());
        m_foot->limitLabelWidth(W - 2 * pad, .3f, .1f);
        m_foot->setVisible(!foot.empty());

        const float h = pad + 12.f + shown * rowH + (foot.empty() ? 0.f : rowH) + pad;
        this->setContentSize({ W, h });
        ui::resize(m_bg, W, h);
        float y = h - pad - 6.f;
        m_head->setPosition({ pad, y });
        y -= 12.f;
        for (int i = 0; i < 4; ++i) {
            m_rows[i]->setPosition({ pad, y });
            m_rowsR[i]->setPosition({ W - pad, y });
            if (i < shown) y -= rowH;
        }
        m_foot->setPosition({ pad, y });
    }

    void setOpacity(GLubyte o) { m_bg->setOpacity(static_cast<GLubyte>(110 * o / 255)); }

private:
    int m_levelId = 0;
    cocos2d::extension::CCScale9Sprite* m_bg = nullptr;
    CCLabelBMFont* m_head = nullptr;
    CCLabelBMFont* m_foot = nullptr;
    std::vector<CCLabelBMFont*> m_rows, m_rowsR;
};

inline void attachRoomHud(PlayLayer* pl, int levelId) {
    if (!pl || pl->getChildByID("sor-room-hud"_spr)) return;
    auto* hud = SorRoomHud::create(levelId);
    const auto win = cocos2d::CCDirector::get()->getWinSize();
    hud->setPosition({ 6.f, win.height - 30.f }); // under the bet badge
    pl->addChild(hud, 1000);
}

// Round target for a level if it is the room's current round level (else 0).
inline int roundTargetFor(int levelId) {
    const auto& st = state();
    return (st.active && st.state == "playing" && st.levelId == levelId) ? st.target : 0;
}


// --------------------------------------------------------- globed filter --
//
// Globed has no "join this room" API for mods, but it lets us hide remote
// players. While you are in a roulette room, everyone in the level who is
// NOT in your room is hidden (player + progress-bar icon) - the same view
// as being in a private Globed room. Leaving the room shows everyone again.
class SorGlobedRoomFilter : public cocos2d::CCNode {
public:
    static SorGlobedRoomFilter* create() {
        auto* n = new SorGlobedRoomFilter();
        n->autorelease();
        n->setID("sor-globed-filter"_spr);
        n->schedule(schedule_selector(SorGlobedRoomFilter::tick), 0.5f);
        return n;
    }
    void tick(float) {
#if SORROW_GLOBED_HEADERS
        if (!globedAvailable() || !globed::api::game::isActive()) return;
        const auto& st = state();
        const bool filtering = st.active;
        auto* pl = PlayLayer::get();
        for (auto& rp : globed::api::game::getPlayers()) {
            if (!rp) continue;
            const int acc = globed::api::player::getAccountId(rp.get());
            const bool inRoom = std::find(st.playerAccounts.begin(), st.playerAccounts.end(), acc) != st.playerAccounts.end();
            const bool hide = filtering && !inRoom;
            if (hide != m_hidden.contains(acc)) {
                globed::api::player::setForceHide(rp.get(), hide);
                if (hide) m_hidden.insert(acc); else m_hidden.erase(acc);
            }
            if (pl && pl->m_progressBar) {
                if (auto* icon = pl->m_progressBar->getChildByIDRecursive(
                        fmt::format("dankmeme.globed2/remote-player-progress-{}", acc)))
                    icon->setVisible(!hide);
            }
        }
#endif
    }
private:
    std::set<int> m_hidden;
};

inline void attachGlobedFilter(PlayLayer* pl) {
    if (pl && !pl->getChildByID("sor-globed-filter"_spr)) pl->addChild(SorGlobedRoomFilter::create());
}

// Discord /gd grouproulette started over Globed: silently join its room.
inline void joinByCode(const std::string& code) {
    auto b = idBody();
    b.set("code", code);
    b.set("name", playerName());
    Bridge::post("rooms/join", b, nullptr);
}

// Globed: re-poll on room-mate changes, show invites.
inline void listenRoomEvents() {
#if SORROW_GLOBED_HEADERS
    if (!globedAvailable()) return;
    static bool done = false;
    if (done) return;
    done = true;
    registerGlobedEvents();
    SorrowRoomEvent::listen([](const SorrowRoomEvent& ev) {
        const auto& s = state();
        if (ev.kind == 2 && s.active && s.code == ev.code) {
            RoomDirector::get()->pokeNow();
        } else if (ev.kind == 1 && !s.active && !ev.code.empty()) {
            Notification::create(fmt::format("{} opened roulette room {} - Roulette > Multiplayer",
                ev.host.empty() ? std::string("Someone") : ev.host, ev.code).c_str(),
                NotificationIcon::Info, 4.f)->show();
        }
    }).leak();
#endif
}

// ---------- Discord-session reporting (independent of roulette mode) ----------

// Mirror of the bot-side session state, refreshed when a level opens.
struct SessionMirror
{
    bool checked = false;
    bool active = false;
    int levelId = 0;
};

inline SessionMirror& session() { static SessionMirror s; return s; }

inline void checkSession() {
    Bridge::getRaw(std::string("/bridge/sorrow/session?discordId=") + Bridge::q(discordId()),
        [](bool ok, int, matjson::Value body) {
            auto& m = session();
            m.checked = true;
            m.active = false;
            m.levelId = 0;
            if (ok && body.isObject() && body["active"].asBool().unwrapOr(false)) {
                m.active = true;
                m.levelId = static_cast<int>(body["levelId"].asInt().unwrapOr(0));
            }
        });
}

// Fire-and-forget: deaths go to `death` (stats + balance), completions to
// `run`. Both are no-ops unless a Discord session for this level is active.
inline void reportSessionRun(const std::string& levelId, const std::string& levelName,
                             int percent, bool completed) {
    if (!session().checked || !session().active) return;
    if (std::to_string(session().levelId) != levelId) return;
    if (completed) {
        Bridge::post("run", [&]() {
            matjson::Value b = matjson::Value::object();
            b.set("discordId", discordId());
            b.set("gdAccountId", gdAccountId());
            b.set("levelId", levelId);
            b.set("levelName", levelName);
            b.set("percent", percent);
            b.set("completed", true);
            return b;
        }(), [](bool ok, int, matjson::Value body) {
            if (ok && body.isObject()) {
                const auto bal = body["balance"].asInt().unwrapOr(-1);
                if (bal >= 0) setGlimmer(bal, 0);
            }
        });
    } else {
        Bridge::post("death", [&]() {
            matjson::Value b = matjson::Value::object();
            b.set("discordId", discordId());
            b.set("gdAccountId", gdAccountId());
            b.set("levelId", levelId);
            b.set("levelName", levelName);
            b.set("percent", percent);
            return b;
        }(), [](bool ok, int, matjson::Value body) {
            if (ok && body.isObject()) {
                const auto bal = body["balance"].asInt().unwrapOr(-1);
                if (bal >= 0) setGlimmer(bal, 0);
            }
        });
    }
}

} // namespace sorrow::rooms
