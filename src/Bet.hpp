// Sorrow Integration - solo attempt betting.
//
// On a level page, a glimmer button sits next to the favorite button (same
// size). Tapping it opens the bet popup: stake, target %, and an
// "every attempt" checkbox. The stake is escrowed on the bot immediately:
//   - die BELOW the target  -> stake lost;
//   - reach the target (or complete) -> stake back + stake * target% profit
//     (completing = 100% = double the bet), presented with an extra icon
//     burst, an animated WIN! banner and the WinGamble sound.
// Stakes and target minimums are capped by the level's difficulty bucket:
//   easy-hard  100k  target locked at 100%
//   harder     10M   target >= 60%
//   insane     1B    target >= 25%
//   demon easy/medium   100B  target >= 10%
//   demon hard/insane   1T    target >= 5%
//   demon extreme       100T  target >= 3%
// PauseLayer gets the same button plus the current bet status.
#pragma once

#include "SorUi.hpp"
#include "SorConst.hpp"
#include "Format.hpp"
#include "Bridge.hpp"
#include "Glimmer.hpp"
#include "AwardFx.hpp"
#include "Hud.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include <fstream>

using namespace geode::prelude;

namespace sorrow {

// ---------------------------------------------------------------- state --

struct AttemptBet {
    std::int64_t amount = 0;
    int targetPct = 100;
};

// Bet placed for the CURRENT attempt (escrowed). Cleared when resolved.
inline AttemptBet& activeBet() { static AttemptBet s; return s; }
inline std::int64_t& activeBetLevel() { static std::int64_t s = 0; return s; }
// A bet/start request is on its way (blocks a second bet on the same attempt).
inline bool& betInFlight() { static bool s = false; return s; }

// The "every attempt" preset. Persisted; keeps re-placing itself on every
// attempt until disabled or the balance runs out.
inline AttemptBet& recurringBet() { static AttemptBet s; return s; }
inline bool& recurringOn() { static bool s = false; return s; }
// The level the re-bet belongs to: it never carries over to other levels.
inline std::int64_t& recurringLevel() { static std::int64_t s = 0; return s; }
inline bool recurringFor(std::int64_t levelId) {
    return recurringOn() && recurringBet().amount > 0 && recurringLevel() == levelId;
}

inline void loadBetPrefs() {
    auto mod = Mod::get();
    recurringBet().amount = static_cast<std::int64_t>(mod->getSavedValue<std::int64_t>("bet-rec-amount", 0));
    recurringBet().targetPct = mod->getSavedValue<int>("bet-rec-target", 100);
    recurringOn() = mod->getSavedValue<bool>("bet-rec-on", false);
    recurringLevel() = mod->getSavedValue<std::int64_t>("bet-rec-level", 0);
    if (recurringLevel() == 0) recurringOn() = false; // old global preset: drop it
}
inline void saveBetPrefs() {
    auto mod = Mod::get();
    mod->setSavedValue<std::int64_t>("bet-rec-amount", recurringBet().amount);
    mod->setSavedValue<int>("bet-rec-target", recurringBet().targetPct);
    mod->setSavedValue<bool>("bet-rec-on", recurringOn());
    mod->setSavedValue<std::int64_t>("bet-rec-level", recurringLevel());
}
inline void stopRecurring() {
    recurringOn() = false;
    saveBetPrefs();
}

// ------------------------------------------------------- difficulty rules --

struct BetRules {
    std::int64_t cap = 100000;
    int minTarget = 100;
    bool targetLocked = true;
    const char* bucket = "easy-hard";      // sent to the bot for validation
    const char* bucketLabel = "Easy-Hard";
};

inline BetRules betRulesForLevel(GJGameLevel* level) {
    const BetRules easyHard  { 100000LL,           100, true,  "easy-hard",     "Easy-Hard" };
    const BetRules harder    { 10000000LL,          60, false, "harder",        "Harder" };
    const BetRules insane    { 1000000000LL,        25, false, "insane",        "Insane" };
    const BetRules demonEasy { 100000000000LL,      10, false, "demon-easy",    "Easy/Medium Demon" };
    const BetRules demonHard { 1000000000000LL,      5, false, "demon-hard",    "Hard/Insane Demon" };
    const BetRules demonExt  { 100000000000000LL,    3, false, "demon-extreme", "Extreme Demon" };
    if (!level) return easyHard;

    // GD does NOT keep m_difficulty up to date on online levels (it stays
    // 0/stale - that is why Harder/Insane were bucketed as Easy-Hard). The
    // face GD itself draws comes from the server rating:
    //   face = m_ratingsSum / m_ratings   (1 Easy 2 Normal 3 Hard 4 Harder 5 Insane)
    //   demon  = m_demon == 1, tier = m_demonDifficulty (server key 43):
    //            3 Easy, 4 Medium, 0 Hard, 5 Insane, 6 Extreme
    // m_difficulty is only a fallback for levels built locally (roulette
    // list) that carry no rating.
    const int ratings = level->m_ratings;
    const int face = ratings > 0 ? level->m_ratingsSum / ratings : -1;
    const int demonFlag = static_cast<int>(level->m_demon.value());
    const int stars = static_cast<int>(level->m_stars.value());
    const int enumDiff = static_cast<int>(level->m_difficulty);

    // demon: the flag must be exactly 1 AND backed by a demon rating (10
    // stars, insane-or-higher face, or a demon enum) - guards against the
    // stale seed values that used to tag every level as a demon.
    // (rated non-demons max out at 9 stars, so a rated Insane can never be
    // misread as a demon even if m_demon held a stale 1)
    const bool isDemon = demonFlag == 1 && (stars >= 10 || (stars == 0 && (face >= 5 || face < 0 || enumDiff >= 6)));
    if (isDemon) {
        switch (level->m_demonDifficulty) {
            case 3: case 4: return demonEasy;          // easy / medium
            case 6:         return demonExt;           // extreme
            case 0: case 5: return demonHard;          // hard / insane
            default: break;
        }
        // locally-built levels: enum faces 7..10
        if (enumDiff == 7 || enumDiff == 8) return demonEasy;
        if (enumDiff == 10) return demonExt;
        return demonHard;
    }

    if (level->m_autoLevel) return easyHard;

    int d = face;
    if (d < 0) d = (enumDiff >= 1 && enumDiff <= 5) ? enumDiff : 0; // no rating: fallback
    switch (d) {
        case 4:  return harder;
        case 5:  return insane;
        default: return easyHard;  // NA / Auto / Easy / Normal / Hard
    }
}

// ---------------------------------------------------------------- sound --

// The WinGamble jingle. FMOD reads MPEG straight from memory; the buffer
// must outlive the sound.
inline void playWinGambleSound() {
    static bool s_logged = false;
    static FMOD::Sound* s_sound = nullptr;
    static std::vector<char> s_buf;

    auto engine = FMODAudioEngine::get();
    if (!engine || !engine->m_system) return;

    if (!s_sound) {
        const auto path = (Mod::get()->getResourcesDir() / "wingamble.mp3").string();
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            if (!s_logged) { s_logged = true; geode::log::warn("[sorrow] win sound: cannot read {}", path); }
            return;
        }
        const auto size = static_cast<size_t>(file.tellg());
        file.seekg(0);
        s_buf.resize(size);
        file.read(s_buf.data(), static_cast<std::streamsize>(size));
        file.close();

        FMOD_CREATESOUNDEXINFO info = {};
        info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        info.length = static_cast<unsigned int>(size);
        const auto res = engine->m_system->createSound(
            s_buf.data(), FMOD_OPENMEMORY | FMOD_CREATESAMPLE | FMOD_LOOP_OFF, &info, &s_sound);
        if (res != FMOD_OK) {
            s_sound = nullptr;
            if (!s_logged) { s_logged = true; geode::log::warn("[sorrow] win sound: createSound -> {}", static_cast<int>(res)); }
            return;
        }
    }

    FMOD::Channel* channel = nullptr;
    if (engine->m_system->playSound(s_sound, nullptr, false, &channel) == FMOD_OK && channel)
        channel->setVolume(1.6f);
}

// ------------------------------------------------------- win/lose stage --
//
// Both results reuse the orbs-style balance counter in the top-right corner
// (icon + number), exactly like a normal glimmer gain:
//   WIN  - a big fountain of glimmer icons bursts from the bottom and arcs
//          INTO the counter; the number turns GREEN and counts up the payout,
//          popping on every arrival.
//   LOSS - the same thing in reverse: icons spill OUT of the counter and fall
//          away; the number turns RED and quickly counts down the stake.
// Icons are sized in screen pixels (glimmer.png is 256px - scale factors
// alone are what made the old giant icons).

