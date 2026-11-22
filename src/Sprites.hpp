// Sorrow Integration - mod sprite registration.
//
// The .geode package ships loose PNGs under resources/<mod-id>/. The loader
// adds that folder to cocos' search path, but NOTHING registers sprite frames
// for _spr lookups unless a spritesheet plist is declared. Every
// "file.png"_spr lookup goes through CCSpriteFrameCache with the key
// "<mod-id>/file.png" (Mod::expandSpriteName), so without registration every
// mod sprite renders as the magenta/black "missing texture" checkerboard.
//
// ensureSprites() builds the frames from the loose files at runtime. It is
// idempotent and must be called before the first UI that uses a mod sprite
// (CreatorLayer button, roulette layer, profile page, rooms popup).

#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "SorConst.hpp"

namespace sorrow {

// Absolute path of a file shipped in the mod resources folder.
inline std::string fullResourcePath(const std::string& name) {
    return (Mod::get()->getResourcesDir() / name).string();
}

// Full-screen "cover" sprite from a resource image (photo backgrounds).
// Scales the image until it covers winSize, centered. nullptr if missing.
inline cocos2d::CCSprite* createCoverSprite(const std::string& name, const cocos2d::CCSize& winSize) {
    const auto path = fullResourcePath(name);
    // No isFileExist pre-check: CCSprite::create returns null for a missing
    // file on its own (the pre-check was unreliable for absolute paths on
    // Android and silently disabled the photo background).
    auto spr = cocos2d::CCSprite::create(path.c_str());
    if (!spr) {
        log::warn("[sorrow] photo background '{}' not found at {}", name, path);
        return nullptr;
    }
    const auto sz = spr->getContentSize();
    if (sz.width < 1.f || sz.height < 1.f) { spr->release(); return nullptr; }
    const float scale = std::max(winSize.width / sz.width, winSize.height / sz.height);
    spr->setScale(scale);
    spr->setPosition({ winSize.width / 2.f, winSize.height / 2.f });
    log::info("[sorrow] photo background loaded from {}", path);
    return spr;
}

inline void registerSpriteFrame(const char* name) {
    const auto path = Mod::get()->getResourcesDir() / name;
    if (!cocos2d::CCFileUtils::get()->isFileExist(path.string())) {
        log::warn("sprite missing from package: {}", name);
        return;
    }
    auto spr = cocos2d::CCSprite::create(path.string().c_str());
    if (!spr || !spr->getTexture()) return;
    auto frame = spr->displayFrame();
    if (!frame) return;

    auto* cache = cocos2d::CCSpriteFrameCache::get();
    // Key used by "name.png"_spr lookups.
    cache->addSpriteFrame(frame, fmt::format("{}/{}", Mod::get()->getID(), name).c_str());
    // Plain key as well - harmless (no vanilla collisions) and covers any
    // direct CCSprite::createWithSpriteFrameName without the suffix.
    cache->addSpriteFrame(frame, name);
}

inline void ensureSprites() {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;

    for (const char* name : {
        "RL_btn_001.png", "RL_btn_002.png", "RL_btn_big.png",
        "RL_goalReached_001.png", "RL_longBtn_001.png", "RL_sideArt_001.png",
        "RL_optionsBtn_001.png", "RL_optionsTxt_001.png", "RL_statsBtn_001.png",
        "RL_square01_001.png",
        "RL_listBorderBackground_001.png", "RL_listBorderBackground_002.png",
        "RL_listBorderBottom_001.png", "RL_listBorderBottom_002.png",
        "RL_listBorderSide_001.png", "RL_listBorderTop_001.png", "RL_listBorderTop_002.png",
        "glimmer.png",
    }) registerSpriteFrame(name);
}

} // namespace sorrow
