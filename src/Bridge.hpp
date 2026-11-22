// Sorrow Integration - HTTP bridge client (v2).
// Talks to https://gd.dread.vc (Cloudflare Worker relaying to the Discord bot).
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <matjson.hpp>

#include "SorConst.hpp"
#include "Store.hpp"

namespace sorrow
{

using ResponseFn = std::function<void(bool ok, int code, matjson::Value body)>;

// v5.x web API: WebRequest::post/get return a WebFuture; geode::async::spawn
// drives it off-thread and delivers the WebResponse on the main thread.
inline void request(const std::string& method, const std::string& path,
                    const std::string& body, ResponseFn cb = nullptr) {
    web::WebRequest req;
    req.header("Content-Type", "application/json");
    req.header("Authorization", "Bearer " + bridgeToken());
    req.header("Bypass-Tunnel-Header", "true");
    if (!body.empty()) req.bodyString(body);

    const std::string url = bridgeUrl() + path;

    geode::async::spawn(
        [req = std::move(req), url, method]() mutable -> web::WebFuture {
            return method == "POST" ? req.post(url) : req.get(url);
        },
        [cb = std::move(cb)](web::WebResponse res) {
            if (!cb) return;
            const int code = res.code();
            const bool ok = code >= 200 && code < 300;
            matjson::Value parsed;
            auto str = res.string();
            if (str) {
                auto parsedRes = matjson::parse(*str);
                if (parsedRes) parsed = std::move(*parsedRes);
            }
            // Callbacks build cocos UI (lists, labels, popups) - they MUST run
            // on the main thread. geode::async::spawn completes on a worker
            // thread; UI built there races the render thread and silently
            // disappears (rooms list empty, profile glimmer row missing).
            geode::queueInMainThread(
                [cb, ok, code, parsed = std::move(parsed)]() mutable {
                    cb(ok, code, std::move(parsed));
                });
        });
}

// Minimal percent-encoding for query values (ids, names, codes).
inline std::string q(const std::string& v) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (char c : v) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') ||
            u == '-' || u == '_' || u == '.' || u == '~') {
            out += c;
        } else {
            out += '%';
            out += hex[u >> 4];
            out += hex[u & 0xF];
        }
    }
    return out;
}

// ---------- generic helpers (action names map 1:1 to bridge handlers) ----------

inline void get(const std::string& action, ResponseFn cb) {
    request("GET", std::string("/bridge/sorrow/") + action, "", std::move(cb));
}

inline void post(const std::string& action, const matjson::Value& body, ResponseFn cb) {
    request("POST", std::string("/bridge/sorrow/") + action, body.dump(), std::move(cb));
}

// Full-path variant for endpoints with query strings already built.
inline void getRaw(const std::string& pathWithQuery, ResponseFn cb) {
    request("GET", pathWithQuery, "", std::move(cb));
}

// ---------- session sync (Discord-started roulettes appearing in-game) ----------

// GET /bridge/sorrow/session?discordId=... -> { active, kind, ... }
inline void fetchSession(ResponseFn cb = nullptr) {
    request("GET", "/bridge/sorrow/session?discordId=" + q(discordId()), "", std::move(cb));
}

// POST /bridge/sorrow/run - percent update for an active solo/group session.
inline void reportRun(const std::string& levelId, const std::string& levelName,
                      int percent, bool completed, bool practice, int attempt,
                      const std::string& groupCode, ResponseFn cb = nullptr) {
    matjson::Value b = matjson::Value::object();
    b.set("discordId", discordId());
    b.set("gdAccountId", gdAccountId());
    b.set("levelId", levelId);
    b.set("levelName", levelName);
    b.set("percent", percent);
    b.set("completed", completed);
    b.set("practice", practice);
    if (attempt > 0) b.set("attempt", attempt);
    if (!groupCode.empty()) b.set("groupCode", groupCode);
    request("POST", "/bridge/sorrow/run", b.dump(), std::move(cb));
}

// ---------- balance / sync ----------
inline std::string urlQuery(const std::string& v) { return q(v); }

// GET /bridge/sorrow/balance?discordId=... (used before the account is linked)
inline void fetchBalance(const std::string& discordId, ResponseFn cb = nullptr) {
    request("GET", "/bridge/sorrow/balance?discordId=" + q(discordId), "", std::move(cb));
}

// GET /bridge/sorrow/balance-sync?gdAccountId=... -> {balance, discordId, linked}
// The BOT balance is the source of truth; the mod mirrors it.
inline void syncBalance(ResponseFn cb = nullptr) {
    request("GET", "/bridge/sorrow/balance-sync?gdAccountId=" + q(gdAccountId()), "", std::move(cb));
}

// ---------- glimmer ----------

// POST /bridge/sorrow/award  (one legit level completion)
// payload: gdAccountId, levelId, levelName, stars, coins, isDemon, levelLength, completed
// response: {awarded, balance, glimmerDelta, demonlist, repeat, reason}
inline void awardGlimmer(int stars, int coins, bool isDemon, int levelLength,
                         const std::string& levelId, const std::string& levelName,
                         ResponseFn cb = nullptr) {
    matjson::Value j = matjson::Value::object();
    j.set("gdAccountId", gdAccountId());
    j.set("levelId", levelId);
    j.set("levelName", levelName);
    j.set("stars", stars);
    j.set("coins", coins);
    j.set("isDemon", isDemon);
    j.set("levelLength", levelLength);
    j.set("completed", true);
    request("POST", "/bridge/sorrow/award", j.dump(), std::move(cb));
}