namespace betfx {

constexpr float ICON_PX = 18.f;      // counter icon height
constexpr float FLY_PX = 15.f;       // flying icon height

inline cocos2d::CCSize winSize() { return cocos2d::CCDirector::get()->getWinSize(); }

inline cocos2d::CCSprite* glimmerSprite(float px) {
    auto* spr = cocos2d::CCSprite::create(
        (Mod::get()->getResourcesDir() / "glimmer.png").string().c_str());
    if (spr) spr->setScale(px / std::max(1.f, spr->getContentHeight()));
    return spr;
}

// Soft coloured glow on the screen edges (centre stays clear).
inline bool screenFx() { return Mod::get()->getSettingValue<bool>("screen-effects"); }
inline bool barFx() { return Mod::get()->getSettingValue<bool>("bet-bar-effects"); }

inline void edgeGlow(cocos2d::CCNode* scene, cocos2d::ccColor3B c, GLubyte edgeAlpha,
                     float in, float hold, float out) {
    if (!screenFx()) return;
    const auto win = winSize();
    auto* holder = cocos2d::CCNode::create();
    holder->setID("sor-bet-glow"_spr);
    scene->addChild(holder, 404);
    const float side = win.width * 0.16f, band = win.height * 0.2f;
    const cocos2d::ccColor4B edge { c.r, c.g, c.b, edgeAlpha }, clear { c.r, c.g, c.b, 0 };
    struct Edge { cocos2d::CCPoint pos; cocos2d::CCSize size; cocos2d::CCPoint dir; };
    const Edge edges[] = {
        { { 0.f, 0.f }, { side, win.height }, { 1.f, 0.f } },
        { { win.width - side, 0.f }, { side, win.height }, { -1.f, 0.f } },
        { { 0.f, 0.f }, { win.width, band }, { 0.f, 1.f } },
        { { 0.f, win.height - band }, { win.width, band }, { 0.f, -1.f } },
    };
    for (const auto& e : edges) {
        auto* g = cocos2d::CCLayerGradient::create(edge, clear, e.dir);
        if (!g) continue;
        g->setContentSize(e.size);
        g->setPosition(e.pos);
        g->setOpacity(0);
        holder->addChild(g);
        g->runAction(cocos2d::CCSequence::create(
            cocos2d::CCFadeTo::create(in, 255), cocos2d::CCDelayTime::create(hold),
            cocos2d::CCFadeTo::create(out, 0), nullptr));
    }
    holder->runAction(cocos2d::CCSequence::create(
        cocos2d::CCDelayTime::create(in + hold + out + 0.05f), cocos2d::CCRemoveSelf::create(), nullptr));
}

inline cocos2d::CCAction* shake(float amp, int swings, float swingT) {
    if (!screenFx()) return cocos2d::CCDelayTime::create(0.f);
    auto* arr = cocos2d::CCArray::create();
    for (int i = 0; i < swings; ++i) {
        const float a = amp * (1.f - static_cast<float>(i) / swings);
        arr->addObject(cocos2d::CCMoveBy::create(swingT, { a, 0.f }));
        arr->addObject(cocos2d::CCMoveBy::create(swingT * 2.f, { -2.f * a, 0.f }));
        arr->addObject(cocos2d::CCMoveBy::create(swingT, { a, 0.f }));
    }
    return cocos2d::CCSequence::create(arr);
}

// Drives the counter: waits `delay`, then eases the number from -> to over
// `dur` in `tint`, popping the counter each time the value moves a step
// (win) and keeping the icon snug against the number. When done the text
// eases back to white, holds, and the whole counter fades away.
class BalanceSwing : public cocos2d::CCNode {
public:
    static BalanceSwing* create(cocos2d::CCNode* counter, CCLabelBMFont* label, cocos2d::CCNode* icon,
                                std::int64_t from, std::int64_t to, float delay, float dur,
                                int pops, bool easeOut) {
        auto* n = new BalanceSwing();
        n->m_counter = counter; n->m_label = label; n->m_icon = icon;
        n->m_from = from; n->m_to = to; n->m_delay = delay; n->m_dur = std::max(0.05f, dur);
        n->m_pops = pops; n->m_easeOut = easeOut;
        if (counter) counter->retain(); // survives another award replacing it
        n->autorelease();
        n->schedule(schedule_selector(BalanceSwing::tick));
        return n;
    }

    void tick(float dt) {
        if (!m_counter || !m_counter->getParent()) { this->removeFromParent(); return; }
        m_t += dt;
        if (m_t < m_delay) return;
        const float p = std::min(1.f, (m_t - m_delay) / m_dur);
        const float e = m_easeOut ? 1.f - (1.f - p) * (1.f - p) : p;
        const auto v = m_from + static_cast<std::int64_t>(static_cast<double>(m_to - m_from) * e);
        m_label->setString(commaNum(v).c_str()); // full number while it moves
        if (m_icon) m_icon->setPositionX(-m_label->getScaledContentWidth() - ICON_PX * 0.5f - 5.f);

        if (m_pops > 0) {
            const int step = static_cast<int>(p * m_pops);
            if (step > m_lastPop) {
                m_lastPop = step;
                m_counter->stopActionByTag(11);
                auto* pop = cocos2d::CCSequence::create(
                    cocos2d::CCScaleTo::create(0.04f, 1.14f),
                    cocos2d::CCScaleTo::create(0.10f, 1.f), nullptr);
                pop->setTag(11);
                m_counter->runAction(pop);
            }
        }

        if (p >= 1.f) {
            this->unschedule(schedule_selector(BalanceSwing::tick));
            m_label->runAction(cocos2d::CCSequence::create(
                cocos2d::CCDelayTime::create(0.7f),
                cocos2d::CCTintTo::create(0.4f, 255, 255, 255), nullptr));
            for (auto* child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(m_counter->getChildren()))
                child->runAction(cocos2d::CCSequence::create(
                    cocos2d::CCDelayTime::create(1.4f), cocos2d::CCFadeOut::create(0.6f), nullptr));
            m_counter->runAction(cocos2d::CCSequence::create(
                cocos2d::CCDelayTime::create(2.05f), cocos2d::CCRemoveSelf::create(), nullptr));
            this->removeFromParent();
        }
    }

    ~BalanceSwing() override { if (m_counter) m_counter->release(); }

private:
    cocos2d::CCNode* m_counter = nullptr;
    CCLabelBMFont* m_label = nullptr;
    cocos2d::CCNode* m_icon = nullptr;
    std::int64_t m_from = 0, m_to = 0;
    float m_t = 0.f, m_delay = 0.f, m_dur = 1.f;
    int m_pops = 0, m_lastPop = 0;
    bool m_easeOut = false;
};

// Fresh top-right counter showing `value` in `color`.
struct Counter { cocos2d::CCNode* node; CCLabelBMFont* label; cocos2d::CCNode* icon; };
inline Counter makeCounter(cocos2d::CCNode* scene, std::int64_t value, cocos2d::ccColor3B color) {
    // stop any running award ticker so it cannot overwrite our number
    if (auto old = scene->getChildByID("sor-glimmer-ticker"_spr)) { old->unscheduleAllSelectors(); old->removeFromParent(); }
    if (auto old = scene->getChildByID("sor-bet-swing"_spr)) { old->unscheduleAllSelectors(); old->removeFromParent(); }
    auto* node = ensureGlimmerCounter(scene, value);
    auto* label = typeinfo_cast<CCLabelBMFont*>(node->getChildByID("sor-glimmer-hud-label"_spr));
    auto* icon = node->getChildByID("sor-glimmer-hud-icon"_spr);
    if (label) {
        label->setColor(color);
        label->setString(commaNum(value).c_str()); // un-truncated during the swing
        if (icon) icon->setPositionX(-label->getScaledContentWidth() - ICON_PX * 0.5f - 5.f);
    }
    return { node, label, icon };
}

// "+20,000" / "-10,000" tag that slides out from under the counter.
inline void deltaTag(cocos2d::CCNode* scene, cocos2d::CCPoint counterPos, float labelW,
                     const std::string& text, cocos2d::ccColor3B color, float delay, float dy) {
    auto* tag = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    tag->setColor(color);
    tag->setAnchorPoint({ 1.f, 1.f });
    tag->setScale(0.38f);
    tag->setOpacity(0);
    tag->setPosition(counterPos + cocos2d::CCPoint { 0.f, -12.f });
    (void)labelW;
    scene->addChild(tag, 501);
    tag->runAction(cocos2d::CCSequence::create(
        cocos2d::CCDelayTime::create(delay),
        cocos2d::CCFadeIn::create(0.12f),
        cocos2d::CCDelayTime::create(0.9f),
        cocos2d::CCSpawn::create(
            cocos2d::CCMoveBy::create(0.55f, { 0.f, dy }),
            cocos2d::CCFadeOut::create(0.55f), nullptr),
        cocos2d::CCRemoveSelf::create(), nullptr));
}

} // namespace betfx

