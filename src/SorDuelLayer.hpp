// Sorrow Integration - the Versus screen (opened from GD's Versus button).
//
//  +--------------------------------------------------------------+
//  | <-                       VERSUS                  [gem] 1.2M  |
//  |  +-----------+  [ Play ][ Invites 2 ][ Races ][ Live ]       |
//  |  | YOUR RANK |  +------------------------------------------+ |
//  |  |   GOLD    |  |  Play: Challenge a player / Open a race  | |
//  |  | 1312 Elo  |  |        Nearby on Globed                  | |
//  |  | #4  W/L/D |  |  ...or the MATCH view while in a duel:   | |
//  |  | streak    |  |   [cube] Alice   VS   Bob [cube]         | |
//  |  | [Board]   |  |        1 - 0   Round 2 starts in 3       | |
//  |  | [History] |  |   [Go to level] [Forfeit]                | |
//  |  +-----------+  +------------------------------------------+ |
//  +--------------------------------------------------------------+
// Every element sits on a fixed grid computed from the window size.
#pragma once

#include "SorDuels.hpp"
#include "SorRoomLayer.hpp"
#include "Sprites.hpp"

#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

namespace sorrow::duelui {

using namespace sorrow::duels;

inline CCLabelBMFont* label(const std::string& text, const char* font, float scale, cocos2d::ccColor3B col = { 255, 255, 255 },
                            cocos2d::CCPoint anchor = { .5f, .5f }) {
    auto* l = CCLabelBMFont::create(text.c_str(), font);
    l->setScale(scale);
    l->setColor(col);
    l->setAnchorPoint(anchor);
    return l;
}
inline CCMenuItemSpriteExtra* button(const std::string& text, const char* bg, float scale, std::function<void()> cb) {
    auto* spr = ButtonSprite::create(text.c_str(), "bigFont.fnt", bg, .8f);
    spr->setScale(scale);
    return CCMenuItemExt::createSpriteExtra(spr, [cb](CCMenuItemSpriteExtra*) { if (cb) cb(); });
}
inline cocos2d::CCNode* glimmerAmount(std::int64_t amount, float h, const char* font = "goldFont.fnt") {
    auto* n = cocos2d::CCNode::create();
    auto* gem = betfx::glimmerSprite(h);
    auto* l = label(sorrow::formatGlimmer(amount), font, h / 26.f, { 255, 255, 255 }, { 0.f, .5f });
    const float gw = gem ? gem->getScaledContentWidth() : 0.f;
    if (gem) { gem->setPosition({ gw / 2.f, h / 2.f }); n->addChild(gem); }
    l->setPosition({ gw + 3.f, h / 2.f });
    n->addChild(l);
    n->setContentSize({ gw + 3.f + l->getScaledContentWidth(), h });
    n->setAnchorPoint({ .5f, .5f });
    return n;
}
inline const char* diffFrame(const std::string& k) {
    static const std::map<std::string, const char*> m = {
        { "auto", "difficulty_auto_btn_001.png" }, { "easy", "difficulty_01_btn_001.png" }, { "normal", "difficulty_02_btn_001.png" },
        { "hard", "difficulty_03_btn_001.png" }, { "harder", "difficulty_04_btn_001.png" }, { "insane", "difficulty_05_btn_001.png" },
        { "hard_demon", "difficulty_06_btn_001.png" }, { "easy_demon", "difficulty_07_btn_001.png" },
        { "medium_demon", "difficulty_08_btn_001.png" }, { "insane_demon", "difficulty_09_btn_001.png" },
        { "extreme_demon", "difficulty_10_btn_001.png" } };
    auto it = m.find(k);
    return it == m.end() ? "difficulty_00_btn_001.png" : it->second;
}
inline std::string diffName(const std::string& k) {
    static const std::map<std::string, std::string> m = {
        { "any", "Any difficulty" }, { "auto", "Auto" }, { "easy", "Easy" }, { "normal", "Normal" }, { "hard", "Hard" },
        { "harder", "Harder" }, { "insane", "Insane" }, { "easy_demon", "Easy Demon" }, { "medium_demon", "Medium Demon" },
        { "hard_demon", "Hard Demon" }, { "insane_demon", "Insane Demon" }, { "extreme_demon", "Extreme Demon" } };
    auto it = m.find(k);
    return it == m.end() ? k : it->second;
}
inline SimplePlayer* cube(const Player& p, float scale) {
    auto* sp = sorrow::makeCubeIcon(p.iconId, p.c1, p.c2, p.glow);
    if (sp) sp->setScale(scale);
    return sp;
}
inline std::string modeDesc(const std::string& m) {
    if (m == "race") return "First to reach the target % wins the round";
    if (m == "showdown") return "Highest % before the clock runs out wins";
    return "One attempt each - your first death counts";
}

// ----------------------------------------------------------------- popups --

// Pick an opponent: search linked players by GD name; Globed players in your
// session ("Nearby") are listed first.
class SorDuelPickPopup : public geode::Popup {
protected:
    std::function<void(std::string, std::string)> m_onPick;
    TextInput* m_input = nullptr;
    cocos2d::CCNode* m_list = nullptr;

    bool setup() {
        this->setTitle("Challenge a Player", "goldFont.fnt", .7f, 18.f);
        const float cx = m_size.width / 2.f;
        m_input = TextInput::create(200.f, "GD username...", "bigFont.fnt");
        m_input->setPosition({ cx - 30.f, m_size.height - 50.f });
        m_mainLayer->addChild(m_input);
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(menu);
        auto* go = button("Search", "GJ_button_01.png", .6f, [this] { this->search(); });
        go->setPosition({ cx + 105.f, m_size.height - 50.f });
        menu->addChild(go);
        m_list = cocos2d::CCNode::create();
        m_mainLayer->addChild(m_list);
        this->search();
        return true;
    }
    void search() {
        this->showText("Searching...");
        WeakRef<SorDuelPickPopup> self = this;
        duels::find(std::string(m_input->getString()), [self](std::vector<FoundPlayer> found) {
            if (auto p = self.lock()) p->showFound(found);
        });
    }
    void showText(const std::string& t) {
        m_list->removeAllChildren();
        auto* l = label(t, "bigFont.fnt", .35f, { 200, 200, 200 });
        l->setPosition({ m_size.width / 2.f, m_size.height / 2.f - 20.f });
        m_list->addChild(l);
    }
    void showFound(const std::vector<FoundPlayer>& found) {
        m_list->removeAllChildren();
        if (found.empty()) {
            this->showText(std::string(m_input->getString()).empty()
                ? "Type a name - players on Globed with you show up here" : "No linked player found");
            return;
        }
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_list->addChild(menu);
        const float w = m_size.width - 30.f;
        for (size_t i = 0; i < std::min<size_t>(found.size(), 5); ++i) {
            const auto& f = found[i];
            const float y = m_size.height - 86.f - i * 30.f;
            auto* bg = sorrow::ui::panel(w, 26.f, { 0, 0, 0 }, 70);
            bg->setPosition({ m_size.width / 2.f, y });
            m_list->addChild(bg);
            auto* nm = label(f.name, "bigFont.fnt", .4f, { 255, 255, 255 }, { 0.f, .5f });
            nm->limitLabelWidth(110.f, .4f, .2f);
            nm->setPosition({ 25.f, y + 4.f });
            m_list->addChild(nm);
            auto* rk = label(fmt::format("{} {}  -  {}W {}L", f.rank, f.elo, f.w, f.l), "goldFont.fnt", .38f, rankColor(f.rank), { 0.f, .5f });
            rk->setPosition({ 25.f, y - 6.f });
            m_list->addChild(rk);
            if (f.nearby) {
                auto* tag = label("NEARBY", "bigFont.fnt", .25f, { 120, 230, 255 });
                tag->setPosition({ 175.f, y });
                m_list->addChild(tag);
            }
            if (f.busy) {
                auto* b = label("In a duel", "bigFont.fnt", .3f, { 150, 150, 150 });
                b->setPosition({ m_size.width - 60.f, y });
                m_list->addChild(b);
            } else {
                auto uid = f.uid, name = f.name;
                auto* btn = button("Challenge", "GJ_button_01.png", .5f, [this, uid, name] {
                    auto cb = m_onPick;
                    this->onClose(nullptr);
                    if (cb) cb(uid, name);
                });
                btn->setPosition({ m_size.width - 60.f, y });
                menu->addChild(btn);
            }
        }
    }
public:
    static SorDuelPickPopup* create(std::function<void(std::string, std::string)> cb) {
        auto* r = new SorDuelPickPopup();
        r->m_onPick = std::move(cb);
        if (r->init(340.f, 250.f)) { r->setup(); r->autorelease(); return r; }
        delete r;
        return nullptr;
    }
};

// Duel / race settings.
class SorDuelCreatePopup : public geode::Popup {
protected:
    CreateOptions m_o;
    Ref<GJGameLevel> m_level;
    cocos2d::CCNode* m_body = nullptr;
    TextInput* m_stake = nullptr;
    std::function<void()> m_done;
    static inline const std::vector<std::string> kDiffs = { "any", "easy", "normal", "hard", "harder", "insane",
        "easy_demon", "medium_demon", "hard_demon", "insane_demon", "extreme_demon" };

