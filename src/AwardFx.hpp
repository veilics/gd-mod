// Sorrow Integration - orbs-style glimmer award presentation.
//
// When glimmer arrives (level completion, chest, bet win), the mod mirrors
// how vanilla presents collected orbs:
//   1. a counter (icon + balance) appears in the TOP-RIGHT of the screen,
//   2. a volley of glimmer icons bursts from the middle and flies into it,
//   3. the counter counts up from the old balance to the new one,
//   4. the feebls jingle plays (GD's own sfx path: preloadEffect+playEffect).
//
// Everything renders on the main thread (bridge callbacks are marshalled
// there by Bridge.hpp).
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/CurrencyRewardLayer.hpp>
#include <Geode/fmod/fmod.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "Format.hpp"
#include "Sprites.hpp"
#include "Store.hpp"

namespace sorrow {

// Custom sounds are not in GD's FMOD banks, and FMOD's own file IO on
// Android cannot open absolute mod paths (raw System::createSound stayed
// silent). Deterministic approach: read the bytes with std::ifstream (the
// same filesystem access the sprite PNGs get) and hand FMOD the buffer
// directly via FMOD_OPENMEMORY - no file layer, no pitch confusion, full
// volume. playEffect(pitch=1, vol=1) stays as a logged fallback.
inline void playGlimmerSound(float pitch = 1.f, float volume = 1.5f) {
    static bool s_logged = false;
    static FMOD::Sound* s_sound = nullptr;
    static std::vector<char> s_buf; // must outlive the FMOD sound

    auto engine = FMODAudioEngine::get();
    if (!engine || !engine->m_system) return;

    if (!s_sound) {
        const auto path = (Mod::get()->getResourcesDir() / "glimmer_get.ogg").string();
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            if (!s_logged) { s_logged = true; geode::log::warn("[sorrow] glimmer sound: cannot read {}", path); }
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
        s_logged = true;
        geode::log::info("[sorrow] glimmer sound: {} bytes from disk, createSound(memory) -> {}",
            size, static_cast<int>(res));
        if (res != FMOD_OK) {
            s_sound = nullptr;
            // Fallback: GD's own sfx mechanism, this time with pitch = 1.
            engine->preloadEffect(path);
            geode::log::info("[sorrow] glimmer sound: playEffect fallback -> {}",
                engine->playEffect(path, 1.f, 0.f, 1.f));
            return;
        }
    }

    FMOD::Channel* channel = nullptr;
    const auto res = engine->m_system->playSound(s_sound, nullptr, false, &channel);
    if (res != FMOD_OK || !channel) {
        geode::log::warn("[sorrow] glimmer sound: playSound -> {} ({})", static_cast<int>(res), !!channel);
        return;
    }
    channel->setVolume(volume); // amplified - FMOD allows >1.0 (clips a bit, intended)
    if (pitch != 1.f) channel->setPitch(pitch); // < 1 = the low "loss" sting
}

// Ticks a label from one balance to another (ease-in), keeping the icon
// snug against the number's left edge while the width changes. On finish it
// fades the whole counter away (orbs-counter behaviour: appears on gain,
// shows the count-up, then leaves).
class SorCountTicker : public cocos2d::CCNode {
public:
    static SorCountTicker* create(CCLabelBMFont* label, cocos2d::CCNode* icon,
                                  std::int64_t from, std::int64_t to, float dur) {
        auto n = new SorCountTicker();
        if (!n || !label) { delete n; return nullptr; }
        n->m_label = label;
        n->m_icon = icon;
        n->m_from = from;
        n->m_to = to;
        n->m_dur = std::max(0.05f, dur);
        n->autorelease();
        n->schedule(schedule_selector(SorCountTicker::tick));
        return n;
    }

