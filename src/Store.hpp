// Sorrow Integration - shared state (header-only).
// v2: bridge constants live in SorConst.hpp; this keeps session, glimmer,
// bet and room state for the HUD, roulette and Globed room sync.
#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <optional>
#include <cstdint>

using namespace geode::prelude;

namespace sorrow {

struct Session {
    enum class Kind { None, Solo, Group };

    Kind kind = Kind::None;
    std::string sid;            // bot-side session id (solo) or group id
    std::string groupCode;      // short code for group/public rooms
    std::string globedRoom;     // room id string, if the bot reported one
    int levelId = 0;
    std::string levelName;
    int target = 0;             // required percent
    bool progressive = true;
    int attempts = 0;           // attempts this level
    int bestThisLevel = 0;
    int deathsThisLevel = 0;
    std::int64_t startedAt = 0;
};

inline Session& currentSession() {
    static Session s;
    return s;
}

inline void resetSession() { currentSession() = Session{}; }

// Glimmer (the Discord bot's currency) mirrored into the mod.
// The BOT balance is the source of truth; the mod syncs on first load and
// after every award/bet. -1 means "unknown" (no bridge response yet).
inline std::int64_t& glimmerBalance() { static std::int64_t v = -1; return v; }
inline std::int64_t& lastGlimmerDelta() { static std::int64_t v = 0; return v; }
inline void setGlimmer(std::int64_t balance, std::int64_t delta) {
    glimmerBalance() = balance;
    lastGlimmerDelta() = delta;
}

// Solo glimmer bet (sorrow bet UI on the roulette layer).
struct Bet {
    bool armed = false;     // a bet was placed for the CURRENT level
    int amount = 0;         // glimmer wagered
    std::string levelId;    // level the bet is attached to
};

inline Bet& currentBet() {
    static Bet b;
    return b;
}

inline void resetBet() { currentBet() = Bet{}; }

// Active public Globed roulette room.
struct Room {
    bool active = false;
    std::string groupCode;
    std::string levelName;
    int levelId = 0;
    int target = 100;
    bool progressive = false;
    bool owner = false;     // this client created the room
    int bet = 0;
};

inline Room& activeRoom() {
    static Room r;
    return r;
}

inline void resetRoom() { activeRoom() = Room{}; }

inline bool isPractice() {
    auto pl = PlayLayer::get();
    return pl && pl->m_isPracticeMode;
}

} // namespace sorrow
