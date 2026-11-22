// Sorrow Integration - roulette levels built by GD itself.
//
// Roulette/room levels used to be rebuilt by hand from a few parsed fields
// (and room rounds from just id + name), so GD never got the level's song
// data: 2.2 song/SFX lists (m_songIDs / m_sfxIDs) and the song info from
// the search response's songs section were lost -> "song doesn't load".
// Now every RobTop response is handed to GD's own code, exactly like a
// search: GameLevelManager::responseToDict + GJGameLevel::create(dict) for
// the level, MusicDownloadManager::createSongsInfo for the songs section.
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>

#include <map>
#include <string>
#include <vector>

namespace sorrow::levels {

inline std::map<int, geode::Ref<GJGameLevel>>& cacheMap() {
    static std::map<int, geode::Ref<GJGameLevel>> m;
    return m;
}

inline GJGameLevel* cached(int id) {
    auto& m = cacheMap();
    if (auto it = m.find(id); it != m.end()) return it->second;
    return nullptr;
}

inline std::vector<std::string> split(const std::string& s, const std::string& sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const auto pos = s.find(sep, start);
        out.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + sep.size();
    }
    return out;
}

// RobTop "k:v:k:v" -> map
inline std::map<int, std::string> kv(const std::string& row) {
    std::map<int, std::string> m;
    const auto parts = split(row, ":");
    for (size_t i = 0; i + 1 < parts.size(); i += 2) {
        try { m[std::stoi(parts[i])] = parts[i + 1]; } catch (...) {}
    }
    return m;
}
inline int num(const std::map<int, std::string>& m, int k, int def = 0) {
    auto it = m.find(k);
    if (it == m.end() || it->second.empty()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}
inline std::string str(const std::map<int, std::string>& m, int k) {
    auto it = m.find(k);
    return it == m.end() ? std::string() : it->second;
}

// Parsed level row (also used by challenges / candidates).
struct LevelRow {
    int id = 0;
    std::string name, author;
    int stars = 0, length = 0, coins = 0;
    int face = 0;          // 0 N/A, 1 Easy .. 5 Insane
    bool demon = false, autoLevel = false;
    int demonTier = 0;     // RobTop key 43: 3 easy, 4 medium, 0 hard, 5 insane, 6 extreme
    int featured = 0, epic = 0; // key 19 / key 42 (1 epic, 2 legendary, 3 mythic)
    std::map<int, std::string> raw;

    std::string rarity() const {
        if (epic >= 3) return "mythic";
        if (epic == 2) return "legendary";
        if (epic == 1) return "epic";
        if (featured > 0) return "featured";
        return "none";
    }
    std::string difficultyLabel() const {
        if (demon) {
            switch (demonTier) { case 3: return "Easy Demon"; case 4: return "Medium Demon"; case 5: return "Insane Demon"; case 6: return "Extreme Demon"; default: return "Hard Demon"; }
        }
        if (autoLevel) return "Auto";
        static const char* f[] = { "N/A", "Easy", "Normal", "Hard", "Harder", "Insane" };
        return f[std::clamp(face, 0, 5)];
    }
};

// Build the GJGameLevel ourselves from RobTop's keys. Song fields are set
// directly (key 35 custom song, 12 official track, 52/53 the 2.2 song/SFX
// lists) - the old path lost them and LevelInfoLayer showed Stereo Madness
// until the level had been downloaded once.
inline GJGameLevel* buildLevel(const LevelRow& r) {
    const auto& m = r.raw;
    auto* lvl = GJGameLevel::create();
    lvl->m_levelID = r.id;
    lvl->m_levelName = r.name;
    lvl->m_levelDesc = str(m, 3);
    lvl->m_levelVersion = num(m, 5, 1);
    lvl->m_userID = num(m, 6);
    lvl->m_ratings = num(m, 8);
    lvl->m_ratingsSum = num(m, 9);
    lvl->m_downloads = num(m, 10);
    lvl->m_audioTrack = num(m, 12);
    lvl->m_gameVersion = num(m, 13, 22);
    lvl->m_likes = num(m, 14);
    lvl->m_levelLength = num(m, 15);
    lvl->m_demon = r.demon ? 1 : 0;
    lvl->m_stars = r.stars;
    lvl->m_featured = r.featured;
    lvl->m_autoLevel = r.autoLevel;
    lvl->m_originalLevel = num(m, 30);
    lvl->m_twoPlayerMode = num(m, 31) != 0;
    lvl->m_songID = num(m, 35);
    lvl->m_coins = r.coins;
    lvl->m_coinsVerified = num(m, 38);
    lvl->m_starsRequested = num(m, 39);
    lvl->m_isEpic = r.epic;
    lvl->m_demonDifficulty = r.demonTier;
    lvl->m_objectCount = num(m, 45);
    lvl->m_songIDs = str(m, 52);
    lvl->m_sfxIDs = str(m, 53);
    lvl->m_creatorName = r.author;
    // the difficulty face GD draws (LevelInfoLayer reads m_difficulty for some paths)
    if (r.demon) lvl->m_difficulty = GJDifficulty::Demon;
    else if (r.autoLevel) lvl->m_difficulty = GJDifficulty::Auto;
    else lvl->m_difficulty = static_cast<GJDifficulty>(std::clamp(r.face, 0, 5));
    lvl->m_levelNotDownloaded = true;
    lvl->m_levelType = GJLevelType::Saved;
    return lvl;
}

// Parse a getGJLevels21 response: rows + creators, and register the songs
// section with GD (the same call GD makes after a search).
inline std::vector<LevelRow> parseResponse(const std::string& raw) {
    std::vector<LevelRow> rows;
    if (raw.empty() || raw == "-1" || raw.starts_with("error")) return rows;
    const auto sections = split(raw, "#");
    if (sections.empty()) return rows;

    if (sections.size() > 2 && sections[2].find("~|~") != std::string::npos) {
        if (auto* mdm = MusicDownloadManager::sharedState())
            mdm->createSongsInfo(sections[2], "");
    }
    std::map<std::string, std::string> users; // userID -> name
    if (sections.size() > 1)
        for (const auto& u : split(sections[1], "|")) {
            const auto parts = split(u, ":");
            if (parts.size() >= 2) users[parts[0]] = parts[1];
        }

    for (const auto& row : split(sections[0], "|")) {
        if (row.empty()) continue;
        LevelRow r;
        r.raw = kv(row);
        r.id = num(r.raw, 1);
        if (r.id <= 0) continue;
        r.name = str(r.raw, 2);
        r.stars = num(r.raw, 18);
        r.length = num(r.raw, 15);
        r.coins = num(r.raw, 37);
        const int den = num(r.raw, 8), numer = num(r.raw, 9);
        r.face = den > 0 ? numer / den : 0;
        r.demon = num(r.raw, 17) != 0;
        r.autoLevel = num(r.raw, 25) != 0;
        r.demonTier = num(r.raw, 43);
        r.featured = num(r.raw, 19);
        r.epic = num(r.raw, 42);
        if (auto it = users.find(str(r.raw, 6)); it != users.end()) r.author = it->second;
        rows.push_back(std::move(r));
    }
    return rows;
}

inline GJGameLevel* cacheRow(const LevelRow& r) {
    auto* lvl = buildLevel(r);
    cacheMap()[r.id] = lvl;
    // custom song with no info yet: ask GD's song manager to fetch it now
    if (lvl->m_songID > 0) {
        if (auto* mdm = MusicDownloadManager::sharedState(); mdm && !mdm->getSongInfoObject(lvl->m_songID))
            mdm->getSongInfo(lvl->m_songID, false);
    }
    return lvl;
}

// Store the level with id `wantId` (or the first one) from a raw response.
inline GJGameLevel* storeFromResponse(const std::string& raw, int wantId) {
    for (const auto& r : parseResponse(raw)) {
        if (wantId > 0 && r.id != wantId) continue;
        return cacheRow(r);
    }
    return nullptr;
}

// RobTop lookup by id (GD-Roulette getLevelInfo request), stored + returned.
inline void fetchById(int id, std::function<void(GJGameLevel*)> cb) {
    if (auto* c = cached(id)) { cb(c); return; }
    geode::utils::web::WebRequest req;
    req.userAgent("");
    req.header("Content-Type", "application/x-www-form-urlencoded");
    req.bodyString(fmt::format("secret=Wmfd2893gb7&type=0&str={}", id));
    geode::async::spawn(
        [req = std::move(req)]() mutable -> geode::utils::web::WebFuture { return req.post("https://www.boomlings.com/database/getGJLevels21.php"); },
        [id, cb = std::move(cb)](geode::utils::web::WebResponse res) {
            std::string raw = res.ok() ? res.string().unwrapOr("") : std::string();
            geode::queueInMainThread([id, cb, raw = std::move(raw)] { cb(storeFromResponse(raw, id)); });
        });
}

// Open the level page (downloads the level data like a normal search result).
// If a level page is already on top (the previous round), it is REPLACED so
// the back button never walks through old round levels.
inline void openLevelPage(GJGameLevel* level) {
    if (!level) return;
    auto* layer = LevelInfoLayer::create(level, false);
    if (!layer) return;
    layer->downloadLevel();
    auto* scene = cocos2d::CCScene::create();
    scene->addChild(layer);
    auto* dir = cocos2d::CCDirector::get();
    auto* running = dir->getRunningScene();
    const bool onLevelPage = running && running->getChildByType<LevelInfoLayer>(0);
    if (onLevelPage) dir->replaceScene(cocos2d::CCTransitionFade::create(.5f, scene));
    else dir->pushScene(cocos2d::CCTransitionFade::create(.5f, scene));
}

// Id-only fallback (RobTop unreachable): GD still fills song info on download.
inline GJGameLevel* bareLevel(int id, const std::string& name) {
    auto* level = GJGameLevel::create();
    level->m_levelID = id;
    level->m_levelName = name;
    level->m_levelNotDownloaded = true;
    level->m_levelType = GJLevelType::Saved;
    return level;
}

} // namespace sorrow::levels