    bool setup() {
        this->setTitle(m_o.kind == "race" ? "Open a Race" : fmt::format("Challenge {}", m_o.oppName).c_str(), "goldFont.fnt", .7f, 18.f);
        m_body = cocos2d::CCNode::create();
        m_mainLayer->addChild(m_body);
        // stake input lives outside the rebuilt body so typing survives toggles
        const float cx = m_size.width / 2.f;
        auto* sl = label("Stake (each)", "goldFont.fnt", .5f, { 255, 255, 255 }, { 0.f, .5f });
        sl->setPosition({ 18.f, 78.f });
        m_mainLayer->addChild(sl);
        m_stake = TextInput::create(110.f, "0", "bigFont.fnt");
        m_stake->setCommonFilter(CommonFilter::Uint);
        m_stake->setMaxCharCount(15);
        m_stake->setScale(.85f);
        m_stake->setPosition({ 160.f, 78.f });
        m_mainLayer->addChild(m_stake);
        auto* chips = CCMenu::create();
        chips->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(chips);
        const std::pair<const char*, std::int64_t> cs[] = { { "10k", 10000 }, { "100k", 100000 }, { "1M", 1000000 } };
        for (int i = 0; i < 3; ++i) {
            auto v = cs[i].second;
            auto* c = button(cs[i].first, "GJ_button_04.png", .45f, [this, v] { m_stake->setString(std::to_string(v).c_str()); });
            c->setPosition({ 245.f + i * 40.f, 78.f });
            chips->addChild(c);
        }
        auto* send = button(m_o.kind == "race" ? "Open Race" : "Send Challenge", "GJ_button_01.png", .75f, [this] { this->onSend(); });
        send->setPosition({ cx, 30.f });
        chips->addChild(send);
        this->rebuild();
        return true;
    }

    // a segmented control: one button per option, the selected one green
    void segment(CCMenu* menu, float x0, float y, float w, const std::vector<std::pair<std::string, std::string>>& opts,
                 const std::string& cur, std::function<void(std::string)> pick) {
        const float step = w / opts.size();
        for (size_t i = 0; i < opts.size(); ++i) {
            const auto val = opts[i].first;
            auto* spr = ButtonSprite::create(opts[i].second.c_str(), static_cast<int>(step / .55f - 12.f), true, "bigFont.fnt",
                val == cur ? "GJ_button_01.png" : "GJ_button_04.png", 24.f, .55f);
            spr->setScale(.62f);
            auto* b = CCMenuItemExt::createSpriteExtra(spr, [this, pick, val](CCMenuItemSpriteExtra*) { pick(val); this->rebuild(); });
            b->setPosition({ x0 + step * (i + .5f), y });
            menu->addChild(b);
        }
    }

    void rebuild() {
        m_body->removeAllChildren();
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_body->addChild(menu);
        const float H = m_size.height, W = m_size.width;

        // mode
        auto* ml = label("Mode", "goldFont.fnt", .5f, { 255, 255, 255 }, { 0.f, .5f });
        ml->setPosition({ 18.f, H - 48.f });
        m_body->addChild(ml);
        this->segment(menu, 80.f, H - 48.f, W - 95.f, { { "race", "Race" }, { "showdown", "Showdown" }, { "sudden", "Sudden Death" } },
            m_o.mode, [this](std::string v) { m_o.mode = v; });
        auto* md = label(modeDesc(m_o.mode), "bigFont.fnt", .27f, { 190, 190, 210 });
        md->setPosition({ 80.f + (W - 95.f) / 2.f, H - 64.f });
        m_body->addChild(md);

        // best of + ranked
        auto* bl = label("Best of", "goldFont.fnt", .5f, { 255, 255, 255 }, { 0.f, .5f });
        bl->setPosition({ 18.f, H - 86.f });
        m_body->addChild(bl);
        this->segment(menu, 80.f, H - 86.f, 120.f, { { "1", "1" }, { "3", "3" }, { "5", "5" } }, std::to_string(m_o.bestOf),
            [this](std::string v) { m_o.bestOf = std::stoi(v); });
        if (m_o.kind == "duel") {
            this->segment(menu, 215.f, H - 86.f, W - 230.f, { { "1", "Ranked" }, { "0", "Casual" } }, m_o.ranked ? "1" : "0",
                [this](std::string v) { m_o.ranked = v == "1"; });
        }

        // target % (race) or time limit
        const bool race = m_o.mode == "race";
        auto* tl = label(race ? "Target" : "Time", "goldFont.fnt", .5f, { 255, 255, 255 }, { 0.f, .5f });
        tl->setPosition({ 18.f, H - 120.f });
        m_body->addChild(tl);
        auto* minus = button("-", "GJ_button_04.png", .55f, [this, race] {
            if (race) m_o.target = std::max(5, m_o.target - 5); else m_o.seconds = std::max(60, m_o.seconds - 60);
            this->rebuild(); });
        minus->setPosition({ 95.f, H - 120.f });
        menu->addChild(minus);
        auto* tv = label(race ? fmt::format("{}%", m_o.target) : fmt::format("{}:00", m_o.seconds / 60), "bigFont.fnt", .5f);
        tv->setPosition({ 140.f, H - 120.f });
        m_body->addChild(tv);
        auto* plus = button("+", "GJ_button_04.png", .55f, [this, race] {
            if (race) m_o.target = std::min(100, m_o.target + 5); else m_o.seconds = std::min(900, m_o.seconds + 60);
            this->rebuild(); });
        plus->setPosition({ 185.f, H - 120.f });
        menu->addChild(plus);

        // level: fixed (from a level page) or a random difficulty
        if (m_level) {
            auto* lv = label(fmt::format("Level: {}", std::string(m_level->m_levelName)), "goldFont.fnt", .5f, { 255, 230, 150 });
            lv->limitLabelWidth(130.f, .5f, .2f);
            lv->setPosition({ 280.f, H - 120.f });
            m_body->addChild(lv);
        } else {
            const auto it = std::find(kDiffs.begin(), kDiffs.end(), m_o.difficulty);
            const int idx = it == kDiffs.end() ? 0 : static_cast<int>(it - kDiffs.begin());
            auto* left = CCMenuItemExt::createSpriteExtraWithFrameName("navArrowBtn_001.png", .35f, [this, idx](CCMenuItemSpriteExtra*) {
                m_o.difficulty = kDiffs[(idx + kDiffs.size() - 1) % kDiffs.size()]; this->rebuild(); });
            left->setRotation(180.f);
            left->setPosition({ 222.f, H - 120.f });
            menu->addChild(left);
            if (auto* face = cocos2d::CCSprite::createWithSpriteFrameName(diffFrame(m_o.difficulty))) {
                face->setScale(.55f);
                face->setPosition({ 252.f, H - 116.f });
                m_body->addChild(face);
            }
            auto* dn = label(diffName(m_o.difficulty), "bigFont.fnt", .28f, { 220, 220, 220 }, { 0.f, .5f });
            dn->setPosition({ 268.f, H - 120.f });
            dn->limitLabelWidth(70.f, .28f, .15f);
            m_body->addChild(dn);
            auto* right = CCMenuItemExt::createSpriteExtraWithFrameName("navArrowBtn_001.png", .35f, [this, idx](CCMenuItemSpriteExtra*) {
                m_o.difficulty = kDiffs[(idx + 1) % kDiffs.size()]; this->rebuild(); });
            right->setPosition({ W - 18.f, H - 120.f });
            menu->addChild(right);
        }
        if (m_o.kind == "race") {
            auto* hint = label("Anyone can join from the Races tab - winner takes the whole pot", "bigFont.fnt", .25f, { 180, 180, 200 });
            hint->setPosition({ W / 2.f, 55.f });
            m_body->addChild(hint);
        }
    }

