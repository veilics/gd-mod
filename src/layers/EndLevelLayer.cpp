#include "RouletteManager.hpp"

#include "../Glimmer.hpp"
#include "../AwardFx.hpp"
#include "../Bet.hpp"

#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/loader/Mod.hpp>

class $modify(RLEndLevelLayer, EndLevelLayer)
{
	struct Fields {
		int tries = 0;
		bool rewarded = false;
	};

	void customSetup()
	{
		EndLevelLayer::customSetup();

		const auto& rlm = RouletteManager::get();
		const auto pl = GameManager::sharedState()->getPlayLayer();

		// Glimmer payout animation: poll for the award response (it usually
		// lands about a second after the end screen shows) and play the
		// orbs-style burst. The response handler toasts instead if this
		// screen never claims it.
		// NOTE: schedule on EVERY end screen. Normal completions (the
		// majority) award Glimmer too - v2.2.1 only polled on roulette-goal
		// end screens, so regular completions never showed the animation.
		// Sorrow: completion resolves the attempt bet as a WIN (100%).
		if (pl && !pl->m_isPracticeMode &&
			sorrow::activeBet().amount > 0 &&
			sorrow::activeBetLevel() == static_cast<std::int64_t>(pl->m_level->m_levelID.value()))
		{
			sorrow::resolveAttemptBet(
				static_cast<std::int64_t>(pl->m_level->m_levelID.value()), 100, true);
		}

		this->schedule(schedule_selector(RLEndLevelLayer::sorRewardTick), 0.25f);

		if (!rlm.isPlaying || !pl || pl->m_isPracticeMode || !rlm.gameState.hasReachedGoal)
			return;

		auto levelCompleteSprite = this->m_mainLayer->getChildByID("level-complete-text");
		levelCompleteSprite->setVisible(false);

		auto goalReachedSprite = cocos2d::CCSprite::create("RL_goalReached_001.png"_spr);
		goalReachedSprite->setScale(.95f);
		goalReachedSprite->setPosition(levelCompleteSprite->getPosition());
		goalReachedSprite->setID("goal-reached-sprite"_spr);
		this->m_mainLayer->addChild(goalReachedSprite);
	}

	void sorRewardTick(float)
	{
		auto& f = m_fields;
		if (f->rewarded) return;
		if (++f->tries > 32)
		{
			this->unschedule(schedule_selector(RLEndLevelLayer::sorRewardTick));
			return;
		}

		const auto pending = sorrow::pendingLevelAward();
		if (pending <= 0) return;

		f->rewarded = true;
		this->unschedule(schedule_selector(RLEndLevelLayer::sorRewardTick));

		// The response landed AFTER the rewards screen opened (the common
		// case): consume it right now on the live screen. If no screen is
		// showing, leave it pending for the screen's init or the 6s fallback.
		sorrow::lastWasDeath() = false;
		if (auto crl = sorrow::activeCRL().lock()) {
			sorrow::pendingLevelAward() = -1;
			sorrow::runVanillaGlimmerSequence(crl, pending);
		}
	}
};
