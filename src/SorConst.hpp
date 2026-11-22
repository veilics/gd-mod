// Sorrow Integration - bridge constants and GD account helpers.
// v2: the bridge URL and token are BUILT IN - no settings needed.
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/GJAccountManager.hpp>

namespace sorrow
{

inline constexpr std::string_view BRIDGE_URL = "https://gd.dread.vc";
inline constexpr std::string_view BRIDGE_TOKEN = "@nM!Q3ybUd8Arwg>t2yn]fvM]Q26My)pispA.d1g";

inline std::string bridgeUrl() { return std::string(BRIDGE_URL); }
inline std::string bridgeToken() { return std::string(BRIDGE_TOKEN); }

// Identity WITHOUT settings: the mod never asks for a Discord ID. The bot
// resolves the Discord user linked to this GD account (/gd link in Discord)
// and we cache it for the session. Empty until Bridge::resolveIdentity runs.
inline std::string& discordIdCache() { static std::string s; return s; }
inline bool& identityResolved() { static bool s = false; return s; }

inline std::string discordId() { return discordIdCache(); }
inline bool isLinked() { return !discordIdCache().empty(); }

// The signed-in GD account id - the bot maps it to the linked Discord user.
inline std::string gdAccountId() {
    if (auto mgr = GJAccountManager::get()) {
        return std::to_string(mgr->m_accountID);
    }
    return "0";
}

// Local player name (room member lists, join payloads).
inline std::string playerName() {
    if (auto mgr = GameManager::get()) {
        return mgr->m_playerName;
    }
    return "";
}

} // namespace sorrow
