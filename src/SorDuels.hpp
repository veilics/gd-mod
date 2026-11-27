// Sorrow Integration - Versus duels (GD's Versus button on the Create page).
//
// The bot owns every duel (gd.js duels/*). The mod side:
//   * duels::state()   - mirror of the last duels/state reply
//   * DuelDirector     - polls (1.2 s in a duel, 10 s otherwise for invites),
//                        announces invites / rounds / results, takes you to
//                        each round's level and presses Play at GO
//   * SorDuelHud       - in-level VS panel: round, score pips, everyone's
//                        best %, the clock, and the 3-2-1-GO overlay
//   * reportProgress   - every death / completion on the round level
//   * Globed           - only duel players are visible, instant re-poll pings,
//                        WIN / LOSS under your icon when the duel ends
#pragma once

#include "SorRooms.hpp"
#include "SorUi.hpp"
#include "Format.hpp"
#include "Bet.hpp"

#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <chrono>

namespace sorrow::duels {

namespace Bridge = ::sorrow;

inline double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
inline int toInt(const matjson::Value& v, int def = 0) {
    if (v.isNumber()) return static_cast<int>(v.asInt().unwrapOr(def));
    if (v.isString()) { try { return std::stoi(v.asString().unwrapOr("")); } catch (...) {} }
    return def;
}
inline std::int64_t toI64(const matjson::Value& v) { return v.isNumber() ? static_cast<std::int64_t>(v.asInt().unwrapOr(0)) : 0; }
inline std::string str(const matjson::Value& v) { return v.asString().unwrapOr(""); }

struct Player {
    std::string uid, name, rank;
    int acc = 0, iconId = 1, c1 = 0, c2 = 3, glow = 0, wins = 0, pct = 0, elo = 1000, best = 0, attempts = 0, timeMs = 0;
    bool ready = false, done = false, forfeited = false, loaded = false;
    std::int64_t side = 0;
};
struct RoundRow { int n = 0; std::string levelName, winner, winnerName, reason; int winnerPct = 0, timeMs = 0; };
struct Event { int seq = 0; std::string kind, text, from, emote; };
struct Result {
    bool has = false;
    std::string winnerId, winnerName, reason;
    std::int64_t pot = 0, myPayout = 0, myStake = 0, mySidePayout = 0, mySideStake = 0;
    int eloBefore = 0, eloAfter = 0;
    bool eloMoved = false;
};
struct Duel {
    bool valid = false;
    std::string id, kind, state, mode, modeName, difficulty, rarity, name, hostId, hostName, invited, invitedName, me;
    int bestOf = 1, target = 50, seconds = 240, maxPlayers = 2, round = 0;
    bool ranked = false, fixedLevel = false, queued = false;
    std::string tourId;
    int tourRound = 0;
    std::int64_t stake = 0, pot = 0, sidePool = 0, myBetAmount = 0;
    std::string myBetOn;
    int levelId = 0, levelStars = 0;
    std::string levelName, levelAuthor, levelDiff;
    double startAt = 0, endsAt = 0, nextAt = 0;   // local steady-clock seconds
    std::vector<Player> players;
    std::vector<RoundRow> rounds;
    std::vector<Event> events;
    Result result;
    bool hasH2h = false;
    int h2hW = 0, h2hL = 0, h2hD = 0;

