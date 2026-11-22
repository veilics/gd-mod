// Sorrow Integration - Rooms UI (v5): Globed-style rooms with cube icons,
// free-amount betting, skip votes, auto-enter on round start.
// Layout constants keep every element on a fixed grid - nothing overlaps.
#pragma once

#include "SorUi.hpp"
#include "SorConst.hpp"
#include "Format.hpp"
#include "Bridge.hpp"
#include "SorRooms.hpp"
#include "Glimmer.hpp"
#include "AwardFx.hpp"
#include "RouletteManager.hpp"

#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

namespace sorrow {

// A small static cube preview, Globed player-icon style: the member's actual
// icon id + colours rendered with SimplePlayer (no physics, no game layer).
inline SimplePlayer* makeCubeIcon(int iconId, int c1, int c2, int glow) {
    auto sp = SimplePlayer::create(std::max(1, iconId));
    auto gm = GameManager::get();
    if (!sp || !gm) return nullptr;
    sp->setColor(gm->colorForIdx(std::max(0, c1)));
    sp->setSecondColor(gm->colorForIdx(std::max(0, c2)));
    if (glow > 0) sp->setGlowOutline(gm->colorForIdx(glow));
    return sp;
}

} // namespace sorrow

// ---------------------------------------------------------------------------
// Bet popup: type ANY amount, or tap a quick chip. Shows current balance.
// ---------------------------------------------------------------------------
class SorBetPopup : public geode::Popup {
protected:
    std::int64_t m_balance = 0;
    std::function<void(std::int64_t)> m_onBet;
    TextInput* m_input = nullptr;

    // 300 x 190. Rows: title 170 | balance pill 144 | input 112 | chips 78 | Bet 28.
    bool setup() {
        const float cx = m_size.width / 2.f;
        this->setTitle("Bet Glimmer", "goldFont.fnt", .65f, 20.f);

        auto balBg = sorrow::ui::panel(190.f, 24.f, { 0, 0, 0 }, 90);
        balBg->setPosition({ cx, m_size.height - 46.f });
        m_mainLayer->addChild(balBg);
        auto balance = CCLabelBMFont::create(
            fmt::format("You have {}", sorrow::formatGlimmer(m_balance)).c_str(), "goldFont.fnt");
        balance->limitLabelWidth(170.f, .5f, .2f);
        balance->setPosition({ cx, m_size.height - 45.f });
        m_mainLayer->addChild(balance);

        m_input = TextInput::create(220.f, "Amount...", "bigFont.fnt");
        m_input->setCommonFilter(CommonFilter::Uint);
        m_input->setMaxCharCount(15);
        m_input->setPosition({ cx, m_size.height - 78.f });
        m_mainLayer->addChild(m_input);

        // Quick-fill chips: same width, evenly spaced, centred.
        auto chipsMenu = CCMenu::create();
        chipsMenu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(chipsMenu);
        const int chips[] = { 10000, 50000, 1000000, -1 }; // -1 = MAX
        const char* names[] = { "10k", "50k", "1M", "MAX" };
        for (int i = 0; i < 4; i++) {
            auto spr = ButtonSprite::create(names[i], 44, true, "bigFont.fnt", "GJ_button_04.png", 26.f, .5f);
            spr->setScale(.85f);
            auto chip = CCMenuItemSpriteExtra::create(spr, this,
                menu_selector(SorBetPopup::onChip));
            chip->setTag(chips[i]);
            chip->setPosition({ cx + (i - 1.5f) * 62.f, m_size.height - 112.f });
            chipsMenu->addChild(chip);
        }

        auto confirmLab = ButtonSprite::create("Bet", "bigFont.fnt", "GJ_button_01.png", .8f);
        confirmLab->setScale(.85f);
        auto confirm = CCMenuItemSpriteExtra::create(confirmLab, this,
            menu_selector(SorBetPopup::onConfirm));
        confirm->setPosition({ cx, 28.f });
        auto confirmMenu = CCMenu::create();
        confirmMenu->setPosition({ 0.f, 0.f });
        confirmMenu->addChild(confirm);
        m_mainLayer->addChild(confirmMenu);
        return true;
    }

    void onChip(CCObject* sender) {
        auto tag = static_cast<CCMenuItemSpriteExtra*>(sender)->getTag();
        std::int64_t amount = tag < 0 ? m_balance : static_cast<std::int64_t>(tag);
        m_input->setString(amount > 0 ? std::to_string(amount).c_str() : "");
    }