    void tick(float dt) {
        m_t += dt;
        const float p = std::min(1.f, m_t / m_dur);
        const float eased = p * p;
        const auto v = m_from + static_cast<std::int64_t>(
            static_cast<double>(m_to - m_from) * eased);
        if (m_label) {
            m_label->setString(sorrow::commaNum(v).c_str()); // full number while counting
            if (m_icon)
                m_icon->setPositionX(-m_label->getScaledContentWidth() - 14.f);
        }
        if (p >= 1.f) {
            this->unschedule(schedule_selector(SorCountTicker::tick));
            // Fade the counter out and remove it, like the vanilla orb
            // counter does after its moment - the balance must not stick
            // around on the end screen.
            if (m_label && m_label->getParent()) {
                auto counter = m_label->getParent();
                for (auto child : geode::cocos::CCArrayExt<CCNode*>(counter->getChildren()))
                    child->runAction(cocos2d::CCFadeOut::create(0.7f));
                counter->runAction(cocos2d::CCSequence::create(
                    cocos2d::CCDelayTime::create(0.75f),
                    cocos2d::CCRemoveSelf::create(),
                    nullptr));
            }
            this->removeFromParent();
        }
    }

private:
    CCLabelBMFont* m_label = nullptr;
    cocos2d::CCNode* m_icon = nullptr;
    std::int64_t m_from = 0, m_to = 0;
    float m_t = 0.f, m_dur = 1.f;
};

// The orbs-style counter: glimmer icon left of the balance, right edge
// pinned to the top-right corner. Fresh on every presentation - the counter
// fades away when the count-up finishes (handled by SorCountTicker).
inline cocos2d::CCNode* ensureGlimmerCounter(cocos2d::CCNode* parent, std::int64_t target) {
    const auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    const cocos2d::CCPoint anchor = { win.width - 28.f, win.height - 28.f };

    // Always start clean: a counter from a previous award may still be
    // mid-fade; kill it so the new presentation owns the corner.
    if (auto old = parent->getChildByID("sor-glimmer-hud"_spr)) {
        old->stopAllActions();
        old->removeFromParent();
    }

    const cocos2d::CCPoint anchorPos = { win.width - 28.f, win.height - 28.f };

    auto counter = cocos2d::CCNode::create();
    counter->setID("sor-glimmer-hud"_spr);
    counter->setPosition(anchorPos);
    counter->setZOrder(500);
    parent->addChild(counter);

    auto icon = cocos2d::CCSprite::create("glimmer.png"_spr);
    if (icon) {
        // glimmer.png is 256px: size it in screen pixels (18px, orb-counter size)
        icon->setScale(18.f / std::max(1.f, icon->getContentHeight()));
        icon->setID("sor-glimmer-hud-icon"_spr);
        counter->addChild(icon);
    }
        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        if (label) {
            label->setID("sor-glimmer-hud-label"_spr);
            label->setAnchorPoint({ 1.f, 0.5f });
            label->setScale(0.5f);
            counter->addChild(label);
        }

    if (auto label = typeinfo_cast<CCLabelBMFont*>(counter->getChildByID("sor-glimmer-hud-label"_spr)))
        label->setString(sorrow::commaNum(std::max<std::int64_t>(0, target)).c_str());
    if (auto icon = counter->getChildByID("sor-glimmer-hud-icon"_spr)) {
        if (auto label = counter->getChildByID("sor-glimmer-hud-label"_spr))
            icon->setPositionX(-label->getScaledContentWidth() - 14.f);
    }
    return counter;
}

// Present a glimmer award the way vanilla presents orbs: counter top-right,
// volley of icons flying in, count-up, jingle. `preferredHost` (e.g. the
// end screen's main layer) is used when provided, otherwise the scene.
inline void presentGlimmerAward(std::int64_t amount, cocos2d::CCNode* preferredHost = nullptr) {
    if (amount <= 0) return;

    auto* parent = preferredHost
        ? preferredHost
        : cocos2d::CCDirector::sharedDirector()->getRunningScene();
    if (!parent) return;

    ensureSprites();
    playGlimmerSound();

    const auto target = glimmerBalance();
    const auto from = std::max<std::int64_t>(0, target - amount);

    auto counter = ensureGlimmerCounter(parent, from);

    // Restart any running count-up from a previous, un-finished award.
    if (auto old = parent->getChildByID("sor-glimmer-ticker"_spr)) {
        old->unscheduleAllSelectors();
        old->removeFromParent();
    }

    auto label = typeinfo_cast<CCLabelBMFont*>(counter->getChildByID("sor-glimmer-hud-label"_spr));
    auto icon = counter->getChildByID("sor-glimmer-hud-icon"_spr);
    if (!label || !icon) return;

    const int count = std::clamp(6 + static_cast<int>(amount / 20000), 8, 16);
    const auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    const cocos2d::CCPoint land = counter->getPosition();

    for (int i = 0; i < count; ++i) {
        auto mini = cocos2d::CCSprite::create("glimmer.png"_spr);
        if (!mini) break;
        const float ang = (i * 47.3f + 12.f) * 0.01745f;
        const float r = 44.f + 10.f * (i % 4);
        mini->setPosition({
            win.width * 0.5f + cosf(ang) * r,
            win.height * 0.42f + sinf(ang) * r });
        mini->setScale((15.f / std::max(1.f, mini->getContentHeight())) * (1.f - 0.05f * (i % 5)));
        mini->setOpacity(0);
        mini->setZOrder(499);
        parent->addChild(mini);

        mini->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(0.08f + 0.055f * i),
            cocos2d::CCSpawn::create(
                cocos2d::CCFadeIn::create(0.08f),
                cocos2d::CCEaseIn::create(
                    cocos2d::CCMoveTo::create(0.45f, land), 2.f),
                cocos2d::CCScaleTo::create(0.45f, 8.f / std::max(1.f, mini->getContentHeight())),
                cocos2d::CCFadeOut::create(0.4f),
                nullptr),
            cocos2d::CCRemoveSelf::create(),
            nullptr));
    }

    // Counter pop + count-up over the volley's flight time.
    counter->stopActionByTag(7);
    auto pop = cocos2d::CCSequence::create(
        cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.18f, 1.15f)),
        cocos2d::CCEaseInOut::create(cocos2d::CCScaleTo::create(0.5f, 1.f), 2.f),
        nullptr);
    pop->setTag(7);
    counter->runAction(pop);

    if (auto ticker = SorCountTicker::create(label, icon, from, target,
            std::min(1.2f, 0.35f + 0.05f * count))) {
        ticker->setID("sor-glimmer-ticker"_spr);
        ticker->setZOrder(501);
        parent->addChild(ticker);
    }
}

