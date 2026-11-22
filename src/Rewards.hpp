// Sorrow Integration - chest glimmer display.
//
// Chest popups get a "+Glimmer" line under their orb counter (restored by
// popular demand in v2.7.1). The orbs-style award presentation itself lives
// in AwardFx.hpp and runs for every glimmer credit, chests included.
#pragma once

#include <Geode/Geode.hpp>

#include "Sprites.hpp"

namespace sorrow {

// Chest popup: "+N" glimmer line under the reward counters.
// Returns the container (for tests) - may be null when sprites failed.
inline cocos2d::CCNode* showChestGlimmerLine(cocos2d::CCNode* parent, cocos2d::CCPoint orbsLabelPos) {
    if (!parent) return nullptr;
    ensureSprites();

    auto node = cocos2d::CCLayer::create();
    node->setID("sorrow-chest-glimmer"_spr);
    node->setPosition(orbsLabelPos + cocos2d::CCPoint(0.f, -26.f));
    parent->addChild(node, 20);

    if (auto icon = cocos2d::CCSprite::create("glimmer.png"_spr)) {
        icon->setScale(0.4f);
        icon->setPosition({ -34.f, 0.f });
        node->addChild(icon);
    }

    auto label = cocos2d::CCLabelBMFont::create("+Glimmer", "bigFont.fnt");
    if (label) {
        label->setScale(0.45f);
        node->addChild(label);
    }

    node->setScale(0.01f);
    node->runAction(cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.35f, 1.f)));

    return node;
}

} // namespace sorrow
