#include "RouletteManager.hpp"

#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include "utils.hpp"
#include "../Bet.hpp"
#include "../SorGoalLabel.hpp"
#include "../SorDuelLayer.hpp"
#include <Geode/ui/BasedButtonSprite.hpp>

using namespace geode::prelude;

class $modify(LevelInfoLayer)
{
	bool init(GJGameLevel* level, bool p1)
	{
		if (!LevelInfoLayer::init(level, p1)) return false;

		// Sorrow: glimmer bet chip next to the favorite button.
		sorrow::attachLevelBetButton(this, static_cast<std::int64_t>(level->m_levelID.value()));

		// Sorrow: "duel on this level" (uploaded levels only), in the same
		// left button column as the Glimmer bet button.
		if (level->m_levelID.value() > 0) {
			auto* vs = CCLabelBMFont::create("VS", "bigFont.fnt");
			vs->setScale(.55f);
			auto* spr = CircleButtonSprite::create(vs, CircleBaseColor::Pink, CircleBaseSize::Small);
			Ref<GJGameLevel> lv = level;
			auto* btn = CCMenuItemExt::createSpriteExtra(spr, [lv](CCMenuItemSpriteExtra*) { sorrow::duels::openVersus(lv); });
			btn->setID("sor-duel-button"_spr);
			sorrow::placeLevelButton(this, btn, spr);
		}

		// Gray "(goal%)" next to the normal-mode percentage - solo roulette goal
		// or room round target. Lives on a schedule, so it also appears once
		// the level finishes downloading (the bar does not exist before that).
		this->addChild(sorrow::SorGoalLabel::create(
			static_cast<int>(level->m_levelID.value()), .4f,
			[this]() { return typeinfo_cast<CCLabelBMFont*>(this->getChildByID("normal-mode-percentage")); }), 3);

		return true;
	}

	void levelDownloadFinished(GJGameLevel* p0)
	{
		auto& rlm = RouletteManager::get();
		if (rlm.isPlaying && this->m_level->m_levelID.value() == rlm.gameState.levelID && rl::utils::isAprilFools())
		{
			p0->m_levelName = this->m_level->m_levelName;
			p0->m_creatorName = this->m_level->m_creatorName;
			p0->m_ratings = this->m_level->m_ratings;
			p0->m_ratingsSum = this->m_level->m_ratingsSum;
			p0->m_demon = this->m_level->m_demon;
			p0->m_demonDifficulty = this->m_level->m_demonDifficulty;
			p0->m_autoLevel = this->m_level->m_autoLevel;
			p0->m_stars = this->m_level->m_stars;
			p0->m_featured = this->m_level->m_featured;
			p0->m_isEpic = this->m_level->m_isEpic;
		}

		LevelInfoLayer::levelDownloadFinished(p0);
	}

	void levelUpdateFinished(GJGameLevel* p0, UpdateResponse p1)
	{
		auto& rlm = RouletteManager::get();
		if (rlm.isPlaying && this->m_level->m_levelID.value() == rlm.gameState.levelID && rl::utils::isAprilFools())
		{
			p0->m_levelName = this->m_level->m_levelName;
			p0->m_creatorName = this->m_level->m_creatorName;
			p0->m_ratings = this->m_level->m_ratings;
			p0->m_ratingsSum = this->m_level->m_ratingsSum;
			p0->m_demon = this->m_level->m_demon;
			p0->m_demonDifficulty = this->m_level->m_demonDifficulty;
			p0->m_autoLevel = this->m_level->m_autoLevel;
			p0->m_stars = this->m_level->m_stars;
			p0->m_featured = this->m_level->m_featured;
			p0->m_isEpic = this->m_level->m_isEpic;
		}

		LevelInfoLayer::levelUpdateFinished(p0, p1);
	}
};