// ---- Vanilla rewards-screen integration (CurrencyRewardLayer) ----
//
// The chest / level-rewards screen is where vanilla shows orbs counting up.
// The glimmer counter mirrors the vanilla orb counter EXACTLY: same icon
// size (scale derived from m_orbsSprite), same label scale, same vertical
// line, and the same pulse (vanilla pulseSprite) on every arrival. Because
// the counter lives INSIDE the layer, it leaves with the layer's own exit
// animation - identical to the orb counters.

inline cocos2d::CCNode* attachGlimmerCounter(CurrencyRewardLayer* crl, std::int64_t initial) {
    auto* orbSprite = crl ? crl->m_orbsSprite : nullptr;
    auto* orbLabel = crl ? crl->m_orbsLabel : nullptr;
    if (!orbSprite || !orbLabel) return nullptr;
    auto* parent = orbSprite->getParent();
    if (!parent) return nullptr;

    if (auto prev = parent->getChildByID("sor-glimmer-crl-counter"_spr))
        prev->removeFromParent();

    const float orbIconW = orbSprite->getScaledContentWidth();
    if (orbIconW <= 0.f) return nullptr;

    // The glimmer line must stack under the LOWEST currency row shown on
    // this screen (orbs/stars/moons/diamonds/keys/shards). We deliberately
    // do NOT read the m_*Sprite member fields here: on some reward configs
    // (death rewards, small chests) those fields are stale or never
    // initialised, and dereferencing them crashed (SIGSEGV in this exact
    // function). Walking the layer's live children array can never touch
    // freed memory, so the scan is crash-proof by construction.
    cocos2d::CCNode* anchorSprite = orbSprite;
    float lowestY = orbSprite->getPositionY();
    for (auto* child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(parent->getChildren())) {
        if (!child || child == orbSprite) continue;
        if (!typeinfo_cast<cocos2d::CCSprite*>(child)) continue; // icons, not labels
        if (child->getPositionY() < lowestY - 4.f) {
            lowestY = child->getPositionY();
            anchorSprite = child;
        }
    }
    const float anchorIconW = anchorSprite->getScaledContentWidth();
    if (anchorIconW <= 0.f) return nullptr;

    auto* icon = cocos2d::CCSprite::create("glimmer.png"_spr);
    auto* label = CCLabelBMFont::create(sorrow::commaNum(initial).c_str(), "bigFont.fnt");
    if (!icon || !label) return nullptr;

    icon->setScale(anchorIconW / icon->getContentWidth());
    label->setScale(orbLabel->getScale() * 0.85f);
    icon->setID("sor-glimmer-crl-icon"_spr);
    label->setID("sor-glimmer-crl-label"_spr);

    // Mirror the vanilla arrangement (label right of icon), but place the
    // counter BELOW the anchor row (diamonds when present, else orbs).
    const float gap = 8.f;
    const auto anchorPos = anchorSprite->getPosition();
    const float dy = orbLabel->getPositionY() - orbSprite->getPosition().y;
    const bool labelRight = (orbLabel->getPositionX() - orbSprite->getPosition().x) >= 0.f;
    const float labelW = label->getScaledContentWidth();
    const float labelDx = anchorIconW * 0.5f + gap + labelW * 0.5f;
    const float rowH = anchorIconW + 8.f;

    auto* node = cocos2d::CCNode::create();
    node->setID("sor-glimmer-crl-counter"_spr);
    node->setPosition({ anchorPos.x, anchorPos.y - rowH });
    node->addChild(icon);
    label->setPosition({ labelRight ? labelDx : -labelDx, dy });
    node->addChild(label);
    parent->addChild(node);
    return node;
}