    bool isMine() const { for (auto& p : players) if (p.uid == me) return true; return false; }
    const Player* find(const std::string& uid) const { for (auto& p : players) if (p.uid == uid) return &p; return nullptr; }
    const Player* opponent() const { for (auto& p : players) if (p.uid != me) return &p; return nullptr; }
    bool live() const { return state == "lobby" || state == "intermission" || state == "loading" || state == "countdown" || state == "playing"; }
    bool racing() const { return state == "loading" || state == "countdown" || state == "playing"; }
    bool bettingOpen() const { return (state == "invite" || state == "lobby" || state == "intermission" || state == "loading" || state == "countdown") && round == 0; }
    std::string summary() const {
        std::string s = modeName.empty() ? mode : modeName;
        if (mode == "race") s += fmt::format(" to {}%", target);
        s += fmt::format(" - Bo{}", bestOf);
        return s;
    }
};
inline std::string fmtTime(int ms) {
    const int s = ms / 1000;
    return fmt::format("{}:{:02}.{}", s / 60, s % 60, (ms % 1000) / 100);
}
struct Match { std::string a, aName, b, bName, winner, duelId, state, score; };
struct Tour {
    bool valid = false, mine = false;
    std::string id, name, state, hostId, hostName, mode, modeName, difficulty, rarity, roundName, winnerId, winnerName;
    int size = 8, bestOf = 1, target = 50, round = 0;
    std::int64_t entry = 0, pot = 0, myPrize = 0;
    int myPlace = 0;
    std::vector<Player> players;
    std::vector<std::vector<Match>> bracket;
    std::vector<std::string> events;
};
struct Hist { std::string id, mode, kind, opp, result, score; std::int64_t net = 0; int elo = 0; };
struct Me {
    std::string uid, rank = "Bronze";
    int elo = 1000, w = 0, l = 0, d = 0, games = 0, streak = 0, bestStreak = 0, position = 0;
    int peakElo = 1000, seasonGames = 0, tourWins = 0;
    std::string peakRank = "Bronze";
    std::int64_t earned = 0, balance = -1;
    struct Reward { bool has = false; std::string seasonName, rank; int place = 0, elo = 0; std::int64_t amount = 0; } reward;
};
struct Queue { bool active = false, ranked = true; std::int64_t stake = 0; double since = 0; int window = 0, searching = 0; };
struct Season { std::string name; double endsAt = 0; };
struct State {
    bool loaded = false, linked = true;
    std::string error;
    Me me;
    Duel duel, watching;
    std::vector<Duel> invites, open, live;
    std::vector<Hist> history;
    Queue queue;
    Tour myTour;
    std::vector<Tour> tours;
    Season season;
    std::string keepId;     // finished duel to keep showing / spectated duel to follow
};
inline State& state() { static State s; return s; }
inline std::function<void()>& onChanged() { static std::function<void()> f; return f; }
inline void changed() { if (onChanged()) onChanged()(); }

inline Duel parseDuel(const matjson::Value& v) {
    Duel d;
    if (!v.isObject()) return d;
    const double t = nowSec();
    d.valid = true;
    d.id = str(v["id"]); d.kind = str(v["kind"]); d.state = str(v["state"]); d.mode = str(v["mode"]);
    d.modeName = str(v["modeName"]); d.difficulty = str(v["difficulty"]); d.rarity = str(v["rarity"]);
    d.name = str(v["name"]); d.hostId = str(v["hostId"]); d.hostName = str(v["hostName"]);
    d.invited = str(v["invited"]); d.invitedName = str(v["invitedName"]); d.me = str(v["me"]);
    d.bestOf = toInt(v["bestOf"], 1); d.target = toInt(v["target"], 50); d.seconds = toInt(v["seconds"], 240);
    d.maxPlayers = toInt(v["maxPlayers"], 2); d.round = toInt(v["round"]);
    d.ranked = v["ranked"].asBool().unwrapOr(false); d.fixedLevel = v["fixedLevel"].asBool().unwrapOr(false);
    d.stake = toI64(v["stake"]); d.pot = toI64(v["pot"]); d.sidePool = toI64(v["sidePool"]);
    d.queued = v["queued"].asBool().unwrapOr(false); d.tourId = str(v["tourId"]); d.tourRound = toInt(v["tourRound"]);
    if (v["myBet"].isObject()) { d.myBetOn = str(v["myBet"]["on"]); d.myBetAmount = toI64(v["myBet"]["amount"]); }
    if (v["level"].isObject()) {
        d.levelId = toInt(v["level"]["id"]); d.levelName = str(v["level"]["name"]);
        d.levelAuthor = str(v["level"]["author"]); d.levelDiff = str(v["level"]["difficulty"]);
        d.levelStars = toInt(v["level"]["stars"]);
    }
    const auto at = [&](const char* k) { const auto ms = toI64(v[k]); return ms == 0 ? 0.0 : t + ms / 1000.0; };
    d.startAt = at("startIn"); d.endsAt = at("endsIn"); d.nextAt = at("nextIn");
    if (auto arr = v["players"].asArray())
        for (const auto& p : *arr) {
            Player pl;
            pl.uid = str(p["uid"]); pl.name = str(p["name"]); pl.rank = str(p["rank"]);
            pl.acc = toInt(p["gdAccountId"]); pl.iconId = toInt(p["iconId"], 1); pl.c1 = toInt(p["color1"]);
            pl.c2 = toInt(p["color2"], 3); pl.glow = toInt(p["glow"]); pl.wins = toInt(p["wins"]); pl.pct = toInt(p["pct"]);
            pl.elo = toInt(p["elo"], 1000); pl.ready = p["ready"].asBool().unwrapOr(false);
            pl.done = p["done"].asBool().unwrapOr(false); pl.forfeited = p["forfeited"].asBool().unwrapOr(false);
            pl.side = toI64(p["side"]);
            pl.best = toInt(p["best"]); pl.attempts = toInt(p["attempts"]); pl.timeMs = toInt(p["time"]);
            pl.loaded = p["loaded"].asBool().unwrapOr(false);
            d.players.push_back(std::move(pl));
        }
    if (auto arr = v["rounds"].asArray())
        for (const auto& r : *arr) {
            RoundRow row;
            row.n = toInt(r["n"]); row.levelName = str(r["level"]["name"]); row.winner = str(r["winner"]);
            row.winnerName = str(r["winnerName"]); row.reason = str(r["reason"]);
            if (!row.winner.empty()) row.winnerPct = toInt(r["results"][row.winner]);
            row.timeMs = toInt(r["time"]);
            d.rounds.push_back(std::move(row));
        }
    if (auto arr = v["events"].asArray())
        for (const auto& e : *arr) d.events.push_back({ toInt(e["seq"]), str(e["kind"]), str(e["text"]), str(e["from"]), str(e["emote"]) });
    if (v["h2h"].isObject()) { d.hasH2h = true; d.h2hW = toInt(v["h2h"]["w"]); d.h2hL = toInt(v["h2h"]["l"]); d.h2hD = toInt(v["h2h"]["d"]); }
    if (v["result"].isObject()) {
        const auto& r = v["result"];
        auto& o = d.result;
        o.has = true;
        o.winnerId = str(r["winnerId"]); o.winnerName = str(r["winnerName"]); o.reason = str(r["reason"]);
        o.pot = toI64(r["pot"]);
        o.myPayout = toI64(r["payouts"][d.me]); o.myStake = toI64(r["stakes"][d.me]);
        o.mySidePayout = toI64(r["sidePayouts"][d.me]); o.mySideStake = toI64(r["sideStakes"][d.me]);
        if (r["elo"][d.me].isObject()) {
            o.eloMoved = true;
            o.eloBefore = toInt(r["elo"][d.me]["before"]); o.eloAfter = toInt(r["elo"][d.me]["after"]);
        }
    }
    return d;
}

inline Tour parseTour(const matjson::Value& v) {
    Tour t;
    if (!v.isObject()) return t;
    t.valid = true;
    t.id = str(v["id"]); t.name = str(v["name"]); t.state = str(v["state"]); t.hostId = str(v["hostId"]); t.hostName = str(v["hostName"]);
    t.mode = str(v["mode"]); t.modeName = str(v["modeName"]); t.difficulty = str(v["difficulty"]); t.rarity = str(v["rarity"]); t.roundName = str(v["roundName"]);
    t.winnerId = str(v["winnerId"]); t.winnerName = str(v["winnerName"]); t.mine = v["mine"].asBool().unwrapOr(false);
    t.size = toInt(v["size"], 8); t.bestOf = toInt(v["bestOf"], 1); t.target = toInt(v["target"], 50); t.round = toInt(v["round"]);
    t.entry = toI64(v["entry"]); t.pot = toI64(v["pot"]);
    const auto me = str(v["me"]);
    if (v["placements"][me].isObject()) { t.myPlace = toInt(v["placements"][me]["place"]); t.myPrize = toI64(v["placements"][me]["prize"]); }
    if (auto arr = v["players"].asArray())
        for (const auto& p : *arr) {
            Player pl;
            pl.uid = str(p["uid"]); pl.name = str(p["name"]); pl.rank = str(p["rank"]); pl.elo = toInt(p["elo"], 1000);
            pl.iconId = toInt(p["iconId"], 1); pl.c1 = toInt(p["color1"]); pl.c2 = toInt(p["color2"], 3); pl.glow = toInt(p["glow"]);
            t.players.push_back(std::move(pl));
        }
    if (auto rounds = v["bracket"].asArray())
        for (const auto& r : *rounds) {
            std::vector<Match> row;
            if (auto ms = r.asArray())
                for (const auto& m : *ms)
                    row.push_back({ str(m["a"]), str(m["aName"]), str(m["b"]), str(m["bName"]), str(m["winner"]), str(m["duelId"]), str(m["state"]), str(m["score"]) });
            t.bracket.push_back(std::move(row));
        }
    if (auto ev = v["events"].asArray()) for (const auto& e : *ev) t.events.push_back(str(e["text"]));
    return t;
}

inline void apply(const matjson::Value& b) {
    auto& s = state();
    s.loaded = true;
    s.linked = true;
    s.error.clear();
    const auto& m = b["me"];
    s.me.uid = str(m["uid"]); s.me.rank = str(m["rank"]); s.me.elo = toInt(m["elo"], 1000);
    s.me.w = toInt(m["w"]); s.me.l = toInt(m["l"]); s.me.d = toInt(m["d"]); s.me.games = toInt(m["games"]);
    s.me.streak = toInt(m["streak"]); s.me.bestStreak = toInt(m["bestStreak"]); s.me.position = toInt(m["position"]);
    s.me.earned = toI64(m["earned"]); s.me.balance = m["balance"].isNumber() ? toI64(m["balance"]) : -1;
    s.me.peakElo = toInt(m["peakElo"], s.me.elo); s.me.peakRank = str(m["peakRank"]);
    s.me.seasonGames = toInt(m["seasonGames"]); s.me.tourWins = toInt(m["tourWins"]);
    s.me.reward = {};
    if (m["seasonReward"].isObject()) {
        const auto& r = m["seasonReward"];
        s.me.reward = { true, str(r["seasonName"]), str(r["rank"]), toInt(r["place"]), toInt(r["elo"]), toI64(r["amount"]) };
    }
    const double t0 = nowSec();
    s.queue = {};
    if (b["queue"].isObject()) {
        const auto& q = b["queue"];
        s.queue = { true, q["ranked"].asBool().unwrapOr(true), toI64(q["stake"]), t0 - toI64(q["waited"]) / 1000.0,
            toInt(q["window"]), toInt(q["searching"]) };
    }
    s.myTour = parseTour(b["myTour"]);
    s.tours.clear();
    if (auto a = b["tours"].asArray()) for (const auto& x : *a) s.tours.push_back(parseTour(x));
    s.season = { str(b["season"]["name"]), t0 + toI64(b["season"]["endsIn"]) / 1000.0 };
    s.duel = parseDuel(b["duel"]);
    s.watching = parseDuel(b["watching"]);
    s.invites.clear(); s.open.clear(); s.live.clear(); s.history.clear();
    if (auto a = b["invites"].asArray()) for (const auto& x : *a) s.invites.push_back(parseDuel(x));
    if (auto a = b["open"].asArray()) for (const auto& x : *a) s.open.push_back(parseDuel(x));
    if (auto a = b["live"].asArray()) for (const auto& x : *a) s.live.push_back(parseDuel(x));
    if (auto a = b["history"].asArray())
        for (const auto& h : *a)
            s.history.push_back({ str(h["id"]), str(h["mode"]), str(h["kind"]), str(h["opp"]), str(h["result"]), str(h["score"]),
                toI64(h["net"]), toInt(h["elo"]) });
    if (s.me.balance >= 0) setGlimmer(s.me.balance, 0);
}

inline matjson::Value idBody() {
    auto b = rooms::idBody();
    b.set("name", playerName());
    rooms::addIcon(b);
    return b;
}
inline std::string errOf(const matjson::Value& body, const char* fallback) {
    auto e = body["error"].asString().unwrapOr("");
    return e.empty() ? std::string(fallback) : e;
}

// ---------------------------------------------------------------- levels --
// One RobTop-rolled level kept ready for the duel's difficulty (the bot host
// can be blocked by RobTop, so players' games roll like GD-Roulette does).
struct Cand { std::string key; std::optional<rooms::Candidate> c; bool rolling = false; std::set<std::string> used; };
inline Cand& cand() { static Cand c; return c; }

inline void rollCandidate(const Duel& d) {
    if (!d.valid || d.fixedLevel || !d.isMine()) return;
    auto& c = cand();
    const auto key = fmt::format("{}|{}|{}", d.id, d.difficulty, d.rarity);
    if (c.key != key) { c.key = key; c.c.reset(); c.used.clear(); }
    if (d.levelId > 0) c.used.insert(std::to_string(d.levelId));
    if (c.c && !c.used.contains(c.c->id)) return;
    if (c.rolling) return;
    c.c.reset();
    c.rolling = true;
    const bool demons = d.difficulty.ends_with("demon");
    const auto base = fmt::format("secret={}&type=0&star=1&len=0,1,2,3,4{}{}", rooms::roll::SECRET,
        rooms::roll::diffQuery(d.difficulty, demons), rooms::roll::rarityQuery(d.rarity == "any" ? "" : d.rarity));
    auto pick = [key, base](int pages) {
        rooms::roll::post(fmt::format("{}&page={}", base, rooms::roll::rnd(0, pages - 1)), [key](std::string resp) {
            auto& c = cand();
            c.rolling = false;
            if (c.key != key || resp.empty() || resp == "-1") return;
            auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp);
            if (parsed.isErr()) return;
            auto r = parsed.unwrap();
            std::vector<size_t> ok;
            for (size_t i = 0; i < r.levels.size(); ++i) {
                if (r.levels[i].length > 4) continue;
                if (c.used.contains(std::to_string(r.levels[i].levelID))) continue;
                ok.push_back(i);
            }
            if (ok.empty()) return;
            const auto i = ok[rooms::roll::rnd(0, static_cast<int>(ok.size()) - 1)];
            levels::storeFromResponse(resp, r.levels[i].levelID);
            c.c = rooms::roll::toCandidate(r, i);
        });
    };
    if (auto it = rooms::roll::pageCounts().find(base); it != rooms::roll::pageCounts().end()) return pick(it->second);
    rooms::roll::post(base + "&page=0", [base, pick, key](std::string resp) {
        int pages = 1;
        if (!resp.empty() && resp != "-1")
            if (auto parsed = rtrp::RtResponseParser::parseLevelResponse(resp); parsed.isOk()) {
                const auto& pg = parsed.unwrap().page;
                if (pg.countPerPage > 0) pages = std::max(1, static_cast<int>(std::ceil(pg.pages / static_cast<double>(pg.countPerPage))));
            }
        pages = std::min(pages, 1000);
        rooms::roll::pageCounts()[base] = pages;
        if (cand().key != key) { cand().rolling = false; return; }
        pick(pages);
    });
}
inline void attachCandidate(matjson::Value& b) {
    const auto& c = cand().c;
    if (!c) return;
    matjson::Value j = matjson::Value::object();
    j.set("id", c->id); j.set("name", c->name); j.set("author", c->author);
    j.set("difficulty", c->difficulty); j.set("length", c->length);
    j.set("stars", c->stars); j.set("demon", c->demon);
    b.set("candidate", j);
}

