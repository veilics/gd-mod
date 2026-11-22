// Sorrow Integration v2 - Discord session pickup, death stats, Globed group sync.
// (The level-complete glimmer award and public-room progress reporting live in
// layers/PlayLayer.cpp, vendored from GD-Roulette; this file only handles the
// Discord-session side and the HUD.)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "SorConst.hpp"
#include "Store.hpp"
#include "Bridge.hpp"
#include "Hud.hpp"
#include "Bet.hpp"
#include "GlobedLink.hpp"
#include "ProfilePage.hpp"
#include "SorRooms.hpp"
#include "SorDuelLayer.hpp"
#include "Chests.hpp"
#include "Garage.hpp"

using namespace geode::prelude;

namespace sorrow {

inline bool currentRunIsLegit() {
    auto pl = PlayLayer::get();
    if (!pl) return false;
    if (pl->m_isPracticeMode) return false;
    if (pl->m_isTestMode) return false;
    return true;
}

inline void attachHud() {
    auto scene = cocos2d::CCScene::get();
    if (!scene || scene->getChildByType<SorrowHud>(0)) return;
    if (auto hud = SorrowHud::create()) scene->addChild(hud);
}

inline std::int64_t nowMs() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace sorrow

class SorrowPlayLayer : public geode::Modify<SorrowPlayLayer, PlayLayer> {
public:
    struct Fields {
        int attempt = 0;
        int best = 0;
        std::int64_t levelStartMs = 0;
        bool reportedThisLevel = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        m_fields->attempt = 0;
        m_fields->best = 0;
        m_fields->reportedThisLevel = false;
        m_fields->levelStartMs = sorrow::nowMs();

        sorrow::attachHud();
        if (auto hud = sorrow::SorrowHud::get()) hud->updateDisplay();
        this->listenGlobed();
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* p1) {
        PlayLayer::destroyPlayer(player, p1);

        auto& s = sorrow::currentSession();
        if (s.kind == sorrow::Session::Kind::None) return;
        if (!player->m_isDead) return;
        if (!sorrow::currentRunIsLegit()) return;

        m_fields->attempt += 1;
        s.attempts += 1;
        s.deathsThisLevel += 1;

        const int percent = this->currentPercent();
        if (percent > s.bestThisLevel) s.bestThisLevel = percent;
        if (percent > m_fields->best) m_fields->best = percent;

        // Death stats for /gd stats.
        sorrow::reportDeath(
            std::to_string(s.levelId), s.levelName, percent, s.attempts,
            [](bool ok, int, matjson::Value body) {
                if (!ok || !body.isObject()) return;
                const auto bal = body["balance"].asInt().unwrapOr(-1);
                if (bal >= 0) sorrow::setGlimmer(bal, 0);
            });

        if (auto hud = sorrow::SorrowHud::get()) hud->updateDisplay();
    }