// WIN: lots of icons fountain into the counter, number turns green and
// counts up the payout.
inline void presentGlimmerWin(std::int64_t payout, std::int64_t stake = 0, cocos2d::CCNode* host = nullptr) {
    (void)stake;
    auto* scene = host ? host : cocos2d::CCDirector::get()->getRunningScene();
    if (!scene || payout <= 0) return;
    ensureSprites();
    playWinGambleSound();
    playGlimmerSound();

    const auto win = betfx::winSize();
    const auto to = std::max<std::int64_t>(0, glimmerBalance());
    const auto from = std::max<std::int64_t>(0, to - payout);
    const cocos2d::ccColor3B green { 110, 255, 130 };

    auto c = betfx::makeCounter(scene, from, green);
    if (!c.label) return;
    const auto land = c.node->getPosition()
        + cocos2d::CCPoint { -c.label->getScaledContentWidth() - betfx::ICON_PX * 0.5f - 5.f, 0.f };

    betfx::edgeGlow(scene, { 120, 255, 140 }, 90, 0.1f, 0.5f, 0.8f);

    const int count = static_cast<int>(std::clamp<std::int64_t>(36 + payout / 15000, 40, 70));
    const auto origin = cocos2d::CCPoint { win.width * 0.5f, win.height * 0.07f };
    float lastArrive = 0.f, firstArrive = 99.f;
    for (int i = 0; i < count; ++i) {
        auto* mini = betfx::glimmerSprite(betfx::FLY_PX * (0.85f + 0.05f * (i % 4)));
        if (!mini) break;
        const float baseScale = mini->getScale();
        mini->setPosition(origin + cocos2d::CCPoint {
            static_cast<float>((std::rand() % 81) - 40), static_cast<float>(std::rand() % 21) });
        scene->addChild(mini, 499);

        // burst (all at once, biased up), short hang, then arc into the counter
        const float ang = 0.35f + (std::rand() % 245) / 100.f; // ~20..160 degrees
        const float dist = 60.f + (std::rand() % 110);
        const auto burstTo = mini->getPosition() + cocos2d::CCPoint { std::cos(ang) * dist, std::sin(ang) * dist };
        const float burstT = 0.26f;
        const float wait = 0.03f * i;
        const float flyT = 0.55f;

        cocos2d::ccBezierConfig arc;
        arc.controlPoint_1 = burstTo + cocos2d::CCPoint { 0.f, 90.f };
        arc.controlPoint_2 = land + cocos2d::CCPoint { (burstTo.x - land.x) * 0.35f, 70.f };
        arc.endPosition = land;

        mini->runAction(cocos2d::CCSequence::create(
            cocos2d::CCEaseOut::create(cocos2d::CCMoveTo::create(burstT, burstTo), 2.4f),
            cocos2d::CCDelayTime::create(wait),
            cocos2d::CCSpawn::create(
                cocos2d::CCEaseIn::create(cocos2d::CCBezierTo::create(flyT, arc), 1.6f),
                cocos2d::CCScaleTo::create(flyT, baseScale * 0.7f), nullptr),
            cocos2d::CCRemoveSelf::create(), nullptr));
        const float arrive = burstT + wait + flyT;
        lastArrive = std::max(lastArrive, arrive);
        firstArrive = std::min(firstArrive, arrive);
    }

    // count up across the arrival window, popping on every few landings
    if (auto* swing = betfx::BalanceSwing::create(c.node, c.label, c.icon, from, to,
            firstArrive, std::max(0.4f, lastArrive - firstArrive), count / 2, false)) {
        swing->setID("sor-bet-swing"_spr);
        scene->addChild(swing);
    }
    betfx::deltaTag(scene, c.node->getPosition(), c.label->getScaledContentWidth(),
                    "+" + commaNum(payout), green, firstArrive, 8.f);
}

// LOSS: the same thing reversed - icons spill out of the counter and drop
// away while the number turns red and quickly counts down the stake.
inline void presentBetLoss(std::int64_t stake, int percent, int target, cocos2d::CCNode* host = nullptr) {
    (void)percent; (void)target;
    auto* scene = host ? host : cocos2d::CCDirector::get()->getRunningScene();
    if (!scene || stake <= 0) return;
    ensureSprites();
    playGlimmerSound(0.55f, 1.2f); // the jingle, pitched way down

    const auto win = betfx::winSize();
    const auto to = std::max<std::int64_t>(0, glimmerBalance()); // stake already escrowed
    const auto from = to + stake;
    const cocos2d::ccColor3B red { 255, 80, 90 };

    auto c = betfx::makeCounter(scene, from, red);
    if (!c.label) return;
    const auto iconPos = c.node->getPosition()
        + cocos2d::CCPoint { -c.label->getScaledContentWidth() - betfx::ICON_PX * 0.5f - 5.f, 0.f };

    betfx::edgeGlow(scene, { 230, 40, 60 }, 150, 0.08f, 0.3f, 0.7f);
    c.node->runAction(betfx::shake(4.f, 3, 0.03f));

    const int count = static_cast<int>(std::clamp<std::int64_t>(18 + stake / 20000, 20, 36));
    const float spill = 0.022f;
    for (int i = 0; i < count; ++i) {
        auto* mini = betfx::glimmerSprite(betfx::FLY_PX * (0.85f + 0.05f * (i % 4)));
        if (!mini) break;
        const float baseScale = mini->getScale();
        mini->setPosition(iconPos);
        mini->setScale(0.f);
        mini->setColor({ 255, 170, 175 });
        scene->addChild(mini, 499);

        // pop out of the counter, then arc down and away (reverse of a gain)
        const auto popTo = iconPos + cocos2d::CCPoint {
            -10.f - static_cast<float>(std::rand() % 40), static_cast<float>((std::rand() % 31) - 22) };
        const auto endPt = cocos2d::CCPoint {
            win.width * (0.25f + (std::rand() % 50) / 100.f), -20.f - static_cast<float>(std::rand() % 30) };
        cocos2d::ccBezierConfig fall;
        fall.controlPoint_1 = popTo + cocos2d::CCPoint { -40.f, 50.f };
        fall.controlPoint_2 = cocos2d::CCPoint { endPt.x + (popTo.x - endPt.x) * 0.4f, win.height * 0.55f };
        fall.endPosition = endPt;
        const float fallT = 0.7f + (std::rand() % 25) / 100.f;

        mini->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(spill * i),
            cocos2d::CCSpawn::create(
                cocos2d::CCEaseOut::create(cocos2d::CCMoveTo::create(0.14f, popTo), 2.f),
                cocos2d::CCScaleTo::create(0.14f, baseScale), nullptr),
            cocos2d::CCSpawn::create(
                cocos2d::CCEaseIn::create(cocos2d::CCBezierTo::create(fallT, fall), 1.7f),
                cocos2d::CCRotateBy::create(fallT, static_cast<float>((std::rand() % 360) - 180)),
                cocos2d::CCSequence::create(
                    cocos2d::CCDelayTime::create(fallT * 0.5f),
                    cocos2d::CCFadeOut::create(fallT * 0.5f), nullptr),
                nullptr),
            cocos2d::CCRemoveSelf::create(), nullptr));
    }

    // fast count-down while the icons pour out
    if (auto* swing = betfx::BalanceSwing::create(c.node, c.label, c.icon, from, to,
            0.05f, std::max(0.45f, spill * count), 0, true)) {
        swing->setID("sor-bet-swing"_spr);
        scene->addChild(swing);
    }
    betfx::deltaTag(scene, c.node->getPosition(), c.label->getScaledContentWidth(),
                    "-" + commaNum(stake), red, 0.05f, -10.f);
}


// WIN / LOSS tag under a Globed progress-bar icon. Globed parents one
// "dankmeme.globed2/remote-player-progress-<accountId>" node per player under
// the progress bar (id 0 = your own icon). Silently does nothing when Globed
// is not running or the player has no icon.
inline void showProgressIconResult(int accountId, bool won) {
    auto* pl = PlayLayer::get();
    if (!pl || !pl->m_progressBar) return;
    auto* icon = pl->m_progressBar->getChildByIDRecursive(
        fmt::format("dankmeme.globed2/remote-player-progress-{}", accountId));
    if (!icon) return;
    if (auto* old = icon->getChildByID("sor-bet-result"_spr)) old->removeFromParent();

    auto* tag = CCLabelBMFont::create(won ? "WIN" : "LOSS", "bigFont.fnt");
    tag->setID("sor-bet-result"_spr);
    tag->setColor(won ? cocos2d::ccColor3B { 110, 255, 130 } : cocos2d::ccColor3B { 255, 80, 90 });
    tag->setPosition({ 0.f, -26.f });     // under the 0.5-scale icon (icon sits at y -10)
    tag->setScale(0.f);
    icon->addChild(tag, 10);
    tag->runAction(cocos2d::CCSequence::create(
        cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.2f, 0.32f)),
        cocos2d::CCDelayTime::create(1.4f),
        cocos2d::CCSpawn::create(
            cocos2d::CCMoveBy::create(0.6f, { 0.f, won ? 6.f : -6.f }),
            cocos2d::CCFadeOut::create(0.6f), nullptr),
        cocos2d::CCRemoveSelf::create(), nullptr));
}

inline void announceBetResult(bool won, int percent) {
    showProgressIconResult(0, won); // our own Globed icon, if shown
    int acc = 0;
    try { acc = std::stoi(gdAccountId()); } catch (...) {}
    broadcastBetResult(acc, won, percent);
}

// ------------------------------------------------------------ bet calls --

using BetResolveFn = std::function<void(bool won, std::int64_t payout, std::int64_t balance)>;

inline void resolveAttemptBet(std::int64_t levelId, int percent, bool completed, BetResolveFn cb = nullptr) {
    if (!activeBet().amount || activeBetLevel() != levelId) return;
    const auto stake = activeBet().amount;
    const auto target = activeBet().targetPct;
    const bool won = completed || percent >= target;
    activeBet().amount = 0; // consumed either way; re-placed by the recurring preset
    announceBetResult(won, percent); // Globed: WIN/LOSS under our progress icon for everyone
    matjson::Value b = matjson::Value::object();
    b.set("discordId", discordId());
    b.set("gdAccountId", gdAccountId());
    b.set("levelId", std::to_string(levelId));
    b.set("won", won);
    b.set("percent", percent);
    Bridge::post("bet/resolve", b, [won, cb, stake, target, percent](bool ok, int, matjson::Value body) {
        std::int64_t payout = 0, balance = -1;
        if (ok && body.isObject()) {
            payout = static_cast<std::int64_t>(body["payout"].asInt().unwrapOr(0));
            balance = static_cast<std::int64_t>(body["balance"].asInt().unwrapOr(-1));
        }
        if (won && payout <= 0) payout = stake + stake * target / 100; // offline fallback math
        Loader::get()->queueInMainThread([won, cb, payout, balance, stake, target, percent]() {
            if (balance >= 0) setGlimmer(balance, 0);
            if (won && payout > 0) presentGlimmerWin(payout, stake);
            else if (!won) presentBetLoss(stake, percent, target);
            if (cb) cb(won, payout, balance);
        });
    });
}