// ---------------------------------------------------------------- globed --
// Tell other mod users in the Globed session that a duel changed (they
// re-poll at once). Reuses the room event with kind 3.
inline void pingGlobed(const std::string& id) {
#if SORROW_GLOBED_HEADERS
    if (!globedAvailable() || id.empty() || !globed::api::game::isActive()) return;
    SorrowRoomEvent ev;
    ev.kind = 3;
    ev.code = id;
    ev.host = playerName();
    ev.send(globed::EventServer::Game);
#else
    (void)id;
#endif
}

// ----------------------------------------------------------------- calls --
inline void poll(const std::function<void()>& done = nullptr);

using DuelCb = std::function<void(bool ok, std::string err)>;
inline void call(const char* path, matjson::Value b, DuelCb cb, bool ping = true) {
    Bridge::post(path, b, [cb, ping, path = std::string(path)](bool ok, int, matjson::Value body) {
        if (!ok) { if (cb) cb(false, errOf(body, "The bot did not answer")); return; }
        const auto d = parseDuel(body["duel"]);
        if (d.valid) {
            if (d.isMine()) { state().duel = d; state().keepId = d.id; }
            if (ping) pingGlobed(d.id);
        }
        changed();
        if (cb) cb(true, "");
        poll();
    });
}

struct CreateOptions {
    std::string kind = "duel", mode = "race", difficulty = "any", rarity = "any";
    std::string oppUid, oppName;
    int bestOf = 1, target = 50, seconds = 240;
    std::int64_t stake = 0;
    bool ranked = true;
    GJGameLevel* level = nullptr;
};
inline void create(const CreateOptions& o, DuelCb cb) {
    auto b = idBody();
    b.set("kind", o.kind); b.set("mode", o.mode); b.set("bestOf", o.bestOf);
    b.set("target", o.target); b.set("seconds", o.seconds); b.set("stake", static_cast<double>(o.stake));
    b.set("difficulty", o.difficulty); b.set("rarity", o.rarity); b.set("ranked", o.ranked);
    if (o.kind == "duel") {
        matjson::Value opp = matjson::Value::object();
        opp.set("discordId", o.oppUid);
        opp.set("name", o.oppName);
        b.set("opponent", opp);
    }
    if (o.level) {
        matjson::Value lv = matjson::Value::object();
        lv.set("id", std::to_string(o.level->m_levelID.value()));
        lv.set("name", std::string(o.level->m_levelName));
        lv.set("author", std::string(o.level->m_creatorName));
        lv.set("stars", o.level->m_stars.value());
        lv.set("demon", o.level->m_demon.value() > 0);
        b.set("level", lv);
    }
    call("duels/create", b, cb);
}
inline void act(const char* path, const std::string& id, DuelCb cb = nullptr) {
    auto b = idBody();
    b.set("id", id);
    if (std::string(path) == "duels/accept") attachCandidate(b);
    call(path, b, cb);
}
inline void ready(const std::string& id, bool r, DuelCb cb = nullptr) {
    auto b = idBody(); b.set("id", id); b.set("ready", r); attachCandidate(b);
    call("duels/ready", b, cb);
}
inline void sideBet(const std::string& id, const std::string& on, std::int64_t amount, DuelCb cb) {
    auto b = idBody(); b.set("id", id); b.set("on", on); b.set("amount", static_cast<double>(amount));
    state().keepId = id; // follow it so the payout shows
    call("duels/bet", b, cb, false);
}

