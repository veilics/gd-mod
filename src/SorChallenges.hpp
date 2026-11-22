// Sorrow Integration - daily / weekly challenges.
//
// The bot (gd.js challenges/*) hands out per-player challenge specs, e.g.
// "Beat a Mythic Harder / Insane" or "Get 45% on an Easy / Medium / Hard
// Demon". This game then rolls the concrete level from RobTop (GD-Roulette
// style search with the right diff/demonFilter + rarity flags) and CHECKS
// the player's save before assigning it: a level you already beat, or are
// already at the target % on, is skipped - every challenge is new progress.
// Progress is claimed from live, non-practice runs on that level.
#pragma once

#include "SorUi.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <random>
#include <set>

#include "SorConst.hpp"
#include "Bridge.hpp"
#include "Format.hpp"
#include "Glimmer.hpp"
#include "Bet.hpp"
#include "SorLevels.hpp"
#include "custom_nodes/RLDifficultyNode.hpp"

namespace sorrow::chal {

struct Challenge {
    std::string id, slot, period, kind, title, rarity;
    std::vector<std::string> diffs;
    int target = 100;
    std::int64_t reward = 0;
    bool done = false;
    int rerollsLeft = 0;
    bool hasLevel = false;
    int levelId = 0;
    std::string levelName, author, difficulty, levelRarity;
};

struct State {
    bool loaded = false;
    std::vector<Challenge> daily, weekly;
    std::int64_t dailyReset = 0, weeklyReset = 0; // ms left at fetch time
    double fetchedAt = 0;
};
inline State& state() { static State s; return s; }
inline std::set<std::string>& rolling() { static std::set<std::string> s; return s; }
inline std::set<std::string>& claiming() { static std::set<std::string> s; return s; }
inline std::function<void()>& onChanged() { static std::function<void()> f; return f; }
inline void changed() { if (onChanged()) onChanged()(); }

inline double nowSec() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// --- local save checks ---
inline int localBest(int levelId) {
    if (auto* glm = GameLevelManager::get())
        if (auto* lvl = glm->getSavedLevel(levelId)) return static_cast<int>(lvl->m_normalPercent.value());
    return 0;
}
inline bool alreadyDone(const Challenge& c, int levelId) {
    const int best = localBest(levelId);
    return c.kind == "beat" ? best >= 100 : best >= c.target;
}

inline Challenge parse(const matjson::Value& v) {
    Challenge c;
    c.id = v["id"].asString().unwrapOr("");
    c.slot = v["slot"].asString().unwrapOr("");
    c.period = v["period"].asString().unwrapOr("");
    c.kind = v["kind"].asString().unwrapOr("beat");
    c.title = v["title"].asString().unwrapOr("");
    c.rarity = v["rarity"].asString().unwrapOr("any");
    c.target = static_cast<int>(v["target"].asInt().unwrapOr(100));
    c.reward = static_cast<std::int64_t>(v["reward"].asInt().unwrapOr(0));
    c.done = v["done"].asBool().unwrapOr(false);
    c.rerollsLeft = static_cast<int>(v["rerollsLeft"].asInt().unwrapOr(0));
    if (auto arr = v["diffs"].asArray()) for (const auto& d : *arr) c.diffs.push_back(d.asString().unwrapOr(""));
    if (v["level"].isObject()) {
        const auto& l = v["level"];
        c.hasLevel = true;
        try { c.levelId = std::stoi(l["id"].asString().unwrapOr("0")); } catch (...) {}
        c.levelName = l["name"].asString().unwrapOr("");
        c.author = l["author"].asString().unwrapOr("");
        c.difficulty = l["difficulty"].asString().unwrapOr("");
        c.levelRarity = l["rarity"].asString().unwrapOr("none");
    }
    return c;
}

inline void apply(const matjson::Value& body) {
    if (!body.isObject() || !body["daily"].isObject()) return;
    auto& s = state();
    s.daily.clear(); s.weekly.clear();
    if (auto arr = body["daily"]["items"].asArray()) for (const auto& v : *arr) s.daily.push_back(parse(v));
    if (auto arr = body["weekly"]["items"].asArray()) for (const auto& v : *arr) s.weekly.push_back(parse(v));
    s.dailyReset = static_cast<std::int64_t>(body["daily"]["resetsInMs"].asInt().unwrapOr(0));
    s.weeklyReset = static_cast<std::int64_t>(body["weekly"]["resetsInMs"].asInt().unwrapOr(0));
    s.fetchedAt = nowSec();
    s.loaded = true;
}

inline Challenge* find(const std::string& id) {
    for (auto* v : { &state().daily, &state().weekly })
        for (auto& c : *v) if (c.id == id) return &c;
    return nullptr;
}

inline matjson::Value idBody() {
    matjson::Value b = matjson::Value::object();
    b.set("discordId", discordId());
    b.set("gdAccountId", gdAccountId());
    return b;
}

void rollFor(const std::string& id, int attempt = 0);
void claim(const Challenge& c, int percent, bool completed, bool fromSave = false);

// After every fetch: roll levels that are missing, re-check assigned ones.
inline void afterFetch() {
    for (auto* v : { &state().daily, &state().weekly })
        for (auto& c : *v) {
            if (c.done) continue;
            if (!c.hasLevel) { rollFor(c.id); continue; }
            // reached since it was assigned (e.g. bot was offline at the time): claim it
            if (alreadyDone(c, c.levelId)) claim(c, localBest(c.levelId), localBest(c.levelId) >= 100, true);
        }
    changed();
}

inline void fetch() {
    Bridge::getRaw("/bridge/sorrow/challenges?gdAccountId=" + q(gdAccountId()) + "&discordId=" + q(discordId()),
        [](bool ok, int, matjson::Value body) {
            if (!ok) { changed(); return; }
            apply(body);
            afterFetch();
        });
}

// ---- RobTop roll for one challenge (spec -> concrete level) ----
inline int rnd(int a, int b) { static std::mt19937 rng(std::random_device{}()); return std::uniform_int_distribution<int>(a, std::max(a, b))(rng); }
inline std::string diffQuery(const std::string& key) {
    static const std::map<std::string, std::string> m = {
        { "harder", "&diff=4" }, { "insane", "&diff=5" },
        { "easy_demon", "&diff=-2&demonFilter=1" }, { "medium_demon", "&diff=-2&demonFilter=2" },
        { "hard_demon", "&diff=-2&demonFilter=3" }, { "insane_demon", "&diff=-2&demonFilter=4" },
        { "extreme_demon", "&diff=-2&demonFilter=5" } };
    auto it = m.find(key);
    return it == m.end() ? std::string("&diff=4,5") : it->second;
}
inline std::string diffLabel(const std::string& key) {
    static const std::map<std::string, std::string> m = {
        { "harder", "Harder" }, { "insane", "Insane" }, { "easy_demon", "Easy Demon" }, { "medium_demon", "Medium Demon" },
        { "hard_demon", "Hard Demon" }, { "insane_demon", "Insane Demon" }, { "extreme_demon", "Extreme Demon" } };
    auto it = m.find(key);
    return it == m.end() ? std::string() : it->second;
}
inline int rarityRank(const std::string& r) {
    if (r == "mythic") return 4; if (r == "legendary") return 3; if (r == "epic") return 2; if (r == "featured") return 1;
    return 0;
}
inline std::string rarityFlag(const std::string& r) {
    if (r == "featured") return "&featured=1";
    if (r == "epic") return "&epic=1";
    if (r == "legendary") return "&legendary=1";
    if (r == "mythic") return "&mythic=1";
    return "";
}
inline std::map<std::string, int>& pages() { static std::map<std::string, int> m; return m; }

inline void post(std::string body, std::function<void(std::string)> cb) {
    geode::utils::web::WebRequest req;
    req.userAgent("");
    req.header("Content-Type", "application/x-www-form-urlencoded");
    req.bodyString(std::move(body));
    geode::async::spawn(
        [req = std::move(req)]() mutable -> geode::utils::web::WebFuture { return req.post("https://www.boomlings.com/database/getGJLevels21.php"); },
        [cb = std::move(cb)](geode::utils::web::WebResponse res) {
            std::string out = res.ok() ? res.string().unwrapOr("") : std::string();
            geode::queueInMainThread([cb, out = std::move(out)] { cb(out); });
        });
}

inline void assign(const std::string& id, const levels::LevelRow& r) {
    auto b = idBody();
    b.set("id", id);
    matjson::Value l = matjson::Value::object();
    l.set("id", std::to_string(r.id)); l.set("name", r.name); l.set("author", r.author);
    l.set("difficulty", r.difficultyLabel()); l.set("rarity", r.rarity()); l.set("demon", r.demon); l.set("stars", r.stars);
    b.set("level", l);
    Bridge::post("challenges/assign", b, [id](bool ok, int, matjson::Value body) {
        rolling().erase(id);
        if (ok) apply(body);
        changed();
    });
}

// Finding a level for a challenge:
//   1) RobTop search with the challenge's difficulty + rarity filters. The
//      result pages are walked IN ORDER from a random start (up to 12 per
//      difficulty), so a small set like Mythic Insanes is covered completely
//      instead of hoping a random page has one. Empty / rate-limited replies
//      are retried.
//   2) Levels saved on this device (everything you have opened/played) that
//      match and that you have not finished yet.
//   3) Still nothing: the bot steps the rarity down one tier (free) and the
//      search runs again.
struct RollJob {
    std::string id;
    std::vector<std::string> keys;   // difficulty keys, shuffled
    size_t keyIdx = 0;
    std::string base;
    int maxPages = 1, start = 0, scanned = 0, retries = 0;
};

inline bool matchesRow(const Challenge& c, const std::string& key, const levels::LevelRow& r, const std::set<int>& used) {
    if (r.length > 4) return false;                                   // platformer
    if (r.difficultyLabel() != diffLabel(key)) return false;
    if (rarityRank(r.rarity()) < rarityRank(c.rarity)) return false;
    if (used.contains(r.id)) return false;
    return !alreadyDone(c, r.id);                                     // CHECK: already beaten / already at %
}

inline std::set<int> usedLevels() {
    std::set<int> used;
    for (auto* v : { &state().daily, &state().weekly }) for (auto& x : *v) if (x.hasLevel) used.insert(x.levelId);
    return used;
}

// 2) saved levels on this device
inline bool rollFromSaved(Challenge& c) {
    auto* glm = GameLevelManager::get();
    if (!glm || !glm->m_onlineLevels) return false;
    const auto used = usedLevels();
    std::vector<levels::LevelRow> ok;
    for (auto [key, lvl] : geode::cocos::CCDictionaryExt<std::string, GJGameLevel*>(glm->m_onlineLevels)) {
        if (!lvl || lvl->m_stars.value() <= 0) continue;              // rated only
        levels::LevelRow r;
        r.id = static_cast<int>(lvl->m_levelID.value());
        r.name = lvl->m_levelName;
        r.author = lvl->m_creatorName;
        r.stars = lvl->m_stars.value();
        r.length = lvl->m_levelLength;
        r.demon = lvl->m_demon.value() > 0;
        r.demonTier = lvl->m_demonDifficulty;
        r.autoLevel = lvl->m_autoLevel;
        r.featured = lvl->m_featured;
        r.epic = lvl->m_isEpic;
        r.face = r.stars >= 8 ? 5 : r.stars >= 6 ? 4 : r.stars >= 4 ? 3 : r.stars == 3 ? 2 : 1;
        for (const auto& k : c.diffs)
            if (matchesRow(c, k, r, used)) { ok.push_back(r); break; }
    }
    if (ok.empty()) return false;
    assign(c.id, ok[rnd(0, static_cast<int>(ok.size()) - 1)]);
    return true;
}

// 3) nothing anywhere: let the bot relax the rarity, then search again
inline void relaxAndRetry(const std::string& id) {
    auto b = idBody();
    b.set("id", id);
    b.set("reason", "nolevel");
    Bridge::post("challenges/reroll", b, [id](bool ok, int, matjson::Value body) {
        rolling().erase(id);
        if (ok && body["relaxed"].asBool().unwrapOr(false)) {
            const auto before = find(id) ? find(id)->rarity : std::string();
            apply(body);
            if (auto* c = find(id); c && c->rarity != before) {
                Notification::create(fmt::format("No {} level left that you haven't done - now: {}",
                    before, c->title).c_str(), NotificationIcon::Info, 3.f)->show();
                rollFor(id);
            }
        }
        changed();
    });
}

inline void rollStep(std::shared_ptr<RollJob> job);

inline void rollNextKey(std::shared_ptr<RollJob> job) {
    auto* c = find(job->id);
    if (!c || c->done || c->hasLevel) { rolling().erase(job->id); changed(); return; }
    if (job->keyIdx >= job->keys.size()) {
        if (rollFromSaved(*c)) return;
        relaxAndRetry(job->id);
        return;
    }
    const auto& key = job->keys[job->keyIdx];
    job->base = fmt::format("secret=Wmfd2893gb7&type=0&star=1&len=0,1,2,3,4{}{}", diffQuery(key), rarityFlag(c->rarity));
    job->scanned = 0;
    job->retries = 0;
    if (auto it = pages().find(job->base); it != pages().end()) {
        job->maxPages = it->second;
        job->start = rnd(0, job->maxPages - 1);
        rollStep(job);
        return;
    }
    post(job->base + "&page=0", [job](std::string resp) {
        if (resp.empty() || resp == "-1") {
            if (++job->retries <= 2) { rollNextKey(job); return; }      // rate limited / hiccup: try again
            ++job->keyIdx; rollNextKey(job); return;                    // this difficulty has nothing
        }
        int total = 0, per = 10;
        const auto sections = levels::split(resp, "#");
        if (sections.size() > 3) {
            const auto pg = levels::split(sections[3], ":");
            try { total = std::stoi(pg.at(0)); per = std::max(1, std::stoi(pg.at(2))); } catch (...) {}
        }
        job->maxPages = std::clamp((total + per - 1) / per, 1, 1000);
        pages()[job->base] = job->maxPages;
        job->start = rnd(0, job->maxPages - 1);
        job->retries = 0;
        rollStep(job);
    });
}

inline void rollStep(std::shared_ptr<RollJob> job) {
    if (job->scanned >= std::min(job->maxPages, 12)) { ++job->keyIdx; rollNextKey(job); return; }
    const int page = (job->start + job->scanned) % job->maxPages;
    post(fmt::format("{}&page={}", job->base, page), [job](std::string resp) {
        auto* c = find(job->id);
        if (!c || c->done || c->hasLevel) { rolling().erase(job->id); changed(); return; }
        if ((resp.empty() || resp == "-1") && ++job->retries <= 2) { rollStep(job); return; }
        job->retries = 0;
        const auto used = usedLevels();
        std::vector<levels::LevelRow> ok;
        for (auto& r : levels::parseResponse(resp))
            if (matchesRow(*c, job->keys[job->keyIdx], r, used)) ok.push_back(std::move(r));
        if (!ok.empty()) {
            const auto& pick = ok[rnd(0, static_cast<int>(ok.size()) - 1)];
            levels::cacheRow(pick);
            assign(job->id, pick);
            return;
        }
        ++job->scanned;
        rollStep(job);
    });
}

inline void rollFor(const std::string& id, int) {
    auto* c = find(id);
    if (!c || c->done || c->hasLevel || c->diffs.empty()) return;
    if (rolling().contains(id)) return;
    rolling().insert(id);
    auto job = std::make_shared<RollJob>();
    job->id = id;
    job->keys = c->diffs;
    for (size_t i = job->keys.size(); i > 1; --i) std::swap(job->keys[i - 1], job->keys[rnd(0, static_cast<int>(i) - 1)]);
    rollNextKey(job);
    changed();
}

inline void reroll(const std::string& id, bool ineligible) {
    auto b = idBody();
    b.set("id", id);
    if (ineligible) b.set("reason", "ineligible");
    Bridge::post("challenges/reroll", b, [id](bool ok, int, matjson::Value body) {
        if (ok) { apply(body); rollFor(id); }
        else Notification::create(body["error"].asString().unwrapOr("Reroll failed").c_str(), NotificationIcon::Warning, 1.5f)->show();
        changed();
    });
}

inline void claim(const Challenge& c, int percent, bool completed, bool fromSave) {
    if (claiming().contains(c.id)) return;
    claiming().insert(c.id);
    auto b = idBody();
    b.set("id", c.id);
    b.set("levelId", std::to_string(c.levelId));
    b.set("percent", percent);
    b.set("completed", completed);
    const auto title = c.title;
    const auto id = c.id;
    Bridge::post("challenges/claim", b, [title, id, fromSave](bool ok, int, matjson::Value body) {
        claiming().erase(id);
        if (!ok) return;
        apply(body);
        const auto reward = static_cast<std::int64_t>(body["reward"].asInt().unwrapOr(0));
        const auto bal = static_cast<std::int64_t>(body["balance"].asInt().unwrapOr(-1));
        if (bal >= 0) setGlimmer(bal, 0);
        Notification::create(fmt::format("Challenge complete: {}", title).c_str(), NotificationIcon::Success, 2.5f)->show();
        if (reward > 0) presentGlimmerWin(reward, 0, nullptr);
        changed();
    });
}

// PlayLayer hooks: a death / completion on a challenge level.
inline void onRun(int levelId, int percent, bool completed) {
    if (!state().loaded) return;
    for (auto* v : { &state().daily, &state().weekly })
        for (auto& c : *v) {
            if (c.done || !c.hasLevel || c.levelId != levelId) continue;
            const bool reached = c.kind == "beat" ? (completed || percent >= 100) : percent >= c.target;
            if (reached) claim(c, completed ? 100 : percent, completed);
        }
}

inline int doneCount() { int n = 0; for (auto* v : { &state().daily, &state().weekly }) for (auto& c : *v) n += c.done; return n; }
inline int totalCount() { return static_cast<int>(state().daily.size() + state().weekly.size()); }

inline std::string leftText(std::int64_t ms) {
    ms -= static_cast<std::int64_t>((nowSec() - state().fetchedAt) * 1000.0);
    const auto m = std::max<std::int64_t>(0, ms / 60000);
    if (m >= 1440) return fmt::format("{}d {}h", m / 1440, (m % 1440) / 60);
    return fmt::format("{}h {}m", m / 60, m % 60);
}

inline GJDifficulty difficultyOf(const std::string& label) {
    if (label == "Harder") return GJDifficulty::Harder;
    if (label == "Insane") return GJDifficulty::Insane;
    if (label == "Easy Demon") return GJDifficulty::DemonEasy;
    if (label == "Medium Demon") return GJDifficulty::DemonMedium;
    if (label == "Hard Demon") return GJDifficulty::Demon;
    if (label == "Insane Demon") return GJDifficulty::DemonInsane;
    if (label == "Extreme Demon") return GJDifficulty::DemonExtreme;
    if (label == "Hard") return GJDifficulty::Hard;
    if (label == "Normal") return GJDifficulty::Normal;
    if (label == "Easy") return GJDifficulty::Easy;
    return GJDifficulty::NA;
}
inline GJFeatureState featureOf(const std::string& r) {
    if (r == "mythic") return GJFeatureState::Mythic;
    if (r == "legendary") return GJFeatureState::Legendary;
    if (r == "epic") return GJFeatureState::Epic;
    if (r == "featured") return GJFeatureState::Featured;
    return GJFeatureState::None;
}

// ----------------------------------------------------------------- popup --
class ChallengesPopup : public geode::Popup {
protected:
    geode::ScrollLayer* m_list = nullptr;
    std::string m_key;