// Set by the room code: returns a reason when attempt bets are not allowed
// on this level (empty = allowed).
inline std::function<std::string(std::int64_t)>& attemptBetBlocker() {
    static std::function<std::string(std::int64_t)> f;
    return f;
}

inline void placeAttemptBet(std::int64_t levelId, std::int64_t amount, int targetPct,
                            const std::string& bucket, int minTarget,
                            const std::function<void(bool, const std::string&)>& cb = nullptr) {
    // One bet per attempt: refuse while a bet is live for this level or a
    // placement request is still on its way (double taps, recurring re-arm).
    if (auto& block = attemptBetBlocker(); block) {
        if (auto why = block(levelId); !why.empty()) { if (cb) cb(false, why); return; }
    }
    if (betInFlight()) { if (cb) cb(false, "A bet is already being placed"); return; }
    if (activeBet().amount > 0 && activeBetLevel() == levelId) {
        if (cb) cb(false, "You already have a bet on this attempt");
        return;
    }
    betInFlight() = true;
    matjson::Value b = matjson::Value::object();
    b.set("discordId", discordId());
    b.set("gdAccountId", gdAccountId());
    b.set("levelId", std::to_string(levelId));
    b.set("amount", amount);
    b.set("targetPercent", targetPct);
    b.set("bucket", bucket);
    Bridge::post("bet/start", b, [cb, levelId, amount, targetPct, bucket, minTarget](bool ok, int, matjson::Value body) {
        Loader::get()->queueInMainThread([cb, ok, body, levelId, amount, targetPct, bucket, minTarget]() {
            betInFlight() = false;
            if (ok) {
                activeBet() = { amount, targetPct };
                activeBetLevel() = levelId;
                const auto balance = static_cast<std::int64_t>(body["balance"].asInt().unwrapOr(-1));
                if (balance >= 0) setGlimmer(balance, 0);
            }
            if (cb) cb(ok, ok ? std::string() : std::string(body["error"].asString().unwrapOr("bet failed")));
        });
    });
}

// Auto-apply the recurring preset (level entry / after each resolved death).
// The preset is clamped to the CURRENT level's difficulty rules - the same
// cap/target-lock table as the popup (this was the bug: re-arms went out
// with bucket="" and no clamping, so difficulty limits never applied).
inline void applyRecurringIfDue(std::int64_t levelId, GJGameLevel* level = nullptr) {
    static bool loaded = false;
    if (!loaded) { loaded = true; loadBetPrefs(); }
    if (!recurringFor(levelId)) return;
    if (activeBet().amount > 0 && activeBetLevel() == levelId) return; // already placed
    if (betInFlight()) return;
    const auto rules = betRulesForLevel(level);
    const auto amount = std::min(recurringBet().amount, rules.cap);
    if (amount <= 0) return;
    int pct = recurringBet().targetPct;
    if (rules.targetLocked) pct = 100;
    else pct = std::max(pct, rules.minTarget);
    placeAttemptBet(levelId, amount, pct, rules.bucket, rules.minTarget,
        [levelId](bool ok, const std::string& err) {
            if (!ok && err.find("glimmer") != std::string::npos) {
                stopRecurring(); // out of funds - stop instead of erroring forever
            } else if (ok) {
                if (auto hud = SorrowHud::get()) hud->showGlimmerDelta(-recurringBet().amount);
            }
        });
}

// ------------------------------------------------------------- popup ----
//
// Layout (330 x 280, y measured from the popup's bottom edge). Every row has
// its own band so nothing can overlap, labels are width-clamped AFTER each
// text change, and groups (balance pill, checkbox + label) are centred from
// their measured widths:
//   260  title
//   234  balance pill   [glimmer icon + amount]
//   202  amount input   (220 wide)
//   170  chips          [1k] [10k] [100k] [MAX]
//   150  divider
//   134/119  "Win at or above" + rule hint (left)  |  target input + % (right)
//    90  [x] Re-bet every attempt   (centred group)
//    58  info pill      payout (left)  |  cap (right)
//    24  [ Place bet ]

namespace betui {
constexpr float W = 330.f, H = 280.f;
constexpr float PAD = 28.f;                 // inner side margin
constexpr float Y_BALANCE = H - 46.f;
constexpr float Y_AMOUNT = H - 78.f;
constexpr float Y_CHIPS = H - 110.f;
constexpr float Y_DIVIDER = H - 130.f;
constexpr float Y_TARGET_LABEL = H - 146.f;
constexpr float Y_TARGET_HINT = H - 161.f;
constexpr float Y_TARGET_INPUT = H - 152.f;
constexpr float Y_EVERY = 90.f;
constexpr float Y_INFO = 58.f;
constexpr float Y_PLACE = 24.f;

inline cocos2d::extension::CCScale9Sprite* panel(float w, float h, GLubyte alpha = 90) {
    return sorrow::ui::panel(w, h, { 0, 0, 0 }, alpha);
}

inline ButtonSprite* chip(const char* text) {
    auto* spr = ButtonSprite::create(text, 44, true, "bigFont.fnt", "GJ_button_04.png", 26.f, .5f);
    spr->setScale(.85f);
    return spr;
}
} // namespace betui

class SorAttemptBetPopup : public geode::Popup {
protected:
    std::int64_t m_levelId = 0;
    BetRules m_rules;
    TextInput* m_amountInput = nullptr;
    TextInput* m_targetInput = nullptr;
    cocos2d::CCLabelBMFont* m_lockedTarget = nullptr;
    CCMenuItemSpriteExtra* m_everyBtn = nullptr;
    bool m_every = false;
    CCLabelBMFont* m_balanceLabel = nullptr;
    cocos2d::CCSprite* m_balanceIcon = nullptr;
    CCLabelBMFont* m_payoutLabel = nullptr;
    CCLabelBMFont* m_capLabel = nullptr;
    int m_balanceTicks = 0;

