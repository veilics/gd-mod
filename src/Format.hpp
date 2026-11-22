// Sorrow Integration - number formatting.
//
// Glimmer balances grow fast (casino jackpots, demonlist bonuses), so the UI
// compacts large values: below one million the exact comma-separated number
// is shown, above that 4,999,707,079,336 becomes "5T", 12,400,000 -> "12.4M".
#pragma once

#include <cstdio>
#include <string>

#include "Store.hpp"

namespace sorrow {

inline std::string commaNum(std::int64_t v) {
    std::string s = std::to_string(v);
    int insert = static_cast<int>(s.length()) - 3;
    while (insert > 0) { s.insert(static_cast<size_t>(insert), ","); insert -= 3; }
    return s;
}

// Compact balance string. < 1M: exact with commas. >= 1M: one decimal +
// tier suffix (M, B, T, Qa, Qi); trailing ".0" trimmed.
inline std::string formatGlimmer(std::int64_t v) {
    if (v < 0) v = 0;
    if (v < 1000000) return commaNum(v);

    static constexpr const char* SUFFIXES[] = { "M", "B", "T", "Qa", "Qi" };
    long double f = static_cast<long double>(v);
    int tier = -1;
    while (f >= 1000.0L && tier < 4) { f /= 1000.0L; ++tier; }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(f));
    std::string s(buf);
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0)
        s.erase(s.size() - 2, 2);
    return s + SUFFIXES[tier];
}

} // namespace sorrow
