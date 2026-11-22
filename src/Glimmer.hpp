// Sorrow Integration - Glimmer earning, sync, sound and toast UI.
// Glimmer is the Discord bot's currency, shared with in-game GD via the
// bridge: the bot balance is authoritative, the mod mirrors it and awards
// it for legit level completions (min 10000, scaled by difficulty/length,
// big demonlist bonus - the formula lives bot-side).
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/utils/cocos.hpp>

#include <chrono>
#include <thread>

#include "Bridge.hpp"
#include "Store.hpp"
#include "Sprites.hpp"
#include "AwardFx.hpp"

namespace sorrow {

// Pending level-award payout: set by the award response, consumed by the
// EndLevelLayer reward animation. -1 = nothing pending.
inline std::int64_t& pendingLevelAward() {
    static std::int64_t s_pending = -1;
    return s_pending;
}

// True when the last PlayLayer::destroyPlayer was a real death (not the
// anticheat spike). Consumed by the death-rewards CurrencyRewardLayer so
// dying pays a small random glimmer too.
inline bool& lastWasDeath() {
    static bool s_death = false;
    return s_death;
}

// Called with the award response: mirror balance; while a level-complete
// screen is up, the payout is deferred to the EndLevelLayer animation
// instead of the small toast.
inline void handleAwardResponse(bool ok, int, matjson::Value body) {
    if (!ok || !body.isObject()) return;
    const auto awarded = body["awarded"].asInt().unwrapOr(0);
    const auto balance = body["balance"].asInt().unwrapOr(-1);
    if (balance >= 0) setGlimmer(balance, 0);
    if (awarded > 0) {
        pendingLevelAward() = awarded;
        // The EndLevelLayer poll (or this fallback) presents the award
        // orbs-style: counter top-right, volley, count-up, jingle. The
        // fallback covers paths with no end screen (bet popups, chest
        // flows that never opened one, responses after the screen closed).
        std::thread([awarded]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(6000));
            geode::queueInMainThread([awarded]() {
                if (pendingLevelAward() == awarded) {
                    pendingLevelAward() = -1;
                    presentGlimmerAward(awarded);
                }
            });
        }).detach();
    }
}

// Award glimmer for a completed level (call from PlayLayer::levelComplete
// when the run was legit). The bot dedupes per level/day; repeat responses
// carry awarded: 0 and a reason.
inline void awardForLevel(GJGameLevel* level) {
    if (!level || level->m_levelID == 0) return;
    const bool practice = isPractice();
    // practice completions never pay
    if (practice) return;

    awardGlimmer(
        level->m_stars,
        level->m_coins,
        level->m_ratingsSum < 0, // RobTop marks demons with negative ratingsSum
        static_cast<int>(level->m_levelLength),
        std::to_string(level->m_levelID),
        level->m_levelName,
        handleAwardResponse);
}

// First-load balance sync: the BOT value wins, and the mod adopts the
// linked discord id the bot reports (if any) for HUD/debug display.
inline void refreshBalance() {
    syncBalance([](bool ok, int, matjson::Value body) {
        if (!ok || !body.isObject()) return;
        setGlimmer(body["balance"].asInt().unwrapOr(-1), 0);
    });
}

inline void syncOnLaunch() {
    syncBalance([](bool ok, int, matjson::Value body) {
        if (!ok || !body.isObject()) return;
        const auto balance = body["balance"].asInt().unwrapOr(-1);
        if (balance >= 0) setGlimmer(balance, 0);
        const bool linked = body["linked"].asBool().unwrapOr(false);
        if (!linked) {
            log::info("Sorrow: this GD account is not linked to a Discord user yet - "
                      "run /gd link in the Sorrow Discord server to earn Glimmer.");
        }
    });
}

} // namespace sorrow
