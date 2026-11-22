// Sorrow Integration - glimmer in the icon-kit garage (GJGarageLayer),
// where vanilla shows stars / moons / coins / orbs / diamonds / shards.
// The NodeIDs mod gives those children ids; we anchor next to the shards
// pair. Without NodeIDs we fall back to the rightmost top-row currency icon.
#pragma once

#include "SorConst.hpp"
#include "Format.hpp"
#include "Glimmer.hpp"
#include "AwardFx.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/binding/GJGarageLayer.hpp>

using namespace geode::prelude;

namespace sorrow {

inline void removeGarageGlimmer(cocos2d::CCNode* root) {
    if (!root) return;
    for (auto child : geode::cocos::CCArrayExt<CCNode*>(root->getChildren())) {
        const std::string id = child->getID();
        if (id.find("sor-garage-glimmer") != std::string::npos) {
            child->removeFromParent();
            continue;
        }
        removeGarageGlimmer(child);
    }
}

// One glimmer currency chip: icon + full balance, sized like the neighbours.
inline cocos2d::CCNode* buildGarageGlimmer(std::int64_t balance, float iconSize) {
    sorrow::ensureSprites();

    auto node = cocos2d::CCNode::create();
    node->setID("sor-garage-glimmer"_spr);
    node->setAnchorPoint({ 0.f, .5f });
    node->setContentSize({ 130.f, iconSize + 4.f });

    auto icon = CCSprite::createWithSpriteFrameName("glimmer.png"_spr);
    if (!icon) return nullptr;
    icon->setScale(iconSize / std::max(1.f, icon->getContentHeight()));
    const float iconW = icon->getScaledContentWidth();

    auto label = CCLabelBMFont::create(
        balance >= 0 ? sorrow::formatGlimmer(balance).c_str() : "...", "bigFont.fnt");
    if (!label) return nullptr;
    label->setScale(.55f);
    label->limitLabelWidth(100.f, .55f, .05f);
    label->setID("sor-garage-glimmer-label"_spr);

    // Number LEFT, icon RIGHT (same arrangement as the profile stat).
    const float labelW = label->getScaledContentWidth();
    label->setPosition({ labelW * .5f + 2.f, (iconSize + 4.f) * .5f });
    icon->setPosition({ labelW + 4.f + iconW * .5f, (iconSize + 4.f) * .5f });
    node->addChild(label);
    node->addChild(icon);
    return node;
}

inline void attachGarageGlimmer(GJGarageLayer* layer) {
    if (!layer) return;
    auto mainLayer = layer;
    removeGarageGlimmer(mainLayer);

    const auto win = cocos2d::CCDirector::sharedDirector()->getWinSize();

    cocos2d::CCNode* anchor = nullptr;
    // NodeIDs path: the shards icon is the last vanilla currency.
    anchor = mainLayer->getChildByID("diamond-shards-icon");
    cocos2d::CCNode* anchorLabel = mainLayer->getChildByID("diamond-shards-label");

    if (!anchor) {
        // Fallback: the rightmost small sprite on the top currency row.
        float bestX = -1.f;
        for (auto child : geode::cocos::CCArrayExt<CCNode*>(mainLayer->getChildren())) {
            auto spr = typeinfo_cast<CCSprite*>(child);
            if (!spr) continue;
            const auto p = spr->getPosition();
            const float w = spr->getScaledContentWidth();
            if (w <= 0.f || w > 45.f) continue;      // currency icons are small
            if (p.y < win.height - 48.f) continue;    // top row only
            if (p.x > bestX) { bestX = p.x; anchor = spr; }
        }
    }
    if (!anchor) return;

    auto item = buildGarageGlimmer(sorrow::glimmerBalance(), 22.f);
    if (!item) return;

    const auto world = anchor->getParent()->convertToWorldSpace(anchor->getPosition());
    const auto local = mainLayer->convertToNodeSpace(world);
    float x = local.x + 34.f;
    float y = local.y;
    if (x > win.width - 70.f) {
        // No room to the right: drop one row below the currencies, aligned.
        x = local.x;
        y = local.y - 34.f;
    }
    item->setPosition({ x, y });
    mainLayer->addChild(item);

    // Fresh balance straight into the label once the sync lands.
    sorrow::syncBalance([mainLayer](bool ok, int, matjson::Value body) {
        if (!ok || !body.isObject()) return;
        const auto balance = body["balance"].asInt().unwrapOr(-1);
        Loader::get()->queueInMainThread([mainLayer, balance]() {
            if (!mainLayer || balance < 0) return;
            if (auto label = typeinfo_cast<CCLabelBMFont*>(
                    mainLayer->getChildByIDRecursive("sor-garage-glimmer-label"_spr))) {
                label->setString(sorrow::formatGlimmer(balance).c_str());
            }
        });
    });
}

} // namespace sorrow

class SorrowGarageLayer : public geode::Modify<SorrowGarageLayer, GJGarageLayer> {
public:
    bool init() {
        if (!GJGarageLayer::init()) return false;
        sorrow::attachGarageGlimmer(this);
        return true;
    }
};
