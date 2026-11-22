// Sorrow Integration - shared UI helpers.
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

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

// A checkbox that does not depend on CCMenuItemToggler (whose toggle-vs-
// callback order made our boxes never show a check): a plain button whose
// box frame we swap ourselves. Tapping the box OR its label toggles it.
// `value` must outlive the button (a member of the popup that owns it).
inline CCMenuItemSpriteExtra* checkbox(bool* value, const std::string& text, float boxScale, float textScale,
                                       float maxTextW = 220.f, std::function<void(bool)> onChange = nullptr) {
    auto* node = cocos2d::CCNode::create();
    auto* box = cocos2d::CCSprite::createWithSpriteFrameName(*value ? "GJ_checkOn_001.png" : "GJ_checkOff_001.png");
    box->setScale(boxScale);
    const float bw = box->getScaledContentWidth(), bh = box->getScaledContentHeight();
    float w = bw;
    if (!text.empty()) {
        auto* lab = cocos2d::CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        lab->limitLabelWidth(maxTextW, textScale, .1f);
        lab->setAnchorPoint({ 0.f, .5f });
        lab->setPosition({ bw + 6.f, bh / 2.f });
        node->addChild(lab);
        w += 6.f + lab->getScaledContentWidth();
    }
    box->setPosition({ bw / 2.f, bh / 2.f });
    node->addChild(box);
    node->setContentSize({ w, bh });
    return geode::cocos::CCMenuItemExt::createSpriteExtra(node, [value, box, onChange](CCMenuItemSpriteExtra*) {
        *value = !*value;
        if (auto* f = cocos2d::CCSpriteFrameCache::get()->spriteFrameByName(*value ? "GJ_checkOn_001.png" : "GJ_checkOff_001.png"))
            box->setDisplayFrame(f);
        if (onChange) onChange(*value);
    });
}

} // namespace sorrow::ui