    void onSend() {
        std::int64_t stake = 0;
        try { stake = std::stoll(std::string(m_stake->getString())); } catch (...) {}
        if (stake > 0 && glimmerBalance() >= 0 && stake > glimmerBalance()) {
            Notification::create("Not enough Glimmer for that stake", NotificationIcon::Warning)->show();
            return;
        }
        m_o.stake = stake;
        m_o.level = m_level;
        auto done = m_done;
        WeakRef<SorDuelCreatePopup> self = this;
        duels::create(m_o, [self, done, kind = m_o.kind](bool ok, std::string err) {
            if (!ok) { Notification::create(err.c_str(), NotificationIcon::Error)->show(); return; }
            Notification::create(kind == "race" ? "Race opened" : "Challenge sent!", NotificationIcon::Success)->show();
            if (auto p = self.lock()) p->onClose(nullptr);
            if (done) done();
        });
    }
public:
    static SorDuelCreatePopup* create(const std::string& kind, const std::string& oppUid, const std::string& oppName,
                                      GJGameLevel* level, std::function<void()> done = nullptr) {
        auto* r = new SorDuelCreatePopup();
        r->m_o.kind = kind;
        r->m_o.oppUid = oppUid;
        r->m_o.oppName = oppName;
        r->m_o.ranked = kind == "duel";
        r->m_level = level;
        r->m_done = std::move(done);
        if (r->init(400.f, 250.f)) { r->setup(); r->autorelease(); return r; }
        delete r;
        return nullptr;
    }
};

class SorDuelBoardPopup : public geode::Popup {
protected:
    bool setup() {
        this->setTitle("Versus Leaderboard", "goldFont.fnt", .7f, 18.f);
        auto* wait = label("Loading...", "bigFont.fnt", .35f, { 200, 200, 200 });
        wait->setPosition({ m_size.width / 2.f, m_size.height / 2.f });
        wait->setID("wait");
        m_mainLayer->addChild(wait);
        WeakRef<SorDuelBoardPopup> self = this;
        duels::leaderboard([self](std::vector<BoardRow> rows) { if (auto p = self.lock()) p->showRows(rows); });
        return true;
    }
    void showRows(const std::vector<BoardRow>& rows) {
        if (auto* w = m_mainLayer->getChildByID("wait")) w->removeFromParent();
        if (rows.empty()) {
            auto* l = label("No ranked duels yet - be the first!", "bigFont.fnt", .35f, { 200, 200, 200 });
            l->setPosition({ m_size.width / 2.f, m_size.height / 2.f });
            m_mainLayer->addChild(l);
            return;
        }
        const float colW = (m_size.width - 30.f) / 2.f;
        for (size_t i = 0; i < std::min<size_t>(rows.size(), 20); ++i) {
            const auto& r = rows[i];
            const float x = 15.f + (i / 10) * colW;
            const float y = m_size.height - 50.f - (i % 10) * 19.f;
            auto* bg = sorrow::ui::panel(colW - 6.f, 17.f, r.me ? cocos2d::ccColor3B { 60, 140, 70 } : cocos2d::ccColor3B { 0, 0, 0 }, 70);
            bg->setAnchorPoint({ 0.f, .5f });
            bg->setPosition({ x, y });
            m_mainLayer->addChild(bg);
            auto* pos = label(fmt::format("#{}", r.pos), "goldFont.fnt", .4f, r.pos <= 3 ? cocos2d::ccColor3B { 255, 215, 80 } : cocos2d::ccColor3B { 255, 255, 255 }, { 0.f, .5f });
            pos->setPosition({ x + 4.f, y });
            m_mainLayer->addChild(pos);
            auto* nm = label(r.name, "bigFont.fnt", .3f, { 255, 255, 255 }, { 0.f, .5f });
            nm->limitLabelWidth(70.f, .3f, .15f);
            nm->setPosition({ x + 28.f, y });
            m_mainLayer->addChild(nm);
            auto* el = label(fmt::format("{} {}", r.elo, r.rank), "goldFont.fnt", .35f, rankColor(r.rank), { 1.f, .5f });
            el->setPosition({ x + colW - 40.f, y });
            m_mainLayer->addChild(el);
            auto* wl = label(fmt::format("{}-{}", r.w, r.l), "bigFont.fnt", .25f, { 190, 190, 190 }, { 1.f, .5f });
            wl->setPosition({ x + colW - 10.f, y });
            m_mainLayer->addChild(wl);
        }
    }
public:
    static SorDuelBoardPopup* create() {
        auto* r = new SorDuelBoardPopup();
        if (r->init(400.f, 260.f)) { r->setup(); r->autorelease(); return r; }
        delete r;
        return nullptr;
    }
};

class SorDuelHistoryPopup : public geode::Popup {
protected:
    bool setup() {
        this->setTitle("Your Duels", "goldFont.fnt", .7f, 18.f);
        const auto& h = state().history;
        if (h.empty()) {
            auto* l = label("No duels yet", "bigFont.fnt", .35f, { 200, 200, 200 });
            l->setPosition({ m_size.width / 2.f, m_size.height / 2.f });
            m_mainLayer->addChild(l);
            return true;
        }
        for (size_t i = 0; i < std::min<size_t>(h.size(), 9); ++i) {
            const auto& r = h[i];
            const float y = m_size.height - 50.f - i * 21.f;
            const auto col = r.result == "win" ? cocos2d::ccColor3B { 90, 230, 120 } : r.result == "loss" ? cocos2d::ccColor3B { 255, 90, 90 } : cocos2d::ccColor3B { 200, 200, 200 };
            auto* bg = sorrow::ui::panel(m_size.width - 30.f, 19.f, { 0, 0, 0 }, 70);
            bg->setPosition({ m_size.width / 2.f, y });
            m_mainLayer->addChild(bg);
            auto* res = label(r.result == "win" ? "WIN" : r.result == "loss" ? "LOSS" : "DRAW", "bigFont.fnt", .32f, col, { 0.f, .5f });
            res->setPosition({ 22.f, y });
            m_mainLayer->addChild(res);
            auto* vs = label(fmt::format("vs {}", r.opp), "bigFont.fnt", .3f, { 255, 255, 255 }, { 0.f, .5f });
            vs->limitLabelWidth(120.f, .3f, .15f);
            vs->setPosition({ 70.f, y });
            m_mainLayer->addChild(vs);
            auto* md = label(fmt::format("{}  {}", r.mode == "sudden" ? "Sudden" : r.mode == "showdown" ? "Showdown" : "Race", r.score), "bigFont.fnt", .25f, { 180, 180, 200 });
            md->setPosition({ 230.f, y });
            m_mainLayer->addChild(md);
            std::string right;
            if (r.elo != 0) right += fmt::format("{}{} Elo  ", r.elo > 0 ? "+" : "", r.elo);
            if (r.net != 0) right += fmt::format("{}{}", r.net > 0 ? "+" : "-", sorrow::formatGlimmer(std::llabs(r.net)));
            auto* rl = label(right, "goldFont.fnt", .38f, r.net >= 0 ? cocos2d::ccColor3B { 255, 225, 120 } : cocos2d::ccColor3B { 255, 130, 110 }, { 1.f, .5f });
            rl->setPosition({ m_size.width - 22.f, y });
            m_mainLayer->addChild(rl);
        }
        return true;
    }
public:
    static SorDuelHistoryPopup* create() {
        auto* r = new SorDuelHistoryPopup();
        if (r->init(400.f, 250.f)) { r->setup(); r->autorelease(); return r; }
        delete r;
        return nullptr;
    }
};

inline void openSideBet(const Duel& d, const Player& on) {
    if (glimmerBalance() <= 0) { Notification::create("You have no Glimmer to bet", NotificationIcon::Warning)->show(); return; }
    const auto id = d.id, uid = on.uid, name = on.name;
    if (auto* p = SorBetPopup::create(glimmerBalance(), 0, [id, uid, name](std::int64_t amount) {
            duels::sideBet(id, uid, amount, [amount, name](bool ok, std::string err) {
                if (!ok) { Notification::create(err.c_str(), NotificationIcon::Error)->show(); return; }
                Notification::create(fmt::format("Bet {} on {}", sorrow::formatGlimmer(amount), name).c_str(), NotificationIcon::Success)->show();
                refreshBalance();
            });
        })) p->show();
}

} // namespace sorrow::duelui

