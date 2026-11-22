// Sorrow Integration - Globed soft-link glue + room broadcast events.
//
// Two integration paths:
//  1. Soft Link API (optional dependency): room id / owner / membership polling.
//  2. Server Events: group roulette broadcast and per-run join pings between room-mates.
//
// If the Globed headers are not available at build time everything degrades to
// polling the bridge only (solo roulette still works).
#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

#include "Store.hpp"

#if SORROW_GLOBED_HEADERS
#include <dankmeme.globed2/include/globed/soft-link/API.hpp>
#include <dankmeme.globed2/include/globed/core/Event.hpp>
#endif

using namespace geode::prelude;

namespace sorrow {

inline bool globedAvailable() {
#if SORROW_GLOBED_HEADERS
    if (!globed::api::available()) return false;
    return globed::api::isAtLeast("2.2.0");
#else
    return false;
#endif
}

// Room id as a string ("0" means global room / unknown).
inline std::string currentRoomId() {
#if SORROW_GLOBED_HEADERS
    if (!globedAvailable()) return "0";
    return std::to_string(globed::api::room::getId());
#else
    return "0";
#endif
}

inline bool isInRoom() {
#if SORROW_GLOBED_HEADERS
    return globedAvailable() && globed::api::room::isInRoom();
#else
    return false;
#endif
}

// Account ids of players in our level/session (polled - Globed has no join/leave push events).
inline std::vector<int> sessionMemberIds() {
#if SORROW_GLOBED_HEADERS
    if (!globedAvailable() || !globed::api::game::isActive()) return {};
    return globed::api::game::getPlayerIds();
#else
    return {};
#endif
}

inline int roomOwnerId() {
#if SORROW_GLOBED_HEADERS
    if (!globedAvailable()) return 0;
    return globed::api::room::getOwner();
#else
    return 0;
#endif
}

// ---------------------------------------------------------------- events

#if SORROW_GLOBED_HEADERS

// Host -> room: a group roulette started. Payload: 4-byte code + 4-byte level id + 2-byte target.
struct SorrowStartEvent : globed::ServerEvent<SorrowStartEvent, globed::EventServer::Game> {
    static inline std::string_view Id = "endrosity.sorrow-integration/start";
    std::string code;
    int levelId = 0;
    int target = 0;

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>(code.size()));
        for (char c : code) out.push_back(static_cast<uint8_t>(c));
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(levelId >> (8 * i)));
        for (int i = 0; i < 2; ++i) out.push_back(static_cast<uint8_t>(target >> (8 * i)));
        return out;
    }

    static geode::Result<SorrowStartEvent> decode(std::span<const uint8_t> data) {
        if (data.size() < 1 + 4 + 2) return Err("payload too short");
        const size_t codeLen = data[0];
        if (data.size() < 1 + codeLen + 6) return Err("payload too short");
        auto ev = SorrowStartEvent{};
        ev.code.assign(reinterpret_cast<const char*>(data.data() + 1), codeLen);
        size_t off = 1 + codeLen;
        for (int i = 0; i < 4; ++i) ev.levelId |= data[off + i] << (8 * i);
        off += 4;
        for (int i = 0; i < 2; ++i) ev.target |= data[off + i] << (8 * i);
        return Ok(ev);
    }
};

// Member -> host: I joined the group roulette.
struct SorrowJoinEvent : globed::ServerEvent<SorrowJoinEvent, globed::EventServer::Game> {
    static inline std::string_view Id = "endrosity.sorrow-integration/join";
    std::string code;

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>(code.size()));
        for (char c : code) out.push_back(static_cast<uint8_t>(c));
        return out;
    }

    static geode::Result<SorrowJoinEvent> decode(std::span<const uint8_t> data) {
        if (data.size() < 1) return Err("payload too short");
        const size_t codeLen = data[0];
        if (data.size() < 1 + codeLen) return Err("payload too short");
        auto ev = SorrowJoinEvent{};
        ev.code.assign(reinterpret_cast<const char*>(data.data() + 1), codeLen);
        return Ok(ev);
    }
};

// Host -> room: the group moved to the next level (progressive mode).
struct SorrowNextEvent : globed::ServerEvent<SorrowNextEvent, globed::EventServer::Game> {
    static inline std::string_view Id = "endrosity.sorrow-integration/next";
    int levelId = 0;
    std::string levelName;
    int target = 0;

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(levelId >> (8 * i)));
        for (int i = 0; i < 2; ++i) out.push_back(static_cast<uint8_t>(target >> (8 * i)));
        out.push_back(static_cast<uint8_t>(levelName.size()));
        for (char c : levelName) out.push_back(static_cast<uint8_t>(c));
        return out;
    }

    static geode::Result<SorrowNextEvent> decode(std::span<const uint8_t> data) {
        if (data.size() < 7) return Err("payload too short");
        auto ev = SorrowNextEvent{};
        for (int i = 0; i < 4; ++i) ev.levelId |= data[i] << (8 * i);
        for (int i = 0; i < 2; ++i) ev.target |= data[4 + i] << (8 * i);
        const size_t nameLen = data[6];
        if (data.size() < 7 + nameLen) return Err("payload too short");
        ev.levelName.assign(reinterpret_cast<const char*>(data.data() + 7), nameLen);
        return Ok(ev);
    }
};