inline void queue(bool ranked, std::int64_t stake, DuelCb cb) {
    auto b = idBody(); b.set("ranked", ranked); b.set("stake", static_cast<double>(stake));
    Bridge::post("duels/queue", b, [cb](bool ok, int, matjson::Value body) {
        if (!ok) { if (cb) cb(false, errOf(body, "The bot did not answer")); return; }
        poll();
        if (cb) cb(true, "");
    });
}
inline void emote(const std::string& id, const std::string& key) {
    auto b = idBody(); b.set("id", id); b.set("emote", key);
    Bridge::post("duels/emote", b, [](bool ok, int, matjson::Value body) {
        if (!ok) { Notification::create(errOf(body, "Could not send").c_str(), NotificationIcon::Warning, 1.f)->show(); return; }
        const auto d = parseDuel(body["duel"]);
        if (d.valid && d.isMine()) state().duel = d;
        changed();
    });
}
inline void unqueue() {
    state().queue = {};
    Bridge::post("duels/unqueue", idBody(), [](bool, int, matjson::Value) { poll(); });
    changed();
}
inline void ackSeason() {
    auto b = idBody(); b.set("what", "season");
    state().me.reward = {};
    Bridge::post("duels/ack", b, nullptr);
}
struct TourOptions { std::string mode = "race", difficulty = "any", rarity = "any", title; int size = 8, bestOf = 1, target = 50, seconds = 240; std::int64_t entry = 0; };
inline void tourCreate(const TourOptions& o, DuelCb cb) {
    auto b = idBody();
    b.set("mode", o.mode); b.set("difficulty", o.difficulty); b.set("rarity", o.rarity); b.set("size", o.size); b.set("bestOf", o.bestOf);
    b.set("target", o.target); b.set("seconds", o.seconds); b.set("entry", static_cast<double>(o.entry)); b.set("title", o.title);
    Bridge::post("tours/create", b, [cb](bool ok, int, matjson::Value body) {
        if (cb) cb(ok, ok ? "" : errOf(body, "The bot did not answer"));
        poll();
    });
}
inline void tourAct(const char* path, const std::string& id, DuelCb cb = nullptr) {
    auto b = idBody(); b.set("id", id);
    Bridge::post(path, b, [cb](bool ok, int, matjson::Value body) {
        if (cb) cb(ok, ok ? "" : errOf(body, "The bot did not answer"));
        poll();
    });
}

struct FoundPlayer { std::string uid, name, rank; int acc = 0, elo = 1000, w = 0, l = 0; bool nearby = false, busy = false; };
inline void find(const std::string& query, std::function<void(std::vector<FoundPlayer>)> cb) {
    auto b = idBody();
    b.set("query", query);
    matjson::Value accs = matjson::Value::array();
    int self = 0;
    try { self = std::stoi(gdAccountId()); } catch (...) {}
    for (int a : sessionMemberIds()) if (a > 0 && a != self) accs.push(a);
    b.set("accounts", accs);
    Bridge::post("duels/find", b, [cb](bool ok, int, matjson::Value body) {
        std::vector<FoundPlayer> out;
        if (ok) if (auto a = body["players"].asArray())
            for (const auto& p : *a)
                out.push_back({ str(p["uid"]), str(p["name"]), str(p["rank"]), toInt(p["gdAccountId"]), toInt(p["elo"], 1000),
                    toInt(p["w"]), toInt(p["l"]), p["near"].asBool().unwrapOr(false), p["busy"].asBool().unwrapOr(false) });
        cb(std::move(out));
    });
}
struct BoardRow { int pos = 0, elo = 1000, w = 0, l = 0, d = 0, streak = 0; std::string name, rank; bool me = false; };
inline void leaderboard(std::function<void(std::vector<BoardRow>)> cb) {
    Bridge::post("duels/leaderboard", idBody(), [cb](bool ok, int, matjson::Value body) {
        std::vector<BoardRow> out;
        if (ok) if (auto a = body["rows"].asArray())
            for (const auto& r : *a)
                out.push_back({ toInt(r["pos"]), toInt(r["elo"], 1000), toInt(r["w"]), toInt(r["l"]), toInt(r["d"]), toInt(r["streak"]),
                    str(r["name"]), str(r["rank"]), r["me"].asBool().unwrapOr(false) });
        cb(std::move(out));
    });
}

inline bool& polling() { static bool b = false; return b; }
inline void poll(const std::function<void()>& done) {
    if (polling()) { if (done) done(); return; }
    if (discordId().empty() && gdAccountId().empty()) { if (done) done(); return; }
    polling() = true;
    auto b = idBody();
    if (!state().keepId.empty()) b.set("id", state().keepId);
    attachCandidate(b);
    Bridge::post("duels/state", b, [done](bool ok, int status, matjson::Value body) {
        polling() = false;
        if (ok) apply(body);
        else {
            state().loaded = true;
            state().error = errOf(body, "Could not reach the Sorrow bot");
            if (status == 401) state().linked = false;
        }
        changed();
        if (done) done();
    });
}

// The level of the current round, if you are racing it.
inline int roundLevel() {
    const auto& d = state().duel;
    return d.valid && d.isMine() && d.racing() ? d.levelId : 0;
}

inline void reportProgress(int levelId, int percent, bool completed) {
    const auto& d = state().duel;
    if (!d.valid || !d.isMine() || !d.racing() || d.levelId != levelId) return;
    auto b = idBody();
    b.set("id", d.id); b.set("levelId", std::to_string(levelId)); b.set("percent", percent); b.set("completed", completed);
    attachCandidate(b);
    const int round = d.round;
    const auto id = d.id;
    Bridge::post("duels/progress", b, [round, id](bool ok, int, matjson::Value body) {
        if (!ok) return;
        const auto nd = parseDuel(body["duel"]);
        if (nd.valid) state().duel = nd;
        if (nd.round != round || nd.state != "playing") pingGlobed(id);
        changed();
    });
}

// ------------------------------------------------------------- the screen --
inline std::function<void(std::string, std::string)>& rankUpHook() { static std::function<void(std::string, std::string)> f; return f; }
inline std::function<void()>& seasonHook() { static std::function<void()> f; return f; }

// Set by SorDuelLayer: opens the Versus screen (used after a duel ends).
inline std::function<void()>& openScreenHook() { static std::function<void()> f; return f; }
inline bool& screenOpen() { static bool b = false; return b; }

// GD account ids of the players in your live duel (for the Globed filter).
inline std::vector<int> visibleAccounts() {
    std::vector<int> out;
    const auto& d = state().duel;
    if (d.valid && d.isMine() && d.live()) for (auto& p : d.players) if (p.acc > 0) out.push_back(p.acc);
    return out;
}