// ------------------------------------------------------------------ screen --
class SorDuelLayer : public cocos2d::CCLayer {
public:
    static cocos2d::CCScene* scene(GJGameLevel* level = nullptr) {
        auto* sc = cocos2d::CCScene::create();
        auto* l = new SorDuelLayer();
        if (l->init(level)) { l->autorelease(); sc->addChild(l); }
        else delete l;
        return sc;
    }
    static int& alive() { static int n = 0; return n; }
    static SorDuelLayer*& current() { static SorDuelLayer* p = nullptr; return p; }
    SorDuelLayer() { ++alive(); }
    ~SorDuelLayer() override {
        --alive();
        if (current() == this) { current() = nullptr; sorrow::duels::onChanged() = nullptr; }
    }
    void own() {
        current() = this;
        sorrow::duels::onChanged() = [this] { this->refresh(false); };
    }

    bool init(GJGameLevel* level) {
        if (!CCLayer::init()) return false;
        using namespace sorrow::duelui;
        m_level = level;
        this->setKeypadEnabled(true);
        const auto win = cocos2d::CCDirector::get()->getWinSize();
        m_w = win.width; m_h = win.height;

        // background: the Sorrow photo dimmed, else a dark GD gradient
        if (auto* photo = sorrow::createCoverSprite("sorrow_bg.png", win)) {
            this->addChild(photo, -3);
            auto* dim = cocos2d::CCLayerColor::create({ 0, 0, 0, 150 });
            this->addChild(dim, -2);
        } else {
            auto* bg = cocos2d::CCSprite::create("GJ_gradientBG.png");
            bg->setScaleX(m_w / bg->getContentWidth());
            bg->setScaleY(m_h / bg->getContentHeight());
            bg->setAnchorPoint({ 0.f, 0.f });
            bg->setColor({ 45, 25, 70 });
            this->addChild(bg, -3);
        }
        auto* title = label("VERSUS", "goldFont.fnt", .95f);
        title->setPosition({ m_w / 2.f, m_h - 20.f });
        this->addChild(title);

        auto* top = CCMenu::create();
        top->setPosition({ 0.f, 0.f });
        this->addChild(top, 5);
        auto* back = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_arrow_01_001.png", .8f, [this](CCMenuItemSpriteExtra*) { this->keyBackClicked(); });
        back->setPosition({ 25.f, m_h - 22.f });
        top->addChild(back);
        auto* refresh = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_updateBtn_001.png", .6f, [](CCMenuItemSpriteExtra*) {
            sorrow::duels::DuelDirector::get()->pokeNow(); });
        refresh->setPosition({ m_w - 25.f, 25.f });
        top->addChild(refresh);

        m_dyn = cocos2d::CCNode::create();
        this->addChild(m_dyn, 2);

        this->own();
        this->schedule(schedule_selector(SorDuelLayer::tick), 0.1f);
        this->refresh(true);
        sorrow::duels::DuelDirector::get()->pokeNow();
        if (m_level) {
            // "Duel on this level" from a level page: go straight to the picker
            Ref<GJGameLevel> lv = m_level;
            geode::queueInMainThread([lv] {
                if (auto* p = sorrow::duelui::SorDuelPickPopup::create([lv](std::string uid, std::string name) {
                        if (auto* c = sorrow::duelui::SorDuelCreatePopup::create("duel", uid, name, lv)) c->show(); })) p->show();
            });
        }
        return true;
    }

    void onEnter() override { CCLayer::onEnter(); sorrow::duels::screenOpen() = true; this->own(); this->refresh(true); }
    void onExit() override { sorrow::duels::screenOpen() = false; CCLayer::onExit(); }

    void keyBackClicked() override {
        cocos2d::CCDirector::get()->popSceneWithTransition(.5f, cocos2d::PopTransition::kPopTransitionFade);
    }

