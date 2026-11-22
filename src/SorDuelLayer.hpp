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
    if (m == "consistency") return "Every attempt counts - highest average % wins";
    return "One attempt each - your first death counts";
}

// Rank trophy: GD's leaderboard trophy tinted for the tier, with sparkles
// that get busier the higher the rank.
inline cocos2d::CCNode* trophy(const std::string& rank, float size) {
    static const std::map<std::string, std::pair<const char*, int>> m = {
        { "Bronze", { "rankIcon_top1000_001.png", 0 } }, { "Silver", { "rankIcon_top500_001.png", 1 } },
        { "Gold", { "rankIcon_top200_001.png", 2 } }, { "Platinum", { "rankIcon_top100_001.png", 3 } },
        { "Diamond", { "rankIcon_top50_001.png", 5 } }, { "Champion", { "rankIcon_top10_001.png", 8 } } };
    auto it = m.find(rank);
    const auto [frame, sparkles] = it == m.end() ? std::pair<const char*, int> { "rankIcon_all_001.png", 0 } : it->second;
    auto* node = cocos2d::CCNode::create();
    node->setContentSize({ size, size });
    node->setAnchorPoint({ .5f, .5f });
    auto* glow = cocos2d::CCSprite::createWithSpriteFrameName("d_circle_02_001.png");
    if (glow) {
        glow->setColor(rankColor(rank));
        glow->setOpacity(70);
        glow->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        glow->setScale(size * 1.3f / std::max(1.f, glow->getContentWidth()));
        glow->setPosition({ size / 2.f, size / 2.f });
        glow->runAction(cocos2d::CCRepeatForever::create(cocos2d::CCSequence::create(
            cocos2d::CCFadeTo::create(1.2f, 120), cocos2d::CCFadeTo::create(1.2f, 50), nullptr)));
        node->addChild(glow);
    }
    auto* spr = cocos2d::CCSprite::createWithSpriteFrameName(frame);
    if (!spr) spr = cocos2d::CCSprite::createWithSpriteFrameName("GJ_achImage_001.png");
    if (spr) {
        spr->setScale(size / std::max(1.f, std::max(spr->getContentWidth(), spr->getContentHeight())));
        spr->setPosition({ size / 2.f, size / 2.f });
        node->addChild(spr);
    }
    for (int i = 0; i < sparkles; ++i) {
        auto* sp = cocos2d::CCSprite::createWithSpriteFrameName("star_small01_001.png");
        if (!sp) break;
        sp->setColor(rankColor(rank));
        sp->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        sp->setScale(0.f);
        const float ang = i * 6.2831f / sparkles;
        sp->setPosition({ size / 2.f + std::cos(ang) * size * .55f, size / 2.f + std::sin(ang) * size * .5f });
        const float d = .15f * i;
        sp->runAction(cocos2d::CCRepeatForever::create(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(d), cocos2d::CCScaleTo::create(.35f, .5f), cocos2d::CCScaleTo::create(.35f, 0.f),
            cocos2d::CCDelayTime::create(1.2f - d), nullptr)));
        node->addChild(sp);
    }
    return node;
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
        l->limitLabelWidth(m_size.width - 40.f, .35f, .15f);
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
            rk->limitLabelWidth(125.f, .38f, .2f);
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

// Duel / race / tournament settings. Fixed grid (400 x 290):
//   title 272 | mode 240 (+ description 222) | rounds 194 | target 160
//   level 122 | stake 76 | hint 50 | send 24
class SorDuelCreatePopup : public geode::Popup {
protected:
    CreateOptions m_o;
    std::string m_kind;                 // duel | race | tour
    int m_cupSize = 8;
    Ref<GJGameLevel> m_level;
    cocos2d::CCNode* m_body = nullptr;
    TextInput* m_stake = nullptr;
    std::function<void()> m_done;
    static inline const std::vector<std::string> kDiffs = { "any", "easy", "normal", "hard", "harder", "insane",
        "easy_demon", "medium_demon", "hard_demon", "insane_demon", "extreme_demon" };

    static cocos2d::CCNode* rowLabel(const char* text) {
        auto* l = label(text, "goldFont.fnt", .5f, { 255, 255, 255 }, { 0.f, .5f });
        return l;
    }

    bool setup() {
        const char* title = m_kind == "race" ? "Open a Race" : m_kind == "tour" ? "Host a Tournament" : nullptr;
        this->setTitle(title ? title : fmt::format("Challenge {}", m_o.oppName).c_str(), "goldFont.fnt", .75f, 18.f);
        m_body = cocos2d::CCNode::create();
        m_mainLayer->addChild(m_body);
        const float W = m_size.width;

        // stake / entry (outside the rebuilt body so typing survives toggles)
        auto* sl = rowLabel(m_kind == "tour" ? "Entry fee" : "Stake (each)");
        sl->setPosition({ 20.f, 76.f });
        m_mainLayer->addChild(sl);
        auto* sbg = sorrow::ui::panel(120.f, 26.f, { 0, 0, 0 }, 90);
        sbg->setPosition({ 175.f, 76.f });
        m_mainLayer->addChild(sbg);
        if (auto* gem = betfx::glimmerSprite(13.f)) { gem->setPosition({ 127.f, 76.f }); m_mainLayer->addChild(gem); }
        m_stake = TextInput::create(100.f, "0", "bigFont.fnt");
        m_stake->setCommonFilter(CommonFilter::Uint);
        m_stake->setMaxCharCount(15);
        m_stake->setScale(.8f);
        m_stake->hideBG();
        m_stake->setPosition({ 183.f, 76.f });
        m_mainLayer->addChild(m_stake);
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(menu);
        const std::pair<const char*, std::int64_t> cs[] = { { "10K", 10000 }, { "100K", 100000 }, { "1M", 1000000 } };
        std::vector<cocos2d::CCNode*> chips;
        for (auto& [name, v] : cs) {
            const auto val = v;
            auto* c = button(name, "GJ_button_04.png", .42f, [this, val] { m_stake->setString(std::to_string(val).c_str()); });
            menu->addChild(c);
            chips.push_back(c);
        }
        sorrow::ui::row(chips, 305.f, 76.f, 5.f);
        const char* send = m_kind == "race" ? "Open Race" : m_kind == "tour" ? "Open Tournament" : "Send Challenge";
        auto* go = button(send, "GJ_button_01.png", .72f, [this] { this->onSend(); });
        go->setPosition({ W / 2.f, 24.f });
        menu->addChild(go);
        this->rebuild();
        return true;
    }