    bool setup() {
        using namespace betui;
        const float cx = W / 2.f;
        this->setTitle("Bet on this attempt", "goldFont.fnt", .62f, 20.f);

        // --- balance pill (icon + amount, centred as a group) ---
        auto* balBg = panel(200.f, 24.f);
        balBg->setPosition({ cx, Y_BALANCE });
        m_mainLayer->addChild(balBg);
        m_balanceIcon = cocos2d::CCSprite::create(
            (Mod::get()->getResourcesDir() / "glimmer.png").string().c_str());
        if (m_balanceIcon) {
            m_balanceIcon->setScale(16.f / std::max(1.f, m_balanceIcon->getContentHeight()));
            m_mainLayer->addChild(m_balanceIcon);
        }
        m_balanceLabel = CCLabelBMFont::create("--", "goldFont.fnt");
        m_balanceLabel->setAnchorPoint({ 0.f, .5f });
        m_mainLayer->addChild(m_balanceLabel);
        this->updateBalanceLine();
        if (glimmerBalance() < 0) refreshBalance();
        this->schedule(schedule_selector(SorAttemptBetPopup::balanceTick), 0.5f);

        // --- amount input ---
        m_amountInput = TextInput::create(220.f, "Amount...", "bigFont.fnt");
        m_amountInput->setCommonFilter(CommonFilter::Uint);
        m_amountInput->setMaxCharCount(15);
        m_amountInput->setPosition({ cx, Y_AMOUNT });
        m_amountInput->setCallback([this](std::string const&) { this->updateInfo(); });
        m_mainLayer->addChild(m_amountInput);

        // --- chips: 1k / 10k / 100k / MAX, evenly spaced, same width ---
        auto* chips = CCMenu::create();
        chips->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(chips);
        const int chipsV[] = { 1000, 10000, 100000, -1 };
        const char* chipsN[] = { "1k", "10k", "100k", "MAX" };
        for (int i = 0; i < 4; i++) {
            auto* btn = CCMenuItemSpriteExtra::create(chip(chipsN[i]), this,
                chipsV[i] < 0 ? menu_selector(SorAttemptBetPopup::onMax)
                              : menu_selector(SorAttemptBetPopup::onChip));
            btn->setTag(chipsV[i]);
            btn->setPosition({ cx + (i - 1.5f) * 62.f, Y_CHIPS });
            chips->addChild(btn);
        }

        // --- divider ---
        auto* line = cocos2d::CCLayerColor::create({ 255, 255, 255, 40 }, W - 2.f * PAD, 1.f);
        line->setPosition({ PAD, Y_DIVIDER });
        m_mainLayer->addChild(line);

        // --- target row ---
        auto* targetLab = CCLabelBMFont::create("Win at or above", "bigFont.fnt");
        targetLab->setAnchorPoint({ 0.f, .5f });
        targetLab->limitLabelWidth(150.f, .42f, .2f);
        targetLab->setPosition({ PAD, Y_TARGET_LABEL });
        m_mainLayer->addChild(targetLab);

        const std::string hint = m_rules.targetLocked
            ? fmt::format("{}: locked at 100%", m_rules.bucketLabel)
            : fmt::format("{}: minimum {}%", m_rules.bucketLabel, m_rules.minTarget);
        auto* hintLab = CCLabelBMFont::create(hint.c_str(), "chatFont.fnt");
        hintLab->setAnchorPoint({ 0.f, .5f });
        hintLab->setColor({ 200, 200, 200 });
        hintLab->limitLabelWidth(170.f, .6f, .2f);
        hintLab->setPosition({ PAD, Y_TARGET_HINT });
        m_mainLayer->addChild(hintLab);

        if (m_rules.targetLocked) {
            m_lockedTarget = CCLabelBMFont::create("100%", "goldFont.fnt");
            m_lockedTarget->setScale(.65f);
            m_lockedTarget->setAnchorPoint({ 1.f, .5f });
            m_lockedTarget->setPosition({ W - PAD, Y_TARGET_INPUT });
            m_mainLayer->addChild(m_lockedTarget);
        } else {
            // input + "%" as one right-aligned group ending at W - PAD.
            auto* pctLab = CCLabelBMFont::create("%", "bigFont.fnt");
            pctLab->setScale(.5f);
            pctLab->setAnchorPoint({ 1.f, .5f });
            pctLab->setPosition({ W - PAD, Y_TARGET_INPUT });
            m_mainLayer->addChild(pctLab);

            const float inputW = 64.f;
            m_targetInput = TextInput::create(inputW, "100", "bigFont.fnt");
            m_targetInput->setCommonFilter(CommonFilter::Uint);
            m_targetInput->setMaxCharCount(3);
            m_targetInput->setPosition({
                W - PAD - pctLab->getScaledContentWidth() - 6.f - inputW / 2.f, Y_TARGET_INPUT });
            m_targetInput->setCallback([this](std::string const&) { this->updateInfo(); });
            m_mainLayer->addChild(m_targetInput);
        }

        // --- every-attempt checkbox + label, centred as one tappable group ---
        auto* everyMenu = CCMenu::create();
        everyMenu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(everyMenu);
        m_everyBtn = ui::checkbox(&m_every, "Re-bet every attempt", .7f, .42f, 200.f, [this](bool on) {
            // applies right away (not only when a bet is placed)
            if (!on) {
                stopRecurring();
                Notification::create("Re-bet off", NotificationIcon::Info, 1.f)->show();
                return;
            }
            auto amount = std::min(this->parsedAmount(), m_rules.cap);
            int pct = this->targetPct();
            if (m_rules.targetLocked) pct = 100; else pct = std::max(pct, m_rules.minTarget);
            if (amount <= 0) {
                Notification::create("Enter an amount - re-bet starts when you place the bet", NotificationIcon::Info, 1.4f)->show();
                return;
            }
            recurringOn() = true;
            recurringBet() = { amount, pct };
            recurringLevel() = m_levelId;
            saveBetPrefs();
            Notification::create(fmt::format("Re-bet on: {} @ {}%", formatGlimmer(amount), pct).c_str(), NotificationIcon::Success, 1.2f)->show();
        });
        m_everyBtn->setPosition({ cx, Y_EVERY });
        everyMenu->addChild(m_everyBtn);

        // --- info pill: payout left, cap right ---
        auto* infoBg = panel(W - 2.f * PAD + 8.f, 26.f, 110);
        infoBg->setPosition({ cx, Y_INFO });
        m_mainLayer->addChild(infoBg);
        m_payoutLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_payoutLabel->setAnchorPoint({ 0.f, .5f });
        m_payoutLabel->setPosition({ PAD + 4.f, Y_INFO });
        m_mainLayer->addChild(m_payoutLabel);
        m_capLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_capLabel->setAnchorPoint({ 1.f, .5f });
        m_capLabel->setColor({ 210, 210, 210 });
        m_capLabel->setPosition({ W - PAD - 4.f, Y_INFO });
        m_mainLayer->addChild(m_capLabel);
        this->updateInfo();

        // --- place bet ---
        auto* betLab = ButtonSprite::create("Place bet", "bigFont.fnt", "GJ_button_01.png", .8f);
        betLab->setScale(.85f);
        auto* betBtn = CCMenuItemSpriteExtra::create(betLab, this,
            menu_selector(SorAttemptBetPopup::onPlace));
        betBtn->setPosition({ cx, Y_PLACE });
        auto* menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        menu->addChild(betBtn);
        m_mainLayer->addChild(menu);
        return true;
    }

    void updateBalanceLine() {
        using namespace betui;
        const auto bal = glimmerBalance();
        m_balanceLabel->setString(bal >= 0 ? formatGlimmer(bal).c_str() : "--");
        m_balanceLabel->limitLabelWidth(160.f, .55f, .2f);
        // re-centre icon + number as one group inside the pill
        const float iconW = m_balanceIcon ? m_balanceIcon->getScaledContentWidth() : 0.f;
        const float gap = m_balanceIcon ? 6.f : 0.f;
        const float groupW = iconW + gap + m_balanceLabel->getScaledContentWidth();
        const float left = W / 2.f - groupW / 2.f;
        if (m_balanceIcon) m_balanceIcon->setPosition({ left + iconW / 2.f, Y_BALANCE });
        m_balanceLabel->setPosition({ left + iconW + gap, Y_BALANCE + 1.f });
    }

    void balanceTick(float) {
        this->updateBalanceLine();
        if (glimmerBalance() >= 0 || ++m_balanceTicks > 12)
            this->unschedule(schedule_selector(SorAttemptBetPopup::balanceTick));
    }

    void updateInfo() {
        const auto pct = this->targetPct();
        auto amount = std::min(this->parsedAmount(), m_rules.cap);
        std::string win;
        if (amount > 0) {
            const auto profit = amount * pct / 100;
            win = fmt::format("Win {} (+{})", formatGlimmer(amount + profit), formatGlimmer(profit));
            m_payoutLabel->setColor({ 255, 255, 255 });
        } else {
            win = fmt::format("Profit: {}% of stake", pct);
            m_payoutLabel->setColor({ 200, 200, 200 });
        }
        m_payoutLabel->setString(win.c_str());
        m_payoutLabel->limitLabelWidth(170.f, .5f, .2f);
        m_capLabel->setString(fmt::format("Max bet {}", formatGlimmer(m_rules.cap)).c_str());
        m_capLabel->limitLabelWidth(90.f, .6f, .2f);
    }

    std::int64_t parsedAmount() const {
        try { return std::stoll(std::string(m_amountInput->getString())); } catch (...) { return 0; }
    }
    int targetPct() const {
        if (m_rules.targetLocked) return 100;
        try {
            const int v = std::stoi(std::string(m_targetInput->getString()));
            return std::clamp(v, 1, 100);
        } catch (...) { return 100; }
    }

    void onMax(CCObject*) {
        const auto bal = glimmerBalance();
        const auto amount = std::min(bal < 0 ? 0 : bal, m_rules.cap);
        m_amountInput->setString(amount > 0 ? std::to_string(amount).c_str() : "");
        this->updateInfo();
    }
    void onChip(CCObject* sender) {
        const auto tag = static_cast<CCMenuItemSpriteExtra*>(sender)->getTag();
        const auto amount = std::min<std::int64_t>(tag, m_rules.cap);
        m_amountInput->setString(std::to_string(amount).c_str());
        this->updateInfo();
    }
    void onPlace(CCObject*) {
        // One bet per attempt. The every-attempt checkbox still applies to
        // the NEXT attempts, so it can be switched on/off from here.
        const bool every = m_every;
        if (betInFlight() || (activeBet().amount > 0 && activeBetLevel() == m_levelId)) {
            std::string msg = "You already have a bet on this attempt";
            if (every) {
                // turn on / update re-bet from the NEXT attempt
                auto amount = std::min(this->parsedAmount(), m_rules.cap);
                int pct = this->targetPct();
                if (m_rules.targetLocked) pct = 100; else pct = std::max(pct, m_rules.minTarget);
                if (amount > 0) {
                    recurringOn() = true;
                    recurringBet() = { amount, pct };
                    recurringLevel() = m_levelId;
                    saveBetPrefs();
                    msg = fmt::format("Re-bet on: {} @ {}% from the next attempt", formatGlimmer(amount), pct);
                }
                Notification::create(msg.c_str(), NotificationIcon::Success, 1.6f)->show();
                this->onClose(nullptr);
                return;
            }
            if (recurringOn()) {
                stopRecurring();
                msg = "Already bet on this attempt - re-bet turned off";
            }
            Notification::create(msg.c_str(), NotificationIcon::Warning, 1.4f)->show();
            return;
        }
        const auto bal = glimmerBalance();
        auto amount = this->parsedAmount();
        if (amount <= 0) {
            Notification::create("Enter an amount", NotificationIcon::Warning, 1.f)->show();
            return;
        }
        if (bal >= 0 && amount > bal) amount = bal; // never bet money you do not have
        if (amount > m_rules.cap) {
            amount = m_rules.cap;
            Notification::create(
                fmt::format("Capped to {} for {}", formatGlimmer(m_rules.cap), m_rules.bucketLabel).c_str(),
                NotificationIcon::Info, 1.f)->show();
        }
        auto pct = this->targetPct();
        if (!m_rules.targetLocked && pct < m_rules.minTarget) {
            pct = m_rules.minTarget;
            Notification::create(
                fmt::format("Target raised to {}% for {}", m_rules.minTarget, m_rules.bucketLabel).c_str(),
                NotificationIcon::Info, 1.f)->show();
        }
        if (amount <= 0) {
            Notification::create("Not enough glimmer", NotificationIcon::Warning, 1.f)->show();
            return;
        }

        this->placeFor(m_levelId, amount, pct);
        if (every) {
            recurringOn() = true;
            recurringBet() = { amount, pct };
            recurringLevel() = m_levelId;
            saveBetPrefs();
        } else {
            stopRecurring();
        }
        this->onClose(nullptr);
    }

public:
    void placeFor(std::int64_t levelId, std::int64_t amount, int pct) {
        placeAttemptBet(levelId, amount, pct, m_rules.bucket, m_rules.minTarget,
            [amount, pct](bool ok, const std::string& err) {
                if (!ok) {
                    Notification::create(err.c_str(), NotificationIcon::Warning, 1.f)->show();
                    return;
                }
                Notification::create(
                    fmt::format("Bet placed: {} @ {}%", formatGlimmer(amount), pct).c_str(),
                    NotificationIcon::Success, 1.f)->show();
                if (auto hud = SorrowHud::get()) hud->showGlimmerDelta(-amount);
            });
    }