inline void fairTick(float dt); // defined below (fair start)

// --------------------------------------------------------------- director --
class DuelDirector : public cocos2d::CCObject {
public:
    static DuelDirector* get() {
        static DuelDirector* s = [] {
            auto* d = new DuelDirector();
            rooms::extraVisibleAccounts() = [] { return visibleAccounts(); };
            cocos2d::CCDirector::get()->getScheduler()->scheduleSelector(
                schedule_selector(DuelDirector::tick), d, 0.25f, false);
            return d;
        }();
        return s;
    }
    void pokeNow() { m_wait = 0.f; }

    void tick(float dt) {
        // travel / auto-play
        if (m_travelIn > 0.f) { m_travelIn -= dt; if (m_travelIn <= 0.f) { m_travelIn = 0.f; this->goToLevel(); } }
        if (m_returnIn > 0.f) { m_returnIn -= dt; if (m_returnIn <= 0.f) { m_returnIn = 0.f; this->returnToScreen(); } }
        fairTick(dt);

        const auto& d = state().duel;
        const bool inDuel = d.valid && d.isMine() && (d.live() || d.state == "invite");
        if (inDuel) rollCandidate(d);
        if (m_busy) return;
        m_wait -= dt;
        if (m_wait > 0.f) return;
        const bool busy = inDuel || screenOpen() || !state().keepId.empty() || state().queue.active
            || (state().myTour.valid && state().myTour.state == "running");
        m_wait = busy ? 1.2f : 10.f;
        m_busy = true;
        const Duel before = state().duel;
        std::set<std::string> knownInvites;
        for (auto& i : state().invites) knownInvites.insert(i.id);
        const bool first = !state().loaded;
        const std::string rankBefore = state().me.rank;
        const int eloBefore = state().me.elo;
        poll([this, before, knownInvites, first, rankBefore, eloBefore] {
            m_busy = false;
            this->diff(before, state().duel, knownInvites, first);
            auto& me = state().me;
            if (!first && state().loaded && !rankBefore.empty() && me.rank != rankBefore && me.elo > eloBefore && rankUpHook())
                rankUpHook()(rankBefore, me.rank);
            if (me.reward.has && !m_rewardShown && seasonHook()) { m_rewardShown = true; seasonHook()(); }
        });
    }

private:
    void diff(const Duel& a, const Duel& b, const std::set<std::string>& knownInvites, bool first) {
        auto& s = state();
        // new invites
        if (!first)
            for (auto& inv : s.invites)
                if (!knownInvites.contains(inv.id) && !m_announced.contains(inv.id)) {
                    m_announced.insert(inv.id);
                    Notification::create(fmt::format("{} challenged you to a duel! ({}{}) - Create > Versus",
                        inv.hostName, inv.summary(), inv.stake > 0 ? ", " + sorrow::formatGlimmer(inv.stake) : "").c_str(),
                        NotificationIcon::Info, 4.f)->show();
                    playGlimmerSound(1.4f, 0.8f);
                }
        // matchmaking found someone / tournament match ready
        if (b.valid && b.isMine() && (!a.valid || a.id != b.id) && (b.queued || !b.tourId.empty()) && b.state != "finished") {
            const auto* opp = b.opponent();
            Notification::create(fmt::format("{} - vs {}", b.queued ? "Match found!" : "Your tournament match is ready",
                opp ? opp->name : std::string("?")).c_str(), NotificationIcon::Success, 3.f)->show();
            playGlimmerSound(1.5f, 1.f);
        }
        if (!b.valid || !b.isMine()) {
            if (a.valid && a.isMine() && a.state == "invite") {
                // our invite disappeared without being accepted
                Notification::create("Your duel invite was declined or expired", NotificationIcon::Info, 2.5f)->show();
                if (s.keepId == a.id) s.keepId.clear();
            }
            this->spectatorResult(s.watching);
            return;
        }
        // event feed -> notifications (not our own actions; not when the screen shows them)
        int maxSeq = m_seenEvent;
        const bool sameDuel = a.valid && a.id == b.id;
        for (auto& e : b.events) {
            if (e.seq <= m_seenEvent || !sameDuel) { maxSeq = std::max(maxSeq, e.seq); continue; }
            maxSeq = std::max(maxSeq, e.seq);
            if (e.kind == "emote" && e.from != state().me.uid) {
                Notification::create(e.text.c_str(), NotificationIcon::None, 2.f)->show();
                continue;
            }
            if (screenOpen()) continue;
            if (e.kind == "accept" || e.kind == "join" || e.kind == "leave" || e.kind == "forfeit" || e.kind == "closed")
                Notification::create(e.text.c_str(), NotificationIcon::Info, 2.5f)->show();
        }
        m_seenEvent = sameDuel ? maxSeq : (b.events.empty() ? 0 : b.events.back().seq);

        // a round was decided
        if (sameDuel && b.rounds.size() > a.rounds.size() && !b.rounds.empty()) {
            const auto& r = b.rounds.back();
            const bool mine = r.winner == s.me.uid;
            Notification::create(r.winner.empty() ? fmt::format("Round {}: no winner", r.n).c_str()
                : mine ? fmt::format("You win round {}! ({}%)", r.n, r.winnerPct).c_str()
                       : fmt::format("{} wins round {} ({}%)", r.winnerName, r.n, r.winnerPct).c_str(),
                mine ? NotificationIcon::Success : NotificationIcon::Info, 2.5f)->show();
            if (mine) playGlimmerSound(1.2f, 0.9f);
        }
        // a new round level: go there
        if (b.racing() && b.levelId > 0 && (b.round != m_seenRound || b.id != m_seenId)) {
            m_seenRound = b.round;
            m_seenId = b.id;
            m_travelIn = 0.05f;
        }
        // finished
        if (b.state == "finished" && b.result.has && m_presented != b.id) {
            m_presented = b.id;
            this->presentResult(b);
        }
    }

    void presentResult(const Duel& d) {
        const auto& r = d.result;
        const bool won = r.winnerId == state().me.uid;
        const bool draw = r.winnerId.empty();
        const Player* me = d.find(state().me.uid);
        const int myPct = me ? me->pct : 0;
        if (PlayLayer::get()) announceBetResult(won, myPct); // Globed WIN / LOSS label
        std::string text = draw ? "The duel is a draw" : won ? "You won the duel!" : fmt::format("{} won the duel", r.winnerName);
        if (r.eloMoved) text += fmt::format(" ({}{} Elo)", r.eloAfter - r.eloBefore >= 0 ? "+" : "", r.eloAfter - r.eloBefore);
        Notification::create(text.c_str(), draw ? NotificationIcon::Info : won ? NotificationIcon::Success : NotificationIcon::Error, 3.5f)->show();
        const auto payout = r.myPayout, stake = r.myStake;
        syncBalance([won, draw, payout, stake, myPct](bool ok, int, matjson::Value bb) {
            const auto bal = ok && bb.isObject() ? bb["balance"].asInt().unwrapOr(-1) : -1;
            geode::queueInMainThread([=] {
                if (bal >= 0) setGlimmer(bal, 0);
                if (won && payout > 0) presentGlimmerWin(payout, stake, nullptr);
                else if (!won && !draw && stake > 0) presentBetLoss(stake, myPct, 0, nullptr);
            });
        });
        // back to the Versus screen - only from the duel's level / level page
        auto* lil = this->levelInfo();
        const bool onDuelPage = lil && lil->m_level && static_cast<int>(lil->m_level->m_levelID.value()) == d.levelId;
        if (PlayLayer::get()) m_returnIn = 4.8f;
        else if (onDuelPage) m_returnIn = 0.6f;
    }

