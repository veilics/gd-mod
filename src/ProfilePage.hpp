// Sorrow Integration - Glimmer on player profiles.
//
// Two surfaces:
//  1. ProfilePage: a Glimmer icon + compact balance as the FIRST (far left)
//     item of the stats row, for linked accounts.
//  2. The profile INFO popup (the FLAlertLayer with the "Stars: ... / Moons:
//     ... " lines): a colored "Glimmer: N" line appended at the end.
//
// Placement notes (researched from geode-sdk/NodeIDs, ProfilePage.cpp): in
// GD 2.2 the stats row is a CCMenu with the ID "stats-menu" carrying a
// RowLayout (the NodeIDs mod creates/IDs it and migrates the vanilla stat
// labels+icons into it). Manual pixel positions get overridden by that
// layout - the v2.3.0 "down and to the right" bug. The correct approach is
// to join the layout: one item node (icon+label inside), AxisLayoutOptions
// with scale limits, inserted as child 0 = far left, then updateLayout().

#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/modify/FLAlertLayer.hpp>
#include <Geode/binding/ProfilePage.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/ui/Layout.hpp>

#include "SorConst.hpp"
#include "Store.hpp"
#include "Format.hpp"
#include "Glimmer.hpp"
#include "Sprites.hpp"

using namespace geode::prelude;

namespace sorrow {

// Last profile page opened + fetched balances, keyed by GD account id.
inline int& lastViewedAccount() { static int v = 0; return v; }
inline std::unordered_map<int, std::int64_t>& balanceCache() {
    static std::unordered_map<int, std::int64_t> m;
    return m;
}

// Balance to show for an account, or -1 when unknown/unlinked.
inline std::int64_t cachedBalanceFor(int accountID) {
    auto& cache = balanceCache();
    if (cache.count(accountID)) return cache[accountID];
    return -1;
}

// Recursively find the first sprite using the given frame name.
inline cocos2d::CCSprite* findSpriteByFrame(cocos2d::CCNode* root, const char* frameName) {
    if (!root) return nullptr;
    if (geode::cocos::isSpriteFrameName(root, frameName)) {
        return static_cast<cocos2d::CCSprite*>(root);
    }
    for (auto child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(root->getChildren())) {
        if (auto hit = findSpriteByFrame(child, frameName)) return hit;
    }
    return nullptr;
}

// Remove every glimmer node we previously added anywhere under `root`
// (previous page loads, v2.3.0 leftovers) so re-attaching never duplicates.
inline void removeGlimmerNodes(cocos2d::CCNode* root) {
    if (!root) return;
    for (auto child : geode::cocos::CCArrayExt<CCNode*>(root->getChildren())) {
        const std::string id = child->getID();
        if (id.find("sor-glimmer-item") != std::string::npos ||
            id.find("sor-glimmer-icon") != std::string::npos ||
            id.find("sor-glimmer-label") != std::string::npos) {
            child->removeFromParent();
            continue;
        }
        removeGlimmerNodes(child);
    }
}

// The glimmer stat as one self-contained item (icon left of the number).
// compact formatting keeps even trillions row-sized.
inline cocos2d::CCNode* buildGlimmerItem(std::int64_t balance) {
    ensureSprites();

    auto item = cocos2d::CCNode::create();
    item->setID("sor-glimmer-item"_spr);
    const float itemH = 34.f;

    auto icon = CCSprite::createWithSpriteFrameName("glimmer.png"_spr);
    if (!icon) return nullptr;
    icon->setID("sor-glimmer-icon"_spr);
    const float iconScale = 30.f / std::max(1.f, icon->getContentHeight());
    icon->setScale(iconScale);
    const float iconW = icon->getContentWidth() * iconScale;

    auto label = CCLabelBMFont::create(
        balance >= 0 ? sorrow::formatGlimmer(balance).c_str() : "...", "bigFont.fnt");
    if (!label) return nullptr;
    label->setID("sor-glimmer-label"_spr);
    label->setScale(0.5f);
    const float labelW = label->getContentWidth() * label->getScale();

    item->setContentSize({ iconW + 2.f + labelW, itemH });
    // Number LEFT, icon RIGHT of the number.
    label->setPosition({ labelW * 0.5f, itemH * 0.5f });
    icon->setPosition({ labelW + 2.f + iconW * 0.5f, itemH * 0.5f });
    item->addChild(label);
    item->addChild(icon);

    // Let the row's RowLayout treat this as one stat: it may shrink it to
    // fit, never stretch it.
    auto opts = geode::AxisLayoutOptions::create();
    opts->setScaleLimits(0.f, 1.f);
    item->setLayoutOptions(opts);

    return item;
}

// Preferred path: inside the "stats-menu" RowLayout, FIRST item (far left).
inline void attachGlimmerInMenu(cocos2d::CCNode* statsMenu, std::int64_t balance) {
    auto item = buildGlimmerItem(balance);
    if (!item) return;
    statsMenu->addChild(item);
    // Move to the front of the child array = first item of the row.
    // (CCNode::insertChild is private in GD's cocos fork; removeObject with
    // bReleaseObj=false keeps the addChild retain exactly balanced.)
    if (auto arr = statsMenu->getChildren()) {
        arr->removeObject(item, false);
        arr->insertObject(item, 0);
    }
    statsMenu->updateLayout();
}

// Adds (or rebuilds) the glimmer icon + balance on the profile stats row.
inline void attachGlimmerStat(ProfilePage* page, int accountID, std::int64_t balance) {
    if (!page) return;
    auto mainLayer = page->m_mainLayer;
    if (!mainLayer) return;

    removeGlimmerNodes(mainLayer);

    if (auto statsMenu = mainLayer->getChildByID("stats-menu")) {
        attachGlimmerInMenu(statsMenu, balance);
        return;
    }

    // Fallback without the stats-menu (no NodeIDs installed): anchor left of
    // the vanilla stars icon, same line.
    auto starsIcon = findSpriteByFrame(mainLayer, "GJ_starsIcon_001.png");
    if (!starsIcon) {
        log::warn("[sorrow] profile: no stats-menu and no stars icon found");
        return;
    }
    auto item = buildGlimmerItem(balance);
    if (!item) return;
    const auto world = starsIcon->getParent()->convertToWorldSpace(starsIcon->getPosition());
    const auto local = mainLayer->convertToNodeSpace(world);
    const float w = item->getContentSize().width;
    const float h = item->getContentSize().height;
    const float iconW = starsIcon->getScaledContentWidth();
    item->setPosition({ local.x - iconW * 0.5f - 12.f - w, local.y - h * 0.5f });
    mainLayer->addChild(item, 10);
}

} // namespace sorrow

