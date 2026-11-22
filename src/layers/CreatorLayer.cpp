#include "RouletteManager.hpp"

#include "../Sprites.hpp"
#include "../roulette/RLRouletteLayer.hpp"
#include "../SorDuelLayer.hpp"

#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

using namespace geode::prelude;

class $modify(RLCreatorLayer, CreatorLayer)
{
	void onRouletteButton(CCObject*)
	{
		if (!Mod::get()->getSettingValue<bool>("roulette-button")) return;
		auto& rlm = RouletteManager::get();

		if ((rlm.rouletteLayer = RLRouletteLayer::create()))
		{
			auto scene = CCScene::create();
			scene->addChild(rlm.rouletteLayer);

			CCDirector::sharedDirector()->pushScene(
				CCTransitionFade::create(.5f, scene)
			);
		}
	}

	// GD's (unreleased) Versus button -> Sorrow duels.
	void onMultiplayer(CCObject*)
	{
		sorrow::duels::openVersus();
	}

	// The Versus button ships grayed out: give it its real colours back.
	void enableVersusButton()
	{
		auto* menu = this->getChildByID("creator-buttons-menu");
		if (!menu) return;
		CCMenuItemSpriteExtra* versus = typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("versus-button"));
		if (!versus) {
			for (auto* c : CCArrayExt<CCNode*>(menu->getChildren())) {
				auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(c);
				if (item && item->getNormalImage() && isSpriteFrameName(item->getNormalImage(), "GJ_versusBtn_001.png")) { versus = item; break; }
			}
		}
		if (!versus) return;
		versus->setEnabled(true);
		versus->setTarget(this, menu_selector(RLCreatorLayer::onMultiplayer));
		std::function<void(CCNode*)> ungray = [&](CCNode* n) {
			if (auto* rgba = typeinfo_cast<CCRGBAProtocol*>(n)) { rgba->setColor(ccColor3B { 255, 255, 255 }); rgba->setOpacity(255); }
			for (auto* c : CCArrayExt<CCNode*>(n->getChildren())) ungray(c);
		};
		if (auto* img = versus->getNormalImage()) ungray(img);
		// a small pulse so people notice it works now
		if (auto* img = versus->getNormalImage(); img && !img->getChildByID("sor-versus-new"_spr)) {
			auto* tag = CCLabelBMFont::create("NEW", "bigFont.fnt");
			tag->setID("sor-versus-new"_spr);
			tag->setScale(.35f);
			tag->setColor({ 255, 215, 80 });
			tag->setPosition({ img->getContentWidth() - 6.f, img->getContentHeight() - 4.f });
			tag->setRotation(15.f);
			tag->runAction(CCRepeatForever::create(CCSequence::create(
				CCScaleTo::create(.5f, .42f), CCScaleTo::create(.5f, .35f), nullptr)));
			img->addChild(tag, 5);
		}
	}

	bool init()
	{
		sorrow::ensureSprites();

		// Entry button can be hidden in the mod settings.
		if (!Mod::get()->getSettingValue<bool>("roulette-button")) {
			if (!CreatorLayer::init()) return false;
			this->enableVersusButton();
			return true;
		}

		if (!CreatorLayer::init()) return false;
		this->enableVersusButton();

		auto winSize = CCDirector::sharedDirector()->getWinSize();

		CCMenuItemSpriteExtra* rouletteButton;

		if (!Mod::get()->getSettingValue<bool>("use-big-button"))
		{
			auto centerLeftMenu = CCMenu::create();
			centerLeftMenu->setPosition({
				this->getChildByID("exit-menu")->getPositionX(),
				CCDirector::sharedDirector()->getWinSize().height / 2.f
			});
			centerLeftMenu->setID("center-left-menu"_spr);
			this->addChild(centerLeftMenu);

			auto rouletteButtonSprite = CircleButtonSprite::createWithSprite(
				"RL_btn_001.png"_spr,
				1.f,
				CircleBaseColor::Green,
				CircleBaseSize::Small
			);
			rouletteButtonSprite->setScale(1.1f);

			rouletteButton = CCMenuItemSpriteExtra::create(
				rouletteButtonSprite,
				this,
				menu_selector(RLCreatorLayer::onRouletteButton)
			);
			rouletteButton->setID("roulette-button"_spr);
			rouletteButton->setLayout(AnchorLayout::create());
			centerLeftMenu->addChild(rouletteButton);
		}
		else
		{
			auto rouletteButtonSprite = CCSprite::create("RL_btn_big.png"_spr);
			rouletteButtonSprite->setScale(.8f);
			rouletteButton = CCMenuItemSpriteExtra::create(
				rouletteButtonSprite,
				this,
				menu_selector(RLCreatorLayer::onRouletteButton)
			);
			rouletteButton->setID("roulette-button"_spr);
			rouletteButton->setLayout(AnchorLayout::create());
			this->getChildByID("creator-buttons-menu")->addChild(rouletteButton);
			this->getChildByID("creator-buttons-menu")->updateLayout();
		}

		if (RouletteManager::get().isPaused)
			RouletteManager::addExclamationMark(rouletteButton);

		return true;
	}
};