    void spectatorResult(const Duel& w) {
        if (!w.valid || w.state != "finished" || !w.result.has || m_presented == w.id) return;
        if (w.result.mySideStake <= 0) return;
        m_presented = w.id;
        const auto pay = w.result.mySidePayout;
        const bool won = pay > w.result.mySideStake;
        Notification::create(won ? fmt::format("Your side bet won! +{}", sorrow::formatGlimmer(pay)).c_str()
            : pay > 0 ? "Side bet refunded" : "Your side bet lost", won ? NotificationIcon::Success : NotificationIcon::Info, 3.f)->show();
        if (won) presentGlimmerWin(pay, w.result.mySideStake, nullptr);
        refreshBalance();
        state().keepId.clear();
    }

    void goToLevel() {
        const auto& d = state().duel;
        if (!d.valid || !d.racing() || d.levelId <= 0) return;
        if (auto* pl = PlayLayer::get()) {
            if (pl->m_level && static_cast<int>(pl->m_level->m_levelID.value()) == d.levelId) return;
            if (m_quitRequested) { m_travelIn = 0.4f; return; }
            m_quitRequested = true;
            pl->onQuit();
            m_travelIn = 1.1f;
            return;
        }
        m_quitRequested = false;
        if (auto* lil = this->levelInfo(); lil && lil->m_level && static_cast<int>(lil->m_level->m_levelID.value()) == d.levelId) return;
        if (m_opening) return;
        m_opening = true;
        const int id = d.levelId;
        const auto name = d.levelName;
        levels::fetchById(id, [this, id, name](GJGameLevel* lvl) {
            m_opening = false;
            if (state().duel.levelId != id || PlayLayer::get()) return;
            levels::openLevelPage(lvl ? lvl : levels::bareLevel(id, name));
        });
    }

    LevelInfoLayer* levelInfo() {
        auto* scene = cocos2d::CCDirector::get()->getRunningScene();
        return scene ? scene->getChildByType<LevelInfoLayer>(0) : nullptr;
    }

    void returnToScreen() {
        if (auto* pl = PlayLayer::get()) {
            if (!m_quitRequested) { m_quitRequested = true; pl->onQuit(); }
            m_returnIn = 0.5f;
            return;
        }
        m_quitRequested = false;
        if (screenOpen()) return;
        if (openScreenHook()) openScreenHook()();
    }

    float m_wait = 0.f, m_travelIn = 0.f, m_returnIn = 0.f;
    bool m_busy = false, m_quitRequested = false, m_opening = false, m_rewardShown = false;
    int m_seenEvent = 0, m_seenRound = -1;
    std::string m_seenId, m_presented;
    std::set<std::string> m_announced;
};

// Globed: instant re-poll when a duel-mate pings.
inline void listenGlobed() {
#if SORROW_GLOBED_HEADERS
    if (!globedAvailable()) return;
    static bool done = false;
    if (done) return;
    done = true;
    registerGlobedEvents();
    SorrowRoomEvent::listen([](const SorrowRoomEvent& ev) {
        if (ev.kind == 3) DuelDirector::get()->pokeNow();
    }).leak();
#endif
}

// ------------------------------------------------------------ in-level hud --
inline cocos2d::ccColor3B rankColor(const std::string& r) {
    if (r == "Champion") return { 255, 90, 200 };
    if (r == "Diamond") return { 110, 170, 255 };
    if (r == "Platinum") return { 120, 230, 220 };
    if (r == "Gold") return { 255, 205, 60 };
    if (r == "Silver") return { 205, 210, 225 };
    return { 215, 140, 70 };
}

class SorDuelHud : public cocos2d::CCNode {
public:
    static SorDuelHud* create(int levelId) {
        auto* n = new SorDuelHud();
        n->m_levelId = levelId;
        n->autorelease();
        n->setID("sor-duel-hud"_spr);
        n->schedule(schedule_selector(SorDuelHud::tick), 0.1f);
        n->tick(0.f);
        return n;
    }