class SorrowProfilePage : public geode::Modify<SorrowProfilePage, ProfilePage> {
public:
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        if (!score) return;
        const auto accountID = score->m_accountID;
        auto* page = static_cast<ProfilePage*>(this);
        sorrow::lastViewedAccount() = static_cast<int>(accountID);
        sorrow::ensureSprites();

        // Own profile: ALWAYS show our stored balance immediately (even 0),
        // and NEVER remove the row later - the viewed account is us.
        const auto ownId = static_cast<int>(std::atoi(sorrow::gdAccountId().c_str()));
        const bool own = (accountID == ownId);
        const std::int64_t ownBal = sorrow::glimmerBalance();
        if (own && ownBal >= 0) {
            sorrow::balanceCache()[static_cast<int>(accountID)] = ownBal;
        }

        const std::int64_t known = sorrow::cachedBalanceFor(static_cast<int>(accountID));
        if (known >= 0) sorrow::attachGlimmerStat(page, accountID, known);

        // Linked check + fresh balance via the bridge (own + other profiles).
        geode::WeakRef<ProfilePage> pageRef{ page };
        sorrow::getRaw(
            "/bridge/sorrow/profile?gdAccountId=" + sorrow::q(std::to_string(accountID)),
            [pageRef, accountID, own](bool ok, int, matjson::Value body) {
                auto lockedPage = pageRef.lock();
                if (!lockedPage) return; // profile closed before the response
                auto* page = static_cast<ProfilePage*>(lockedPage);
                if (!ok || !body.isObject()) return;
                log::info("Sorrow profile lookup (account {}): linked={} balance={}",
                    accountID,
                    body["linked"].asBool().unwrapOr(false),
                    body["balance"].asInt().unwrapOr(-1));
                auto& cache = sorrow::balanceCache();
                if (!body["linked"].asBool().unwrapOr(false)) {
                    // Unlinked: hide the stat - EXCEPT on our own profile,
                    // where the row stays (own balance is mirrored locally).
                    if (own) return;
                    cache[static_cast<int>(accountID)] = -1;
                    if (auto mainLayer = page->m_mainLayer)
                        sorrow::removeGlimmerNodes(mainLayer);
                    return;
                }
                const auto balance = static_cast<std::int64_t>(
                    body["balance"].asInt().unwrapOr(0));
                cache[static_cast<int>(accountID)] = balance;
                sorrow::attachGlimmerStat(page, accountID, balance);
            });
    }
};

// The profile INFO popup (FLAlertLayer listing Stars/Moons/Coins/Demons):
// append a Glimmer line for the viewed account when its balance is known.
class SorrowFLAlert : public geode::Modify<SorrowFLAlert, FLAlertLayer> {
public:
    bool init(FLAlertLayerProtocol* delegate, char const* title, gd::string desc,
              char const* btn1, char const* btn2, float width, bool scroll,
              float height, float textScale) {
        std::string content = desc;
        const bool isProfileInfo = content.find("Stars:") != std::string::npos &&
                                   content.find("Moons:") != std::string::npos;
        if (isProfileInfo) {
            const auto acc = sorrow::lastViewedAccount();
            std::int64_t bal = sorrow::cachedBalanceFor(acc);
            if (acc == static_cast<int>(std::atoi(sorrow::gdAccountId().c_str())) &&
                sorrow::glimmerBalance() >= 0) {
                bal = sorrow::glimmerBalance();
            }
            if (bal >= 0) {
                content += fmt::format("\n<cc>Glimmer:</c> {}", sorrow::formatGlimmer(bal));
                desc = gd::string(content);
            }
        }
        return FLAlertLayer::init(delegate, title, desc, btn1, btn2, width, scroll,
                                  height, textScale);
    }
};