// The vanilla-style sequence on the rewards screen: jingle, a stream of
// glimmer icons arcing up into the counter, the earned amount counting up,
// and vanilla pulseSprite on the icon at every arrival. Shows EARNED amount
// (0 -> amount) like every other counter on this screen.

// The rewards screen that is currently showing (for consuming a level award
// the moment its response lands, even if the screen opened before the
// response arrived - the init-time consumption alone misses that race).
inline geode::WeakRef<CurrencyRewardLayer>& activeCRL() {
    static geode::WeakRef<CurrencyRewardLayer> ref;
    return ref;
}

// One-shot arrival callback (GD's CCCallFunc::create only takes selectors,
// not lambdas): pulses the counter icon and steps the count-up.
class SorArrivalPulse : public cocos2d::CCNode {
public:
    static SorArrivalPulse* create(CurrencyRewardLayer* crl, CCLabelBMFont* label,
                                   cocos2d::CCSprite* icon, std::int64_t vTo, float delay) {
        auto n = new SorArrivalPulse();
        if (!n || !crl || !label || !icon) { delete n; return nullptr; }
        n->m_crl = crl;
        n->m_label = label;
        n->m_icon = icon;
        n->m_vTo = vTo;
        n->autorelease();
        n->runAction(cocos2d::CCSequence::create(
            cocos2d::CCDelayTime::create(delay),
            cocos2d::CCCallFunc::create(n, callfunc_selector(SorArrivalPulse::fire)),
            nullptr));
        crl->addChild(n, 101);
        return n;
    }