private:
    // -------------------------------------------------------------- frame --
    std::string viewKey() {
        using namespace sorrow::duels;
        const auto& s = state();
        std::string k = fmt::format("{}|{}|{}|{}|{}|{}|{}|{}|{}|", s.loaded, s.linked, s.error, m_tab, m_page, s.me.elo, s.me.games,
            s.invites.size(), s.open.size());
        k += fmt::format("{}|", s.live.size());
        for (auto* d : { &s.duel, &s.watching }) {
            if (!d->valid) { k += "-|"; continue; }
            k += fmt::format("{}:{}:{}:{}:{}:{}:{}|", d->id, d->state, d->round, d->players.size(), d->levelId, d->sidePool, d->myBetAmount);
            for (auto& p : d->players) k += fmt::format("{}{}{}{}{}", p.uid, p.wins, p.pct, p.ready, p.forfeited);
            if (!d->events.empty()) k += std::to_string(d->events.back().seq);
        }
        for (auto& i : s.invites) k += i.id;
        for (auto& i : s.open) k += fmt::format("{}{}", i.id, i.players.size());
        for (auto& i : s.live) k += fmt::format("{}{}{}", i.id, i.state, i.round);
        return k;
    }
    void refresh(bool force) {
        const auto k = this->viewKey();
        if (!force && k == m_key) return;
        m_key = k;
        this->build();
    }
    void tick(float) {
        // live countdown texts
        using namespace sorrow::duels;
        if (!m_status) return;
        const Duel* d = this->shownDuel();
        if (!d) return;
        m_status->setString(this->statusText(*d).c_str());
        if (sorrow::glimmerBalance() != m_shownBalance) this->buildBalance();
    }

    const sorrow::duels::Duel* shownDuel() {
        auto& s = sorrow::duels::state();
        if (s.duel.valid) return &s.duel;
        if (s.watching.valid) return &s.watching;
        return nullptr;
    }

    void buildBalance() {
        using namespace sorrow::duelui;
        if (m_balance) m_balance->removeFromParent();
        m_shownBalance = sorrow::glimmerBalance();
        m_balance = cocos2d::CCNode::create();
        this->addChild(m_balance, 4);
        auto* bg = sorrow::ui::panel(120.f, 24.f, { 0, 0, 0 }, 110);
        bg->setPosition({ m_w - 72.f, m_h - 22.f });
        m_balance->addChild(bg);
        auto* amt = glimmerAmount(std::max<std::int64_t>(0, m_shownBalance), 15.f);
        amt->setPosition({ m_w - 72.f, m_h - 22.f });
        m_balance->addChild(amt);
    }

    void build() {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        m_dyn->removeAllChildren();
        m_status = nullptr;
        this->buildBalance();
        auto& s = state();
        m_menu = CCMenu::create();
        m_menu->setPosition({ 0.f, 0.f });
        m_dyn->addChild(m_menu, 5);

        // geometry
        const float left = 12.f, cardW = 140.f, top = m_h - 44.f, bottom = 32.f;
        m_px = left + cardW + 10.f;
        m_pw = m_w - m_px - 12.f;
        m_py = bottom;
        m_ph = top - 26.f - bottom;

        this->buildProfile(left, bottom, cardW, top - bottom);

        if (!s.loaded) { this->centerText("Connecting to the Sorrow bot..."); return; }
        if (!s.linked) { this->centerText("Link your account first: run /gd link in Discord"); return; }
        if (!s.error.empty() && !s.duel.valid) { this->centerText(s.error); return; }

        const Duel* d = this->shownDuel();
        if (d) { this->buildMatch(*d); return; }

        // tabs
        const std::vector<std::pair<int, std::string>> tabs = { { 0, "Play" },
            { 1, s.invites.empty() ? std::string("Invites") : fmt::format("Invites ({})", s.invites.size()) },
            { 2, fmt::format("Races{}", s.open.empty() ? "" : fmt::format(" ({})", s.open.size())) }, { 3, "Live" } };
        std::vector<cocos2d::CCNode*> tabNodes;
        for (auto& [id, name] : tabs) {
            auto* spr = ButtonSprite::create(name.c_str(), "bigFont.fnt", id == m_tab ? "GJ_button_01.png" : "GJ_button_04.png", .8f);
            spr->setScale(.55f);
            const int tid = id;
            auto* b = CCMenuItemExt::createSpriteExtra(spr, [this, tid](CCMenuItemSpriteExtra*) { m_tab = tid; m_page = 0; this->refresh(true); });
            m_menu->addChild(b);
            tabNodes.push_back(b);
        }
        sorrow::ui::row(tabNodes, m_px + m_pw / 2.f, top - 10.f, 6.f);
        if (!s.invites.empty() && m_tab != 1) {
            auto* dot = cocos2d::CCSprite::createWithSpriteFrameName("exMark_001.png");
            if (dot) { dot->setScale(.35f); dot->setPosition(tabNodes[1]->getPosition() + cocos2d::CCPoint { tabNodes[1]->getScaledContentWidth() / 2.f, 8.f }); m_dyn->addChild(dot, 6); }
        }

        auto* panel = sorrow::ui::panel(m_pw, m_ph, { 0, 0, 0 }, 110);
        panel->setAnchorPoint({ 0.f, 0.f });
        panel->setPosition({ m_px, m_py });
        m_dyn->addChild(panel);

        switch (m_tab) {
            case 0: this->buildPlay(); break;
            case 1: this->buildList(s.invites, "No invites - challenge someone from Play", 1); break;
            case 2: this->buildList(s.open, "No open races - open one from Play", 2); break;
            default: this->buildList(s.live, "No duels running right now", 3); break;
        }
    }

    void centerText(const std::string& t) {
        auto* l = sorrow::duelui::label(t, "bigFont.fnt", .38f, { 220, 220, 220 });
        l->limitLabelWidth(m_pw - 20.f, .38f, .2f);
        l->setPosition({ m_px + m_pw / 2.f, m_py + m_ph / 2.f });
        m_dyn->addChild(l, 3);
    }

    // ------------------------------------------------------------ profile --
    void buildProfile(float x, float y, float w, float h) {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        const auto& me = state().me;
        auto* bg = sorrow::ui::panel(w, h, { 0, 0, 0 }, 120);
        bg->setAnchorPoint({ 0.f, 0.f });
        bg->setPosition({ x, y });
        m_dyn->addChild(bg);
        const float cx = x + w / 2.f;
        float yy = y + h - 14.f;
        auto* head = label("YOUR RANK", "bigFont.fnt", .3f, { 190, 190, 210 });
        head->setPosition({ cx, yy });
        m_dyn->addChild(head);
        yy -= 22.f;
        // rank banner
        const auto col = rankColor(me.rank);
        auto* glow = sorrow::ui::panel(w - 20.f, 26.f, col, 70);
        glow->setPosition({ cx, yy });
        m_dyn->addChild(glow);
        auto* rk = label(me.rank, "goldFont.fnt", .75f, col);
        rk->setPosition({ cx, yy + 1.f });
        m_dyn->addChild(rk);
        yy -= 24.f;
        auto* elo = label(fmt::format("{} Elo", me.elo), "bigFont.fnt", .42f);
        elo->setPosition({ cx, yy });
        m_dyn->addChild(elo);
        yy -= 14.f;
        auto* pos = label(me.position > 0 ? fmt::format("#{} on the board", me.position) : std::string("Unranked"), "bigFont.fnt", .26f, { 180, 180, 200 });
        pos->setPosition({ cx, yy });
        m_dyn->addChild(pos);
        yy -= 20.f;
        // W / L / D columns
        const std::tuple<const char*, int, cocos2d::ccColor3B> cols[] = {
            { "W", me.w, { 90, 230, 120 } }, { "L", me.l, { 255, 100, 100 } }, { "D", me.d, { 200, 200, 200 } } };
        for (int i = 0; i < 3; ++i) {
            const float xx = x + w * (i + .5f) / 3.f;
            auto* n = label(std::to_string(std::get<1>(cols[i])), "goldFont.fnt", .55f, std::get<2>(cols[i]));
            n->setPosition({ xx, yy });
            m_dyn->addChild(n);
            auto* t = label(std::get<0>(cols[i]), "bigFont.fnt", .22f, { 170, 170, 190 });
            t->setPosition({ xx, yy - 11.f });
            m_dyn->addChild(t);
        }
        yy -= 30.f;
        const std::string streak = me.streak > 0 ? fmt::format("{} win streak", me.streak) : me.streak < 0 ? fmt::format("{} losses in a row", -me.streak) : "No streak";
        auto* st = label(streak, "bigFont.fnt", .27f, me.streak >= 3 ? cocos2d::ccColor3B { 255, 170, 60 } : cocos2d::ccColor3B { 220, 220, 220 });
        st->setPosition({ cx, yy });
        m_dyn->addChild(st);
        yy -= 13.f;
        auto* earned = label(fmt::format("Duel earnings {}{}", me.earned < 0 ? "-" : "+", sorrow::formatGlimmer(std::llabs(me.earned))), "bigFont.fnt", .25f,
            me.earned >= 0 ? cocos2d::ccColor3B { 255, 220, 110 } : cocos2d::ccColor3B { 255, 130, 110 });
        earned->limitLabelWidth(w - 12.f, .25f, .15f);
        earned->setPosition({ cx, yy });
        m_dyn->addChild(earned);

        auto* board = button("Leaderboard", "GJ_button_02.png", .5f, [] { if (auto* p = SorDuelBoardPopup::create()) p->show(); });
        auto* hist = button("History", "GJ_button_04.png", .5f, [] { if (auto* p = SorDuelHistoryPopup::create()) p->show(); });
        board->setPosition({ cx, y + 40.f });
        hist->setPosition({ cx, y + 16.f });
        m_menu->addChild(board);
        m_menu->addChild(hist);
    }

    // --------------------------------------------------------------- play --
    void buildPlay() {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        const float cw = (m_pw - 36.f) / 2.f, ch = 92.f;
        const float y = m_py + m_ph - 12.f - ch / 2.f;
        struct Card { const char* title; const char* desc; cocos2d::ccColor3B col; std::function<void()> cb; };
        Ref<GJGameLevel> lv = m_level;
        const Card cards[] = {
            { "CHALLENGE", "1v1 a player - Race, Showdown\nor Sudden Death, ranked Elo", { 80, 150, 255 }, [lv] {
                if (auto* p = SorDuelPickPopup::create([lv](std::string uid, std::string name) {
                        if (auto* c = SorDuelCreatePopup::create("duel", uid, name, lv)) c->show(); })) p->show(); } },
            { "OPEN A RACE", "Up to 8 players on the same\nlevel - winner takes the pot", { 255, 150, 60 }, [lv] {
                if (auto* c = SorDuelCreatePopup::create("race", "", "", lv)) c->show(); } },
        };
        for (int i = 0; i < 2; ++i) {
            const auto& c = cards[i];
            auto* node = cocos2d::CCNode::create();
            node->setContentSize({ cw, ch });
            auto* bg = sorrow::ui::panel(cw, ch, c.col, 90);
            bg->setPosition({ cw / 2.f, ch / 2.f });
            node->addChild(bg);
            auto* t = label(c.title, "goldFont.fnt", .7f);
            t->setPosition({ cw / 2.f, ch - 22.f });
            node->addChild(t);
            auto* dsc = label(c.desc, "bigFont.fnt", .27f, { 230, 230, 240 });
            dsc->setAlignment(cocos2d::kCCTextAlignmentCenter);
            dsc->setPosition({ cw / 2.f, ch / 2.f - 6.f });
            node->addChild(dsc);
            auto* go = label("TAP TO START", "bigFont.fnt", .22f, { 255, 255, 255 });
            go->setOpacity(160);
            go->setPosition({ cw / 2.f, 11.f });
            node->addChild(go);
            auto cb = c.cb;
            auto* item = CCMenuItemExt::createSpriteExtra(node, [cb](CCMenuItemSpriteExtra*) { cb(); });
            item->setPosition({ m_px + 12.f + cw / 2.f + i * (cw + 12.f), y });
            m_menu->addChild(item);
        }
        if (m_level) {
            auto* l = label(fmt::format("Level: {}", std::string(m_level->m_levelName)), "goldFont.fnt", .45f, { 255, 230, 150 });
            l->setPosition({ m_px + m_pw / 2.f, y - ch / 2.f - 10.f });
            m_dyn->addChild(l);
        }

        // nearby on Globed
        const float ny = m_py + 62.f;
        auto* nh = label("NEARBY ON GLOBED", "bigFont.fnt", .3f, { 140, 220, 255 }, { 0.f, .5f });
        nh->setPosition({ m_px + 14.f, ny + 22.f });
        m_dyn->addChild(nh);
        if (m_nearby.empty()) {
            auto* hint = label(sorrow::sessionMemberIds().empty() ? "Play a level on Globed to see players near you here"
                                                          : "Nobody near you has linked Sorrow yet",
                "bigFont.fnt", .26f, { 180, 180, 200 });
            hint->setPosition({ m_px + m_pw / 2.f, ny });
            m_dyn->addChild(hint);
        } else {
            const float bw = (m_pw - 24.f - 12.f) / 3.f;
            for (size_t i = 0; i < std::min<size_t>(m_nearby.size(), 3); ++i) {
                const auto& f = m_nearby[i];
                const float x = m_px + 12.f + bw / 2.f + i * (bw + 6.f);
                auto* bg = sorrow::ui::panel(bw, 40.f, { 0, 0, 0 }, 90);
                bg->setPosition({ x, ny - 4.f });
                m_dyn->addChild(bg);
                auto* nm = label(f.name, "bigFont.fnt", .3f);
                nm->limitLabelWidth(bw - 50.f, .3f, .15f);
                nm->setAnchorPoint({ 0.f, .5f });
                nm->setPosition({ x - bw / 2.f + 6.f, ny + 4.f });
                m_dyn->addChild(nm);
                auto* rk = label(fmt::format("{} {}", f.rank, f.elo), "goldFont.fnt", .32f, rankColor(f.rank), { 0.f, .5f });
                rk->setPosition({ x - bw / 2.f + 6.f, ny - 10.f });
                m_dyn->addChild(rk);
                if (!f.busy) {
                    auto uid = f.uid, name = f.name;
                    Ref<GJGameLevel> lv2 = m_level;
                    auto* b = button("VS", "GJ_button_01.png", .45f, [uid, name, lv2] {
                        if (auto* c = SorDuelCreatePopup::create("duel", uid, name, lv2)) c->show(); });
                    b->setPosition({ x + bw / 2.f - 20.f, ny - 4.f });
                    m_menu->addChild(b);
                }
            }
        }
        // refresh nearby every few seconds while on this tab
        if (!m_nearbyBusy && sorrow::duels::nowSec() - m_nearbyAt > 6.0) {
            m_nearbyBusy = true;
            m_nearbyAt = sorrow::duels::nowSec();
            WeakRef<SorDuelLayer> self = this;
            sorrow::duels::find("", [self](std::vector<sorrow::duels::FoundPlayer> f) {
                auto p = self.lock();
                if (!p) return;
                p->m_nearbyBusy = false;
                std::erase_if(f, [](auto& x) { return !x.nearby; });
                const bool diff = f.size() != p->m_nearby.size();
                p->m_nearby = std::move(f);
                if (diff) p->refresh(true);
            });
        }
    }

    // ------------------------------------------------------- list tabs --
    void buildList(const std::vector<sorrow::duels::Duel>& items, const char* empty, int kind) {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        if (items.empty()) {
            auto* l = label(empty, "bigFont.fnt", .32f, { 190, 190, 210 });
            l->setPosition({ m_px + m_pw / 2.f, m_py + m_ph / 2.f });
            m_dyn->addChild(l);
            return;
        }
        const int per = 4;
        const int pages = (static_cast<int>(items.size()) + per - 1) / per;
        m_page = std::clamp(m_page, 0, pages - 1);
        const float rh = 40.f;
        for (int i = 0; i < per; ++i) {
            const int idx = m_page * per + i;
            if (idx >= static_cast<int>(items.size())) break;
            const auto& d = items[idx];
            const float y = m_py + m_ph - 26.f - i * (rh + 6.f);
            auto* bg = sorrow::ui::panel(m_pw - 20.f, rh, { 0, 0, 0 }, 90);
            bg->setPosition({ m_px + m_pw / 2.f, y });
            m_dyn->addChild(bg);
            // left: who
            std::string who;
            if (kind == 1) who = fmt::format("{} challenges you", d.hostName);
            else if (kind == 2) who = fmt::format("{}'s race  ({}/{})", d.hostName, d.players.size(), d.maxPlayers);
            else if (d.players.size() == 2) who = fmt::format("{}  vs  {}", d.players[0].name, d.players[1].name);
            else who = fmt::format("Race - {} players", d.players.size());
            if (kind == 1 && !d.players.empty())
                if (auto* ic = cube(d.players[0], .5f)) { ic->setPosition({ m_px + 26.f, y }); m_dyn->addChild(ic); }
            const float tx = m_px + (kind == 1 ? 44.f : 20.f);
            auto* wl = label(who, "bigFont.fnt", .33f, { 255, 255, 255 }, { 0.f, .5f });
            wl->limitLabelWidth(m_pw * .52f, .33f, .15f);
            wl->setPosition({ tx, y + 7.f });
            m_dyn->addChild(wl);
            std::string sub = d.summary();
            if (d.ranked) sub += " - Ranked";
            if (kind == 3) {
                if (d.state == "lobby") sub = "Waiting to start - " + sub;
                else {
                    std::string sc;
                    for (auto& p : d.players) sc += (sc.empty() ? "" : "-") + std::to_string(p.wins);
                    sub = fmt::format("Round {} - {} - {}", std::max(1, d.round), sc, sub);
                }
            } else sub += " - " + diffName(d.difficulty);
            auto* sl = label(sub, "bigFont.fnt", .24f, { 180, 180, 205 }, { 0.f, .5f });
            sl->limitLabelWidth(m_pw * .55f, .24f, .12f);
            sl->setPosition({ tx, y - 7.f });
            m_dyn->addChild(sl);
            // stake / pot
            if (d.stake > 0 || d.sidePool > 0) {
                auto* amt = glimmerAmount(kind == 3 ? d.pot + d.sidePool : d.stake, 12.f);
                amt->setPosition({ m_px + m_pw * .66f, y });
                m_dyn->addChild(amt);
            }
            // actions
            const auto id = d.id;
            std::vector<cocos2d::CCNode*> btns;
            if (kind == 1) {
                const auto stake = d.stake;
                btns.push_back(button("Accept", "GJ_button_01.png", .5f, [id, stake] {
                    if (stake > 0 && sorrow::glimmerBalance() >= 0 && sorrow::glimmerBalance() < stake) {
                        Notification::create(fmt::format("You need {} Glimmer", sorrow::formatGlimmer(stake)).c_str(), NotificationIcon::Warning)->show();
                        return;
                    }
                    sorrow::duels::act("duels/accept", id, [](bool ok, std::string err) {
                        if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show();
                        else sorrow::refreshBalance();
                    });
                }));
                btns.push_back(button("No", "GJ_button_06.png", .5f, [id] { sorrow::duels::act("duels/decline", id); }));
            } else if (kind == 2) {
                btns.push_back(button("Join", "GJ_button_01.png", .5f, [id] {
                    sorrow::duels::act("duels/join", id, [](bool ok, std::string err) {
                        if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show(); else sorrow::refreshBalance(); });
                }));
            } else {
                btns.push_back(button("Watch", "GJ_button_02.png", .5f, [this, id] {
                    sorrow::duels::state().keepId = id;
                    sorrow::duels::DuelDirector::get()->pokeNow();
                }));
            }
            for (auto* b : btns) m_menu->addChild(b);
            sorrow::ui::row(btns, m_px + m_pw - 20.f - [&] { float t = 0; for (auto* b : btns) t += b->getScaledContentWidth(); return (t + 4.f * (btns.size() - 1)) / 2.f; }(), y, 4.f);
        }
        if (pages > 1) {
            auto* prev = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_arrow_03_001.png", .5f, [this](CCMenuItemSpriteExtra*) { --m_page; this->refresh(true); });
            prev->setPosition({ m_px + 14.f, m_py + 12.f });
            prev->setVisible(m_page > 0);
            m_menu->addChild(prev);
            auto* next = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_arrow_03_001.png", .5f, [this](CCMenuItemSpriteExtra*) { ++m_page; this->refresh(true); });
            static_cast<cocos2d::CCSprite*>(next->getNormalImage())->setFlipX(true);
            next->setPosition({ m_px + m_pw - 14.f, m_py + 12.f });
            next->setVisible(m_page < pages - 1);
            m_menu->addChild(next);
            auto* pg = label(fmt::format("{}/{}", m_page + 1, pages), "bigFont.fnt", .3f);
            pg->setPosition({ m_px + m_pw / 2.f, m_py + 12.f });
            m_dyn->addChild(pg);
        }
    }

    // -------------------------------------------------------------- match --
    std::string statusText(const sorrow::duels::Duel& d) {
        using namespace sorrow::duels;
        const double t = nowSec();
        const bool mine = d.isMine();
        if (d.state == "invite")
            return mine ? fmt::format("Waiting for {} to accept...", d.invitedName) : fmt::format("{} invited {}", d.hostName, d.invitedName);
        if (d.state == "lobby") {
            int ready = 0; for (auto& p : d.players) ready += p.ready;
            return fmt::format("Lobby - {}/{} ready", ready, d.players.size());
        }
        if (d.state == "intermission")
            return d.round == 0 ? "Rolling a level..." : fmt::format("Next round in {}...", std::max(0, static_cast<int>(std::ceil(d.nextAt - t)) + 6));
        if (d.state == "countdown")
            return fmt::format("Round {} - {} starts in {}", d.round, d.levelName, std::max(0, static_cast<int>(std::ceil(d.startAt - t))));
        if (d.state == "playing") {
            const int s = std::max(0, static_cast<int>(d.endsAt - t));
            return fmt::format("Round {} LIVE - {} - {}:{:02} left", d.round, d.levelName, s / 60, s % 60);
        }
        if (d.state == "finished") return "";
        if (d.state == "declined") return "Declined";
        if (d.state == "cancelled") return "Cancelled";
        if (d.state == "expired") return "Expired";
        return d.state;
    }

    void buildMatch(const sorrow::duels::Duel& d) {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        const auto& me = state().me;
        const bool mine = d.isMine();
        auto* panel = sorrow::ui::panel(m_pw, m_ph + 26.f, { 0, 0, 0 }, 110);
        panel->setAnchorPoint({ 0.f, 0.f });
        panel->setPosition({ m_px, m_py });
        m_dyn->addChild(panel);
        const float cx = m_px + m_pw / 2.f;
        const float topY = m_py + m_ph + 12.f;

        // header: mode summary, difficulty / level, pot
        std::string head = d.summary();
        if (d.ranked) head += " - Ranked";
        if (!mine) head = "WATCHING - " + head;
        auto* hl = label(head, "bigFont.fnt", .32f, { 255, 220, 120 });
        hl->limitLabelWidth(m_pw - 140.f, .32f, .15f);
        hl->setPosition({ cx, topY });
        m_dyn->addChild(hl);
        auto* sub = label(d.levelId > 0 ? fmt::format("{} by {}", d.levelName, d.levelAuthor.empty() ? "?" : d.levelAuthor)
                                        : diffName(d.difficulty), "bigFont.fnt", .25f, { 190, 190, 210 });
        sub->limitLabelWidth(m_pw - 140.f, .25f, .12f);
        sub->setPosition({ cx, topY - 13.f });
        m_dyn->addChild(sub);
        if (d.levelId <= 0)
            if (auto* face = cocos2d::CCSprite::createWithSpriteFrameName(diffFrame(d.difficulty))) {
                face->setScale(.4f);
                face->setPosition({ m_px + 22.f, topY - 6.f });
                m_dyn->addChild(face);
            }
        if (d.pot > 0 || d.sidePool > 0) {
            auto* potBg = sorrow::ui::panel(84.f, 20.f, { 60, 40, 0 }, 120);
            potBg->setPosition({ m_px + m_pw - 50.f, topY - 6.f });
            m_dyn->addChild(potBg);
            auto* pot = glimmerAmount(d.state == "invite" ? d.stake * 2 : d.pot, 12.f);
            pot->setPosition({ m_px + m_pw - 50.f, topY - 6.f });
            m_dyn->addChild(pot);
            if (d.sidePool > 0) {
                auto* sp = label(fmt::format("+{} side bets", sorrow::formatGlimmer(d.sidePool)), "bigFont.fnt", .2f, { 255, 220, 120 });
                sp->setPosition({ m_px + m_pw - 50.f, topY - 21.f });
                m_dyn->addChild(sp);
            }
        }

        const float midY = m_py + m_ph / 2.f + 18.f;
        const int need = d.bestOf / 2 + 1;
        auto playersShown = d.players;
        if (d.kind == "duel" && d.state == "invite" && playersShown.size() == 1) {
            sorrow::duels::Player ghost; ghost.name = d.invitedName; ghost.rank = "?"; ghost.uid = d.invited; ghost.iconId = 1;
            playersShown.push_back(ghost);
        }
        if (playersShown.size() <= 2) {
            // VS layout
            for (size_t i = 0; i < playersShown.size(); ++i) {
                const auto& p = playersShown[i];
                const float x = m_px + m_pw * (i == 0 ? .22f : .78f);
                const bool isMe = p.uid == me.uid;
                auto* ring = sorrow::ui::panel(64.f, 64.f, isMe ? cocos2d::ccColor3B { 60, 150, 80 } : cocos2d::ccColor3B { 40, 40, 60 }, 110);
                ring->setPosition({ x, midY + 16.f });
                m_dyn->addChild(ring);
                if (d.state == "invite" && i == 1) {
                    auto* q = label("?", "goldFont.fnt", 1.2f, { 200, 200, 200 });
                    q->setPosition({ x, midY + 16.f });
                    m_dyn->addChild(q);
                } else if (auto* ic = cube(p, 1.15f)) {
                    ic->setPosition({ x, midY + 16.f });
                    if (p.forfeited) ic->setOpacity(90);
                    m_dyn->addChild(ic);
                }
                auto* nm = label(p.name, "goldFont.fnt", .6f, isMe ? cocos2d::ccColor3B { 150, 255, 170 } : cocos2d::ccColor3B { 255, 255, 255 });
                nm->limitLabelWidth(m_pw * .36f, .6f, .25f);
                nm->setPosition({ x, midY - 26.f });
                m_dyn->addChild(nm);
                if (p.rank != "?") {
                    auto* rk = label(fmt::format("{} - {}", p.rank, p.elo), "bigFont.fnt", .25f, rankColor(p.rank));
                    rk->setPosition({ x, midY - 39.f });
                    m_dyn->addChild(rk);
                }
                // win pips
                for (int k = 0; k < need; ++k) {
                    auto* pip = cocos2d::CCSprite::createWithSpriteFrameName(k < p.wins ? "GJ_starsIcon_001.png" : "GJ_starsIcon_gray_001.png");
                    if (!pip) continue;
                    pip->setScale(.45f);
                    pip->setPosition({ x + (k - (need - 1) / 2.f) * 13.f, midY - 52.f });
                    m_dyn->addChild(pip);
                }
                // this round's best %
                if (d.racing() || d.state == "intermission") {
                    auto* pc = label(d.mode == "sudden" && !p.done && d.racing() ? "alive" : fmt::format("{}%", p.pct), "bigFont.fnt", .32f,
                        { 255, 255, 255 });
                    pc->setPosition({ x, midY + 54.f });
                    m_dyn->addChild(pc);
                }
                if (p.ready && d.state == "lobby") {
                    auto* rd = label("READY", "bigFont.fnt", .25f, { 120, 255, 140 });
                    rd->setPosition({ x, midY + 54.f });
                    m_dyn->addChild(rd);
                }
                if (p.forfeited) {
                    auto* ff = label("FORFEIT", "bigFont.fnt", .3f, { 255, 90, 90 });
                    ff->setPosition({ x, midY + 16.f });
                    m_dyn->addChild(ff);
                }
                // spectators: side bet buttons
                if (!mine && d.bettingOpen() && d.myBetAmount == 0 && p.rank != "?") {
                    const Duel dd = d;
                    const auto pl = p;
                    auto* b = button("Bet", "GJ_button_02.png", .42f, [dd, pl] { openSideBet(dd, pl); });
                    b->setPosition({ x + 44.f, midY + 40.f });
                    m_menu->addChild(b);
                }
                if (p.side > 0) {
                    auto* sb = label(fmt::format("{} backed", sorrow::formatGlimmer(p.side)), "bigFont.fnt", .2f, { 255, 220, 120 });
                    sb->setPosition({ x, midY - 64.f });
                    m_dyn->addChild(sb);
                }
            }
            auto* vs = label("VS", "goldFont.fnt", 1.1f, { 255, 120, 80 });
            vs->setPosition({ cx, midY + 22.f });
            m_dyn->addChild(vs);
            if (playersShown.size() == 2 && d.state != "invite") {
                auto* score = label(fmt::format("{} - {}", playersShown[0].wins, playersShown[1].wins), "bigFont.fnt", .6f);
                score->setPosition({ cx, midY - 6.f });
                m_dyn->addChild(score);
                auto* bo = label(fmt::format("first to {}", need), "bigFont.fnt", .22f, { 180, 180, 200 });
                bo->setPosition({ cx, midY - 22.f });
                m_dyn->addChild(bo);
            }
        } else {
            // race: up to 8 players in two columns
            auto sorted = playersShown;
            std::stable_sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.wins > b.wins || (a.wins == b.wins && a.pct > b.pct); });
            const float colW = (m_pw - 30.f) / 2.f;
            for (size_t i = 0; i < std::min<size_t>(sorted.size(), 8); ++i) {
                const auto& p = sorted[i];
                const float x = m_px + 12.f + (i / 4) * (colW + 6.f);
                const float y = midY + 48.f - (i % 4) * 25.f;
                const bool isMe = p.uid == me.uid;
                auto* bg = sorrow::ui::panel(colW, 22.f, isMe ? cocos2d::ccColor3B { 50, 120, 60 } : cocos2d::ccColor3B { 0, 0, 0 }, 90);
                bg->setAnchorPoint({ 0.f, .5f });
                bg->setPosition({ x, y });
                m_dyn->addChild(bg);
                if (auto* ic = cube(p, .45f)) { ic->setPosition({ x + 12.f, y }); m_dyn->addChild(ic); }
                auto* nm = label(p.name, "bigFont.fnt", .3f, p.forfeited ? cocos2d::ccColor3B { 140, 140, 140 } : cocos2d::ccColor3B { 255, 255, 255 }, { 0.f, .5f });
                nm->limitLabelWidth(colW * .45f, .3f, .12f);
                nm->setPosition({ x + 24.f, y });
                m_dyn->addChild(nm);
                auto* info = label(d.state == "lobby" ? (p.ready ? "READY" : "") : fmt::format("{}W  {}%", p.wins, p.pct), "bigFont.fnt", .26f,
                    p.ready && d.state == "lobby" ? cocos2d::ccColor3B { 120, 255, 140 } : cocos2d::ccColor3B { 255, 220, 120 }, { 1.f, .5f });
                info->setPosition({ x + colW - 6.f, y });
                m_dyn->addChild(info);
            }
        }

        // status / result
        const float statusY = m_py + 50.f;
        if (d.state == "finished" && d.result.has) {
            const auto& r = d.result;
            const bool won = r.winnerId == me.uid, draw = r.winnerId.empty();
            std::string big = !mine ? (draw ? "DRAW" : fmt::format("{} WINS", r.winnerName)) : draw ? "DRAW" : won ? "VICTORY" : "DEFEAT";
            auto* res = label(big, "goldFont.fnt", .85f, draw ? cocos2d::ccColor3B { 220, 220, 220 } : (won || !mine) ? cocos2d::ccColor3B { 120, 255, 140 } : cocos2d::ccColor3B { 255, 90, 90 });
            res->setPosition({ cx, statusY + 6.f });
            m_dyn->addChild(res);
            std::string line;
            if (mine && r.myPayout > r.myStake) line = fmt::format("+{} Glimmer", sorrow::formatGlimmer(r.myPayout - r.myStake));
            else if (mine && r.myStake > 0 && r.myPayout == 0) line = fmt::format("-{} Glimmer", sorrow::formatGlimmer(r.myStake));
            if (r.eloMoved) line += fmt::format("{}{}{} Elo", line.empty() ? "" : "   ", r.eloAfter - r.eloBefore >= 0 ? "+" : "", r.eloAfter - r.eloBefore);
            if (r.mySideStake > 0) line += fmt::format("{}Side bet: {}", line.empty() ? "" : "   ",
                r.mySidePayout > r.mySideStake ? "+" + sorrow::formatGlimmer(r.mySidePayout) : r.mySidePayout > 0 ? std::string("refunded") : "-" + sorrow::formatGlimmer(r.mySideStake));
            if (!line.empty()) {
                auto* ll = label(line, "bigFont.fnt", .3f, { 255, 225, 130 });
                ll->setPosition({ cx, statusY - 13.f });
                m_dyn->addChild(ll);
            }
        } else {
            m_status = label(this->statusText(d), "bigFont.fnt", .32f, { 230, 230, 255 });
            m_status->setPosition({ cx, statusY });
            m_dyn->addChild(m_status);
            if (!d.events.empty()) {
                auto* ev = label(d.events.back().text, "bigFont.fnt", .22f, { 170, 170, 200 });
                ev->limitLabelWidth(m_pw - 30.f, .22f, .1f);
                ev->setPosition({ cx, statusY - 14.f });
                m_dyn->addChild(ev);
            }
        }

        // actions
        std::vector<cocos2d::CCNode*> btns;
        const auto id = d.id;
        if (!mine) {
            btns.push_back(button("Close", "GJ_button_04.png", .55f, [this] {
                sorrow::duels::state().keepId.clear(); sorrow::duels::state().watching = {}; this->refresh(true); }));
        } else if (d.state == "invite") {
            btns.push_back(button("Cancel", "GJ_button_06.png", .55f, [id] { sorrow::duels::act("duels/leave", id, [](bool, std::string) { sorrow::refreshBalance(); }); }));
        } else if (d.state == "lobby") {
            const auto* p = d.find(me.uid);
            const bool rdy = p && p->ready;
            btns.push_back(button(rdy ? "Unready" : "Ready", rdy ? "GJ_button_04.png" : "GJ_button_01.png", .55f, [id, rdy] { sorrow::duels::ready(id, !rdy); }));
            if (d.hostId == me.uid && d.players.size() >= 2)
                btns.push_back(button("Start", "GJ_button_02.png", .55f, [id] { sorrow::duels::act("duels/start", id); }));
            btns.push_back(button(d.hostId == me.uid ? "Close Race" : "Leave", "GJ_button_06.png", .55f, [id] {
                sorrow::duels::act("duels/leave", id, [](bool, std::string) { sorrow::refreshBalance(); }); }));
        } else if (d.live()) {
            if (d.racing() && d.levelId > 0)
                btns.push_back(button("Go to Level", "GJ_button_01.png", .55f, [] { sorrow::duels::DuelDirector::get()->pokeNow(); goToRound(); }));
            btns.push_back(button("Forfeit", "GJ_button_06.png", .55f, [id] {
                geode::createQuickPopup("Forfeit?", "You lose the duel and your stake.", "Cancel", "Forfeit", [id](auto*, bool yes) {
                    if (yes) sorrow::duels::act("duels/leave", id);
                });
            }));
        } else if (d.state == "finished") {
            if (d.kind == "duel")
                btns.push_back(button("Rematch", "GJ_button_01.png", .55f, [id] {
                    sorrow::duels::act("duels/rematch", id, [](bool ok, std::string err) {
                        if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show();
                        else Notification::create("Rematch sent!", NotificationIcon::Success)->show(); });
                }));
            btns.push_back(button("Close", "GJ_button_04.png", .55f, [this] {
                auto& s = sorrow::duels::state(); s.keepId.clear(); s.duel = {}; this->refresh(true); }));
        } else {
            btns.push_back(button("Close", "GJ_button_04.png", .55f, [this] {
                auto& s = sorrow::duels::state(); s.keepId.clear(); s.duel = {}; this->refresh(true); }));
        }
        for (auto* b : btns) m_menu->addChild(b);
        sorrow::ui::row(btns, cx, m_py + 16.f, 8.f);
    }

    static void goToRound() {
        const auto& d = sorrow::duels::state().duel;
        if (d.levelId <= 0) return;
        const int id = d.levelId;
        const auto name = d.levelName;
        sorrow::levels::fetchById(id, [id, name](GJGameLevel* lvl) {
            sorrow::levels::openLevelPage(lvl ? lvl : sorrow::levels::bareLevel(id, name));
        });
    }

    Ref<GJGameLevel> m_level;
    float m_w = 0, m_h = 0, m_px = 0, m_py = 0, m_pw = 0, m_ph = 0;
    int m_tab = 0, m_page = 0;
    bool m_nearbyBusy = false;
    double m_nearbyAt = -100.0;
    std::vector<sorrow::duels::FoundPlayer> m_nearby;
    std::string m_key;
    std::int64_t m_shownBalance = -2;
    cocos2d::CCNode* m_dyn = nullptr;
    cocos2d::CCNode* m_balance = nullptr;
    CCMenu* m_menu = nullptr;
    CCLabelBMFont* m_status = nullptr;
};

namespace sorrow::duels {
// Open the Versus screen: back on top of the stack if it is below the level
// page we came from, else as a new scene.
inline void openVersus(GJGameLevel* level = nullptr) {
    auto* dir = cocos2d::CCDirector::get();
    auto* running = dir->getRunningScene();
    if (!level && SorDuelLayer::alive() > 0 && running && running->getChildByType<LevelInfoLayer>(0)) {
        dir->popSceneWithTransition(.5f, cocos2d::PopTransition::kPopTransitionFade);
        return;
    }
    dir->pushScene(cocos2d::CCTransitionFade::create(.5f, SorDuelLayer::scene(level)));
}
inline void installScreenHook() {
    openScreenHook() = [] { openVersus(); };
}
} // namespace sorrow::duels
