// Sorrow Integration - glimmer from the vanilla rewards screen.
//
// Hooks CurrencyRewardLayer (used for level-completion rewards, DEATH
// rewards, and chest/Treasure popups). Every screen credits glimmer:
// - Treasure (chest): payload-scaled amount, 1000..200000.
// - Default + death flag (PlayLayer::destroyPlayer ran): small random
//   amount in the 10k..25k band, sound plays with the orbs you got.
// - Default + pending level award: the award is presented here (and the
//   response handler also consumes it via activeCRL() if the response
//   lands after this screen opened).
#pragma once

#include <Geode/modify/CurrencyRewardLayer.hpp>
#include <Geode/binding/CurrencyRewardLayer.hpp>

#include "AwardFx.hpp"
#include "Rewards.hpp"
#include "Glimmer.hpp"
#include "Store.hpp"

using namespace geode::prelude;

class $modify(SorrowChestRewards, CurrencyRewardLayer)
{
    struct Fields {
        bool m_sorHandled = false;
    };

    bool init(int orbs, int stars, int moons, int diamonds, CurrencySpriteType demonKey,
              int keyCount, CurrencySpriteType shardType, int shardsCount,
              cocos2d::CCPoint position, CurrencyRewardType rewardType,
              float yOffset, float time)
    {
        if (!CurrencyRewardLayer::init(orbs, stars, moons, diamonds, demonKey,
                keyCount, shardType, shardsCount, position, rewardType, yOffset, time))
            return false;

        sorrow::activeCRL() = this;

        // 1) Level-completion award already waiting (response arrived before
        // this screen opened)? Present it here, vanilla-style.
        if (const auto pending = sorrow::pendingLevelAward(); pending > 0) {
            sorrow::pendingLevelAward() = -1;
            sorrow::runVanillaGlimmerSequence(this, pending);
        }

        if (rewardType == CurrencyRewardType::Treasure) {
            // 2) Chest: payload-scaled credit + the "+Glimmer" line under
            // the orb counter.
            const auto payload = orbs + stars * 10 + moons * 10 + diamonds * 2;
            const int amount = std::clamp(1000 + payload * 25, 1000, 200000);
            m_fields->m_sorHandled = true;
            this->sorCreditAndPresent(amount, "chest");
            if (m_orbsLabel && m_orbsLabel->getParent())
                sorrow::showChestGlimmerLine(m_orbsLabel->getParent(), m_orbsLabel->getPosition());
        } else if (sorrow::lastWasDeath()) {
            // 3) Death rewards: small random glimmer within the minimum band.
            m_fields->m_sorHandled = true;
            sorrow::lastWasDeath() = false;
            const int amount = 10000 + static_cast<int>(std::rand() % 15001);
            this->sorCreditAndPresent(amount, "death");
        }

        return true;
    }

    void onEnter()
    {
        CurrencyRewardLayer::onEnter();

        // Death-rewards screens can be built WITHOUT going through
        // create()/init (deserialized nodes skip init) - that is why the
        // death jingle sometimes never fired. Catch the death payout here:
        // onEnter runs on every real CurrencyRewardLayer no matter how it
        // was constructed, and m_sorHandled stops init-handled screens
        // from double-crediting.
        if (!m_fields->m_sorHandled && this->m_rewardType == CurrencyRewardType::Default
            && sorrow::lastWasDeath()) {
            m_fields->m_sorHandled = true;
            sorrow::lastWasDeath() = false;
            log::info("[sorrow] death payout via onEnter (init path skipped)");
            const int amount = 10000 + static_cast<int>(std::rand() % 15001);
            this->sorCreditAndPresent(amount, "death");
        }
        m_fields->m_sorHandled = true;
    }

    void onExit()
    {
        if (sorrow::activeCRL() == this) sorrow::activeCRL() = nullptr;
        CurrencyRewardLayer::onExit();
    }

private:
    // Credit through the bridge; when the response lands (main thread),
    // play the vanilla-style sequence on this screen (or the HUD fallback
    // if the screen is already gone).
    void sorCreditAndPresent(int amount, const char* source)
    {
        geode::WeakRef<CurrencyRewardLayer> ref = this;
        sorrow::creditGlimmer(amount, source, [ref](bool ok, int, matjson::Value body) {
            if (!ok || !body.isObject()) return;
            const auto credited = body["credited"].asInt().unwrapOr(0);
            const auto balance = body["balance"].asInt().unwrapOr(-1);
            if (balance >= 0) sorrow::setGlimmer(balance, 0);
            if (credited <= 0) return;
            if (auto crl = ref.lock()) {
                if (!crl->getParent()) {
                    // Chest screen already closed: present on the scene.
                    sorrow::presentGlimmerAward(credited);
                } else if (const auto pending = sorrow::pendingLevelAward(); pending > 0) {
                    sorrow::pendingLevelAward() = -1;
                    sorrow::runVanillaGlimmerSequence(crl, pending + credited);
                } else {
                    sorrow::runVanillaGlimmerSequence(crl, credited);
                }
            } else {
                sorrow::presentGlimmerAward(credited);
            }
        });
    }
};