// POST /bridge/sorrow/credit  (chest rewards and other direct payouts)
// payload: gdAccountId, amount, source
// response: {credited, balance} - same shape the award handler mirrors from.
inline void creditGlimmer(int amount, const std::string& source, ResponseFn cb = nullptr) {
    matjson::Value j = matjson::Value::object();
    j.set("gdAccountId", gdAccountId());
    j.set("amount", amount);
    j.set("source", source);
    request("POST", "/bridge/sorrow/credit", j.dump(), std::move(cb));
}

// ---------- bets ----------

// POST /bridge/sorrow/bet/start {gdAccountId, levelId, levelName, amount}
// response: {accepted, balance}
inline void betStart(int amount, const std::string& levelId, const std::string& levelName,
                     ResponseFn cb = nullptr) {
    matjson::Value j = matjson::Value::object();
    j.set("discordId", discordId());
    j.set("gdAccountId", gdAccountId());
    j.set("levelId", levelId);
    j.set("levelName", levelName);
    j.set("amount", amount);
    request("POST", "/bridge/sorrow/bet/start", j.dump(), std::move(cb));
}

// POST /bridge/sorrow/bet/resolve {gdAccountId, levelId, won, percent}
// response: {settled, won, payout, balance, glimmerDelta}
inline void betResolve(const std::string& levelId, bool won, int percent,
                       ResponseFn cb = nullptr) {
    matjson::Value j = matjson::Value::object();
    j.set("discordId", discordId());
    j.set("gdAccountId", gdAccountId());
    j.set("levelId", levelId);
    j.set("won", won);
    if (percent > 0) j.set("percent", percent);
    request("POST", "/bridge/sorrow/bet/resolve", j.dump(), std::move(cb));
}

// ---------- roulette lists (bot parity incl. demonlist) ----------

// GET /bridge/sorrow/roulette/level?source=&difficulty=&rarity=
// response: {level:{id,name,author,difficulty,stars,length,demon,coins,source,position,thumbnail},source}
inline void rouletteLevel(const std::string& source, const std::string& difficulty,
                          const std::string& rarity, ResponseFn cb = nullptr) {
    std::string path = "/bridge/sorrow/roulette/level?source=" + q(source);
    if (!difficulty.empty()) path += "&difficulty=" + q(difficulty);
    if (!rarity.empty()) path += "&rarity=" + q(rarity);
    request("GET", path, "", std::move(cb));
}

// ---------- public rooms ----------

// ---------- legacy death/run reporting (HUD + /gd stats) ----------

// POST /bridge/sorrow/death {gdAccountId, levelId, levelName, percent, attempt}
inline void reportDeath(const std::string& levelId, const std::string& levelName,
                        int percent, int attempt, ResponseFn cb = nullptr) {
    matjson::Value j = matjson::Value::object();
    j.set("discordId", discordId());
    j.set("gdAccountId", gdAccountId());
    j.set("levelId", levelId);
    j.set("levelName", levelName);
    j.set("percent", percent);
    j.set("attempt", attempt);
    request("POST", "/bridge/sorrow/death", j.dump(), std::move(cb));
}

// POST /bridge/sorrow/run {gdAccountId, levelId, levelName, percent, completed, duration}
inline void reportRun(const std::string& levelId, const std::string& levelName,
                      int percent, bool completed, double duration, ResponseFn cb = nullptr) {
    matjson::Value j = matjson::Value::object();
    j.set("discordId", discordId());
    j.set("gdAccountId", gdAccountId());
    j.set("levelId", levelId);
    j.set("levelName", levelName);
    j.set("percent", percent);
    j.set("completed", completed);
    j.set("duration", duration);
    request("POST", "/bridge/sorrow/run", j.dump(), std::move(cb));
}

// GET /bridge/sorrow/profile?gdAccountId=...
// response: {linked, discordId, name, balance}
inline void fetchProfile(ResponseFn cb = nullptr) {
    request("GET", "/bridge/sorrow/profile?gdAccountId=" + q(gdAccountId()), "", std::move(cb));
}

// Resolve (and cache) the Discord user linked to this GD account via /gd link.
// This is the ONLY identity source now - no mod settings involved. Call at
// startup and whenever the Rooms UI opens (picks up a fresh /gd link).
inline void resolveIdentity(ResponseFn cb = nullptr) {
    if (gdAccountId().empty() || gdAccountId() == "0") return;
    fetchProfile([cb](bool ok, int code, matjson::Value body) {
        sorrow::identityResolved() = true;
        sorrow::discordIdCache().clear();
        if (ok && body.isObject() && body["linked"].asBool().unwrapOr(false)) {
            auto id = body["discordId"].asString();
            if (id && !id.unwrap().empty()) sorrow::discordIdCache() = id.unwrap();
        }
        if (cb) cb(ok, code, std::move(body));
    });
}

// GET /bridge/sorrow/session?gdAccountId=... (solo roulette handoff)
inline void fetchSoloSession(ResponseFn cb = nullptr) {
    request("GET", "/bridge/sorrow/session?gdAccountId=" + q(gdAccountId()), "", std::move(cb));
}

} // namespace sorrow

// Alias for the vendored GD-Roulette code (global scope).
namespace Bridge = sorrow;
