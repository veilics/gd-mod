// Sorrow Integration - shared UI helpers.
#pragma once

#include <Geode/Geode.hpp>

namespace sorrow::ui {

// square02b_001.png has ~26pt corners. A 9-slice shorter (or narrower) than
// two corners gets a NEGATIVE middle slice and draws a dark stripe through
// the panel - the "weird striped rectangles" on rows, badges and HUDs. The
// fix: draw the sprite at a smaller scale and a proportionally larger
// content size, so the corners always fit.
inline void resize(cocos2d::extension::CCScale9Sprite* p, float w, float h) {
    if (!p) return;
    const float s = std::clamp(std::min(w, h) / 64.f, 0.2f, 1.f);
    p->setScale(s);
    p->setContentSize({ w / s, h / s });
}

inline cocos2d::extension::CCScale9Sprite* panel(float w, float h, cocos2d::ccColor3B c = { 0, 0, 0 }, GLubyte a = 100) {
    auto* p = cocos2d::extension::CCScale9Sprite::create("square02b_001.png", { 0.f, 0.f, 80.f, 80.f });
    p->setColor(c);
    p->setOpacity(a);
    resize(p, w, h);
    return p;
}

// Lay out a row of nodes centred on `cx` with `gap` between their real
// (scaled) widths - used for button rows so nothing overlaps.
inline void row(const std::vector<cocos2d::CCNode*>& nodes, float cx, float y, float gap) {
    float total = 0.f;
    for (auto* n : nodes) total += n->getScaledContentWidth();
    total += gap * (nodes.size() > 1 ? nodes.size() - 1 : 0);
    float x = cx - total / 2.f;
    for (auto* n : nodes) {
        const float w = n->getScaledContentWidth();
        n->setPosition({ x + w / 2.f, y });
        x += w + gap;
    }
}

} // namespace sorrow::ui