    // segmented control: equal widths, selected one green
    void segment(CCMenu* menu, float x0, float y, float w, const std::vector<std::pair<std::string, std::string>>& opts,
                 const std::string& cur, std::function<void(std::string)> pick) {
        // Every button gets the same absolute width, then is scaled so it
        // fills its slot minus a gap: equal widths, never overlapping.
        const float step = w / opts.size();
        for (size_t i = 0; i < opts.size(); ++i) {
            const auto val = opts[i].first;
            auto* spr = ButtonSprite::create(opts[i].second.c_str(), 150, true, "bigFont.fnt",
                val == cur ? "GJ_button_01.png" : "GJ_button_04.png", 30.f, .55f);
            spr->setScale(std::min(.6f, (step - 6.f) / std::max(1.f, spr->getContentWidth())));
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
        const float W = m_size.width;
        const float x0 = 100.f, segW = W - x0 - 16.f;

        // mode
        auto* ml = rowLabel("Mode"); ml->setPosition({ 20.f, 240.f }); m_body->addChild(ml);
        this->segment(menu, x0, 240.f, segW, { { "race", "Race" }, { "showdown", "Showdown" }, { "consistency", "Consistency" }, { "sudden", "Sudden" } },
            m_o.mode, [this](std::string v) { m_o.mode = v; });
        auto* md = label(modeDesc(m_o.mode), "bigFont.fnt", .26f, { 200, 200, 225 });
        md->limitLabelWidth(segW, .26f, .12f);
        md->setPosition({ x0 + segW / 2.f, 222.f });
        m_body->addChild(md);

        // rounds + ranked (duel) / size (tour)
        auto* bl = rowLabel("Best of"); bl->setPosition({ 20.f, 194.f }); m_body->addChild(bl);
        this->segment(menu, x0, 194.f, 120.f, { { "1", "1" }, { "3", "3" }, { "5", "5" } }, std::to_string(m_o.bestOf),
            [this](std::string v) { m_o.bestOf = std::stoi(v); });
        if (m_kind == "duel")
            this->segment(menu, x0 + 132.f, 194.f, segW - 132.f, { { "1", "Ranked" }, { "0", "Casual" } }, m_o.ranked ? "1" : "0",
                [this](std::string v) { m_o.ranked = v == "1"; });
        else if (m_kind == "tour")
            this->segment(menu, x0 + 132.f, 194.f, segW - 132.f, { { "4", "4 players" }, { "8", "8 players" } }, std::to_string(m_cupSize),
                [this](std::string v) { m_cupSize = std::stoi(v); });

        // target % (race) or time limit
        const bool race = m_o.mode == "race";
        auto* tl = rowLabel(race ? "Target" : "Time limit"); tl->setPosition({ 20.f, 160.f }); m_body->addChild(tl);
        auto* minus = button("-", "GJ_button_04.png", .6f, [this, race] {
            if (race) m_o.target = std::max(5, m_o.target - 5); else m_o.seconds = std::max(60, m_o.seconds - 60);
            this->rebuild(); });
        minus->setPosition({ x0 + 16.f, 160.f });
        menu->addChild(minus);
        auto* vbg = sorrow::ui::panel(70.f, 26.f, { 0, 0, 0 }, 90);
        vbg->setPosition({ x0 + 62.f, 160.f });
        m_body->addChild(vbg);
        auto* tv = label(race ? fmt::format("{}%", m_o.target) : fmt::format("{}:00", m_o.seconds / 60), "bigFont.fnt", .45f);
        tv->setPosition({ x0 + 62.f, 160.f });
        m_body->addChild(tv);
        auto* plus = button("+", "GJ_button_04.png", .6f, [this, race] {
            if (race) m_o.target = std::min(100, m_o.target + 5); else m_o.seconds = std::min(900, m_o.seconds + 60);
            this->rebuild(); });
        plus->setPosition({ x0 + 108.f, 160.f });
        menu->addChild(plus);
        if (race) {
            auto* hint = label(m_o.target >= 100 ? "Completion race" : "Highest % wins if time runs out", "bigFont.fnt", .24f, { 180, 180, 205 }, { 0.f, .5f });
            hint->limitLabelWidth(segW - 132.f, .24f, .1f);
            hint->setPosition({ x0 + 128.f, 160.f });
            m_body->addChild(hint);
        }

        // level
        auto* ll = rowLabel("Level"); ll->setPosition({ 20.f, 122.f }); m_body->addChild(ll);
        auto* lbg = sorrow::ui::panel(segW, 34.f, { 0, 0, 0 }, 90);
        lbg->setAnchorPoint({ 0.f, .5f });
        lbg->setPosition({ x0, 122.f });
        m_body->addChild(lbg);
        if (m_level && m_kind != "tour") {
            auto* lv = label(std::string(m_level->m_levelName), "goldFont.fnt", .55f, { 255, 230, 150 });
            lv->limitLabelWidth(segW - 20.f, .55f, .25f);
            lv->setPosition({ x0 + segW / 2.f, 122.f });
            m_body->addChild(lv);
        } else {
            const auto it = std::find(kDiffs.begin(), kDiffs.end(), m_o.difficulty);
            const int idx = it == kDiffs.end() ? 0 : static_cast<int>(it - kDiffs.begin());
            auto* left = CCMenuItemExt::createSpriteExtraWithFrameName("navArrowBtn_001.png", .4f, [this, idx](CCMenuItemSpriteExtra*) {
                m_o.difficulty = kDiffs[(idx + kDiffs.size() - 1) % kDiffs.size()]; this->rebuild(); });
            left->setScaleX(-1.f);
            left->setPosition({ x0 + 16.f, 122.f });
            menu->addChild(left);
            if (auto* face = cocos2d::CCSprite::createWithSpriteFrameName(diffFrame(m_o.difficulty))) {
                face->setScale(.5f);
                face->setPosition({ x0 + segW / 2.f - 50.f, 124.f });
                m_body->addChild(face);
            }
            auto* dn = label(diffName(m_o.difficulty), "bigFont.fnt", .38f, { 255, 255, 255 }, { 0.f, .5f });
            dn->limitLabelWidth(130.f, .38f, .2f);
            dn->setPosition({ x0 + segW / 2.f - 30.f, 122.f });
            m_body->addChild(dn);
            auto* right = CCMenuItemExt::createSpriteExtraWithFrameName("navArrowBtn_001.png", .4f, [this, idx](CCMenuItemSpriteExtra*) {
                m_o.difficulty = kDiffs[(idx + 1) % kDiffs.size()]; this->rebuild(); });
            right->setPosition({ x0 + segW - 16.f, 122.f });
            menu->addChild(right);
        }
        const char* hintText = m_kind == "race" ? "Anyone can join from the Races tab - winner takes the whole pot"
            : m_kind == "tour" ? "Single elimination - prize pool: 1st 70%, 2nd 30%"
            : "Winner takes both stakes - ranked duels move your Elo";
        auto* hint = label(hintText, "bigFont.fnt", .25f, { 190, 190, 215 });
        hint->limitLabelWidth(W - 30.f, .25f, .12f);
        hint->setPosition({ W / 2.f, 50.f });
        m_body->addChild(hint);
    }

    void onSend() {
        std::int64_t stake = 0;
        try { stake = std::stoll(std::string(m_stake->getString())); } catch (...) {}
        if (stake > 0 && sorrow::glimmerBalance() >= 0 && stake > sorrow::glimmerBalance()) {
            Notification::create("Not enough Glimmer", NotificationIcon::Warning)->show();
            return;
        }
        auto done = m_done;
        WeakRef<SorDuelCreatePopup> self = this;
        auto finish = [self, done](const char* okText) {
            return [self, done, okText](bool ok, std::string err) {
                if (!ok) { Notification::create(err.c_str(), NotificationIcon::Error)->show(); return; }
                Notification::create(okText, NotificationIcon::Success)->show();
                sorrow::refreshBalance();
                if (auto p = self.lock()) p->onClose(nullptr);
                if (done) done();
            };
        };
        if (m_kind == "tour") {
            TourOptions t;
            t.mode = m_o.mode; t.difficulty = m_o.difficulty; t.size = m_cupSize; t.bestOf = m_o.bestOf;
            t.target = m_o.target; t.seconds = m_o.seconds; t.entry = stake;
            duels::tourCreate(t, finish("Tournament opened"));
            return;
        }
        m_o.stake = stake;
        m_o.level = m_level;
        duels::create(m_o, finish(m_kind == "race" ? "Race opened" : "Challenge sent!"));
    }
public:
    static SorDuelCreatePopup* create(const std::string& kind, const std::string& oppUid, const std::string& oppName,
                                      GJGameLevel* level, std::function<void()> done = nullptr) {
        auto* r = new SorDuelCreatePopup();
        r->m_kind = kind;
        r->m_o.kind = kind == "tour" ? "race" : kind;
        r->m_o.oppUid = oppUid;
        r->m_o.oppName = oppName;
        r->m_o.ranked = kind == "duel";
        r->m_level = level;
        r->m_done = std::move(done);
        if (r->init(400.f, 290.f)) { r->setup(); r->autorelease(); return r; }
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

// Rank up: trophy, old -> new rank, sparkles.
class SorRankUpPopup : public geode::Popup {
protected:
    std::string m_from, m_to;
    bool setup() {
        const float cx = m_size.width / 2.f, H = m_size.height;
        auto* t = label("RANK UP!", "goldFont.fnt", 1.f, { 255, 230, 120 });
        t->setPosition({ cx, H - 30.f });
        m_mainLayer->addChild(t);
        t->setScale(0.f);
        t->runAction(cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(.35f, 1.f)));
        auto* tr = trophy(m_to, 70.f);
        tr->setPosition({ cx, H / 2.f + 6.f });
        m_mainLayer->addChild(tr);
        tr->setScale(.2f);
        tr->runAction(cocos2d::CCEaseElasticOut::create(cocos2d::CCScaleTo::create(.8f, 1.f), .5f));
        auto* line = label(fmt::format("{}  >  {}", m_from, m_to), "bigFont.fnt", .5f, rankColor(m_to));
        line->setPosition({ cx, 52.f });
        m_mainLayer->addChild(line);
        auto* sub = label(fmt::format("{} Elo", sorrow::duels::state().me.elo), "goldFont.fnt", .5f);
        sub->setPosition({ cx, 32.f });
        m_mainLayer->addChild(sub);
        playGlimmerSound(1.3f, 1.2f);
        if (auto* scene = cocos2d::CCDirector::get()->getRunningScene())
            betfx::edgeGlow(scene, rankColor(m_to), 120, .1f, .3f, .8f);
        return true;
    }
public:
    static SorRankUpPopup* create(const std::string& from, const std::string& to) {
        auto* r = new SorRankUpPopup();
        r->m_from = from; r->m_to = to;
        if (r->init(260.f, 200.f)) { r->setup(); r->autorelease(); return r; }
        delete r;
        return nullptr;
    }
};

// End-of-season reward.
class SorSeasonPopup : public geode::Popup {
protected:
    bool setup() {
        const auto& r = sorrow::duels::state().me.reward;
        const float cx = m_size.width / 2.f, H = m_size.height;
        this->setTitle(fmt::format("{} Season Ended", r.seasonName).c_str(), "goldFont.fnt", .6f, 18.f);
        auto* tr = trophy(r.rank, 60.f);
        tr->setPosition({ cx, H - 78.f });
        m_mainLayer->addChild(tr);
        auto* rk = label(fmt::format("Final rank: {}  -  #{}  -  {} Elo", r.rank, r.place, r.elo), "bigFont.fnt", .35f, rankColor(r.rank));
        rk->setPosition({ cx, H - 122.f });
        m_mainLayer->addChild(rk);
        auto* amt = glimmerAmount(r.amount, 22.f);
        amt->setPosition({ cx, H - 150.f });
        m_mainLayer->addChild(amt);
        auto* note = label("Your Elo was pulled halfway back to 1000 for the new season", "bigFont.fnt", .24f, { 190, 190, 215 });
        note->setPosition({ cx, 30.f });
        m_mainLayer->addChild(note);
        const auto amount = r.amount;
        geode::queueInMainThread([amount] { presentGlimmerWin(amount, 0, nullptr); });
        sorrow::duels::ackSeason();
        return true;
    }
public:
    static SorSeasonPopup* create() {
        auto* r = new SorSeasonPopup();
        if (r->init(320.f, 210.f)) { r->setup(); r->autorelease(); return r; }
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

        // background: GD's regular menu look (blue gradient + corner art)
        auto* bg = cocos2d::CCSprite::create("GJ_gradientBG.png");
        bg->setScaleX(m_w / bg->getContentWidth());
        bg->setScaleY(m_h / bg->getContentHeight());
        bg->setAnchorPoint({ 0.f, 0.f });
        bg->setColor({ 0, 102, 255 });
        this->addChild(bg, -3);
        for (int side = 0; side < 2; ++side) {
            if (auto* art = cocos2d::CCSprite::createWithSpriteFrameName("GJ_sideArt_001.png")) {
                art->setAnchorPoint({ 0.f, 0.f });
                art->setFlipX(side == 1);
                art->setPosition({ side == 0 ? -1.f : m_w + 1.f - art->getContentWidth(), -1.f });
                this->addChild(art, -2);
            }
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
        k += fmt::format("|q{}{}{}|", s.queue.active, s.queue.ranked, s.queue.searching);
        for (auto* t : { &s.myTour }) {
            if (!t->valid) continue;
            k += fmt::format("{}{}{}{}", t->id, t->state, t->round, t->players.size());
            for (auto& r : t->bracket) for (auto& m : r) k += m.winner + m.state + m.score;
        }
        for (auto& t : s.tours) k += fmt::format("{}{}{}", t.id, t.state, t.players.size());
        k += s.me.rank + s.season.name;
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
        if (m_queueLabel && state().queue.active) {
            const int w = static_cast<int>(nowSec() - state().queue.since);
            m_queueLabel->setString(fmt::format("Searching {}:{:02}", w / 60, w % 60).c_str());
        }
        if (sorrow::glimmerBalance() != m_shownBalance) this->buildBalance();
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
        auto* bg = sorrow::ui::panel(110.f, 24.f, { 0, 0, 0 }, 110);
        bg->setPosition({ 105.f, m_h - 22.f });
        m_balance->addChild(bg);
        auto* amt = glimmerAmount(std::max<std::int64_t>(0, m_shownBalance), 15.f);
        amt->setPosition({ 105.f, m_h - 22.f });
        m_balance->addChild(amt);
    }

    void build() {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        m_dyn->removeAllChildren();
        m_status = nullptr;
        m_queueLabel = nullptr;
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
            { 1, s.invites.empty() ? std::string("Invites") : fmt::format("Invites {}", s.invites.size()) },
            { 2, s.open.empty() ? std::string("Races") : fmt::format("Races {}", s.open.size()) },
            { 4, s.myTour.valid ? std::string("My Cup") : s.tours.empty() ? std::string("Cups") : fmt::format("Cups {}", s.tours.size()) },
            { 3, "Live" } };
        std::vector<cocos2d::CCNode*> tabNodes;
        for (auto& [id, name] : tabs) {
            auto* spr = ButtonSprite::create(name.c_str(), "bigFont.fnt", id == m_tab ? "GJ_button_01.png" : "GJ_button_04.png", .8f);
            spr->setScale(.5f);
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
            case 4: this->buildCups(); break;
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
        const auto& s = state();
        const auto& me = s.me;
        auto* bg = sorrow::ui::panel(w, h, { 0, 0, 0 }, 120);
        bg->setAnchorPoint({ 0.f, 0.f });
        bg->setPosition({ x, y });
        m_dyn->addChild(bg);
        const float cx = x + w / 2.f;
        float yy = y + h - 11.f;
        // season header
        if (!s.season.name.empty()) {
            const int left = std::max(0, static_cast<int>((s.season.endsAt - nowSec()) / 86400.0));
            auto* sh = label(fmt::format("SEASON {}", s.season.name), "bigFont.fnt", .22f, { 160, 200, 255 });
            sh->limitLabelWidth(w - 10.f, .22f, .12f);
            sh->setPosition({ cx, yy });
            m_dyn->addChild(sh);
            yy -= 9.f;
            auto* sl = label(left > 0 ? fmt::format("ends in {}d", left) : std::string("ends today"), "bigFont.fnt", .2f, { 150, 150, 180 });
            sl->setPosition({ cx, yy });
            m_dyn->addChild(sl);
        }
        yy -= 30.f;
        auto* tr = trophy(me.rank, 40.f);
        tr->setPosition({ cx, yy });
        m_dyn->addChild(tr);
        yy -= 30.f;
        auto* rk = label(me.rank, "goldFont.fnt", .62f, rankColor(me.rank));
        rk->setPosition({ cx, yy });
        m_dyn->addChild(rk);
        yy -= 15.f;
        auto* elo = label(fmt::format("{} Elo{}", me.elo, me.position > 0 ? fmt::format("  #{}", me.position) : ""), "bigFont.fnt", .34f);
        elo->setPosition({ cx, yy });
        m_dyn->addChild(elo);
        yy -= 11.f;
        auto* peak = label(fmt::format("Peak {} {}", me.peakRank, me.peakElo), "bigFont.fnt", .22f, rankColor(me.peakRank));
        peak->setPosition({ cx, yy });
        m_dyn->addChild(peak);
        yy -= 20.f;
        const std::tuple<const char*, int, cocos2d::ccColor3B> cols[] = {
            { "WINS", me.w, { 90, 230, 120 } }, { "LOSSES", me.l, { 255, 100, 100 } }, { "CUPS", me.tourWins, { 255, 205, 60 } } };
        for (int i = 0; i < 3; ++i) {
            const float xx = x + w * (i + .5f) / 3.f;
            auto* n = label(std::to_string(std::get<1>(cols[i])), "goldFont.fnt", .5f, std::get<2>(cols[i]));
            n->setPosition({ xx, yy });
            m_dyn->addChild(n);
            auto* t = label(std::get<0>(cols[i]), "bigFont.fnt", .17f, { 170, 170, 190 });
            t->setPosition({ xx, yy - 10.f });
            m_dyn->addChild(t);
        }
        yy -= 25.f;
        const std::string streak = me.streak > 0 ? fmt::format("{} win streak", me.streak) : me.streak < 0 ? fmt::format("{} losses in a row", -me.streak) : "No streak";
        auto* st = label(streak, "bigFont.fnt", .24f, me.streak >= 3 ? cocos2d::ccColor3B { 255, 170, 60 } : cocos2d::ccColor3B { 220, 220, 220 });
        st->setPosition({ cx, yy });
        m_dyn->addChild(st);
        yy -= 11.f;
        auto* earned = label(fmt::format("Earnings {}{}", me.earned < 0 ? "-" : "+", sorrow::formatGlimmer(std::llabs(me.earned))), "bigFont.fnt", .22f,
            me.earned >= 0 ? cocos2d::ccColor3B { 255, 220, 110 } : cocos2d::ccColor3B { 255, 130, 110 });
        earned->limitLabelWidth(w - 12.f, .22f, .12f);
        earned->setPosition({ cx, yy });
        m_dyn->addChild(earned);

        auto* board = button("Leaderboard", "GJ_button_02.png", .45f, [] { if (auto* p = SorDuelBoardPopup::create()) p->show(); });
        auto* hist = button("History", "GJ_button_04.png", .45f, [] { if (auto* p = SorDuelHistoryPopup::create()) p->show(); });
        board->setPosition({ cx, y + 36.f });
        hist->setPosition({ cx, y + 14.f });
        m_menu->addChild(board);
        m_menu->addChild(hist);
    }

    // --------------------------------------------------------------- play --
    // Quick Match banner on top, three mode cards, Nearby on Globed below.
    cocos2d::CCNode* card(float w, float h, cocos2d::ccColor3B col, const char* icon, float iconSize, const std::string& title, const std::string& desc) {
        using namespace sorrow::duelui;
        auto* node = cocos2d::CCNode::create();
        node->setContentSize({ w, h });
        auto* bg = sorrow::ui::panel(w, h, col, 120);
        bg->setPosition({ w / 2.f, h / 2.f });
        node->addChild(bg);
        auto* shine = sorrow::ui::panel(w - 6.f, h * .45f, { 255, 255, 255 }, 18);
        shine->setPosition({ w / 2.f, h * .72f });
        node->addChild(shine);
        if (icon)
            if (auto* ic = cocos2d::CCSprite::createWithSpriteFrameName(icon)) {
                ic->setScale(iconSize / std::max(1.f, std::max(ic->getContentWidth(), ic->getContentHeight())));
                ic->setPosition({ w / 2.f, h - 6.f - iconSize / 2.f });
                node->addChild(ic);
            }
        auto* t = label(title, "goldFont.fnt", .5f);
        t->limitLabelWidth(w - 10.f, .5f, .25f);
        t->setPosition({ w / 2.f, 21.f });
        node->addChild(t);
        auto* d = label(desc, "bigFont.fnt", .19f, { 225, 225, 240 });
        d->limitLabelWidth(w - 8.f, .19f, .1f);
        d->setPosition({ w / 2.f, 8.f });
        node->addChild(d);
        return node;
    }

    void buildPlay() {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        const auto& s = state();
        const float x0 = m_px + 10.f, iw = m_pw - 20.f;
        float y = m_py + m_ph - 10.f;

        // ---- Quick Match
        const float qh = 52.f;
        y -= qh / 2.f;
        auto* qbg = sorrow::ui::panel(iw, qh, { 255, 170, 40 }, 95);
        qbg->setPosition({ x0 + iw / 2.f, y });
        m_dyn->addChild(qbg);
        auto* qt = label("QUICK MATCH", "goldFont.fnt", .6f, { 255, 255, 255 }, { 0.f, .5f });
        qt->setPosition({ x0 + 12.f, y + 9.f });
        m_dyn->addChild(qt);
        if (s.queue.active) {
            if (auto* spin = cocos2d::CCSprite::create("loadingCircle.png")) {
                spin->setScale(.28f);
                spin->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
                spin->setPosition({ x0 + iw - 105.f, y });
                spin->runAction(cocos2d::CCRepeatForever::create(cocos2d::CCRotateBy::create(1.f, 360.f)));
                m_dyn->addChild(spin);
            }
            m_queueLabel = label("Searching 0:00", "bigFont.fnt", .3f, { 255, 255, 255 }, { 0.f, .5f });
            m_queueLabel->setPosition({ x0 + 12.f, y - 9.f });
            m_dyn->addChild(m_queueLabel);
            auto* info = label(fmt::format("{}{}  -  {} searching", s.queue.ranked ? "Ranked" : "Casual",
                s.queue.ranked ? fmt::format(" +-{} Elo", s.queue.window) : std::string(""), s.queue.searching), "bigFont.fnt", .22f, { 255, 240, 210 }, { 0.f, .5f });
            info->setPosition({ x0 + 112.f, y - 9.f });
            m_dyn->addChild(info);
            auto* cancel = button("Cancel", "GJ_button_06.png", .5f, [] { sorrow::duels::unqueue(); });
            cancel->setPosition({ x0 + iw - 42.f, y });
            m_menu->addChild(cancel);
        } else {
            auto* qd = label("Get paired with someone near your Elo", "bigFont.fnt", .22f, { 255, 240, 210 }, { 0.f, .5f });
            qd->setPosition({ x0 + 12.f, y - 9.f });
            m_dyn->addChild(qd);
            auto* ranked = button("Ranked", "GJ_button_01.png", .55f, [] {
                sorrow::duels::queue(true, 0, [](bool ok, std::string err) { if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show(); });
            });
            auto* casual = button("Casual", "GJ_button_02.png", .55f, [] {
                sorrow::duels::queue(false, 0, [](bool ok, std::string err) { if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show(); });
            });
            m_menu->addChild(ranked);
            m_menu->addChild(casual);
            sorrow::ui::row({ ranked, casual }, x0 + iw - 85.f, y, 6.f);
        }
        y -= qh / 2.f + 8.f;

        // ---- mode cards
        const float ch = 70.f, cw = (iw - 16.f) / 3.f;
        y -= ch / 2.f;
        Ref<GJGameLevel> lv = m_level;
        struct C { cocos2d::ccColor3B col; const char* icon; const char* title; const char* desc; std::function<void()> cb; };
        const C cards[] = {
            { { 60, 130, 255 }, "GJ_versusBtn_001.png", "Challenge", "1v1 any player", [lv] {
                if (auto* p = SorDuelPickPopup::create([lv](std::string uid, std::string name) {
                        if (auto* c = SorDuelCreatePopup::create("duel", uid, name, lv)) c->show(); })) p->show(); } },
            { { 255, 120, 60 }, "GJ_timeIcon_001.png", "Open Race", "Up to 8 players", [lv] {
                if (auto* c = SorDuelCreatePopup::create("race", "", "", lv)) c->show(); } },
            { { 200, 80, 255 }, "rankIcon_top10_001.png", "Tournament", "Bracket for 4 or 8", [] {
                if (auto* c = SorDuelCreatePopup::create("tour", "", "", nullptr)) c->show(); } },
        };
        for (int i = 0; i < 3; ++i) {
            const auto& c = cards[i];
            auto* node = this->card(cw, ch, c.col, c.icon, 30.f, c.title, c.desc);
            auto cb = c.cb;
            auto* item = CCMenuItemExt::createSpriteExtra(node, [cb](CCMenuItemSpriteExtra*) { cb(); });
            item->setPosition({ x0 + cw / 2.f + i * (cw + 8.f), y });
            m_menu->addChild(item);
        }
        y -= ch / 2.f + 8.f;
        if (m_level) {
            auto* l = label(fmt::format("Duels from here use: {}", std::string(m_level->m_levelName)), "bigFont.fnt", .22f, { 255, 230, 150 });
            l->setPosition({ x0 + iw / 2.f, y + 2.f });
            m_dyn->addChild(l);
        }

        // ---- nearby on Globed
        const float ny = m_py + 26.f;
        auto* nh = label("NEARBY ON GLOBED", "bigFont.fnt", .24f, { 160, 220, 255 }, { 0.f, .5f });
        nh->setPosition({ x0, ny + 24.f });
        m_dyn->addChild(nh);
        if (m_nearby.empty()) {
            auto* hint = label(sorrow::sessionMemberIds().empty() ? "Play a level on Globed to see players near you here"
                                                                  : "Nobody near you has linked Sorrow yet",
                "bigFont.fnt", .24f, { 190, 200, 225 });
            hint->setPosition({ x0 + iw / 2.f, ny });
            m_dyn->addChild(hint);
        } else {
            const float bw = (iw - 12.f) / 3.f;
            for (size_t i = 0; i < std::min<size_t>(m_nearby.size(), 3); ++i) {
                const auto& f = m_nearby[i];
                const float x = x0 + bw / 2.f + i * (bw + 6.f);
                auto* bg = sorrow::ui::panel(bw, 34.f, { 0, 0, 0 }, 100);
                bg->setPosition({ x, ny });
                m_dyn->addChild(bg);
                auto* nm = label(f.name, "bigFont.fnt", .28f, { 255, 255, 255 }, { 0.f, .5f });
                nm->limitLabelWidth(bw - 50.f, .28f, .12f);
                nm->setPosition({ x - bw / 2.f + 6.f, ny + 6.f });
                m_dyn->addChild(nm);
                auto* rk = label(fmt::format("{} {}", f.rank, f.elo), "goldFont.fnt", .3f, rankColor(f.rank), { 0.f, .5f });
                rk->setPosition({ x - bw / 2.f + 6.f, ny - 7.f });
                m_dyn->addChild(rk);
                if (!f.busy) {
                    auto uid = f.uid, name = f.name;
                    Ref<GJGameLevel> lv2 = m_level;
                    auto* b = button("VS", "GJ_button_01.png", .42f, [uid, name, lv2] {
                        if (auto* c = SorDuelCreatePopup::create("duel", uid, name, lv2)) c->show(); });
                    b->setPosition({ x + bw / 2.f - 18.f, ny });
                    m_menu->addChild(b);
                }
            }
        }
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

    // --------------------------------------------------------------- cups --
    void buildCups() {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        const auto& s = state();
        if (s.myTour.valid) { this->buildBracket(s.myTour); return; }
        const float x0 = m_px + 10.f, iw = m_pw - 20.f;
        auto* host = button("Host a Tournament", "GJ_button_01.png", .55f, [] {
            if (auto* c = SorDuelCreatePopup::create("tour", "", "", nullptr)) c->show(); });
        host->setPosition({ m_px + m_pw / 2.f, m_py + 16.f });
        m_menu->addChild(host);
        if (s.tours.empty()) {
            auto* l = label("No tournaments open - host one!", "bigFont.fnt", .32f, { 200, 200, 225 });
            l->setPosition({ m_px + m_pw / 2.f, m_py + m_ph / 2.f + 10.f });
            m_dyn->addChild(l);
            return;
        }
        for (size_t i = 0; i < std::min<size_t>(s.tours.size(), 4); ++i) {
            const auto& t = s.tours[i];
            const float y = m_py + m_ph - 26.f - i * 44.f;
            auto* bg = sorrow::ui::panel(iw, 40.f, { 0, 0, 0 }, 100);
            bg->setPosition({ x0 + iw / 2.f, y });
            m_dyn->addChild(bg);
            auto* tr = trophy("Gold", 26.f);
            tr->setPosition({ x0 + 18.f, y });
            m_dyn->addChild(tr);
            auto* nm = label(t.name, "goldFont.fnt", .45f, { 255, 255, 255 }, { 0.f, .5f });
            nm->limitLabelWidth(iw * .45f, .45f, .2f);
            nm->setPosition({ x0 + 36.f, y + 7.f });
            m_dyn->addChild(nm);
            auto* sub = label(fmt::format("{}  -  {}/{} players  -  {}{}", t.modeName, t.players.size(), t.size,
                diffName(t.difficulty), t.state == "running" ? "  -  LIVE" : ""), "bigFont.fnt", .22f, { 190, 190, 215 }, { 0.f, .5f });
            sub->limitLabelWidth(iw * .55f, .22f, .1f);
            sub->setPosition({ x0 + 36.f, y - 8.f });
            m_dyn->addChild(sub);
            if (t.entry > 0) {
                auto* amt = glimmerAmount(t.entry, 12.f);
                amt->setPosition({ x0 + iw * .72f, y });
                m_dyn->addChild(amt);
            }
            if (t.state == "open") {
                const auto id = t.id;
                auto* j = button("Join", "GJ_button_01.png", .5f, [id] {
                    sorrow::duels::tourAct("tours/join", id, [](bool ok, std::string err) {
                        if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show(); else sorrow::refreshBalance(); });
                });
                j->setPosition({ x0 + iw - 30.f, y });
                m_menu->addChild(j);
            }
        }
    }

    // Bracket: one column per round, a box per match; winners in gold.
    void buildBracket(const sorrow::duels::Tour& t) {
        using namespace sorrow::duelui;
        using namespace sorrow::duels;
        const auto& me = state().me;
        const float x0 = m_px + 8.f, iw = m_pw - 16.f;
        const float top = m_py + m_ph - 8.f;
        auto* nm = label(t.name, "goldFont.fnt", .55f, { 255, 255, 255 }, { 0.f, .5f });
        nm->limitLabelWidth(iw * .5f, .55f, .25f);
        nm->setPosition({ x0 + 4.f, top - 8.f });
        m_dyn->addChild(nm);
        const std::string st = t.state == "open" ? fmt::format("Waiting - {}/{} players", t.players.size(), t.size)
            : t.state == "running" ? t.roundName : t.state == "finished" ? fmt::format("{} won!", t.winnerName) : t.state;
        auto* sl = label(st, "bigFont.fnt", .26f, t.state == "finished" ? cocos2d::ccColor3B { 255, 215, 80 } : cocos2d::ccColor3B { 200, 220, 255 }, { 0.f, .5f });
        sl->setPosition({ x0 + 4.f, top - 22.f });
        m_dyn->addChild(sl);
        if (t.pot > 0) {
            auto* pot = glimmerAmount(t.pot, 13.f);
            pot->setPosition({ x0 + iw - 50.f, top - 10.f });
            m_dyn->addChild(pot);
            auto* pl = label(fmt::format("1st {}  2nd {}", sorrow::formatGlimmer(t.pot * 7 / 10), sorrow::formatGlimmer(t.pot - t.pot * 7 / 10)),
                "bigFont.fnt", .18f, { 255, 225, 140 });
            pl->setPosition({ x0 + iw - 50.f, top - 24.f });
            m_dyn->addChild(pl);
        }
        const float areaTop = top - 34.f, areaBottom = m_py + 30.f;
        if (t.state == "open" || t.bracket.empty()) {
            // player list in two columns
            for (size_t i = 0; i < t.players.size(); ++i) {
                const auto& p = t.players[i];
                const float x = x0 + (i / 4) * (iw / 2.f);
                const float y = areaTop - 12.f - (i % 4) * 22.f;
                auto* bg = sorrow::ui::panel(iw / 2.f - 6.f, 19.f, p.uid == me.uid ? cocos2d::ccColor3B { 50, 120, 60 } : cocos2d::ccColor3B { 0, 0, 0 }, 100);
                bg->setAnchorPoint({ 0.f, .5f });
                bg->setPosition({ x, y });
                m_dyn->addChild(bg);
                if (auto* ic = cube(p, .4f)) { ic->setPosition({ x + 11.f, y }); m_dyn->addChild(ic); }
                auto* n = label(p.name, "bigFont.fnt", .27f, { 255, 255, 255 }, { 0.f, .5f });
                n->limitLabelWidth(iw / 2.f - 80.f, .27f, .12f);
                n->setPosition({ x + 22.f, y });
                m_dyn->addChild(n);
                auto* r = label(fmt::format("{} {}", p.rank, p.elo), "goldFont.fnt", .3f, rankColor(p.rank), { 1.f, .5f });
                r->setPosition({ x + iw / 2.f - 12.f, y });
                m_dyn->addChild(r);
            }
        } else {
            const int cols = static_cast<int>(std::round(std::log2(t.bracket[0].size() * 2)));
            const float colW = iw / std::max(1, cols);
            for (int c = 0; c < cols; ++c) {
                const int matches = static_cast<int>(t.bracket[0].size()) >> c;
                const float slotH = (areaTop - areaBottom) / std::max(1, matches);
                auto* rn = label(c == cols - 1 ? "FINAL" : c == cols - 2 ? "SEMIS" : c == cols - 3 ? "QUARTERS" : fmt::format("R{}", c + 1),
                    "bigFont.fnt", .2f, { 170, 190, 230 });
                rn->setPosition({ x0 + colW * (c + .5f), areaTop + 2.f });
                m_dyn->addChild(rn);
                for (int i = 0; i < matches; ++i) {
                    const float cx = x0 + colW * (c + .5f), cy = areaTop - slotH * (i + .5f) - 4.f;
                    const Match* m = c < static_cast<int>(t.bracket.size()) && i < static_cast<int>(t.bracket[c].size()) ? &t.bracket[c][i] : nullptr;
                    const float bw = colW - 10.f, bh = 28.f;
                    const bool mine = m && (m->a == me.uid || m->b == me.uid);
                    const bool live = m && !m->winner.empty() ? false : m && !m->duelId.empty();
                    auto* box = sorrow::ui::panel(bw, bh, mine ? cocos2d::ccColor3B { 40, 110, 60 } : live ? cocos2d::ccColor3B { 120, 60, 20 } : cocos2d::ccColor3B { 0, 0, 0 }, 120);
                    box->setPosition({ cx, cy });
                    m_dyn->addChild(box);
                    for (int k = 0; k < 2; ++k) {
                        const std::string uid = m ? (k == 0 ? m->a : m->b) : "";
                        const std::string nmT = m ? (k == 0 ? m->aName : m->bName) : "";
                        const bool won = m && !m->winner.empty() && m->winner == uid;
                        const bool lost = m && !m->winner.empty() && !uid.empty() && m->winner != uid;
                        auto* l = label(uid.empty() ? (m && (m->a.empty() || m->b.empty()) && !m->winner.empty() ? "bye" : "-") : nmT, "bigFont.fnt", .22f,
                            won ? cocos2d::ccColor3B { 255, 215, 80 } : lost ? cocos2d::ccColor3B { 130, 130, 140 } : cocos2d::ccColor3B { 255, 255, 255 }, { 0.f, .5f });
                        l->limitLabelWidth(bw - 22.f, .22f, .1f);
                        l->setPosition({ cx - bw / 2.f + 5.f, cy + (k == 0 ? 6.f : -6.f) });
                        m_dyn->addChild(l);
                    }
                    if (m && !m->score.empty()) {
                        auto* sc = label(m->score, "bigFont.fnt", .2f, { 200, 200, 220 }, { 1.f, .5f });
                        sc->setPosition({ cx + bw / 2.f - 4.f, cy });
                        m_dyn->addChild(sc);
                    } else if (live) {
                        auto* lv = label("LIVE", "bigFont.fnt", .18f, { 255, 140, 60 }, { 1.f, .5f });
                        lv->setPosition({ cx + bw / 2.f - 4.f, cy });
                        m_dyn->addChild(lv);
                    }
                }
            }
        }
        // actions
        std::vector<cocos2d::CCNode*> btns;
        const auto id = t.id;
        if (t.state == "open") {
            if (t.hostId == me.uid)
                btns.push_back(button(fmt::format("Start ({})", t.players.size()), "GJ_button_01.png", .5f, [id] {
                    sorrow::duels::tourAct("tours/start", id, [](bool ok, std::string err) { if (!ok) Notification::create(err.c_str(), NotificationIcon::Error)->show(); }); }));
            btns.push_back(button(t.hostId == me.uid ? "Cancel Cup" : "Leave", "GJ_button_06.png", .5f, [id] {
                sorrow::duels::tourAct("tours/leave", id, [](bool, std::string) { sorrow::refreshBalance(); }); }));
        } else if (t.state == "finished" && t.myPlace > 0) {
            auto* res = label(t.myPlace == 1 ? fmt::format("You won! +{}", sorrow::formatGlimmer(t.myPrize)) : fmt::format("2nd place +{}", sorrow::formatGlimmer(t.myPrize)),
                "goldFont.fnt", .5f, { 255, 220, 100 });
            res->setPosition({ m_px + m_pw / 2.f, m_py + 16.f });
            m_dyn->addChild(res);
        }
        for (auto* b : btns) m_menu->addChild(b);
        if (!btns.empty()) sorrow::ui::row(btns, m_px + m_pw / 2.f, m_py + 16.f, 8.f);
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
        if (!d.tourId.empty()) head = "CUP - " + head;
        else if (d.queued) head = (d.ranked ? "RANKED MATCH - " : "CASUAL MATCH - ") + head;
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
                    const std::string pcText = d.mode == "sudden" && !p.done && d.racing() ? std::string("alive")
                        : d.mode == "consistency" ? fmt::format("avg {}%  ({} runs)", p.pct, p.attempts) : fmt::format("{}%", p.pct);
                    auto* pc = label(pcText, "bigFont.fnt", .3f, { 255, 255, 255 });
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
            std::string evText = d.events.empty() ? std::string() : d.events.back().text;
            if (!d.rounds.empty() && d.rounds.back().timeMs > 0 && d.state == "intermission")
                evText = fmt::format("{} won round {} in {}", d.rounds.back().winnerName, d.rounds.back().n, sorrow::duels::fmtTime(d.rounds.back().timeMs));
            if (!evText.empty()) {
                auto* ev = label(evText, "bigFont.fnt", .22f, { 170, 170, 200 });
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
    CCLabelBMFont* m_queueLabel = nullptr;
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
    rankUpHook() = [](std::string from, std::string to) {
        if (auto* p = sorrow::duelui::SorRankUpPopup::create(from, to)) p->show();
    };
    seasonHook() = [] { if (auto* p = sorrow::duelui::SorSeasonPopup::create()) p->show(); };
}
} // namespace sorrow::duels