    void onConfirm(CCObject*) {
        auto text = std::string(m_input->getString());
        std::int64_t amount = 0;
        try { amount = std::stoll(text); } catch (...) { amount = 0; }
        if (amount <= 0) {
            Notification::create("Enter an amount", NotificationIcon::Warning)->show();
            return;
        }
        if (amount > m_balance) {
            Notification::create("Not enough glimmer", NotificationIcon::Warning)->show();
            return;
        }
        if (m_onBet) m_onBet(amount);
        this->onClose(nullptr);
    }

public:
    static SorBetPopup* create(std::int64_t balance, std::int64_t current,
                               std::function<void(std::int64_t)> cb) {
        auto ret = new SorBetPopup();
        ret->m_balance = balance;
        ret->m_onBet = std::move(cb);
        if (ret->init(300.f, 190.f)) {
            ret->setup(); // plain Popup: no automatic setup hook
            ret->m_input->setString(current > 0 ? std::to_string(current).c_str() : "");
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Room history: every round of the current/last game + the room's past games.
// ---------------------------------------------------------------------------
class SorRoomHistoryPopup : public geode::Popup {
protected:
    bool setup() {
        const float W = m_size.width;
        this->setTitle(fmt::format("Room {} - History", sorrow::rooms::state().code), "goldFont.fnt", .6f, 18.f);
        auto* list = geode::ScrollLayer::create({ 14.f, 14.f, W - 28.f, m_size.height - 50.f });
        m_mainLayer->addChild(list);
        const auto& st = sorrow::rooms::state();
        auto* content = list->m_contentLayer;
        const float listW = list->getContentSize().width, rowH = 22.f;

        std::vector<std::pair<std::string, cocos2d::ccColor3B>> rows;
        rows.push_back({ "This game", { 255, 210, 80 } });
        if (st.history.empty()) rows.push_back({ "No rounds played yet", { 170, 170, 170 } });
        for (auto it = st.history.rbegin(); it != st.history.rend(); ++it) {
            const auto& h = *it;
            if (h.kind == "elim")
                rows.push_back({ fmt::format("R{}  {}  ({}%)  -  out: {}", h.round, h.levelName, h.target,
                    h.eliminated.empty() ? std::string("nobody") : h.eliminated), { 255, 150, 150 } });
            else
                rows.push_back({ fmt::format("R{}  {}  -  {} at {}% (needed {}%){}", h.round, h.levelName, h.winnerName, h.percent, h.target,
                    h.team >= 0 && h.team < static_cast<int>(st.teams.size()) ? fmt::format("  [{}]", st.teams[h.team].name) : std::string()),
                    { 255, 255, 255 } });
        }
        rows.push_back({ "", { 0, 0, 0 } });
        rows.push_back({ "Past games", { 255, 210, 80 } });
        if (st.games.empty()) rows.push_back({ "No finished games yet", { 170, 170, 170 } });
        for (auto it = st.games.rbegin(); it != st.games.rend(); ++it)
            rows.push_back({ fmt::format("{}  -  {} rounds  -  {}  -  pot {}", it->mode, it->rounds,
                it->winnerName.empty() ? std::string("no winner") : it->winnerName, sorrow::formatGlimmer(it->pot)), { 210, 230, 255 } });

        const float total = std::max(list->getContentSize().height, rowH * rows.size());
        content->setContentSize({ listW, total });
        float y = total;
        for (const auto& [text, color] : rows) {
            y -= rowH;
            if (text.empty()) continue;
            auto* l = CCLabelBMFont::create(text.c_str(), color == cocos2d::ccColor3B { 255, 210, 80 } ? "goldFont.fnt" : "chatFont.fnt");
            l->setAnchorPoint({ 0.f, .5f });
            l->setColor(color);
            l->limitLabelWidth(listW - 16.f, color == cocos2d::ccColor3B { 255, 210, 80 } ? .55f : .65f, .2f);
            l->setPosition({ 8.f, y + rowH / 2.f });
            content->addChild(l);
        }
        list->scrollToTop();
        return true;
    }
public:
    static SorRoomHistoryPopup* create() {
        auto* ret = new SorRoomHistoryPopup();
        if (ret->init(400.f, 260.f)) { ret->setup(); ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Multiplayer roulette popup (440 x 290). Fixed grid, y from the bottom:
//   270 title                                   balance (top-right)
//   246 tabs  [Browse] [Create] [Lobby]
//   222 status line
//   Browse: list 44..204, [Refresh] at 22
//   Create: rows 196/168/140/112/84 (label | < value >), toggles 52, [Create] 22
//   Lobby : header 202 (code, Leave, pot) | info 182 | members 52..170 |
//           buttons 28 [Bet] [Ready/Start/Go to level] [Skip]
// ---------------------------------------------------------------------------
class SorrowRoomLayer : public geode::Popup {
protected:
    static constexpr float W = 440.f, H = 290.f, CX = 220.f;
    enum class View { Browse, Create, Lobby };

    View m_view = View::Browse;
    std::vector<ButtonSprite*> m_tabSpr;
    std::vector<CCMenuItemSpriteExtra*> m_tabBtn;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_balance = nullptr;
    cocos2d::CCSprite* m_balIcon = nullptr;
    float m_statusTimer = 0.f;

    // view roots (everything of a view hangs off one node -> show/hide is one call)
    cocos2d::CCNode* m_browseRoot = nullptr;
    cocos2d::CCNode* m_createRoot = nullptr;
    cocos2d::CCNode* m_lobbyRoot = nullptr;

    // browse
    geode::ScrollLayer* m_list = nullptr;
    std::vector<sorrow::rooms::BrowseItem> m_rooms;

    // create
    struct Stepper { CCLabelBMFont* value; int idx; };
    std::vector<Stepper> m_steps;
    bool m_prog = true, m_dem = false;
    CCMenuItemSpriteExtra* m_createBtn = nullptr;

    // lobby
    CCLabelBMFont* m_roomLabel = nullptr;
    CCLabelBMFont* m_potLabel = nullptr;
    CCLabelBMFont* m_info = nullptr;
    geode::ScrollLayer* m_members = nullptr;
    CCMenuItemSpriteExtra* m_betBtn = nullptr;
    CCMenuItemSpriteExtra* m_primaryBtn = nullptr;
    ButtonSprite* m_primarySpr = nullptr;
    CCMenuItemSpriteExtra* m_skipBtn = nullptr;
    ButtonSprite* m_skipSpr = nullptr;
    std::string m_membersKey;
    CCLabelBMFont* m_roundsRowLabel = nullptr;
    CCMenuItemSpriteExtra* m_teamBtn = nullptr;
    CCMenuItemSpriteExtra* m_leaveBtn = nullptr;
    CCMenuItemSpriteExtra* m_histBtn = nullptr;

    static cocos2d::ccColor3B teamColor(int t) {
        static const cocos2d::ccColor3B c[] = { { 255, 80, 80 }, { 80, 150, 255 }, { 90, 220, 110 }, { 255, 210, 70 } };
        return c[std::clamp(t, 0, 3)];
    }

    // ---- option tables (create view) ----
    struct Opt { const char* label; const char* value; int num; };
    static const std::vector<Opt>& sources() {
        static const std::vector<Opt> v = { { "Regular", "regular", 0 }, { "Trending", "trending", 0 }, { "Demonlist", "demonlist", 0 } };
        return v;
    }
    static const std::vector<Opt>& diffs() {
        static const std::vector<Opt> v = {
            { "Any", "any", 0 }, { "Easy", "easy", 0 }, { "Normal", "normal", 0 }, { "Hard", "hard", 0 },
            { "Harder", "harder", 0 }, { "Insane", "insane", 0 }, { "Easy Demon", "easy_demon", 0 },
            { "Medium Demon", "medium_demon", 0 }, { "Hard Demon", "hard_demon", 0 },
            { "Insane Demon", "insane_demon", 0 }, { "Extreme Demon", "extreme_demon", 0 } };
        return v;
    }
    static const std::vector<Opt>& roundsOpt() {
        static const std::vector<Opt> v = { { "1", "", 1 }, { "2", "", 2 }, { "3", "", 3 }, { "4", "", 4 }, { "5", "", 5 },
            { "6", "", 6 }, { "7", "", 7 }, { "8", "", 8 }, { "9", "", 9 }, { "10", "", 10 } };
        return v;
    }
    static const std::vector<Opt>& targets() {
        static const std::vector<Opt> v = { { "Auto", "", 0 }, { "1%", "", 1 }, { "5%", "", 5 }, { "10%", "", 10 },
            { "15%", "", 15 }, { "20%", "", 20 }, { "25%", "", 25 }, { "30%", "", 30 }, { "40%", "", 40 },
            { "50%", "", 50 }, { "60%", "", 60 }, { "75%", "", 75 }, { "100%", "", 100 } };
        return v;
    }
    static const std::vector<Opt>& maxes() {
        static const std::vector<Opt> v = { { "2", "", 2 }, { "3", "", 3 }, { "4", "", 4 }, { "6", "", 6 },
            { "8", "", 8 }, { "10", "", 10 }, { "12", "", 12 }, { "16", "", 16 } };
        return v;
    }
    static const std::vector<Opt>& modes() {
        static const std::vector<Opt> v = { { "Classic", "classic", 0 }, { "Teams (2)", "teams", 2 }, { "Teams (3)", "teams", 3 },
            { "Teams (4)", "teams", 4 }, { "Elimination", "elimination", 0 } };
        return v;
    }
    static const std::vector<Opt>& times() {
        static const std::vector<Opt> v = { { "1 min", "", 60 }, { "2 min", "", 120 }, { "3 min", "", 180 },
            { "5 min", "", 300 }, { "10 min", "", 600 } };
        return v;
    }
    bool elimSelected() const { return m_steps.size() > 2 && std::string(modes()[m_steps[2].idx].value) == "elimination"; }
    // rows: 0 levels, 1 difficulty, 2 mode, 3 rounds | round time, 4 start target, 5 max players
    const std::vector<Opt>& table(int row) const {
        switch (row) {
            case 0: return sources(); case 1: return diffs(); case 2: return modes();
            case 3: return elimSelected() ? times() : roundsOpt();
            case 4: return targets(); default: return maxes();
        }
    }

    static ButtonSprite* btn(const char* text, const char* tex, int width, float scale = .6f) {
        return ButtonSprite::create(text, width, true, "bigFont.fnt", tex, 28.f, scale);
    }
    CCMenu* menuIn(cocos2d::CCNode* parent) {
        auto* m = CCMenu::create();
        m->setPosition({ 0.f, 0.f });
        parent->addChild(m, 5);
        return m;
    }
    CCLabelBMFont* label(cocos2d::CCNode* parent, const char* text, const char* font, float scale,
                         cocos2d::CCPoint pos, cocos2d::CCPoint anchor = { .5f, .5f }) {
        auto* l = CCLabelBMFont::create(text, font);
        l->setScale(scale);
        l->setAnchorPoint(anchor);
        l->setPosition(pos);
        parent->addChild(l, 4);
        return l;
    }

    bool setup() {
        this->setTitle("Multiplayer Roulette", "goldFont.fnt", .7f, 20.f);
        if (m_title) m_title->limitLabelWidth(210.f, .7f, .3f); // clear of the balance (top-right)

        // balance (top-right): number right-aligned, gem left of it
        m_balance = label(m_mainLayer, "--", "goldFont.fnt", .45f, { W - 16.f, H - 20.f }, { 1.f, .5f });
        m_balIcon = cocos2d::CCSprite::create((Mod::get()->getResourcesDir() / "glimmer.png").string().c_str());
        if (m_balIcon) { m_balIcon->setScale(13.f / std::max(1.f, m_balIcon->getContentHeight())); m_mainLayer->addChild(m_balIcon, 4); }

        // tabs
        auto* tabs = menuIn(m_mainLayer);
        const char* names[] = { "Browse", "Create", "Lobby" };
        for (int i = 0; i < 3; ++i) {
            auto* spr = btn(names[i], "GJ_button_04.png", 84);
            auto* b = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SorrowRoomLayer::onTab));
            b->setTag(i);
            b->setPosition({ CX + (i - 1) * 104.f, H - 44.f });
            tabs->addChild(b);
            m_tabSpr.push_back(spr);
            m_tabBtn.push_back(b);
        }
        m_status = label(m_mainLayer, "", "bigFont.fnt", .36f, { CX, H - 68.f });

        m_browseRoot = cocos2d::CCNode::create(); m_mainLayer->addChild(m_browseRoot);
        m_createRoot = cocos2d::CCNode::create(); m_mainLayer->addChild(m_createRoot);
        m_lobbyRoot = cocos2d::CCNode::create(); m_mainLayer->addChild(m_lobbyRoot);
        this->buildBrowse();
        this->buildCreate();
        this->buildLobby();

        sorrow::resolveIdentity([this](bool, int, matjson::Value) {
            if (!sorrow::isLinked()) this->setStatus("Not linked - run /gd link in Discord first", 8.f);
        });
        sorrow::refreshBalance();
        sorrow::rooms::listenRoomEvents();
        this->showView(sorrow::rooms::state().active ? View::Lobby : View::Browse);
        if (!sorrow::rooms::state().active) this->populateList();
        sorrow::rooms::RoomDirector::get()->pokeNow();

        this->schedule(schedule_selector(SorrowRoomLayer::onTick), .25f);
        this->onTick(0.f);
        return true;
    }

    void setStatus(const std::string& text, float timeout = 3.5f) {
        m_status->setString(text.c_str());
        m_status->limitLabelWidth(W - 40.f, .36f, .15f);
        m_statusTimer = timeout;
    }

    void onTick(float dt) {
        if (m_statusTimer > 0.f) { m_statusTimer -= dt; if (m_statusTimer <= 0.f) m_status->setString(""); }
        const auto bal = sorrow::glimmerBalance();
        m_balance->setString(bal >= 0 ? sorrow::formatGlimmer(bal).c_str() : "--");
        m_balance->limitLabelWidth(56.f, .45f, .2f);
        if (m_balIcon) m_balIcon->setPosition({ W - 16.f - m_balance->getScaledContentWidth() - 10.f, H - 20.f });

        const auto& st = sorrow::rooms::state();
        if (m_view == View::Lobby) {
            if (!st.active) { this->showView(View::Browse); this->populateList(); this->setStatus("You are not in a room"); return; }
            this->syncLobby();
        }
        m_tabBtn[2]->setEnabled(st.active);
        m_tabSpr[2]->setOpacity(st.active ? 255 : 110);
    }

    // ----------------------------------------------------------- browse ----
    void buildBrowse() {
        m_list = geode::ScrollLayer::create({ 16.f, 44.f, W - 32.f, 160.f });
        m_browseRoot->addChild(m_list);
        auto* m = menuIn(m_browseRoot);
        auto* r = CCMenuItemSpriteExtra::create(btn("Refresh", "GJ_button_02.png", 80), this,
            menu_selector(SorrowRoomLayer::onRefresh));
        r->setPosition({ CX, 22.f });
        m->addChild(r);
    }

    void onRefresh(CCObject*) { this->populateList(); }

    void populateList() {
        this->setStatus("Loading rooms...");
        geode::Ref<SorrowRoomLayer> self = this;
        sorrow::rooms::fetchList([self](bool ok, std::vector<sorrow::rooms::BrowseItem> rooms) {
            if (!self->getParent()) return;
            self->m_rooms = rooms;
            self->setStatus(ok ? (rooms.empty() ? "No rooms yet - create one!" : "") : "Could not reach the bot");
            self->rebuildList();
        });
    }

    void rebuildList() {
        auto* content = m_list->m_contentLayer;
        content->removeAllChildren();
        const float listW = m_list->getContentSize().width, rowH = 34.f;
        const float total = std::max(m_list->getContentSize().height, rowH * m_rooms.size());
        content->setContentSize({ listW, total });
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        content->addChild(menu, 5);
        const auto myGlobed = sorrow::currentRoomId();
        float y = total;
        for (size_t i = 0; i < m_rooms.size(); ++i) {
            const auto& r = m_rooms[i];
            y -= rowH;
            auto* cell = sorrow::ui::panel(listW - 4.f, rowH - 4.f, { 0, 0, 0 }, 80);
            cell->setAnchorPoint({ 0.f, 0.f });
            cell->setPosition({ 2.f, y + 2.f });
            content->addChild(cell);
            const float cy = y + rowH / 2.f;

            label(content, r.code.c_str(), "goldFont.fnt", .55f, { 12.f, cy + 1.f }, { 0.f, .5f });
            auto* host = label(content, r.host.c_str(), "bigFont.fnt", .4f, { 68.f, cy + 6.f }, { 0.f, .5f });
            host->limitLabelWidth(150.f, .4f, .1f);
            const std::string st = r.state == "playing" ? fmt::format("Round {}/{}", r.round, r.rounds)
                                 : r.state == "finished" ? std::string("Finished") : std::string("Lobby");
            auto* info = label(content, fmt::format("{}  -  {}/{} players  -  {} rounds{}", st, r.players, r.maxPlayers, r.rounds,
                r.pot > 0 ? fmt::format("  -  pot {}", sorrow::formatGlimmer(r.pot)) : std::string()).c_str(),
                "chatFont.fnt", .55f, { 68.f, cy - 7.f }, { 0.f, .5f });
            info->limitLabelWidth(230.f, .55f, .2f);
            info->setColor({ 210, 210, 210 });
            if (myGlobed != "0" && r.globedRoom == myGlobed) {
                auto* g = label(content, "GLOBED", "bigFont.fnt", .26f, { listW - 74.f, cy + 9.f }, { 1.f, .5f });
                g->setColor({ 120, 220, 255 });
            }
            const bool full = r.players >= r.maxPlayers && r.maxPlayers > 0;
            auto* join = CCMenuItemSpriteExtra::create(btn(full ? "Full" : "Join", full ? "GJ_button_04.png" : "GJ_button_01.png", 52, .55f),
                this, menu_selector(SorrowRoomLayer::onJoinRow));
            join->setTag(static_cast<int>(i));
            join->setEnabled(!full);
            join->setPosition({ listW - 36.f, cy });
            menu->addChild(join);
        }
        m_list->scrollToTop();
    }

    void onJoinRow(CCObject* sender) {
        const int idx = sender->getTag();
        if (idx < 0 || idx >= static_cast<int>(m_rooms.size())) return;
        if (sorrow::rooms::state().active) { this->setStatus("Leave your current room first"); return; }
        this->setStatus("Joining...");
        geode::Ref<SorrowRoomLayer> self = this;
        sorrow::rooms::join(m_rooms[idx].code, [self](bool ok, std::string err) {
            if (!self->getParent()) return;
            if (!ok) { self->setStatus(err); return; }
            sorrow::rooms::RoomDirector::get()->resetSeen();
            self->showView(View::Lobby);
            self->setStatus("Joined - bet before the host starts!");
        });
    }

    // ----------------------------------------------------------- create ----
    void buildCreate() {
        const char* rows[] = { "Levels", "Difficulty", "Mode", "Rounds", "Start target", "Max players" };
        const int defaults[] = { 0, 0, 0, 2, 0, 4 };
        const float ys[] = { 200.f, 174.f, 148.f, 122.f, 96.f, 70.f };
        auto* m = menuIn(m_createRoot);
        for (int i = 0; i < 6; ++i) {
            auto* rowLab = label(m_createRoot, rows[i], "bigFont.fnt", .42f, { 40.f, ys[i] }, { 0.f, .5f });
            if (i == 3) m_roundsRowLabel = rowLab;
            auto* l = CCSprite::createWithSpriteFrameName("edit_leftBtn_001.png");
            auto* r = CCSprite::createWithSpriteFrameName("edit_rightBtn_001.png");
            auto* lb = CCMenuItemSpriteExtra::create(l, this, menu_selector(SorrowRoomLayer::onStepDown));
            auto* rb = CCMenuItemSpriteExtra::create(r, this, menu_selector(SorrowRoomLayer::onStepUp));
            lb->setTag(i); rb->setTag(i);
            lb->setPosition({ 238.f, ys[i] });
            rb->setPosition({ 378.f, ys[i] });
            m->addChild(lb); m->addChild(rb);
            auto* v = label(m_createRoot, "", "goldFont.fnt", .55f, { 308.f, ys[i] });
            m_steps.push_back({ v, defaults[i] });
            this->refreshStep(i);
        }
        // toggles: real GD checkboxes + labels
        m_prog = true;
        auto* progBtn = sorrow::ui::checkbox(&m_prog, "Progressive targets", .6f, .4f, 150.f);
        auto* demBtn = sorrow::ui::checkbox(&m_dem, "Demons only", .6f, .4f, 120.f);
        m->addChild(progBtn);
        m->addChild(demBtn);
        sorrow::ui::row({ progBtn, demBtn }, CX, 46.f, 30.f);
        m_createBtn = CCMenuItemSpriteExtra::create(btn("Create room", "GJ_button_01.png", 120), this,
            menu_selector(SorrowRoomLayer::onCreate));
        m_createBtn->setPosition({ CX, 20.f });
        m->addChild(m_createBtn);
    }

    void refreshStep(int row) {
        auto& s = m_steps[row];
        const auto& t = table(row);
        s.idx = (s.idx % static_cast<int>(t.size()) + static_cast<int>(t.size())) % static_cast<int>(t.size());
        s.value->setString(t[s.idx].label);
        s.value->limitLabelWidth(112.f, .55f, .2f);
        if (row == 1) { // difficulty only applies to regular/trending
            const bool dl = m_steps[0].idx == 2;
            s.value->setOpacity(dl ? 90 : 255);
        }
        if (row == 2 && m_steps.size() > 3) { // mode decides what row 3 means
            m_steps[3].idx = elimSelected() ? 2 : 2; // 3 min / 3 rounds
            if (m_roundsRowLabel) m_roundsRowLabel->setString(elimSelected() ? "Round time" : "Rounds");
            this->refreshStep(3);
        }
    }
    void onStepDown(CCObject* s) { if (s->getTag() >= static_cast<int>(m_steps.size())) return; m_steps[s->getTag()].idx--; this->refreshStep(s->getTag()); if (s->getTag() == 0) this->refreshStep(1); }
    void onStepUp(CCObject* s) { if (s->getTag() >= static_cast<int>(m_steps.size())) return; m_steps[s->getTag()].idx++; this->refreshStep(s->getTag()); if (s->getTag() == 0) this->refreshStep(1); }

    void onCreate(CCObject*) {
        if (!sorrow::isLinked()) { this->setStatus("Not linked - run /gd link in Discord first"); return; }
        if (sorrow::rooms::state().active) { this->setStatus("Leave your current room first"); return; }
        sorrow::rooms::CreateOptions o;
        o.source = sources()[m_steps[0].idx].value;
        o.difficulty = diffs()[m_steps[1].idx].value;
        const auto& mode = modes()[m_steps[2].idx];
        o.mode = mode.value;
        o.teamCount = mode.num > 0 ? mode.num : 2;
        if (elimSelected()) o.roundSeconds = times()[m_steps[3].idx].num;
        else o.rounds = roundsOpt()[m_steps[3].idx].num;
        o.startTarget = targets()[m_steps[4].idx].num;
        o.maxPlayers = maxes()[m_steps[5].idx].num;
        o.progressive = m_prog;
        o.demons = m_dem;
        o.rarity = RouletteManager::get().sorRarity.empty() ? "any" : RouletteManager::get().sorRarity;
        this->setStatus("Creating...");
        m_createBtn->setEnabled(false);
        geode::Ref<SorrowRoomLayer> self = this;
        sorrow::rooms::create(o, [self](bool ok, std::string err) {
            if (!self->getParent()) return;
            self->m_createBtn->setEnabled(true);
            if (!ok) { self->setStatus(err); return; }
            sorrow::rooms::RoomDirector::get()->resetSeen();
            self->showView(View::Lobby);
            self->setStatus(sorrow::isInRoom() ? "Room open - Globed room-mates were invited" : "Room open - share the code!");
        });
    }

    // ------------------------------------------------------------ lobby ----
    void buildLobby() {
        m_roomLabel = label(m_lobbyRoot, "", "goldFont.fnt", .6f, { 20.f, 202.f }, { 0.f, .5f });
        auto* m = menuIn(m_lobbyRoot);
        auto* leave = CCMenuItemSpriteExtra::create(btn("Leave", "GJ_button_06.png", 52, .5f), this,
            menu_selector(SorrowRoomLayer::onLeave));
        leave->setID("leave");
        m->addChild(leave);
        m_leaveBtn = leave;
        auto* hist = CCMenuItemSpriteExtra::create(btn("History", "GJ_button_02.png", 64, .5f), this,
            menu_selector(SorrowRoomLayer::onHistory));
        m->addChild(hist);
        m_histBtn = hist;
        m_potLabel = label(m_lobbyRoot, "", "goldFont.fnt", .5f, { W - 20.f, 202.f }, { 1.f, .5f });
        m_info = label(m_lobbyRoot, "", "bigFont.fnt", .34f, { 20.f, 182.f }, { 0.f, .5f });

        m_members = geode::ScrollLayer::create({ 16.f, 52.f, W - 32.f, 118.f });
        m_lobbyRoot->addChild(m_members);

        m_betBtn = CCMenuItemSpriteExtra::create(btn("Bet", "GJ_button_02.png", 70), this,
            menu_selector(SorrowRoomLayer::onBet));
        m_betBtn->setPosition({ CX - 140.f, 28.f });
        m->addChild(m_betBtn);
        m_primarySpr = btn("Ready", "GJ_button_01.png", 120);
        m_primaryBtn = CCMenuItemSpriteExtra::create(m_primarySpr, this, menu_selector(SorrowRoomLayer::onPrimary));
        m_primaryBtn->setPosition({ CX, 28.f });
        m->addChild(m_primaryBtn);
        m_skipSpr = btn("Skip", "GJ_button_04.png", 70);
        m_skipBtn = CCMenuItemSpriteExtra::create(m_skipSpr, this, menu_selector(SorrowRoomLayer::onSkip));
        m_skipBtn->setPosition({ CX + 140.f, 28.f });
        m->addChild(m_skipBtn);
        m_teamBtn = CCMenuItemSpriteExtra::create(btn("Team", "GJ_button_05.png", 70), this, menu_selector(SorrowRoomLayer::onTeam));
        m_teamBtn->setPosition({ CX + 140.f, 28.f });
        m->addChild(m_teamBtn);
    }

    void syncLobby() {
        const auto& st = sorrow::rooms::state();
        m_roomLabel->setString(fmt::format("Room {}", st.code).c_str());
        m_roomLabel->limitLabelWidth(100.f, .6f, .2f);
        // pack Leave + History right after the room name (real widths, 8pt gaps)
        {
            float x = 20.f + m_roomLabel->getScaledContentWidth() + 10.f;
            for (auto* b : { m_leaveBtn, m_histBtn }) {
                const float w = b->getScaledContentWidth();
                b->setPosition({ x + w / 2.f, 202.f });
                x += w + 8.f;
            }
        }
        m_potLabel->setString(fmt::format("Pot {}", sorrow::formatGlimmer(st.pot)).c_str());
        m_potLabel->limitLabelWidth(115.f, .5f, .2f);

        std::string info;
        auto clock = [&]() { const int sec = static_cast<int>(st.roundLeftMs / 1000); return fmt::format("{}:{:02}", sec / 60, sec % 60); };
        int alive = 0;
        for (size_t i = 0; i < st.playerEliminated.size(); ++i) if (!st.playerEliminated[i]) alive++;
        std::string teamLine;
        for (const auto& t : st.teams) { if (!teamLine.empty()) teamLine += "  -  "; teamLine += fmt::format("{} {}", t.name, t.score); }
        if (st.state == "playing" && st.mode == "elimination")
            info = fmt::format("Elimination  -  round {}  -  reach {}%  -  {} left  -  {} alive", st.round, st.target, clock(), alive);
        else if (st.state == "playing" && st.mode == "teams")
            info = fmt::format("Round {}/{}  -  reach {}%  -  {}", st.round, st.rounds, st.target, teamLine);
        else if (st.state == "playing")
            info = fmt::format("Round {}/{}  -  {}  -  reach {}%", st.round, st.rounds, st.levelName, st.target);
        else if (st.state == "finished")
            info = st.winnerName.empty() ? std::string("Game over - nobody scored (bets refunded)")
                                         : fmt::format("Game over  -  {} wins!  Bet again for a rematch", st.winnerName);
        else if (st.mode == "elimination")
            info = fmt::format("Elimination lobby  -  {} per round  -  {}/{} players  -  bets open",
                fmt::format("{} min", std::max(1, st.roundSeconds / 60)), st.playerIds.size(), st.maxPlayers);
        else if (st.mode == "teams")
            info = fmt::format("Teams lobby  -  {} rounds  -  {}  -  bets open", st.rounds, teamLine);
        else
            info = fmt::format("Lobby  -  {} rounds  -  {}/{} players  -  bets open", st.rounds,
                st.playerIds.size(), st.maxPlayers);
        m_info->setString(info.c_str());
        m_info->limitLabelWidth(W - 40.f, .34f, .15f);

        const bool playing = st.state == "playing";
        m_betBtn->setVisible(!playing);
        m_skipBtn->setVisible(playing);
        m_teamBtn->setVisible(!playing && st.mode == "teams");
        const char* primary = playing ? "Go to level"
            : st.isHost ? (st.state == "finished" ? "Play again" : "Start game")
            : (st.imReady ? "Unready" : "Ready");
        if (std::string(m_primarySpr->m_label->getString()) != primary) {
            m_primarySpr->m_label->setString(primary);
            m_primarySpr->m_label->limitLabelWidth(108.f, .6f, .2f);
        }

        // members: rebuild only when something visible changed
        std::string key;
        for (size_t i = 0; i < st.playerIds.size(); ++i)
            key += fmt::format("{}|{}|{}|{}|{}|{}|{}|{}|{};", st.playerIds[i], st.playerScores[i], st.playerPercents[i],
                st.playerReady[i], st.playerBets[i], st.hostId == st.playerIds[i], st.playerTeams[i],
                st.playerEliminated[i], st.playerCleared[i]);
        key += st.state;
        if (key == m_membersKey) return;
        m_membersKey = key;

        auto* content = m_members->m_contentLayer;
        content->removeAllChildren();
        const float listW = m_members->getContentSize().width, rowH = 28.f;
        const float total = std::max(m_members->getContentSize().height, rowH * st.playerIds.size());
        content->setContentSize({ listW, total });
        const std::string me = sorrow::discordId();
        float y = total;
        for (size_t i = 0; i < st.playerIds.size(); ++i) {
            y -= rowH;
            const float cy = y + rowH / 2.f;
            auto* cell = sorrow::ui::panel(listW - 4.f, rowH - 3.f,
                st.playerIds[i] == me ? cocos2d::ccColor3B { 20, 60, 30 } : cocos2d::ccColor3B { 0, 0, 0 }, 80);
            cell->setAnchorPoint({ 0.f, 0.f });
            cell->setPosition({ 2.f, y + 1.5f });
            content->addChild(cell);
            if (st.mode == "teams") {
                const auto tc = teamColor(st.playerTeams[i]);
                auto* strip = cocos2d::CCLayerColor::create({ tc.r, tc.g, tc.b, 230 }, 3.f, rowH - 7.f);
                strip->setPosition({ 4.f, y + 3.5f });
                content->addChild(strip, 2);
            }
            const bool out = st.playerEliminated[i];
            if (auto* cube = sorrow::makeCubeIcon(st.playerIconIds[i], st.playerCol1[i], st.playerCol2[i], st.playerGlow[i])) {
                if (out) cube->setOpacity(90);
                cube->setScale(.5f);
                cube->setPosition({ 18.f, cy });
                content->addChild(cube, 3);
            }
            std::string name = st.playerNames[i];
            if (st.playerIds[i] == me) name += " (you)";
            auto* n = label(content, name.c_str(), "bigFont.fnt", .4f, { 36.f, cy }, { 0.f, .5f });
            n->limitLabelWidth(128.f, .4f, .1f);
            if (out) n->setColor({ 130, 130, 130 });
            else if (st.mode == "teams") n->setColor(teamColor(st.playerTeams[i]));
            if (st.playerIds[i] == st.hostId) {
                auto* crown = label(content, "HOST", "bigFont.fnt", .24f, { 36.f + n->getScaledContentWidth() + 6.f, cy + 1.f }, { 0.f, .5f });
                crown->setColor({ 255, 210, 80 });
            }
            auto* sc = label(content, fmt::format("{} pts", st.playerScores[i]).c_str(), "goldFont.fnt", .45f, { 262.f, cy }, { 1.f, .5f });
            sc->limitLabelWidth(52.f, .45f, .2f);
            if (st.state == "playing" || out) {
                const std::string txt = out ? std::string("OUT") : st.playerCleared[i] ? std::string("SAFE") : fmt::format("{}%", st.playerPercents[i]);
                auto* pc = label(content, txt.c_str(), "bigFont.fnt", .36f, { 306.f, cy }, { 1.f, .5f });
                pc->setColor(out ? cocos2d::ccColor3B { 255, 90, 90 } : st.playerCleared[i] ? cocos2d::ccColor3B { 110, 255, 130 } : cocos2d::ccColor3B { 200, 230, 255 });
            }
            if (st.playerBets[i] > 0) {
                auto* b = label(content, sorrow::formatGlimmer(st.playerBets[i]).c_str(), "goldFont.fnt", .42f, { listW - 34.f, cy }, { 1.f, .5f });
                b->limitLabelWidth(64.f, .42f, .2f);
            }
            if (st.state != "playing" && st.playerReady[i]) {
                auto* ok = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png");
                ok->setScale(.45f);
                ok->setPosition({ listW - 16.f, cy });
                content->addChild(ok, 3);
            }
        }
    }

    void onBet(CCObject*) {
        if (!sorrow::isLinked()) { this->setStatus("Not linked - run /gd link in Discord first"); return; }
        if (sorrow::rooms::state().state == "playing") { this->setStatus("Bets are locked while the game runs"); return; }
        geode::Ref<SorrowRoomLayer> self = this;
        auto* popup = SorBetPopup::create(sorrow::glimmerBalance(), 0, [self](std::int64_t amount) {
            self->setStatus("Betting...");
            sorrow::rooms::bet(amount, [self, amount](bool ok, std::int64_t, std::string err) {
                if (!self->getParent()) return;
                if (!ok) { self->setStatus(err); return; }
                self->setStatus(fmt::format("Bet {} - pays out when the whole roulette ends", sorrow::formatGlimmer(amount)));
                Notification::create(fmt::format("Room bet placed: {}", sorrow::formatGlimmer(amount)).c_str(),
                    NotificationIcon::Success, 1.f)->show();
            });
        });
        if (popup) popup->show();
    }

    void onPrimary(CCObject*) {
        const auto& st = sorrow::rooms::state();
        geode::Ref<SorrowRoomLayer> self = this;
        if (st.state == "playing") {
            sorrow::rooms::RoomDirector::get()->travelNow();
            this->onClose(nullptr);
            return;
        }
        m_primaryBtn->setEnabled(false);
        auto done = [self](bool ok, std::string err) {
            if (!self->getParent()) return;
            self->m_primaryBtn->setEnabled(true);
            if (!ok) self->setStatus(err);
            sorrow::rooms::RoomDirector::get()->pokeNow();
        };
        if (st.isHost) { this->setStatus("Starting..."); sorrow::rooms::startGame(done); }
        else sorrow::rooms::ready(!st.imReady, done);
    }

    void onTeam(CCObject*) {
        geode::Ref<SorrowRoomLayer> self = this;
        sorrow::rooms::switchTeam([self](bool ok, std::string err) {
            if (!self->getParent()) return;
            if (!ok) self->setStatus(err);
        });
    }

    void onHistory(CCObject*) {
        if (auto* p = SorRoomHistoryPopup::create()) p->show();
    }

    void onSkip(CCObject*) {
        geode::Ref<SorrowRoomLayer> self = this;
        sorrow::rooms::skip([self](bool ok, int votes, int needed, std::string err) {
            if (!self->getParent()) return;
            if (!ok) { self->setStatus(err); return; }
            self->setStatus(votes >= needed ? "Skipped - new level rolled" : fmt::format("Skip vote {}/{}", votes, needed));
        });
    }

    void onLeave(CCObject*) {
        geode::Ref<SorrowRoomLayer> self = this;
        sorrow::rooms::leave([self](bool ok) {
            if (!self->getParent()) return;
            self->showView(View::Browse);
            self->populateList();
            self->setStatus(ok ? "Left the room" : "Could not reach the bot");
        });
    }

    // ------------------------------------------------------------ views ----
    void showView(View v) {
        m_view = v;
        m_browseRoot->setVisible(v == View::Browse);
        m_createRoot->setVisible(v == View::Create);
        m_lobbyRoot->setVisible(v == View::Lobby);
        // ScrollLayers swallow touches even when the parent is hidden
        m_list->setTouchEnabled(v == View::Browse);
        m_members->setTouchEnabled(v == View::Lobby);
        for (int i = 0; i < 3; ++i)
            m_tabSpr[i]->updateBGImage(static_cast<int>(v) == i ? "GJ_button_01.png" : "GJ_button_04.png");
        if (v == View::Lobby) { m_membersKey.clear(); this->syncLobby(); }
    }

    void onTab(CCObject* sender) {
        const int t = sender->getTag();
        if (t == 0) { this->showView(View::Browse); this->populateList(); }
        else if (t == 1) this->showView(View::Create);
        else if (sorrow::rooms::state().active) this->showView(View::Lobby);
    }

public:
    static SorrowRoomLayer* create() {
        auto ret = new SorrowRoomLayer();
        if (ret->init(W, H)) {
            ret->setup();
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