// Anyone -> session: my attempt bet just resolved. Receivers show WIN/LOSS
// under my icon on their progress bar.
// Payload: i32 accountId, u8 won, u8 percent.
struct SorrowBetResultEvent : globed::ServerEvent<SorrowBetResultEvent, globed::EventServer::Game> {
    static inline std::string_view Id = "endrosity.sorrow-integration/bet-result";
    int accountId = 0;
    bool won = false;
    int percent = 0;

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(accountId) >> (8 * i)));
        out.push_back(won ? 1 : 0);
        out.push_back(static_cast<uint8_t>(std::clamp(percent, 0, 100)));
        return out;
    }

    static geode::Result<SorrowBetResultEvent> decode(std::span<const uint8_t> data) {
        if (data.size() < 6) return Err("payload too short");
        SorrowBetResultEvent ev;
        uint32_t a = 0;
        for (int i = 0; i < 4; ++i) a |= static_cast<uint32_t>(data[i]) << (8 * i);
        ev.accountId = static_cast<int>(a);
        ev.won = data[4] != 0;
        ev.percent = data[5];
        return Ok(ev);
    }
};

// Room sync between mod users in the same Globed room.
//   kind 1 = invite (a roulette room was opened)   kind 2 = room changed
//   (round started / won / game over - receivers re-poll the bot at once)
// Payload: u8 kind, u8 codeLen, code, u8 nameLen, host name.
struct SorrowRoomEvent : globed::ServerEvent<SorrowRoomEvent, globed::EventServer::Game> {
    static inline std::string_view Id = "endrosity.sorrow-integration/room";
    int kind = 0;
    std::string code;
    std::string host;

    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>(kind));
        out.push_back(static_cast<uint8_t>(std::min<size_t>(code.size(), 16)));
        for (size_t i = 0; i < std::min<size_t>(code.size(), 16); ++i) out.push_back(static_cast<uint8_t>(code[i]));
        out.push_back(static_cast<uint8_t>(std::min<size_t>(host.size(), 40)));
        for (size_t i = 0; i < std::min<size_t>(host.size(), 40); ++i) out.push_back(static_cast<uint8_t>(host[i]));
        return out;
    }

    static geode::Result<SorrowRoomEvent> decode(std::span<const uint8_t> data) {
        if (data.size() < 2) return Err("payload too short");
        SorrowRoomEvent ev;
        ev.kind = data[0];
        size_t off = 1;
        const size_t cl = data[off++];
        if (data.size() < off + cl + 1) return Err("payload too short");
        ev.code.assign(reinterpret_cast<const char*>(data.data() + off), cl);
        off += cl;
        const size_t hl = data[off++];
        if (data.size() < off + hl) return Err("payload too short");
        ev.host.assign(reinterpret_cast<const char*>(data.data() + off), hl);
        return Ok(ev);
    }
};

inline void broadcastBetResult(int accountId, bool won, int percent) {
    if (!globedAvailable() || accountId <= 0) return;
    if (!globed::api::game::isActive()) return; // only meaningful inside a Globed level session
    SorrowBetResultEvent ev;
    ev.accountId = accountId;
    ev.won = won;
    ev.percent = percent;
    ev.send(globed::EventServer::Game);
}

inline void broadcastRoom(int kind, const std::string& code, const std::string& host) {
    if (!globedAvailable() || !isInRoom()) return;
    SorrowRoomEvent ev;
    ev.kind = kind;
    ev.code = code;
    ev.host = host;
    ev.send(globed::EventServer::Game);
}

inline void announceStart(const std::string& code, int levelId, int target) {
    if (!globedAvailable() || !isInRoom()) return;
    SorrowStartEvent ev;
    ev.code = code;
    ev.levelId = levelId;
    ev.target = target;
    ev.send(globed::EventServer::Game);
}

inline void broadcastNext(int levelId, const std::string& levelName, int target) {
    if (!globedAvailable() || !isInRoom()) return;
    SorrowNextEvent ev;
    ev.levelId = levelId;
    ev.levelName = levelName;
    ev.target = target;
    ev.send(globed::EventServer::Game);
}

inline void registerGlobedEvents() {
    if (!globedAvailable()) return;
    globed::api::waitForGlobed([] {
        globed::api::net::registerEvent("endrosity.sorrow-integration/start", globed::EventServer::Game);
        globed::api::net::registerEvent("endrosity.sorrow-integration/join", globed::EventServer::Game);
        globed::api::net::registerEvent("endrosity.sorrow-integration/next", globed::EventServer::Game);
        globed::api::net::registerEvent("endrosity.sorrow-integration/bet-result", globed::EventServer::Game);
        globed::api::net::registerEvent("endrosity.sorrow-integration/room", globed::EventServer::Game);
    });
}

#else

inline void registerGlobedEvents() {}
inline void announceStart(const std::string&, int, int) {}
inline void broadcastNext(int, const std::string&, int) {}
inline void broadcastBetResult(int, bool, int) {}
inline void broadcastRoom(int, const std::string&, const std::string&) {}

#endif // SORROW_GLOBED_HEADERS

} // namespace sorrow