    void tick(float) {
        const auto& d = state().duel;
        const bool show = d.valid && d.isMine() && d.levelId == m_levelId && (d.racing() || d.state == "intermission" || d.state == "finished");
        this->setVisible(show);
        if (!show) return;
        const auto key = fmt::format("{}|{}|{}|", d.state, d.round, d.players.size()) + [&] {
            std::string k; for (auto& p : d.players) k += fmt::format("{}:{}:{}:{}|", p.uid, p.wins, p.pct, p.done); return k; }();
        if (key != m_key) { m_key = key; this->rebuild(d); }
        this->updateClock(d);
    }

private:
    void rebuild(const Duel& d) {
        this->removeAllChildren();
        m_clock = nullptr; m_big = nullptr;
        const auto win = cocos2d::CCDirector::get()->getWinSize();
        const float w = 150.f;
        const int rows = std::min<int>(static_cast<int>(d.players.size()), 6);
        const float h = 22.f + rows * 15.f;
        auto* root = cocos2d::CCNode::create();
        root->setPosition({ win.width - w - 6.f, win.height - h - 28.f });
        this->addChild(root);
        auto* bg = sorrow::ui::panel(w, h, { 0, 0, 0 }, 120);
        bg->setAnchorPoint({ 0.f, 0.f });
        root->addChild(bg);

        auto* head = CCLabelBMFont::create(fmt::format("VS  R{}/{}  {}", std::max(1, d.round), d.bestOf,
            d.mode == "race" ? fmt::format("RACE {}%", d.target) : d.mode == "sudden" ? std::string("ONE LIFE")
                : d.mode == "consistency" ? std::string("AVG %") : std::string("SHOWDOWN")).c_str(), "bigFont.fnt");
        head->setScale(0.28f);
        head->setAnchorPoint({ 0.f, 0.5f });
        head->setPosition({ 6.f, h - 9.f });
        head->setColor({ 255, 210, 90 });
        root->addChild(head);
        m_clock = CCLabelBMFont::create("", "bigFont.fnt");
        m_clock->setScale(0.28f);
        m_clock->setAnchorPoint({ 1.f, 0.5f });
        m_clock->setPosition({ w - 6.f, h - 9.f });
        root->addChild(m_clock);

        const int need = d.bestOf / 2 + 1;
        auto sorted = d.players;
        std::stable_sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.wins > b.wins || (a.wins == b.wins && a.pct > b.pct); });
        for (int i = 0; i < rows; ++i) {
            const auto& p = sorted[i];
            const float y = h - 24.f - i * 15.f;
            const bool me = p.uid == state().me.uid;
            if (auto* ic = makeIcon(p)) { ic->setScale(0.32f); ic->setPosition({ 12.f, y }); root->addChild(ic); }
            auto* nm = CCLabelBMFont::create(p.name.c_str(), "bigFont.fnt");
            nm->limitLabelWidth(50.f, 0.3f, 0.1f);
            nm->setAnchorPoint({ 0.f, 0.5f });
            nm->setPosition({ 22.f, y });
            nm->setColor(p.forfeited ? cocos2d::ccColor3B { 130, 130, 130 } : me ? cocos2d::ccColor3B { 140, 255, 170 } : cocos2d::ccColor3B { 255, 255, 255 });
            root->addChild(nm);
            // win pips
            for (int k = 0; k < need; ++k) {
                auto* pip = cocos2d::CCLayerColor::create(k < p.wins ? cocos2d::ccColor4B { 255, 205, 60, 255 } : cocos2d::ccColor4B { 255, 255, 255, 50 }, 4.f, 4.f);
                pip->setPosition({ 76.f + k * 6.f, y - 2.f });
                root->addChild(pip);
            }
            // best % bar
            auto* barBg = cocos2d::CCLayerColor::create({ 255, 255, 255, 35 }, 34.f, 4.f);
            barBg->setPosition({ 76.f + need * 6.f + 2.f, y - 2.f });
            root->addChild(barBg);
            auto* bar = cocos2d::CCLayerColor::create(me ? cocos2d::ccColor4B { 120, 255, 150, 255 } : cocos2d::ccColor4B { 255, 160, 90, 255 },
                34.f * std::clamp(p.pct, 0, 100) / 100.f, 4.f);
            bar->setPosition(barBg->getPosition());
            root->addChild(bar);
            const std::string pcText = d.mode == "sudden" && !p.done ? std::string("...")
                : d.mode == "consistency" ? fmt::format("{}% x{}", p.pct, p.attempts) : fmt::format("{}%", p.pct);
            auto* pc = CCLabelBMFont::create(pcText.c_str(), "bigFont.fnt");
            pc->setScale(0.26f);
            pc->setAnchorPoint({ 1.f, 0.5f });
            pc->setPosition({ w - 5.f, y });
            root->addChild(pc);
        }

        m_big = CCLabelBMFont::create("", "goldFont.fnt");
        m_big->setPosition({ win.width / 2.f, win.height / 2.f + 40.f });
        m_big->setVisible(false);
        this->addChild(m_big, 10);
    }

    void updateClock(const Duel& d) {
        if (!m_clock) return;
        const double t = nowSec();
        std::string clock, big;
        if (d.state == "loading") clock = "LOADING...";
        else if (d.state == "countdown" && d.startAt > t) clock = "GET READY";
        else if (d.state == "playing" && d.endsAt > t) {
            const int s = static_cast<int>(d.endsAt - t);
            clock = fmt::format("{}:{:02}", s / 60, s % 60);
        } else if (d.state == "intermission") clock = "NEXT ROUND...";
        else if (d.state == "finished") clock = "FINISHED";
        m_clock->setString(clock.c_str());
        if (m_big) {
            if (big != m_lastBig) {
                m_lastBig = big;
                m_big->setString(big.c_str());
                m_big->setVisible(!big.empty());
                if (!big.empty()) {
                    m_big->stopAllActions();
                    m_big->setScale(2.2f);
                    m_big->setOpacity(255);
                    m_big->runAction(cocos2d::CCSpawn::create(
                        cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.3f, 1.4f)),
                        cocos2d::CCSequence::create(cocos2d::CCDelayTime::create(0.55f), cocos2d::CCFadeOut::create(0.3f), nullptr), nullptr));
                    playGlimmerSound(big == "GO!" ? 1.5f : 0.9f, 0.7f);
                }
            }
        }
    }

    static SimplePlayer* makeIcon(const Player& p) {
        auto sp = SimplePlayer::create(std::max(1, p.iconId));
        auto gm = GameManager::get();
        if (!sp || !gm) return nullptr;
        sp->setColor(gm->colorForIdx(std::max(0, p.c1)));
        sp->setSecondColor(gm->colorForIdx(std::max(0, p.c2)));
        if (p.glow > 0) sp->setGlowOutline(gm->colorForIdx(p.glow));
        return sp;
    }

    int m_levelId = 0;
    std::string m_key, m_lastBig;
    CCLabelBMFont* m_clock = nullptr;
    CCLabelBMFont* m_big = nullptr;
};

inline void attachHud(PlayLayer* pl, int levelId) {
    if (pl && !pl->getChildByID("sor-duel-hud"_spr)) pl->addChild(SorDuelHud::create(levelId), 1002);
}

} // namespace sorrow::duels