    void fire() {
        if (m_crl && m_label && m_icon) {
            m_crl->pulseSprite(m_icon);
            m_label->setString(sorrow::commaNum(m_vTo).c_str());
            const float labelW = m_label->getScaledContentWidth();
            const bool labelRight = m_label->getPositionX() >= 0.f;
            m_label->setPositionX(
                (labelRight ? 1.f : -1.f)
                    * (m_icon->getScaledContentWidth() * 0.5f + 8.f + labelW * 0.5f));
        }
        this->removeFromParent();
    }

private:
    CurrencyRewardLayer* m_crl = nullptr;
    CCLabelBMFont* m_label = nullptr;
    cocos2d::CCSprite* m_icon = nullptr;
    std::int64_t m_vTo = 0;
};
inline void runVanillaGlimmerSequence(CurrencyRewardLayer* crl, std::int64_t amount) {
    // Only attach to a layer that is actually on screen; a closed chest's
    // layer falls back to the scene-level presentation in the caller.
    if (!crl || !crl->getParent() || amount <= 0) return;
    ensureSprites();

    auto* counter = attachGlimmerCounter(crl, 0);
    auto* label = counter ? typeinfo_cast<CCLabelBMFont*>(
        counter->getChildByID("sor-glimmer-crl-label"_spr)) : nullptr;
    auto* icon = counter ? counter->getChildByID("sor-glimmer-crl-icon"_spr) : nullptr;
    if (!counter || !label || !icon) { presentGlimmerAward(amount); return; }

    playGlimmerSound();

    // Fly the icons in the SAME coordinate space the counter lives in (the
    // orb sprite's parent). v2.5.0 parented them to the layer itself, whose
    // space differs from the counters' container - that is why nothing was
    // visible. Start at the screen's bottom center (where vanilla currency
    // bursts from), converted into that space.
    auto* parent = counter->getParent();
    const auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();
    const int count = std::clamp(8 + static_cast<int>(amount / 30000), 10, 18);
    const auto land = counter->getPosition();
    const auto startWorld = cocos2d::CCPoint { win.width * 0.5f, win.height * 0.06f };
    const auto start = parent ? parent->convertToNodeSpace(startWorld) : startWorld;

    const auto chunk = amount / count;
    for (int i = 0; i < count; ++i) {
        auto* mini = cocos2d::CCSprite::create("glimmer.png"_spr);
        if (!mini) break;
        mini->setScale(icon->getScale() * (0.95f - 0.03f * (i % 4)));
        // Orb-fountain style: the whole pile is visible from frame 0,
        // clustered at the spawn point.
        const auto scatter = cocos2d::CCPoint {
            static_cast<float>((std::rand() % 61) - 30),
            static_cast<float>((std::rand() % 33) - 12) };
        mini->setPosition(start + scatter);
        mini->setZOrder(400);
        if (parent) parent->addChild(mini, 100);

        const bool last = (i == count - 1);
        const auto vTo = last ? amount : chunk * (i + 1);
        const float burstT = 0.22f;
        const float flightDelay = burstT + 0.045f * i;

        // Phase 1 - the BURST: every icon pops radially at once (biased
        // upward), like the vanilla orb fountain.
        const float ang = (std::rand() % 628) / 100.f;
        const float dist = 55.f + (std::rand() % 80);
        const auto burstTo = mini->getPosition()
            + cocos2d::CCPoint { std::cos(ang) * dist * 0.8f,
                                 std::abs(std::sin(ang)) * dist + 30.f };

        // Phase 2 - fly, BOUNCE just below the counter, settle into it.
        const auto bouncePt = cocos2d::CCPoint { land.x, land.y - 46.f };

        cocos2d::ccBezierConfig up;
        up.controlPoint_1 = burstTo + cocos2d::CCPoint { 0.f, 120.f };
        up.controlPoint_2 = cocos2d::CCPoint {
            burstTo.x + (bouncePt.x - burstTo.x) * 0.55f, bouncePt.y + 160.f };
        up.endPosition = bouncePt;

        cocos2d::ccBezierConfig settle;
        settle.controlPoint_1 = cocos2d::CCPoint { bouncePt.x - 12.f, bouncePt.y + 70.f };
        settle.controlPoint_2 = cocos2d::CCPoint { land.x - 4.f, land.y + 34.f };
        settle.endPosition = land;

        mini->runAction(cocos2d::CCSequence::create(
            cocos2d::CCEaseOut::create(cocos2d::CCMoveTo::create(burstT, burstTo), 2.2f),
            cocos2d::CCDelayTime::create(0.045f * i),
            cocos2d::CCSpawn::create(
                cocos2d::CCEaseIn::create(
                    cocos2d::CCBezierTo::create(0.5f, up), 1.4f),
                cocos2d::CCScaleTo::create(0.5f, icon->getScale() * 0.7f),
                nullptr),
            cocos2d::CCEaseOut::create(cocos2d::CCBezierTo::create(0.34f, settle), 1.3f),
            cocos2d::CCRemoveSelf::create(),
            nullptr));

        if (auto arrival = SorArrivalPulse::create(crl, label,
                static_cast<cocos2d::CCSprite*>(icon), vTo, flightDelay + 0.84f)) {
            arrival->setID("sor-glimmer-arrival"_spr);
        }
    }
}

} // namespace sorrow