    static SorAttemptBetPopup* create(std::int64_t levelId, const BetRules& rules) {
        auto ret = new SorAttemptBetPopup();
        ret->m_levelId = levelId;
        ret->m_rules = rules;
        if (ret->init(betui::W, betui::H)) {
            ret->m_every = recurringFor(levelId); // box starts checked (this level only)
            ret->setup(); // plain Popup: no automatic setup hook
            if (ret->m_every) {
                ret->m_amountInput->setString(std::to_string(recurringBet().amount).c_str());
                if (ret->m_targetInput)
                    ret->m_targetInput->setString(std::to_string(recurringBet().targetPct).c_str());
                ret->updateInfo();
            }
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ------------------------------------------------- attempt bet badge ----
//
// Small pill pinned to the top-left of the PlayLayer that always tells you
// whether THIS attempt has a bet on it:
//   [gem] BET 10k @ 60%        (gold, bright)   - bet is live
//   [gem] BET 10k @ 60%  AUTO                  - live, re-bets every attempt
//   [gem] Placing bet...       (grey)          - request in flight
//   [gem] No bet               (dim)           - nothing riding on it
// Polls the bet state a few times a second (cheap) and pulses when it flips.
class SorBetBadge : public cocos2d::CCNode {
public:
    static SorBetBadge* create(std::int64_t levelId) {
        auto* n = new SorBetBadge();
        if (!n->init(levelId)) { delete n; return nullptr; }
        n->autorelease();
        return n;
    }

    bool init(std::int64_t levelId) {
        if (!CCNode::init()) return false;
        m_levelId = levelId;
        this->setID("sor-bet-badge"_spr);
        this->setAnchorPoint({ 0.f, 1.f });

        m_bg = ui::panel(80.f, 20.f, { 0, 0, 0 }, 90);
        m_bg->setAnchorPoint({ 0.f, 0.f });
        this->addChild(m_bg);

        m_icon = cocos2d::CCSprite::create((Mod::get()->getResourcesDir() / "glimmer.png").string().c_str());
        if (m_icon) {
            m_icon->setScale(13.f / std::max(1.f, m_icon->getContentHeight()));
            this->addChild(m_icon);
        }
        m_label = CCLabelBMFont::create("No bet", "bigFont.fnt");
        m_label->setAnchorPoint({ 0.f, .5f });
        this->addChild(m_label);

        // "Re-bet: 10k Glimmer @ 60%" under the badge while every-attempt is on
        m_rebetBg = ui::panel(80.f, 15.f, { 0, 0, 0 }, 90);
        m_rebetBg->setAnchorPoint({ 0.f, 1.f });
        this->addChild(m_rebetBg);
        m_rebet = CCLabelBMFont::create("", "bigFont.fnt");
        m_rebet->setAnchorPoint({ 0.f, .5f });
        m_rebet->setColor({ 140, 220, 255 });
        this->addChild(m_rebet);

        this->refresh(true);
        this->schedule(schedule_selector(SorBetBadge::poll), 0.15f);
        return true;
    }

    void poll(float) {
        if (auto* pl = typeinfo_cast<PlayLayer*>(this->getParent()))
            this->setVisible(!pl->m_isPracticeMode);
        this->refresh(false);
    }

    void refresh(bool force) {
        int state = 0; // 0 none, 1 placing, 2 live
        std::string text = "No bet";
        if (activeBet().amount > 0 && activeBetLevel() == m_levelId) {
            state = 2;
            text = fmt::format("BET {} @ {}%", formatGlimmer(activeBet().amount), activeBet().targetPct);
        } else if (betInFlight()) {
            state = 1;
            text = "Placing bet...";
        }
        const std::string rebet = recurringFor(m_levelId)
            ? fmt::format("Re-bet: {} Glimmer @ {}%", formatGlimmer(recurringBet().amount), recurringBet().targetPct) : std::string();
        if (!force && state == m_state && text == m_text && rebet == m_rebetText) return;
        m_rebetText = rebet;
        const bool flipped = state != m_state;
        m_state = state;
        m_text = text;

        m_label->setString(text.c_str());
        m_label->limitLabelWidth(150.f, .34f, .1f);
        const cocos2d::ccColor3B col = state == 2 ? cocos2d::ccColor3B { 255, 215, 80 }
                                     : state == 1 ? cocos2d::ccColor3B { 200, 200, 200 }
                                                  : cocos2d::ccColor3B { 170, 170, 170 };
        m_label->setColor(col);

        const float padX = 6.f, gap = 5.f, h = 20.f;
        const float iconW = m_icon ? m_icon->getScaledContentWidth() : 0.f;
        const float w = padX + iconW + gap + m_label->getScaledContentWidth() + padX;
        this->setContentSize({ w, h });
        ui::resize(m_bg, w, h);
        m_bg->setOpacity(state == 2 ? 140 : 90);
        if (m_icon) {
            m_icon->setPosition({ padX + iconW / 2.f, h / 2.f });
            m_icon->setOpacity(state == 2 ? 255 : 120);
            m_icon->setColor(state == 2 ? cocos2d::ccColor3B { 255, 255, 255 } : cocos2d::ccColor3B { 150, 150, 150 });
        }
        m_label->setPosition({ padX + iconW + gap, h / 2.f + 0.5f });
        m_label->setOpacity(state == 0 ? 170 : 255);

        m_rebet->setVisible(!rebet.empty());
        m_rebetBg->setVisible(!rebet.empty());
        if (!rebet.empty()) {
            m_rebet->setString(rebet.c_str());
            m_rebet->limitLabelWidth(150.f, .26f, .1f);
            const float rw = m_rebet->getScaledContentWidth() + 2.f * padX;
            ui::resize(m_rebetBg, rw, 15.f);
            m_rebetBg->setPosition({ 0.f, -2.f });
            m_rebet->setPosition({ padX, -2.f - 7.5f });
        }

        if (flipped && !force) {
            this->stopActionByTag(21);
            auto* pop = cocos2d::CCSequence::create(
                cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.12f, 1.12f)),
                cocos2d::CCScaleTo::create(0.15f, 1.f), nullptr);
            pop->setTag(21);
            this->runAction(pop);
        }
    }

private:
    std::int64_t m_levelId = 0;
    cocos2d::extension::CCScale9Sprite* m_bg = nullptr;
    cocos2d::CCSprite* m_icon = nullptr;
    CCLabelBMFont* m_label = nullptr;
    cocos2d::extension::CCScale9Sprite* m_rebetBg = nullptr;
    CCLabelBMFont* m_rebet = nullptr;
    int m_state = -1;
    std::string m_text, m_rebetText;
};

inline void attachBetBadge(PlayLayer* layer, std::int64_t levelId) {
    if (!layer || layer->getChildByID("sor-bet-badge"_spr)) return;
    auto* badge = SorBetBadge::create(levelId);
    if (!badge) return;
    const auto win = cocos2d::CCDirector::get()->getWinSize();
    badge->setPosition({ 6.f, win.height - 6.f });
    layer->addChild(badge, 1000);
}

// --------------------------------------------- progress bar bet effects ----
//
// While a bet rides on the attempt the progress bar turns into a golden
// tension meter (Settings > "Bet Progress Bar Effects" turns it all off):
//   * a gold target tick with a Glimmer gem on top and the target % under it,
//   * from 30% away the fill melts into a molten-gold gradient, a light
//     shine sweeps along it (faster the closer you get) and a golden halo
//     breathes around the whole bar,
//   * gold sparks spray off the fill tip, embers drift up off the fill and
//     the % label shimmers gold and throbs,
//   * the gem pings and the marker shivers in the last stretch, with a faint
//     golden glow at the screen edges,
//   * crossing the target LOCKS IN: white flash into solid shimmering gold,
//     a fountain of gems and sparks from the marker, "LOCKED IN" pops up.
// Everything is parented to the progress bar so it follows GD's bar
// position settings, and is fully restored when there is no bet.
class SorBetProgressFx : public cocos2d::CCNode {
public:
    static SorBetProgressFx* create(PlayLayer* pl, std::int64_t levelId) {
        auto* n = new SorBetProgressFx();
        n->m_pl = pl;
        n->m_levelId = levelId;
        n->autorelease();
        n->setID("sor-bet-progress-fx"_spr);
        n->scheduleUpdate();
        return n;
    }

    ~SorBetProgressFx() override { this->teardown(); }