    bool setup() {
        this->setTitle("Challenges", "goldFont.fnt", .75f, 20.f);
        m_list = geode::ScrollLayer::create({ 14.f, 14.f, m_size.width - 28.f, m_size.height - 52.f });
        m_mainLayer->addChild(m_list);
        geode::Ref<ChallengesPopup> self = this;
        onChanged() = [self] { if (self->getParent()) self->rebuild(true); };
        fetch();
        this->rebuild(true);
        this->schedule(schedule_selector(ChallengesPopup::tick), 1.f);
        return true;
    }

    void tick(float) { this->rebuild(false); }

    void rebuild(bool force) {
        auto& s = state();
        std::string key = fmt::format("{}|{}|{}", s.loaded, rolling().size(), leftText(s.dailyReset));
        for (auto* v : { &s.daily, &s.weekly }) for (auto& c : *v) key += fmt::format("{}{}{}{}|", c.id, c.done, c.levelId, c.rerollsLeft);
        if (!force && key == m_key) return;
        m_key = key;

        auto* content = m_list->m_contentLayer;
        content->removeAllChildren();
        const float listW = m_list->getContentSize().width, rowH = 46.f, headH = 26.f;
        const float total = std::max(m_list->getContentSize().height,
            2 * headH + rowH * static_cast<float>(s.daily.size() + s.weekly.size()) + (s.loaded ? 0.f : 40.f));
        content->setContentSize({ listW, total });
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        content->addChild(menu, 10);
        float y = total;

        if (!s.loaded) {
            auto* l = CCLabelBMFont::create(isLinked() ? "Loading challenges..." : "Link your account with /gd link first", "bigFont.fnt");
            l->limitLabelWidth(listW - 20.f, .45f, .2f);
            l->setPosition({ listW / 2.f, y - 30.f });
            content->addChild(l);
            return;
        }
        auto section = [&](const char* name, std::int64_t reset) {
            y -= headH;
            auto* h = CCLabelBMFont::create(name, "goldFont.fnt");
            h->setScale(.6f);
            h->setAnchorPoint({ 0.f, .5f });
            h->setPosition({ 6.f, y + headH / 2.f });
            content->addChild(h);
            auto* r = CCLabelBMFont::create(fmt::format("resets in {}", leftText(reset)).c_str(), "chatFont.fnt");
            r->setScale(.6f);
            r->setColor({ 190, 190, 190 });
            r->setAnchorPoint({ 1.f, .5f });
            r->setPosition({ listW - 6.f, y + headH / 2.f });
            content->addChild(r);
        };
        auto row = [&](const Challenge& c) {
            y -= rowH;
            const float cy = y + rowH / 2.f;
            auto* bg = ui::panel(listW - 4.f, rowH - 4.f,
                c.done ? cocos2d::ccColor3B { 20, 70, 30 } : cocos2d::ccColor3B { 0, 0, 0 }, 90);
            bg->setAnchorPoint({ 0.f, 0.f });
            bg->setPosition({ 2.f, y + 2.f });
            content->addChild(bg, 0);

            const auto diff = c.hasLevel ? difficultyOf(c.difficulty) : difficultyOf(diffLabel(c.diffs.empty() ? "" : c.diffs.back()));
            auto* icon = RLDifficultyNode::create({ diff, featureOf(c.hasLevel ? c.levelRarity : c.rarity) });
            icon->setScale(.55f);
            icon->setPosition({ 26.f, cy });
            content->addChild(icon, 2);

            std::string head = c.hasLevel
                ? (c.kind == "beat" ? fmt::format("Beat {}", c.levelName) : fmt::format("Get {}% on {}", c.target, c.levelName))
                : c.title;
            auto* t = CCLabelBMFont::create(head.c_str(), "bigFont.fnt");
            t->setAnchorPoint({ 0.f, .5f });
            t->limitLabelWidth(172.f, .4f, .15f);
            t->setPosition({ 50.f, cy + 7.f });
            content->addChild(t, 2);
            std::string sub;
            if (c.hasLevel) {
                const std::string rar = c.levelRarity == "none" ? std::string() : std::string(1, static_cast<char>(std::toupper(c.levelRarity[0]))) + c.levelRarity.substr(1) + " ";
                sub = fmt::format("{}{}  by {}  -  your best {}%", rar, c.difficulty, c.author, localBest(c.levelId));
            } else sub = rolling().contains(c.id) ? "Finding a level you have not done..." : "No level found yet - tap reroll";
            auto* st = CCLabelBMFont::create(sub.c_str(), "chatFont.fnt");
            st->setAnchorPoint({ 0.f, .5f });
            st->setColor({ 200, 200, 200 });
            st->limitLabelWidth(172.f, .55f, .2f);
            st->setPosition({ 50.f, cy - 9.f });
            content->addChild(st, 2);

            // reward: "+1.2M" with the glimmer gem right after it
            auto* rw = CCLabelBMFont::create(fmt::format("+{}", formatGlimmer(c.reward)).c_str(), "goldFont.fnt");
            rw->setAnchorPoint({ 1.f, .5f });
            rw->limitLabelWidth(62.f, .5f, .2f);
            rw->setPosition({ listW - 116.f, cy });
            content->addChild(rw, 2);
            if (auto* gem = cocos2d::CCSprite::create((Mod::get()->getResourcesDir() / "glimmer.png").string().c_str())) {
                gem->setScale(13.f / std::max(1.f, gem->getContentHeight()));
                gem->setPosition({ listW - 108.f, cy });
                content->addChild(gem, 2);
            }

            if (c.done) {
                auto* ok = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png");
                ok->setScale(.7f);
                ok->setPosition({ listW - 52.f, cy });
                content->addChild(ok, 2);
                return;
            }
            if (c.hasLevel) {
                auto* play = CCMenuItemSpriteExtra::create(
                    ButtonSprite::create("Play", 46, true, "bigFont.fnt", "GJ_button_01.png", 26.f, .55f),
                    this, menu_selector(ChallengesPopup::onPlay));
                play->setTag(c.levelId);
                play->setPosition({ listW - 66.f, cy });
                menu->addChild(play);
            }
            if (c.rerollsLeft > 0 || !c.hasLevel) {
                auto* spr = CCSprite::createWithSpriteFrameName("GJ_updateBtn_001.png");
                spr->setScale(.5f);
                auto* rr = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ChallengesPopup::onReroll));
                rr->setUserObject(cocos2d::CCString::create(c.id));
                rr->setPosition({ listW - 16.f, cy });
                menu->addChild(rr);
            }
        };
        section("Daily", s.dailyReset);
        for (auto& c : s.daily) row(c);
        section("Weekly", s.weeklyReset);
        for (auto& c : s.weekly) row(c);
    }

    void onPlay(CCObject* sender) {
        const int id = sender->getTag();
        this->onClose(nullptr);
        levels::fetchById(id, [id](GJGameLevel* lvl) { levels::openLevelPage(lvl ? lvl : levels::bareLevel(id, "")); });
    }
    void onReroll(CCObject* sender) {
        auto* sid = static_cast<cocos2d::CCString*>(static_cast<CCNode*>(sender)->getUserObject());
        if (!sid) return;
        const std::string id = sid->getCString();
        auto* c = find(id);
        if (!c) return;
        if (!c->hasLevel) { rolling().erase(id); rollFor(id); this->rebuild(true); return; }
        reroll(id, false);
    }
    void onClose(CCObject* s) override { onChanged() = nullptr; geode::Popup::onClose(s); }

public:
    static ChallengesPopup* create() {
        auto* ret = new ChallengesPopup();
        if (ret->init(440.f, 290.f)) { ret->setup(); ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
};

} // namespace sorrow::chal
