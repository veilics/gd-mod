#include "RouletteManager.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/loader/Mod.hpp>

#include <fmt/format.h>
#include "../Bet.hpp"
#include "../SorGoalLabel.hpp"

using namespace geode::prelude;

class $modify(PauseLayer)
{
	void customSetup()
	{
		PauseLayer::customSetup();

		const auto* playLayer = GameManager::sharedState()->getPlayLayer();

		// Sorrow: bet chip + current bet status on the pause menu.
		if (playLayer && !playLayer->m_isPracticeMode)
		{
			sorrow::attachPauseBetControls(
				this, static_cast<std::int64_t>(playLayer->m_level->m_levelID.value()),
				playLayer->m_level);
		}

		const auto& rlm = RouletteManager::get();

		// Room round target (solo roulette keeps the upstream label below).
		if (playLayer && !(rlm.isPlaying && playLayer->m_level->m_levelID.value() == rlm.gameState.levelID))
		{
			this->addChild(sorrow::SorGoalLabel::create(static_cast<int>(playLayer->m_level->m_levelID.value()), .5f,
				[this]() { return typeinfo_cast<CCLabelBMFont*>(this->getChildByID("normal-progress-label")); }), 4);
		}

		if (playLayer && rlm.isPlaying && playLayer->m_level->m_levelID.value() == rlm.gameState.levelID)
		{
			CCLabelBMFont* normalPercentageLabel = static_cast<CCLabelBMFont*>(this->getChildByID("normal-progress-label"));

			if (!normalPercentageLabel) return;

			auto goalPercentage = CCLabelBMFont::create(
				fmt::format("({}%)", rlm.currentPercentageGoal).c_str(),
				"bigFont.fnt"
			);
			goalPercentage->setScale(.5f);

			float goalOffset = (
				(normalPercentageLabel->getContentWidth() * normalPercentageLabel->getScaleX()) / 2.f
			) + (
				(goalPercentage->getContentWidth() * goalPercentage->getScaleX()) / 2.f
			) + 5.f;

			goalPercentage->setPosition({
				normalPercentageLabel->getPositionX() + goalOffset,
				normalPercentageLabel->getPositionY()
			});
			goalPercentage->setColor({ 125, 125, 125 });
			goalPercentage->setZOrder(4);
			goalPercentage->setID("goal-progress-label"_spr);
			this->addChild(goalPercentage);
		}
	}
};