    void update(float dt) override {
        auto* pl = m_pl;
        if (!pl || !pl->m_progressBar || !pl->m_progressFill) return;
        const bool live = activeBet().amount > 0 && activeBetLevel() == m_levelId && !pl->m_isPracticeMode
            && betfx::barFx();
        if (!live) { this->teardown(); return; }

        const int target = std::clamp(activeBet().targetPct, 1, 100);
        const float pct = std::clamp(pl->getCurrentPercent(), 0.f, 100.f);

        if (!m_built || target != m_target) this->build(target);
        if (pct + 1.f < m_lastPct) { m_passed = false; this->resetLook(); } // new attempt -> re-arm
        m_lastPct = pct;
        m_time += dt;

        auto* bar = pl->m_progressBar;
        const float fillW = std::max(0.f, pl->m_progressWidth * pct / 100.f);
        const float tipX = m_startX + fillW;

        if (!m_passed && pct >= static_cast<float>(target)) this->lockIn();

        // closeness: 0 until 30% away, 1 at the target (eased); 1 once locked in
        const float raw = m_passed ? 1.f : std::clamp((pct - (target - 30.f)) / 30.f, 0.f, 1.f);
        const float c = raw * raw * (3.f - 2.f * raw);
        const float wave = 0.5f + 0.5f * std::sin(m_time * (2.f + 10.f * c));

        // fill melts into gold
        const cocos2d::ccColor3B gold { 255, 200, 50 }, amber { 255, 140, 20 };
        if (!m_passed) pl->m_progressFill->setColor(lerp(m_fillColor, c > 0.8f ? lerp(gold, amber, (c - 0.8f) / 0.2f * wave) : gold, c));
        else pl->m_progressFill->setColor(lerp(gold, { 255, 235, 140 }, wave));

        // (no overlay rectangles: the fill sprite itself carries the gold -
        //  extra layers drew boxes around / over the bar on some layouts)

        // % label shimmers gold and throbs
        if (auto* lab = pl->m_percentageLabel) {
            const float sh = 0.5f + 0.5f * std::sin(m_time * 6.f);
            lab->setColor(lerp({ 255, 255, 255 }, lerp({ 255, 235, 130 }, { 255, 165, 30 }, sh), c));
            const float throb = c > 0.6f ? 0.08f * c * (0.5f + 0.5f * std::sin(m_time * (10.f + 8.f * c))) : 0.f;
            lab->setScale(m_labelScale * (1.f + throb));
        }

        // marker: grows, shivers at the end, gem pings
        if (m_marker) {
            const float jitter = !m_passed && c > 0.8f ? std::sin(m_time * 60.f) * 1.5f * c : 0.f;
            m_marker->setPositionX(m_markerX + jitter);
            m_marker->setScale(1.f + 0.35f * c);
            if (m_gem) m_gem->setRotation(std::sin(m_time * 3.f) * 8.f * c);
            m_pingAcc += dt;
            if (!m_passed && c > 0.4f && m_pingAcc > 1.1f - 0.7f * c) { m_pingAcc = 0.f; this->ping(); }
        }

        // sparks off the tip + embers off the fill
        if (!m_passed) {
            m_sparkAcc += dt * (c * c * 34.f);
            while (m_sparkAcc >= 1.f && c > 0.05f) { m_sparkAcc -= 1.f; this->spark(bar, { tipX, m_fillY }, c); }
        }
        m_emberAcc += dt * (c > 0.35f ? (c - 0.35f) * 18.f : 0.f);
        while (m_emberAcc >= 1.f) { m_emberAcc -= 1.f; this->ember(bar, m_startX + fillW * (std::rand() % 1000) / 1000.f); }

        // faint golden glow at the screen edges in the last stretch
        m_edgeAcc += dt;
        if (!m_passed && c > 0.85f && m_edgeAcc > 1.2f) {
            m_edgeAcc = 0.f;
            if (auto* scene = cocos2d::CCDirector::get()->getRunningScene())
                betfx::edgeGlow(scene, { 255, 190, 50 }, static_cast<GLubyte>(45.f * c), 0.2f, 0.1f, 0.7f);
        }
    }

private:
    static cocos2d::ccColor3B lerp(cocos2d::ccColor3B a, cocos2d::ccColor3B b, float t) {
        t = std::clamp(t, 0.f, 1.f);
        return { static_cast<GLubyte>(a.r + (b.r - a.r) * t),
                 static_cast<GLubyte>(a.g + (b.g - a.g) * t),
                 static_cast<GLubyte>(a.b + (b.b - a.b) * t) };
    }
    static cocos2d::CCLayerColor* dot(cocos2d::ccColor3B col, float w, float h) {
        auto* d = cocos2d::CCLayerColor::create({ col.r, col.g, col.b, 255 }, w, h);
        d->ignoreAnchorPointForPosition(false);
        d->setAnchorPoint({ .5f, .5f });
        d->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        return d;
    }

    void build(int target) {
        this->teardown();
        auto* pl = m_pl;
        auto* bar = pl->m_progressBar;
        m_target = target;
        m_fillColor = pl->m_progressFill->getColor();
        if (pl->m_percentageLabel) m_labelScale = pl->m_percentageLabel->getScale();
        const float w = bar->getContentWidth(), h = bar->getContentHeight();
        const auto fr = pl->m_progressFill->boundingBox();
        m_startX = pl->m_progressFill->getPositionX();
        m_fillH = std::max(2.f, fr.size.height);
        m_fillY = fr.getMidY();
        m_markerX = m_startX + pl->m_progressWidth * target / 100.f;

        // target marker: gold tick, gem above, target % below
        m_marker = cocos2d::CCNodeRGBA::create();
        m_marker->setCascadeOpacityEnabled(true);
        m_marker->setPosition({ m_markerX, h / 2.f });
        bar->addChild(m_marker, 50);
        auto* tick = cocos2d::CCLayerGradient::create({ 255, 250, 200, 230 }, { 255, 160, 20, 230 });
        tick->setContentSize({ 1.f, m_fillH });
        tick->setPosition({ -0.5f, m_fillY - h / 2.f - m_fillH / 2.f });
        m_marker->addChild(tick);
        if ((m_gem = betfx::glimmerSprite(10.f))) {
            m_gem->setPosition({ 0.f, h / 2.f + 9.f });
            m_marker->addChild(m_gem);
        }
        auto* tl = CCLabelBMFont::create(fmt::format("{}%", target).c_str(), "goldFont.fnt");
        tl->setScale(0.32f);
        tl->setPosition({ 0.f, -h / 2.f - 7.f });
        m_marker->addChild(tl);

        m_built = true;
        m_passed = false;
        m_shinePos = -15.f;
    }

    void resetLook() {
        auto* pl = m_pl;
        if (!pl || !m_built) return;
        if (pl->m_progressFill) { pl->m_progressFill->stopAllActions(); pl->m_progressFill->setColor(m_fillColor); }
        if (pl->m_percentageLabel) {
            pl->m_percentageLabel->setColor({ 255, 255, 255 });
            pl->m_percentageLabel->setScale(m_labelScale);
        }
        if (m_marker) { m_marker->setScale(1.f); m_marker->setPositionX(m_markerX); }

    }

    void teardown() {
        if (!m_built) return;
        this->resetLook();
        if (m_marker) m_marker->removeFromParent();
        m_marker = nullptr; m_gem = nullptr;
        m_built = false;
        m_passed = false;
    }

    void spark(cocos2d::CCNode* bar, cocos2d::CCPoint at, float c) {
        const cocos2d::ccColor3B cols[] = { { 255, 240, 160 }, { 255, 200, 50 }, { 255, 150, 30 } };
        auto* s = dot(cols[std::rand() % 3], 1.5f, 1.5f);
        s->setPosition(at + cocos2d::CCPoint { 0.f, static_cast<float>((std::rand() % 3) - 1) });
        bar->addChild(s, 45);
        const float dx = -4.f - static_cast<float>(std::rand() % 18);
        const float dy = static_cast<float>((std::rand() % 13) - 6) * (0.6f + c);
        const float t = 0.3f + (std::rand() % 25) / 100.f;
        s->runAction(cocos2d::CCSequence::create(
            cocos2d::CCSpawn::create(
                cocos2d::CCEaseOut::create(cocos2d::CCMoveBy::create(t, { dx, dy }), 2.f),
                cocos2d::CCScaleTo::create(t, 0.3f),
                cocos2d::CCFadeOut::create(t), nullptr),
            cocos2d::CCRemoveSelf::create(), nullptr));
    }

    void ember(cocos2d::CCNode* bar, float x) {
        auto* e = dot({ 255, 215, 90 }, 1.5f, 1.5f);
        e->setPosition({ x, m_fillY + m_fillH / 2.f });
        bar->addChild(e, 44);
        const float t = 0.6f + (std::rand() % 50) / 100.f;
        e->runAction(cocos2d::CCSequence::create(
            cocos2d::CCSpawn::create(
                cocos2d::CCMoveBy::create(t, { static_cast<float>((std::rand() % 9) - 4), 10.f + (std::rand() % 12) }),
                cocos2d::CCFadeOut::create(t), nullptr),
            cocos2d::CCRemoveSelf::create(), nullptr));
    }

    // a ghost gem that swells off the marker
    void ping() {
        if (!m_marker || !m_gem) return;
        auto* g = betfx::glimmerSprite(10.f);
        if (!g) return;
        const float s0 = g->getScale();
        g->setPosition(m_gem->getPosition());
        g->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
        g->setColor({ 255, 220, 120 });
        m_marker->addChild(g, -1);
        g->runAction(cocos2d::CCSequence::create(
            cocos2d::CCSpawn::create(cocos2d::CCScaleTo::create(0.5f, s0 * 2.2f), cocos2d::CCFadeOut::create(0.5f), nullptr),
            cocos2d::CCRemoveSelf::create(), nullptr));
    }