// ===================================================================== fair start
// Rooms and duels both "load" each round level before the race starts:
//   1. your game opens the level page; once the LEVEL is downloaded and its
//      SONG is downloaded (or turned out to be unavailable - then it is
//      skipped after 25 s) Play is pressed for you;
//   2. inside the level the game HOLDS at the start (no physics, music
//      paused) and tells the bot rooms/loaded | duels/loaded;
//   3. when everyone is in (or after 60 s) the bot counts down 4 s; at GO the
//      attempt restarts from 0 with the music and the race is on.
// A SorGateOverlay shows who is still loading, then 3 - 2 - 1 - GO.
namespace sorrow::fair {

struct Gate {
    bool active = false, duel = false;
    std::string id, phase;      // phase: loading | countdown | live
    int round = 0, levelId = 0, loaded = 0, total = 0;
    double goAt = 0;
    std::vector<std::pair<std::string, bool>> who; // name, loaded
};

inline Gate current() {
    Gate g;
    const auto& d = duels::state().duel;
    if (d.valid && d.isMine() && d.levelId > 0 && d.racing()) {
        g.active = true; g.duel = true; g.id = d.id; g.round = d.round; g.levelId = d.levelId;
        g.phase = d.state == "loading" ? "loading" : d.state == "countdown" ? "countdown" : "live";
        g.goAt = d.startAt;
        for (auto& p : d.players) if (!p.forfeited) { g.total++; g.loaded += p.loaded; g.who.push_back({ p.name, p.loaded }); }
        return g;
    }
    const auto& r = rooms::state();
    if (r.active && r.state == "playing" && r.levelId > 0 && !r.gate.phase.empty()) {
        g.active = true; g.id = r.code; g.round = r.gate.round; g.levelId = r.levelId;
        g.phase = r.gate.phase; g.goAt = r.gate.goAt; g.loaded = r.gate.loaded; g.total = r.gate.total;
        for (size_t i = 0; i < r.playerIds.size(); ++i) {
            if (i < r.playerEliminated.size() && r.playerEliminated[i]) continue;
            g.who.push_back({ r.playerNames[i], i < r.playerLoaded.size() && r.playerLoaded[i] });
        }
    }
    return g;
}

inline bool shouldHold(const Gate& g, int levelId) {
    if (!g.active || g.levelId != levelId) return false;
    if (g.phase == "loading") return true;
    return g.phase == "countdown" && duels::nowSec() < g.goAt;
}

// ---- song: download it before playing; skip if it never arrives
struct SongPrep { int id = 0; double started = 0; bool requested = false; };
inline SongPrep& songPrep() { static SongPrep s; return s; }
inline bool songReady(GJGameLevel* lvl, std::string* why = nullptr) {
    if (!lvl) return true;
    const int id = lvl->m_songID;
    if (id <= 0) return true;                               // official song: bundled
    auto* mdm = MusicDownloadManager::sharedState();
    if (mdm->isSongDownloaded(id)) return true;
    auto& sp = songPrep();
    if (sp.id != id) sp = { id, duels::nowSec(), false };
    if (!sp.requested) {
        sp.requested = true;
        if (mdm->getSongInfoObject(id)) mdm->downloadSong(id);
        else mdm->getSongInfo(id, true);                    // fetch info, then download
    }
    if (duels::nowSec() - sp.started > 25.0) return true;   // unavailable / too slow: skip it
    if (why) *why = "Downloading the song...";
    return false;
}
inline std::string& status() { static std::string s; return s; }

// ---- don't drag someone back in after they left the level themselves
inline std::string& suppressKey() { static std::string s; return s; }
inline std::string keyOf(const Gate& g) { return fmt::format("{}|{}|{}", g.id, g.round, g.levelId); }
inline void userQuit(int levelId) {
    auto g = current();
    if (g.active && g.levelId == levelId) suppressKey() = keyOf(g);
}

// Called every 0.25 s by the duel director: press Play when level + song are ready.
inline void tick(float) {
    status().clear();
    auto g = current();
    if (!g.active || PlayLayer::get()) return;
    if (suppressKey() == keyOf(g)) return;
    auto* scene = cocos2d::CCDirector::get()->getRunningScene();
    auto* lil = scene ? scene->getChildByType<LevelInfoLayer>(0) : nullptr;
    if (!lil || !lil->m_level || static_cast<int>(lil->m_level->m_levelID.value()) != g.levelId) return;
    if (scene->getChildByType<FLAlertLayer>(0)) return;
    if (std::string(lil->m_level->m_levelString).empty()) { status() = "Downloading the level..."; return; }
    if (!songReady(lil->m_level, &status())) return;
    static double nextTry = 0;
    if (duels::nowSec() < nextTry) return;
    nextTry = duels::nowSec() + 1.0;
    lil->onPlay(nullptr);
}

// ---- hold inside the level until GO
struct Hold { std::string key; bool held = false, reported = false, released = false; double releasedAt = 0; };
inline Hold& hold() { static Hold h; return h; }

inline void reportLoaded(const Gate& g) {
    auto b = rooms::idBody();
    b.set("levelId", std::to_string(g.levelId));
    if (g.duel) {
        b.set("id", g.id);
        Bridge::post("duels/loaded", b, [](bool ok, int, matjson::Value body) {
            if (!ok) return;
            const auto d = duels::parseDuel(body["duel"]);
            if (d.valid && d.isMine()) duels::state().duel = d;
            duels::changed();
        });
    } else {
        b.set("code", g.id);
        Bridge::post("rooms/loaded", b, [](bool ok, int, matjson::Value body) {
            if (ok && body["room"].isObject()) rooms::applyRoomJson(body["room"], rooms::state());
            rooms::pokeDirector();
        });
    }
    duels::DuelDirector::get()->pokeNow();
}

// From GJBaseGameLayer::update: true = skip this frame (holding).
inline bool holdFrame(PlayLayer* pl) {
    if (!pl || !pl->m_level) return false;
    const int lid = static_cast<int>(pl->m_level->m_levelID.value());
    auto g = current();
    if (!g.active || g.levelId != lid) return false;
    auto& h = hold();
    const auto key = keyOf(g);
    if (h.key != key) h = { key };
    if (shouldHold(g, lid)) {
        h.held = true;
        FMODAudioEngine::get()->pauseAllMusic(true);
        if (!h.reported && g.phase == "loading") { h.reported = true; reportLoaded(g); }
        return true;
    }
    if (h.held && !h.released) {
        h.released = true;
        h.releasedAt = duels::nowSec();
        pl->resetLevel();                          // the attempt starts at GO, from 0
        FMODAudioEngine::get()->resumeAllMusic();
        return true;
    }
    return false;
}

// Overlay in the level: who is still loading, then 3 - 2 - 1 - GO.
class SorGateOverlay : public cocos2d::CCNode {
public:
    static SorGateOverlay* create(int levelId) {
        auto* n = new SorGateOverlay();
        n->m_levelId = levelId;
        n->autorelease();
        n->setID("sor-gate-overlay"_spr);
        n->schedule(schedule_selector(SorGateOverlay::tick), 0.1f);
        n->build();
        return n;
    }
private:
    void build() {
        const auto win = cocos2d::CCDirector::get()->getWinSize();
        m_dim = cocos2d::CCLayerColor::create({ 0, 0, 0, 110 });
        this->addChild(m_dim);
        m_panel = sorrow::ui::panel(260.f, 74.f, { 0, 0, 0 }, 170);
        m_panel->setPosition({ win.width / 2.f, win.height / 2.f + 40.f });
        this->addChild(m_panel);
        m_title = CCLabelBMFont::create("WAITING FOR PLAYERS", "goldFont.fnt");
        m_title->setScale(.6f);
        m_title->setPosition({ win.width / 2.f, win.height / 2.f + 62.f });
        this->addChild(m_title);
        m_count = CCLabelBMFont::create("", "bigFont.fnt");
        m_count->setScale(.4f);
        m_count->setPosition({ win.width / 2.f, win.height / 2.f + 44.f });
        this->addChild(m_count);
        m_names = CCLabelBMFont::create("", "bigFont.fnt");
        m_names->setPosition({ win.width / 2.f, win.height / 2.f + 24.f });
        this->addChild(m_names);
        m_big = CCLabelBMFont::create("", "goldFont.fnt");
        m_big->setPosition({ win.width / 2.f, win.height / 2.f + 30.f });
        this->addChild(m_big, 5);
    }
    void tick(float) {
        auto g = current();
        const double t = duels::nowSec();
        const auto& h = hold();
        const bool held = shouldHold(g, m_levelId);
        const bool goFlash = h.released && h.key == keyOf(g) && t - h.releasedAt < 0.9;
        this->setVisible(held || goFlash);
        if (!held && !goFlash) { m_lastBig.clear(); return; }
        const bool loading = held && g.phase == "loading";
        m_dim->setVisible(held);
        m_panel->setVisible(loading);
        m_title->setVisible(loading);
        m_count->setVisible(loading);
        m_names->setVisible(loading);
        std::string big;
        if (loading) {
            m_count->setString(fmt::format("{} / {} ready", g.loaded, std::max(g.total, 1)).c_str());
            std::string names;
            for (auto& [n, ok] : g.who) names += (names.empty() ? "" : "   ") + std::string(ok ? "" : "...") + n;
            m_names->setString(names.c_str());
            m_names->limitLabelWidth(250.f, .3f, .12f);
        } else if (held) {
            big = std::to_string(std::max(1, static_cast<int>(std::ceil(g.goAt - t))));
        } else big = "GO!";
        if (big != m_lastBig) {
            m_lastBig = big;
            m_big->setString(big.c_str());
            m_big->setVisible(!big.empty());
            if (!big.empty()) {
                m_big->stopAllActions();
                m_big->setScale(2.4f);
                m_big->setOpacity(255);
                m_big->setColor(big == "GO!" ? cocos2d::ccColor3B { 120, 255, 140 } : cocos2d::ccColor3B { 255, 255, 255 });
                m_big->runAction(cocos2d::CCSpawn::create(
                    cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(.3f, 1.5f)),
                    cocos2d::CCSequence::create(cocos2d::CCDelayTime::create(.6f), cocos2d::CCFadeOut::create(.3f), nullptr), nullptr));
                playGlimmerSound(big == "GO!" ? 1.5f : .9f, .7f);
            }
        }
    }
    int m_levelId = 0;
    std::string m_lastBig;
    cocos2d::CCLayerColor* m_dim = nullptr;
    cocos2d::extension::CCScale9Sprite* m_panel = nullptr;
    CCLabelBMFont* m_title = nullptr;
    CCLabelBMFont* m_count = nullptr;
    CCLabelBMFont* m_names = nullptr;
    CCLabelBMFont* m_big = nullptr;
};

inline void attachOverlay(PlayLayer* pl, int levelId) {
    if (pl && !pl->getChildByID("sor-gate-overlay"_spr)) pl->addChild(SorGateOverlay::create(levelId), 1003);
}

} // namespace sorrow::fair

namespace sorrow::duels {
inline void fairTick(float dt) { fair::tick(dt); }
}