    void levelComplete() {
        PlayLayer::levelComplete();

        auto& s = sorrow::currentSession();
        if (s.kind == sorrow::Session::Kind::None) return;
        if (!sorrow::currentRunIsLegit()) return;
        if (m_fields->reportedThisLevel) return;
        m_fields->reportedThisLevel = true;

        s.bestThisLevel = 100;
        const int duration = static_cast<int>(sorrow::nowMs() - m_fields->levelStartMs);

        sorrow::reportRun(
            std::to_string(s.levelId), s.levelName, 100, true, false, s.attempts,
            s.kind == sorrow::Session::Kind::Group ? s.groupCode : "",
            [this](bool ok, int, matjson::Value body) {
                auto& sess = sorrow::currentSession();
                if (!ok || !body.isObject()) { sorrow::resetSession(); return; }

                const auto bal = body["balance"].asInt().unwrapOr(-1);
                const auto delta = body["glimmerDelta"].asInt().unwrapOr(0);
                if (bal >= 0) sorrow::setGlimmer(bal, delta);
                if (delta > 0) {
                    if (auto hud = sorrow::SorrowHud::get()) hud->showGlimmerDelta(delta);
                }

                const bool done = body["done"].asBool().unwrapOr(false);
                if (done) {
                    sorrow::resetSession();
                    if (auto hud = sorrow::SorrowHud::get()) hud->fade(false);
                    return;
                }
                if (body.contains("nextTarget")) {
                    sess.target = static_cast<int>(body["nextTarget"].asInt().unwrapOr(sess.target + 1));
                }
                if (sess.kind == sorrow::Session::Kind::Solo && body.contains("nextLevelId")) {
                    sess.levelId = static_cast<int>(body["nextLevelId"].asInt().unwrapOr(0));
                    if (body.contains("nextLevelName")) {
                        sess.levelName = body["nextLevelName"].asString().unwrapOr(sess.levelName);
                    }
                }
                m_fields->levelStartMs = sorrow::nowMs();
                m_fields->reportedThisLevel = false;
                if (auto hud = sorrow::SorrowHud::get()) hud->updateDisplay();
            });
    }

private:
    int currentPercent() {
        if (auto pl = PlayLayer::get()) {
            return static_cast<int>(pl->m_gameState.m_currentProgress);
        }
        return 0;
    }

    // Globed listeners for DISCORD group roulettes (/gd grouproulette).
    void listenGlobed() {
        if (!sorrow::globedAvailable()) return;
#if SORROW_GLOBED_HEADERS
        static geode::ListenerHandle* startHandle = nullptr;
        static geode::ListenerHandle* nextHandle = nullptr;

        if (!startHandle) {
            startHandle = sorrow::SorrowStartEvent::listen(
                [](const sorrow::SorrowStartEvent& ev) {
                    auto& s = sorrow::currentSession();
                    if (s.kind == sorrow::Session::Kind::Group && s.groupCode == ev.code) return;
                    if (!Mod::get()->getSettingValue<bool>("group-auto-join")) return;

                    s = sorrow::Session{};
                    s.kind = sorrow::Session::Kind::Group;
                    s.groupCode = ev.code;
                    s.levelId = ev.levelId;
                    s.levelName = "Level " + std::to_string(ev.levelId);
                    s.target = ev.target;
                    s.startedAt = sorrow::nowMs();

                    // Public room round: also join the bot room so standings
                    // and wins are tracked for this member too.
                    sorrow::rooms::joinByCode(ev.code);

                    if (auto hud = sorrow::SorrowHud::get()) hud->updateDisplay();
                }).leak();
        }
        static geode::ListenerHandle* betHandle = nullptr;
        if (!betHandle) {
            betHandle = sorrow::SorrowBetResultEvent::listen(
                [](const sorrow::SorrowBetResultEvent& ev) {
                    sorrow::showProgressIconResult(ev.accountId, ev.won);
                }).leak();
        }
        if (!nextHandle) {
            nextHandle = sorrow::SorrowNextEvent::listen(
                [](const sorrow::SorrowNextEvent& ev) {
                    auto& s = sorrow::currentSession();
                    if (s.kind != sorrow::Session::Kind::Group) return;
                    s.levelId = ev.levelId;
                    s.levelName = ev.levelName;
                    s.target = ev.target;
                    s.attempts = 0;
                    s.bestThisLevel = 0;
                    s.deathsThisLevel = 0;
                    if (auto hud = sorrow::SorrowHud::get()) hud->updateDisplay();
                }).leak();
        }
#endif
    }
};

// Startup: resolve identity once. The bot maps the GD account to the Discord
// user linked with /gd link - the mod has NO Discord ID setting.
$on_mod(Loaded) {
    sorrow::resolveIdentity();
    geode::queueInMainThread([] {
        sorrow::rooms::RoomDirector::get();   // follows your room wherever you are
        sorrow::rooms::listenRoomEvents();    // Globed room sync + invites
        sorrow::duels::DuelDirector::get();   // Versus duels: invites, rounds, results
        sorrow::duels::installScreenHook();
        sorrow::duels::listenGlobed();
    });
}