    void lockIn() {
        m_passed = true;
        auto* pl = m_pl;
        auto* bar = pl->m_progressBar;
        const float h = bar->getContentHeight();
        const cocos2d::ccColor3B gold { 255, 205, 60 };

        if (auto* lab = pl->m_percentageLabel) {
            lab->runAction(cocos2d::CCSequence::create(
                cocos2d::CCScaleTo::create(0.08f, m_labelScale * 1.35f),
                cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.3f, m_labelScale)), nullptr));
        }
        if (m_marker) {
            m_marker->runAction(cocos2d::CCSequence::create(
                cocos2d::CCScaleTo::create(0.08f, 2.f),
                cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.35f, 1.3f)), nullptr));
        }
        // fountain of gems + sparks from the marker
        for (int i = 0; i < 10; ++i) {
            auto* g = betfx::glimmerSprite(7.f + (std::rand() % 4));
            if (!g) break;
            g->setPosition({ m_markerX, h / 2.f + 6.f });
            bar->addChild(g, 48);
            const float dx = static_cast<float>((std::rand() % 81) - 40);
            const float t = 0.6f + (std::rand() % 30) / 100.f;
            g->runAction(cocos2d::CCSequence::create(
                cocos2d::CCSpawn::create(
                    cocos2d::CCJumpBy::create(t, { dx, -6.f - (std::rand() % 10) }, 22.f + (std::rand() % 18), 1),
                    cocos2d::CCRotateBy::create(t, static_cast<float>((std::rand() % 360) - 180)),
                    cocos2d::CCSequence::create(cocos2d::CCDelayTime::create(t * 0.55f), cocos2d::CCFadeOut::create(t * 0.45f), nullptr),
                    nullptr),
                cocos2d::CCRemoveSelf::create(), nullptr));
        }
        for (int i = 0; i < 22; ++i) {
            const float a = i * 0.2856f;
            auto* s = dot(i % 2 ? gold : cocos2d::ccColor3B { 255, 250, 210 }, 2.f, 2.f);
            s->setPosition({ m_markerX, h / 2.f });
            bar->addChild(s, 46);
            const float r = 18.f + (std::rand() % 14);
            s->runAction(cocos2d::CCSequence::create(
                cocos2d::CCSpawn::create(
                    cocos2d::CCEaseOut::create(cocos2d::CCMoveBy::create(0.5f, { std::cos(a) * r, std::sin(a) * r * 0.7f }), 2.f),
                    cocos2d::CCFadeOut::create(0.5f), nullptr),
                cocos2d::CCRemoveSelf::create(), nullptr));
        }
        auto* tag = CCLabelBMFont::create("LOCKED IN!", "goldFont.fnt");
        tag->setScale(0.f);
        tag->setPosition({ m_markerX, h / 2.f + 26.f });
        bar->addChild(tag, 47);
        tag->runAction(cocos2d::CCSequence::create(
            cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.25f, 0.5f)),
            cocos2d::CCDelayTime::create(0.6f),
            cocos2d::CCSpawn::create(cocos2d::CCMoveBy::create(0.6f, { 0.f, 12.f }), cocos2d::CCFadeOut::create(0.6f), nullptr),
            cocos2d::CCRemoveSelf::create(), nullptr));
        if (auto* scene = cocos2d::CCDirector::get()->getRunningScene())
            betfx::edgeGlow(scene, { 255, 200, 60 }, 110, 0.06f, 0.15f, 0.6f);
        playGlimmerSound(1.25f, 0.9f);
    }

    PlayLayer* m_pl = nullptr;
    std::int64_t m_levelId = 0;
    bool m_built = false, m_passed = false;
    int m_target = 0;
    float m_markerX = 0.f, m_startX = 0.f, m_fillH = 4.f, m_fillY = 0.f;
    float m_lastPct = 0.f, m_time = 0.f, m_sparkAcc = 0.f, m_emberAcc = 0.f, m_pingAcc = 0.f, m_edgeAcc = 0.f;
    float m_shinePos = -15.f, m_labelScale = 0.5f;
    cocos2d::ccColor3B m_fillColor { 255, 255, 255 };
    cocos2d::CCNodeRGBA* m_marker = nullptr;
    cocos2d::CCSprite* m_gem = nullptr;
    cocos2d::CCLayerColor* m_halo = nullptr;
    cocos2d::CCLayerGradient* m_goldFill = nullptr;
    cocos2d::CCNodeRGBA* m_shine = nullptr;
};

inline void attachBetProgressFx(PlayLayer* layer, std::int64_t levelId) {
    if (!layer || layer->getChildByID("sor-bet-progress-fx"_spr)) return;
    if (auto* fx = SorBetProgressFx::create(layer, levelId)) layer->addChild(fx);
}

// ------------------------------------------------- click-target hosts ----

// Invisible node living on the layer so the menu button has a stable target.
class SorBetButtonHost : public cocos2d::CCNode {
protected:
    std::int64_t m_levelId = 0;
    BetRules m_rules;
public:
    static SorBetButtonHost* create(std::int64_t levelId, const BetRules& rules) {
        auto ret = new SorBetButtonHost();
        ret->m_levelId = levelId;
        ret->m_rules = rules;
        ret->autorelease();
        return ret;
    }
    void onBet(CCObject*) {
        if (auto& block = attemptBetBlocker(); block) {
            if (auto why = block(m_levelId); !why.empty()) {
                Notification::create(why.c_str(), NotificationIcon::Info, 2.5f)->show();
                return;
            }
        }
        if (auto p = SorAttemptBetPopup::create(m_levelId, m_rules)) p->show();
    }
};

inline cocos2d::CCSprite* makeGlimmerCircle(float scale) {
    auto top = cocos2d::CCSprite::create(
        (Mod::get()->getResourcesDir() / "glimmer.png").string().c_str());
    if (!top) return nullptr;
    auto circle = CircleButtonSprite::create(top, CircleBaseColor::Green, CircleBaseSize::Small);
    if (circle) circle->setScale(scale);
    return circle;
}

// Level-page buttons (Glimmer bet, VS) go into GD's left button column
// (Geode node id "left-side-menu", a laid-out column), sized like the
// buttons already there, so they never sit on top of other mods' buttons.
// Falls back to a small menu under the title when that menu is missing.
inline void placeLevelButton(LevelInfoLayer* layer, CCMenuItemSpriteExtra* btn, cocos2d::CCNode* spr) {
    if (!layer || !btn) return;
    if (auto* col = layer->getChildByID("left-side-menu"); col && col->getLayout()) {
        float ref = 0.f;
        for (auto* c : geode::cocos::CCArrayExt<cocos2d::CCNode*>(col->getChildren()))
            ref = std::max(ref, c->getScaledContentHeight());
        if (ref <= 0.f) ref = 40.f;
        if (spr) spr->setScale(spr->getScale() * ref / std::max(1.f, spr->getScaledContentHeight()));
        btn->setContentSize(spr ? spr->getScaledContentSize() : btn->getContentSize());
        if (spr) spr->setPosition(btn->getContentSize() / 2.f);
        col->addChild(btn);
        col->updateLayout();
        return;
    }
    auto* menu = static_cast<CCMenu*>(layer->getChildByID("sor-level-buttons"));
    if (!menu) {
        menu = CCMenu::create();
        menu->setID("sor-level-buttons");
        const auto win = cocos2d::CCDirector::get()->getWinSize();
        menu->setPosition({ 30.f, win.height / 2.f - 60.f });
        menu->setLayout(geode::ColumnLayout::create()->setGap(6.f)->setAxisReverse(true));
        menu->setContentSize({ 40.f, 120.f });
        layer->addChild(menu, 60);
    }
    menu->addChild(btn);
    menu->updateLayout();
}

inline void attachLevelBetButton(LevelInfoLayer* layer, std::int64_t levelId) {
    if (!layer || layer->getChildByID("sor-bet-host")) return;
    auto host = SorBetButtonHost::create(levelId, betRulesForLevel(layer->m_level));
    host->setID("sor-bet-host");
    layer->addChild(host);

    auto spr = makeGlimmerCircle(1.f);
    if (!spr) return;
    auto btn = CCMenuItemSpriteExtra::create(spr, host, menu_selector(SorBetButtonHost::onBet));
    btn->setID("sor-bet-button"_spr);
    placeLevelButton(layer, btn, spr);
}

// Pause menu: the same chip plus a status line about the bet.
inline void attachPauseBetControls(PauseLayer* layer, std::int64_t levelId, GJGameLevel* level) {
    if (!layer || layer->getChildByID("sor-bet-menu")) return;

    auto host = SorBetButtonHost::create(levelId, betRulesForLevel(level));
    host->setID("sor-bet-host");
    layer->addChild(host);

    const auto win = cocos2d::CCDirector::get()->getWinSize();
    auto spr = makeGlimmerCircle(.8f);
    if (!spr) return;
    auto btn = CCMenuItemSpriteExtra::create(spr, host,
        menu_selector(SorBetButtonHost::onBet));
    btn->setPosition({ 46.f, 46.f });

    auto menu = CCMenu::create();
    menu->setPosition({ 0.f, 0.f });
    menu->setID("sor-bet-menu");
    menu->addChild(btn);
    menu->setZOrder(80);
    layer->addChild(menu);

    std::string status;
    if (activeBet().amount > 0 && activeBetLevel() == levelId) {
        status = fmt::format("Bet: {} @ {}%", formatGlimmer(activeBet().amount), activeBet().targetPct);
    } else if (recurringFor(levelId)) {
        status = fmt::format("Re-bet: {} Glimmer @ {}%", formatGlimmer(recurringBet().amount), recurringBet().targetPct);
    }
    if (!status.empty()) {
        auto lab = CCLabelBMFont::create(status.c_str(), "goldFont.fnt");
        lab->setScale(.34f);
        lab->setAnchorPoint({ 0.f, .5f });
        lab->setPosition({ 84.f, 46.f });
        layer->addChild(lab);
    }
}

} // namespace sorrow
