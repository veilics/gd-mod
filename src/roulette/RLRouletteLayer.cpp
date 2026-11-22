#include "../SorUi.hpp"
#include "RLRouletteLayer.hpp"

// TODO: no..
#include <Geode/Geode.hpp>

#include <matjson.hpp>

#include <rtrp/objects/LevelObject.hpp>

#include "../listfetcher/ListFetcher.hpp"
#include "RLRouletteInfoAlertLayer.hpp"
#include "../custom_layers/RLDifficultySelectAlertLayer.hpp"
#include "../custom_nodes/RLDifficultyNode.hpp"
#include "../custom_nodes/RLLoadingCircle.hpp"
#include "../custom_nodes/RLRepeatingBackground.hpp"
#include "../custom_nodes/RLProgressBar.hpp"
#include "RouletteManager.hpp"
#include "DataManager.hpp"

#include "../SorRooms.hpp"
#include "../SorRoomLayer.hpp"
#include "utils.hpp"
#include "constants.hpp"

// Sorrow: recursive frame dimmer - tints every sprite in a subtree dark and
// translucent (used when the photo background is active so GD-Roulette's
// gold frame chrome stops covering it).
static void sorDimTree(cocos2d::CCNode* node, GLubyte opacity) {
	if (auto spr = typeinfo_cast<cocos2d::CCSprite*>(node)) {
		spr->setOpacity(opacity);
		spr->setColor({ 34, 34, 44 });
	}
	for (auto child : geode::cocos::CCArrayExt<cocos2d::CCNode*>(node->getChildren()))
		sorDimTree(child, opacity);
}

#include "../SorRoomLayer.hpp"
#include "../custom_layers/RLIntegerInputAlertLayer.hpp"
#include "../SorChallenges.hpp"
#include <Geode/binding/SimplePlayer.hpp>

using namespace geode::prelude;


bool RLRouletteLayer::s_sorOpenRoomsOnEnter = false;

RLRouletteLayer::RLRouletteLayer()
	: main_bg{ nullptr },
	info_menu{ nullptr },
	main_menu{ nullptr },
	playing_menu{ nullptr },
	finished_menu{ nullptr },
	error_menu{ nullptr },
	m_loading_circle{ nullptr },
	m_roulette_info_layer{ nullptr },
	m_confirmation_layer{ nullptr },
	m_demon_select_layer{ nullptr },
	m_stats_layer{ nullptr },
	m_selected_difficulty{ GJDifficulty::Easy },
	m_selected_demon_difficulty{ GJDifficulty::Demon },
	m_level{ geode::Err("Uninitialized level") }
{
	ListFetcher::get().setFinishedFetchingCallback([&] {
		finishLevelRoulette();
	});
}

RLRouletteLayer* RLRouletteLayer::create()
{
	auto* ret = new RLRouletteLayer();

	if (ret && ret->init())
		ret->autorelease();
	else
	{
		delete ret;
		ret = nullptr;
	}

	return ret;
}

bool RLRouletteLayer::init()
{
	if (!BaseCustomLayer::init("RL_sideArt_001.png"_spr)) return false;

	// Photo mode: the gold corner art is part of the frame chrome - hide the
	// whole side-art menu (corners) along with the beams and blocks.
	if (m_hasPhotoBg) m_sideArtMenu->setVisible(false);

	auto& rlm = RouletteManager::get();
	auto winSize = CCDirector::sharedDirector()->getWinSize();

	m_selected_difficulty = rlm.previousDifficulty;
	m_selected_demon_difficulty = rlm.previousDemonDifficulty;

	this->setID("RLRouletteLayer");

	main_bg = CCLayer::create();
	info_menu = CCMenu::create();
	main_menu = CCMenu::create();
	playing_menu = CCMenu::create();
	finished_menu = CCMenu::create();
	error_menu = CCMenu::create();

	main_bg->setID("main-bg");
	main_menu->setID("main-menu");
	info_menu->setID("info-menu");

	playing_menu->setID("playing-menu");
	playing_menu->setVisible(false);

	finished_menu->setID("finished-menu");
	finished_menu->setVisible(false);

	error_menu->setID("error-menu");
	error_menu->setVisible(false);

	this->addChild(main_bg, -1);
	this->addChild(info_menu, 11);
	this->addChild(main_menu, 11);
	this->addChild(playing_menu, 11);
	this->addChild(finished_menu, 11);
	this->addChild(error_menu, 11);


	auto title = CCLabelBMFont::create("Level Roulette", "goldFont.fnt");
	title->setPosition({ winSize.width / 2.f, winSize.height - 20.f });
	title->limitLabelWidth(230.f, 1.f, 0.f); // clear of the top-right balance
	title->setID("title-label");
	this->addChild(title);


	// Sorrow: the photo background (set up by BaseCustomLayer) replaces the
	// repeating vanilla bg when it loaded. The node is ALWAYS created - other
	// code fetches it by ID to start/stop the scroll - just hidden.
	{
		auto repeatingBg = RLRepeatingBackground::create(
			fmt::format("game_bg_{:02}_001.png", rl::utils::randomNumber(1, 59)).c_str()
		);
		repeatingBg->setColor({ 40, 40, 40 });
		repeatingBg->setID("repeating-bg");
		m_backgroundMenu->addChild(repeatingBg, -1);
		repeatingBg->setVisible(!m_hasPhotoBg);
	}


	info_menu->setPosition({ .0f, .0f });
	info_menu->setAnchorPoint({ .0f, .0f });

	auto infoButtonSprite = CCSprite::create("RL_optionsBtn_001.png"_spr);
	infoButtonSprite->setScale(.48f);
	auto infoButton = CCMenuItemSpriteExtra::create(
		infoButtonSprite,
		this,
		menu_selector(RLRouletteLayer::onInfoButton)
	);
	infoButton->setPosition({ winSize.width - 20.f, 25.f });
	infoButton->setSizeMult(1.2f);
	infoButton->setID("info-button");
	info_menu->addChild(infoButton);

	if (!Mod::get()->hasSavedValue("show-options-sprite"))
	{
		auto optionsSprite = CCSprite::create("RL_optionsTxt_001.png"_spr);
		optionsSprite->setPosition({ -28.f, 40.f });
		optionsSprite->setID("options-sprite");
		infoButton->addChild(optionsSprite);
	}

	auto statsButtonSprite = CCSprite::create("RL_statsBtn_001.png"_spr);
	statsButtonSprite->setScale(.48f);
	auto statsButton = CCMenuItemSpriteExtra::create(
		statsButtonSprite,
		this,
		menu_selector(RLRouletteLayer::onStatsButton)
	);
	statsButton->setPosition({ winSize.width - 20.f, winSize.height - 20.f });
	statsButton->setVisible(false);
	statsButton->setID("stats-button");
	info_menu->addChild(statsButton);

	// Sorrow: public roulette rooms (Globed + bot rooms)
	// Sorrow: left-corner stack - Rooms on top, Rarity below. Manually
	// positioned (no layout) so they sit clear of the START button and the
	// frame; labels re-fit with updateBGImage when their text changes.
	m_sorBottomBar = CCMenu::create();
	m_sorBottomBar->setPosition({ 0.f, 0.f });
	m_sorBottomBar->setID("sor-bottom-bar");
	this->addChild(m_sorBottomBar, 12);

	// Photo background: instead of GD-Roulette's gold frame (which drew its
	// corners over the photo), a plain translucent panel keeps text readable.
	if (m_hasPhotoBg)
	{
		m_photoPanel = sorrow::ui::panel(380.f, 222.f, { 0, 0, 0 }, 120);
		m_photoPanel->setPosition({ winSize.width / 2.f, winSize.height / 2.f });
		m_photoPanel->setID("sor-photo-panel");
		this->addChild(m_photoPanel, -1);
	}

	// GJListLayer type border
	{
		auto borderTitle = CCLabelBMFont::create("Select Difficulty", "bigFont.fnt");
		borderTitle->setScale(.75f);
		borderTitle->setPosition({ .0f, 104.f });
		borderTitle->setID("border-title");
		main_bg->addChild(borderTitle);

		{
			// @geode-ignore(unknown-resource)
			auto background1 = CCNode::create();
			background1->setID("background-1");
			main_bg->addChild(background1);

			auto topBorderSprite = CCSprite::createWithSpriteFrameName("RL_listBorderTop_001.png"_spr);
			topBorderSprite->setScale(.925f);
			topBorderSprite->setPosition({ .0f, 100.f });
			topBorderSprite->setID("top-border");
			background1->addChild(topBorderSprite, -1);

			auto bottomBorderSprite = CCSprite::createWithSpriteFrameName("RL_listBorderBottom_001.png"_spr);
			bottomBorderSprite->setScale(.925f);
			bottomBorderSprite->setPosition({ .0f, -100.f });
			bottomBorderSprite->setID("bottom-border");
			background1->addChild(bottomBorderSprite, -1);

			auto sideBorderSpriteLeft = CCSprite::createWithSpriteFrameName("RL_listBorderSide_001.png"_spr);
			sideBorderSpriteLeft->setScaleX(1.375f);
			sideBorderSpriteLeft->setScaleY(2.7f);
			sideBorderSpriteLeft->setPosition({ -167.f, .0f });
			sideBorderSpriteLeft->setID("side-border-left");
			background1->addChild(sideBorderSpriteLeft, -2);

			auto sideBorderSpriteRight = CCSprite::createWithSpriteFrameName("RL_listBorderSide_001.png"_spr);
			sideBorderSpriteRight->setScaleX(1.375f);
			sideBorderSpriteRight->setScaleY(2.7f);
			sideBorderSpriteRight->setFlipX(true);
			sideBorderSpriteRight->setPosition({ 167.f, .0f });
			sideBorderSpriteRight->setID("side-border-right");
			background1->addChild(sideBorderSpriteRight, -2);

			auto bg = CCSprite::createWithSpriteFrameName("RL_listBorderBackground_001.png"_spr);
			bg->setColor({ 40, 40, 40 });
			bg->setScaleX(4.2f);
			bg->setScaleY(2.3f);
			bg->setID("background-1");
			// Photo bg active: the fill goes translucent so the photo shows
			// through while text stays readable (was opaque = covered photo).
			bg->setOpacity(m_hasPhotoBg ? 150 : 255);
			background1->addChild(bg, -3);
		}

		{
			// @geode-ignore(unknown-resource)
			auto background2 = CCNode::create();
			background2->setVisible(false);
			background2->setID("background-2");
			main_bg->addChild(background2);

			auto topBorderSprite = CCSprite::createWithSpriteFrameName("RL_listBorderTop_002.png"_spr);
			topBorderSprite->setScale(.925f);
			topBorderSprite->setPosition({ .0f, 100.f });
			topBorderSprite->setID("top-border");
			background2->addChild(topBorderSprite, -1);

			auto bottomBorderSprite = CCSprite::createWithSpriteFrameName("RL_listBorderBottom_002.png"_spr);
			bottomBorderSprite->setScale(.925f);
			bottomBorderSprite->setPosition({ .0f, -100.f });
			bottomBorderSprite->setID("bottom-border");
			background2->addChild(bottomBorderSprite, -1);

			auto sideBorderSpriteLeft = CCSprite::createWithSpriteFrameName("RL_listBorderSide_001.png"_spr);
			sideBorderSpriteLeft->setScaleX(1.375f);
			sideBorderSpriteLeft->setScaleY(2.7f);
			sideBorderSpriteLeft->setPosition({ -167.f, .0f });
			sideBorderSpriteLeft->setID("side-border-left");
			background2->addChild(sideBorderSpriteLeft, -2);

			auto sideBorderSpriteRight = CCSprite::createWithSpriteFrameName("RL_listBorderSide_001.png"_spr);
			sideBorderSpriteRight->setScaleX(1.375f);
			sideBorderSpriteRight->setScaleY(2.7f);
			sideBorderSpriteRight->setFlipX(true);
			sideBorderSpriteRight->setPosition({ 167.f, .0f });
			sideBorderSpriteRight->setID("side-border-right");
			background2->addChild(sideBorderSpriteRight, -2);

			auto bg = CCSprite::createWithSpriteFrameName("RL_listBorderBackground_002.png"_spr);
			bg->setColor({ 40, 40, 40 });
			bg->setScaleX(21.5f);
			bg->setScaleY(1.1f);
			bg->setID("background-2");
			bg->setOpacity(m_hasPhotoBg ? 150 : 255);
			background2->addChild(bg, -3);
		}

		// With the photo background on, HIDE the whole frame chrome (gold
		// border beams, bottom blocks, fills) - the photo IS the background.
		if (m_hasPhotoBg) {
			for (const char* id : { "background-1", "background-2" }) {
				if (auto n = main_bg->getChildByID(id)) n->setVisible(false);
			}
		}

		main_bg->ignoreAnchorPointForPosition(false);
		main_bg->setPosition({ winSize.width, winSize.height });
	}


	main_menu->addChild(
		createDifficultyButton(
			GJDifficulty::Easy,
			CCSprite::createWithSpriteFrameName("difficulty_01_btn_001.png"),
			{ -100.f, 40.f }, 1.2f
		)
	);
	main_menu->addChild(
		createDifficultyButton(
			GJDifficulty::Normal,
			CCSprite::createWithSpriteFrameName("difficulty_02_btn_001.png"),
			{ .0f, 40.f }, 1.2f
		)
	);
	main_menu->addChild(
		createDifficultyButton(
			GJDifficulty::Hard,
			CCSprite::createWithSpriteFrameName("difficulty_03_btn_001.png"),
			{ 100.f, 40.f }, 1.2f
		)
	);
	main_menu->addChild(
		createDifficultyButton(
			GJDifficulty::Harder,
			CCSprite::createWithSpriteFrameName("difficulty_04_btn_001.png"),
			{ -100.f, -30.f }, 1.2f
		)
	);
	main_menu->addChild(
		createDifficultyButton(
			GJDifficulty::Insane,
			CCSprite::createWithSpriteFrameName("difficulty_05_btn_001.png"),
			{ .0f, -30.f }, 1.2f
		)
	);
	main_menu->addChild(
		createDifficultyButton(
			GJDifficulty::Demon,
			RLDifficultyNode::create(
				m_selected_difficulty >= GJDifficulty::Demon
					? m_selected_demon_difficulty
					: static_cast<GJDifficulty>(-2)
			),
			{ 100.f, -30.f }, 1.2f
		)
	);
	auto plusButton = CCMenuItemSpriteExtra::create(
		CCSprite::createWithSpriteFrameName("GJ_plus2Btn_001.png"),
		this,
		menu_selector(RLRouletteLayer::onPlusButton)
	);
	plusButton->setPosition({ 140.f, -25.f });
	plusButton->setSizeMult(1.2f);
	plusButton->setID("demon-plus-button");
	plusButton->setVisible(
		rl::utils::getIndexOf(
			DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(), true
		) == 0 &&
		m_selected_difficulty >= GJDifficulty::Demon
	);
	main_menu->addChild(plusButton);

	// Rebuilt: a proper ButtonSprite - text auto-sized and centered inside a
	// native green button (the raw RL_longBtn sprite mis-scaled the label on
	// wide screens: oversized button, text cut at the bottom edge).
	auto startButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Start", "bigFont.fnt", "GJ_button_01.png", 1.f),
		this,
		menu_selector(RLRouletteLayer::onStartButton)
	);
	startButton->setPosition({ .0f, -132.f });
	startButton->setID("start-button");
	startButton->setScale(.9f);
	main_menu->addChild(startButton);


	m_loading_circle = RLLoadingCircle::create();
	m_loading_circle->positionCenter();
	m_loading_circle->fadeOut();
	this->addChild(m_loading_circle);


	auto levelNameButtonLabel = CCLabelBMFont::create("LevelName", "bigFont.fnt");
	levelNameButtonLabel->limitLabelWidth(100.f, 1.f, .1f);
	levelNameButtonLabel->setID("button-label");
	auto levelNameButton = CCMenuItemSpriteExtra::create(
		levelNameButtonLabel,
		this,
		menu_selector(RLRouletteLayer::onLevelInfo)
	);
	levelNameButton->setSizeMult(1.2f);
	levelNameButton->setPosition({ 50.f, 60.f });
	levelNameButton->setID("level-name-button");
	levelNameButton->setTag(1);
	playing_menu->addChild(levelNameButton);

	auto levelCreatorButtonLabel = CCLabelBMFont::create("LevelCreator", "goldFont.fnt");
	levelCreatorButtonLabel->limitLabelWidth(140.f, .8f, .2f);
	levelCreatorButtonLabel->setID("button-label");
	auto levelCreatorButton = CCMenuItemSpriteExtra::create(
		levelCreatorButtonLabel,
		this,
		menu_selector(RLRouletteLayer::onLevelInfo)
	);
	levelCreatorButton->setSizeMult(1.2f);
	levelCreatorButton->setPosition({ 50.f, 37.f });
	levelCreatorButton->setID("level-creator-button");
	levelCreatorButton->setTag(2);
	playing_menu->addChild(levelCreatorButton);

	auto levelPlayButtonSprite = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
	levelPlayButtonSprite->setScale(.9f);
	auto levelPlayButtonItem = CCMenuItemSpriteExtra::create(
		levelPlayButtonSprite,
		this,
		menu_selector(RLRouletteLayer::onPlayButton)
	);
	levelPlayButtonItem->setPosition({ -100.f, -40.f });
	levelPlayButtonItem->setID("play-button");
	levelPlayButtonItem->setTag(4);
	playing_menu->addChild(levelPlayButtonItem);

	auto difficultyNode = RLDifficultyNode::create({
		GJDifficulty::Normal, GJFeatureState::None
	});
	difficultyNode->setPosition({ -100.f, 40.f });
	difficultyNode->setScale(1.5f);
	difficultyNode->setID("difficulty-node");
	difficultyNode->setTag(5);
	playing_menu->addChild(difficultyNode);

	auto attemptText = CCLabelBMFont::create(
		fmt::format("Attempt {}", rlm.gameState.levelAttempts).c_str(),
		"goldFont.fnt"
	);
	attemptText->setPosition({ 50.f, -27.f });
	attemptText->setScale(.8f);
	attemptText->setID("attempt-count-label");
	playing_menu->addChild(attemptText);

	auto percentageText = CCLabelBMFont::create(
		fmt::format("{}%", rlm.currentPercentageGoal).c_str(),
		"goldFont.fnt"
	);
	percentageText->setPosition({ 50.f, -60.f });
	percentageText->setScale(.8f);
	percentageText->setID("percentage-text");
	playing_menu->addChild(percentageText);

	auto percentageBg = cocos2d::extension::CCScale9Sprite::create("square02b_small.png");
	percentageBg->setPosition({ 20.f, -77.f });
	percentageBg->setContentSize({ 60.f, 30.f });
	percentageBg->setAnchorPoint({ .0f, .0f });
	percentageBg->setColor({ 0, 0, 0 });
	percentageBg->setOpacity(100);
	percentageBg->setID("percentage-bg");
	playing_menu->addChild(percentageBg, -1);

	auto progressBar = RLProgressBar::create(rlm.gameState.levelPercentage, false);
	progressBar->setPosition({ .0f, 102.f });
	progressBar->setColor({ 255, 255, 0 });
	progressBar->setScale(.9f);
	progressBar->setID("progress-bar");
	playing_menu->addChild(progressBar);

	auto skipButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Skip", 50, true, "bigFont.fnt", "GJ_button_06.png", .0f, 1.f),
		this,
		menu_selector(RLRouletteLayer::onSkipButton)
	);
	skipButton->setPosition({ -90.f, -135.f });
	skipButton->setID("skip-button");
	playing_menu->addChild(skipButton);

	auto nextButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Next", 50, true, "bigFont.fnt", "GJ_button_01.png", .0f, .8f),
		this,
		menu_selector(RLRouletteLayer::onNextButton)
	);
	nextButton->setPosition({ .0f, -135.f });
	nextButton->setID("next-button");
	playing_menu->addChild(nextButton);

	auto resetButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Reset", 50, true, "bigFont.fnt", "GJ_button_06.png", .0f, .8f),
		this,
		menu_selector(RLRouletteLayer::onResetButton)
	);
	resetButton->setPosition({ 90.f, -135.f });
	resetButton->setID("reset-button");
	playing_menu->addChild(resetButton);


	auto ggText = CCLabelBMFont::create("GG!", "goldFont.fnt");
	ggText->setPosition({ .0f, 65.f });
	ggText->setID("gg-label");
	finished_menu->addChild(ggText);

	auto ggSkipsUsedText = CCLabelBMFont::create("Skips Used: ", "bigFont.fnt");
	ggSkipsUsedText->setPosition({ .0f, 35.f });
	ggSkipsUsedText->setID("skips-label");
	finished_menu->addChild(ggSkipsUsedText);

	auto ggNumLevelsText = CCLabelBMFont::create("Levels Played: ", "bigFont.fnt");
	ggNumLevelsText->setPosition({ .0f, 7.f });
	ggNumLevelsText->setID("levels-played-label");
	finished_menu->addChild(ggNumLevelsText);

	auto ggTotalAttempts = CCLabelBMFont::create("Total Attempts: ", "bigFont.fnt");
	ggTotalAttempts->limitLabelWidth(100.f, 1.f, .85f);
	ggTotalAttempts->setPosition({ .0f, -20.f });
	ggTotalAttempts->setID("total-attempts-label");
	finished_menu->addChild(ggTotalAttempts);

	auto levelsPlayedButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("View Levels", 100, true, "bigFont.fnt", "GJ_button_02.png", .0f, .8f),
		this,
		menu_selector(RLRouletteLayer::onLevelsPlayedButton)
	);
	levelsPlayedButton->setPosition({ -60.f, -140.f });
	levelsPlayedButton->setID("levels-played-button");
	finished_menu->addChild(levelsPlayedButton);

	auto finishedResetButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Reset", 50, true, "bigFont.fnt", "GJ_button_06.png", .0f, .8f),
		this,
		menu_selector(RLRouletteLayer::onResetButton)
	);
	finishedResetButton->setPosition({ 80.f, -140.f });
	finishedResetButton->setID("reset-button");
	finished_menu->addChild(finishedResetButton);


	auto errorText = CCLabelBMFont::create("An error has occurred", "bigFont.fnt");
	errorText->setPosition({ .0f, 105.f });
	errorText->setColor({ 255, 0, 0 });
	errorText->setScale(.725f);
	errorText->setID("title-label");
	error_menu->addChild(errorText);

	auto errorReasonText = TextArea::create(
		"ErrorReason",
		"chatFont.fnt",
		1.f, 300.f, { .5f, .5f }, 20.f, false
	);
	errorReasonText->setPosition({ .0f, 15.f });
	errorReasonText->setID("reason-label");
	error_menu->addChild(errorReasonText);

	auto refreshButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Refresh", 50, true, "bigFont.fnt", "GJ_button_01.png", .0f, .9f),
		this,
		menu_selector(RLRouletteLayer::onRefreshButton)
	);
	refreshButton->setPosition({ -70.f, -140.f });
	refreshButton->setID("refresh-button");
	error_menu->addChild(refreshButton);

	auto errorResetButton = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Reset", 50, true, "bigFont.fnt", "GJ_button_06.png", .0f, .8f),
		this,
		menu_selector(RLRouletteLayer::onResetButton)
	);
	errorResetButton->setID("reset-button");
	error_menu->addChild(errorResetButton);
	errorResetButton->setPosition({ 70.f, -140.f });


	onListChanged();

	// Old GD-Roulette difficulty row / plus / start / frame title: parked
	// off-screen (their state logic still runs, the new screens drive it).
	for (const auto d : { GJDifficulty::Easy, GJDifficulty::Normal, GJDifficulty::Hard, GJDifficulty::Harder, GJDifficulty::Insane, GJDifficulty::Demon })
		getDifficultyButton(d)->setPosition({ -10000.f, -10000.f });
	main_menu->getChildByID("demon-plus-button")->setPositionX(-10000.f);
	main_menu->getChildByID("start-button")->setPosition({ -10000.f, -10000.f });
	main_bg->getChildByID("border-title")->setPositionX(-10000.f);
	info_menu->getChildByID("info-button")->setPosition({ -10000.f, -10000.f });

	this->sorBuildHome();
	this->sorBuildSolo();

	// After a room game ends the director brings you back here and opens
	// the room popup (lobby) - see RoomDirector::returnToRoomScreen.
	sorrow::rooms::RoomDirector::openRoomsHook() = [] {
		auto& r = RouletteManager::get();
		auto* running = CCDirector::sharedDirector()->getRunningScene();
		if (r.rouletteLayer && r.rouletteLayer->getParent() == running) {
			r.rouletteLayer->onRoomsButton(nullptr);
			return true;
		}
		// not on the roulette screen: open it, the popup follows on enter
		if (auto* layer = RLRouletteLayer::create()) {
			r.rouletteLayer = layer;
			s_sorOpenRoomsOnEnter = true;
			auto* scene = CCScene::create();
			scene->addChild(layer);
			CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(.5f, scene));
		}
		return true;
	};
	m_sorHome = true;
	this->sorCountRooms();
	this->schedule(schedule_selector(RLRouletteLayer::sorTick), .1f);
	this->sorTick(0.f);


	if (rlm.gameState.levelPercentage == 100 && rlm.gameState.hasReachedGoal)
	{
		main_bg->getChildByID("border-title")->setVisible(false);
		sorShowFrame(2);
		main_menu->setVisible(false);

		onNextButton(nullptr);
	}
	else if (rlm.isPaused)
	{
		rlm.isPlaying = true;
		rlm.isPaused = false;

		rlm.gameTimer.setElapsedTime(rlm.gameState.playTime);
		rlm.gameTimer.start();

		main_bg->getChildByID("border-title")->setVisible(false);
		sorShowFrame(2);
		main_menu->setVisible(false);

		setupForNextLevel(false, true);

		rl::utils::createNotificationToast(
			this, "Resuming round of Roulette...", 1.f, 85.f
		);

		ListFetcher::get().getLevelInfo(rlm.gameState.levelID, m_level);
	}

	return true;
}

// Which list frame is up: 0 none, 1 difficulty select, 2 playing/finished.
// With the photo background the gold chrome stays hidden whatever the view.
void RLRouletteLayer::sorShowFrame(int which)
{
	m_sorFrame = which;
	if (auto n = main_bg->getChildByID("background-1")) n->setVisible(!m_hasPhotoBg && which == 1);
	if (auto n = main_bg->getChildByID("background-2")) n->setVisible(!m_hasPhotoBg && which == 2);
}

void RLRouletteLayer::sorTick(float dt)
{
	const bool mainView = main_menu && main_menu->isVisible();
	const bool home = mainView && m_sorHome;
	const bool solo = mainView && !m_sorHome;
	if (m_sorBottomBar) m_sorBottomBar->setVisible(false);
	if (m_homeRoot) { m_homeRoot->setVisible(home); m_homeRoot->setEnabled(home); }
	if (m_soloRoot) { m_soloRoot->setVisible(solo); m_soloMenu->setEnabled(solo); for (auto* c : m_gridCells) c->setEnabled(solo && m_gridRoot->isVisible()); }
	if (auto t = this->getChildByID("title-label")) t->setVisible(!solo);
	if (home) {
		m_homeTimer -= dt;
		if (m_homeTimer <= 0.f) { m_homeTimer = 1.f; this->sorRefreshHome(); }
	}
	if (m_photoPanel) {
		const bool any = solo || playing_menu->isVisible() || finished_menu->isVisible() || error_menu->isVisible();
		m_photoPanel->setVisible(any);
		if (solo) sorrow::ui::resize(m_photoPanel, 420.f, 262.f); else sorrow::ui::resize(m_photoPanel, 380.f, 222.f);
		m_photoPanel->setPositionY(solo ? 156.f : CCDirector::sharedDirector()->getWinSize().height / 2.f);
	}
	if (m_hasPhotoBg) {
		for (const char* id : { "background-1", "background-2" })
			if (auto n = main_bg->getChildByID(id); n && n->isVisible()) n->setVisible(false);
	}
}

void RLRouletteLayer::onClose(CCObject*)
{
	auto& rlm = RouletteManager::get();

	if (!rlm.isPlaying && main_menu->isVisible() && !m_sorHome)
	{
		this->onSorBack(nullptr); // Solo setup -> Home
		return;
	}

	if (rlm.isPlaying)
	{
		m_confirmation_layer = RLConfirmationAlertLayer::create({
			"Woah there!",
			"Would you like to <cr>quit</c> or <co>pause</c> the roulette?",
			[&](auto cl) {
				rlm.gameTimer.pause();
				rlm.saveState();
				rlm.isPlaying = false;
				rlm.isPaused = true;

				onClose(nullptr);
			},
			[&](auto cl) {
				rlm.reset();

				onClose(nullptr);
			},
			"Pause", "Quit"
		});
		if (m_confirmation_layer)
			m_confirmation_layer->show();
	}
	else
	{
		this->setKeypadEnabled(false);
		this->setKeyboardEnabled(false);
		rlm.rouletteLayer = nullptr;

		CCDirector::sharedDirector()->popSceneWithTransition(
			.5f, PopTransition::kPopTransitionFade
		);
	}
}

static const std::array<std::string, 5> SOR_RARITY_CYCLE{
	"any", "featured", "epic", "legendary", "mythic"
};

void RLRouletteLayer::onRarityButton(CCObject*)
{
	auto& rlm = RouletteManager::get();
	size_t idx = 0;
	for (size_t i = 0; i < SOR_RARITY_CYCLE.size(); ++i)
		if (SOR_RARITY_CYCLE[i] == rlm.sorRarity) idx = i;
	rlm.sorRarity = SOR_RARITY_CYCLE[(idx + 1) % SOR_RARITY_CYCLE.size()];

	auto pretty = [](const std::string& r) -> std::string {
		if (r == "featured") return "Featured";
		if (r == "epic") return "Epic";
		if (r == "legendary") return "Legendary";
		if (r == "mythic") return "Mythic";
		return std::string("Any");
	};
	this->sorSyncBottomBar();
	Notification::create(fmt::format("Rarity filter: {}", pretty(rlm.sorRarity)).c_str(), NotificationIcon::Success, 2.f)->show();
}

void RLRouletteLayer::sorSyncBottomBar()
{
	if (m_roomsBtnSprite && m_roomsBtnSprite->m_label) {
		const auto& code = sorrow::rooms::state().code;
		m_roomsBtnSprite->m_label->setString(code.empty() ? "Multiplayer" : fmt::format("Room {}", code).c_str());
		m_roomsBtnSprite->m_label->limitLabelWidth(88.f, .6f, .2f);
	}
	if (m_rarityBtnSprite && m_rarityBtnSprite->m_label) {
		const auto& r = RouletteManager::get().sorRarity;
		m_rarityBtnSprite->m_label->setString(fmt::format("Rarity: {}",
			r == "featured" ? "Featured" : r == "epic" ? "Epic" : r == "legendary" ? "Legendary" : r == "mythic" ? "Mythic" : "Any").c_str());
		m_rarityBtnSprite->m_label->limitLabelWidth(88.f, .6f, .2f);
	}
}

void RLRouletteLayer::onRoomsButton(CCObject*)
{
	auto layer = SorrowRoomLayer::create();
	if (layer) {
		layer->m_noElasticity = true;
		layer->show();
	}
}

void RLRouletteLayer::onInfoButton(CCObject*)
{
	if (RouletteManager::get().isPlaying)
		return rl::utils::createNotificationToast(
			this, "no", .5f, 85.f
		);

	if (!Mod::get()->hasSavedValue("show-options-sprite"))
	{
		info_menu->getChildByID("info-button")->getChildByID("options-sprite")->setVisible(false);
		Mod::get()->setSavedValue<bool>("show-options-sprite", false);
	}

	if ((m_roulette_info_layer = RLRouletteInfoAlertLayer::create()))
		m_roulette_info_layer->show();
}

void RLRouletteLayer::onStatsButton(CCObject*)
{
	auto& rlm = RouletteManager::get();

	rlm.gameState.playTime = rlm.gameTimer.getElapsedTime();

	if ((m_stats_layer = RLStatsAlertLayer::create(rlm.gameState)))
		m_stats_layer->show();
}

void RLRouletteLayer::onDifficultyButton(CCObject* sender)
{
	if (rl::utils::getIndexOf(DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(), true) != 0)
		return;

	auto button = static_cast<CCMenuItemSpriteExtra*>(sender);
	auto difficulty = static_cast<GJDifficulty>(sender->getTag());
	auto array = DataManager::get<DMArrayKey::DIFFICULTY_ARRAY>();
	int prevIdx = rl::utils::getIndexOf(array.asVector(), true);

	array.set(prevIdx, false);
	array.set(rl::constants::save_data::DIFF_TO_IDX.at(difficulty), true);
	RouletteManager::get().previousDifficulty = difficulty;
	m_selected_difficulty = difficulty;

	static_cast<RLDifficultyNode*>(
		getDifficultyButton(GJDifficulty::Demon)->getChildByID("sprite-node")
	)->setDifficulty(
		m_selected_difficulty == GJDifficulty::Demon
			? RouletteManager::get().previousDemonDifficulty
			: static_cast<GJDifficulty>(-2)
	);
	getDifficultyButton(rl::constants::save_data::IDX_TO_DIFF.at(prevIdx))->setColor({ 125, 125, 125 });
	getDifficultyButton(difficulty)->setColor({ 255, 255, 255 });

	main_menu->getChildByID("demon-plus-button")->setVisible(
		m_selected_difficulty >= GJDifficulty::Demon
	);
}

void RLRouletteLayer::onStartButton(CCObject*)
{
	auto& rlm = RouletteManager::get();

	main_menu->setVisible(false);
	main_bg->getChildByID("border-title")->setVisible(false);
	sorShowFrame(2);

	rlm.gameState.startTime = rl::utils::getUnixEpoch();
	rlm.gameTimer.start();

	getRandomListLevel();
	setupForNextLevel(false, true);

	rlm.isPlaying = true;
}

void RLRouletteLayer::onPlusButton(CCObject*)
{
	m_demon_select_layer = RLDifficultySelectAlertLayer::create({
		"Demon Filter",
		{ static_cast<GJDifficulty>(-2), GJDifficulty::DemonEasy, GJDifficulty::DemonMedium, GJDifficulty::Demon, GJDifficulty::DemonInsane, GJDifficulty::DemonExtreme },
		m_selected_demon_difficulty,
		[&](GJDifficulty currentDifficulty, GJDifficulty previousDifficulty)
		{
			auto array = DataManager::get<DMArrayKey::DEMON_DIFFICULTY_ARRAY>();

			m_selected_demon_difficulty = currentDifficulty;

			array.set(rl::constants::save_data::DEMON_DIFF_TO_IDX.at(previousDifficulty), false);
			array.set(rl::constants::save_data::DEMON_DIFF_TO_IDX.at(currentDifficulty), true);
			RouletteManager::get().previousDemonDifficulty = currentDifficulty;

			static_cast<RLDifficultyNode*>(
				getDifficultyButton(GJDifficulty::Demon)->getChildByID("sprite-node")
			)->setDifficulty(currentDifficulty);
		}
	});
	if (m_demon_select_layer)
	{
		m_demon_select_layer->m_scene = this;
		m_demon_select_layer->show();
	}
}

void RLRouletteLayer::onLevelInfo(CCObject* sender)
{
	const auto& level = m_level.unwrap();
	auto textButton = static_cast<CCMenuItemSpriteExtra*>(sender);
	std::string text;

	switch (textButton->getTag())
	{
		case 1: text = level.first.name; break;
		case 2: text = level.second.name; break;
		case 3: text = fmt::format("{}", level.first.levelID); break;
		default: text = "[invalid]"; break;
	}

	clipboard::write(text);
	rl::utils::createNotificationToast(this, "Copied to Clipboard", .5f, 85.f);
}

void RLRouletteLayer::onPlayButton(CCObject*)
{
	if (ListFetcher::get().is_fetching)
		return;

	LevelInfoLayer* layer;

	if (m_level.isOkAnd([](const auto& lvl) { return lvl.first.levelID != 0; }))
	{
		const auto& lvl = m_level.unwrap();

		layer = LevelInfoLayer::create(rl::utils::createLevelFromResponse(lvl), false);
		layer->downloadLevel();
	}
	else
		layer = LevelInfoLayer::create(GJGameLevel::create(), true);

	auto scene = CCScene::create();
	scene->addChild(layer);

	CCDirector::sharedDirector()->pushScene(
		CCTransitionFade::create(.5f, scene)
	);
}

void RLRouletteLayer::onNextButton(CCObject*)
{
	if (ListFetcher::get().is_fetching)
		return;

	auto& rlm = RouletteManager::get();

	if (rlm.gameState.levelPercentage == 100)
	{
		auto winSize = CCDirector::sharedDirector()->getWinSize();

		rlm.gameTimer.pause();
		rlm.hasFinishedARound = rlm.gameState.startTime >= ROULETTE_BUILD_TIMESTAMP;

		setupForNextLevel();

		static_cast<CCLabelBMFont*>(finished_menu->getChildByID("skips-label"))->setString(
			fmt::format("Skips Used: {}", rlm.gameState.skipsUsed).c_str()
		);
		static_cast<CCLabelBMFont*>(finished_menu->getChildByID("levels-played-label"))->setString(
			fmt::format("Levels Played: {}", rlm.gameState.numLevels).c_str()
		);
		static_cast<CCLabelBMFont*>(finished_menu->getChildByID("total-attempts-label"))->setString(
			fmt::format("Total Attempts: {}", rlm.gameState.totalAttempts).c_str()
		);

		info_menu->getChildByID("stats-button")->setVisible(true);
		info_menu->getChildByID("stats-button")->runAction(
			CCSequence::create(
				CCFadeOut::create(.25f),
				CCMoveTo::create(.0f, { winSize.width / 2.f, 100.f }),
				CCFadeIn::create(.25f),
				nullptr
			)
		);

		playing_menu->setVisible(false);
		finished_menu->setVisible(true);
	}
	else if (rlm.gameState.hasReachedGoal)
	{
		rlm.gameState.hasReachedGoal = false;
		rlm.currentPercentageGoal = rlm.gameState.levelPercentageGoal;
		rlm.gameState.levelPercentage = 0;
		rlm.gameState.levelAttempts = 0;

		setupForNextLevel(false, true, 40.f);

		static_cast<CCLabelBMFont*>(
			playing_menu->getChildByID("percentage-text")
		)->setString(fmt::format("{}%", rlm.currentPercentageGoal).c_str());
		static_cast<CCLabelBMFont*>(
			playing_menu->getChildByID("attempt-count-label")
		)->setString(fmt::format("Attempt {}", rlm.gameState.levelAttempts).c_str());

		if (m_level.isOkAnd([](const auto& level) { return level.first.levelID != 0; }))
			rlm.gameState.playedLevels.emplace_back(m_level.unwrap().first.levelID);

		getRandomListLevel();
	}
	else
		rl::utils::createNotificationToast(this, fmt::format("You need to get at least {}%!", rlm.currentPercentageGoal), .5f, 85.f);
}

void RLRouletteLayer::onRefreshButton(CCObject*)
{
	RouletteManager::get().gameState.hasReachedGoal = true;

	error_menu->setVisible(false);
	static_cast<RLRepeatingBackground*>(
		m_backgroundMenu->getChildByID("repeating-bg")
	)->start();

	onNextButton(nullptr);
}

void RLRouletteLayer::onLevelsPlayedButton(CCObject*)
{
	auto list = GJLevelList::create();

	auto listData = rl::utils::createGJListDictFromLevels(
		RouletteManager::get().gameState.playedLevels,
		RouletteManager::get().gameState.startTime,
		true
	);

	list->dataLoaded(listData.get());
	list->m_difficulty = static_cast<int>(
		m_selected_difficulty == GJDifficulty::Demon
			? m_selected_demon_difficulty
			: m_selected_difficulty
	);
	list->m_isEditable = true;

	auto lll = LevelListLayer::create(list);
	auto scene = CCScene::create();
	scene->addChild(lll);

	CCDirector::sharedDirector()->pushScene(
		CCTransitionFade::create(
			.5f,
			scene
		)
	);

	// onBack bug fix
	lll->m_levelList->m_listType = GJLevelType::Default;
}

void RLRouletteLayer::onResetButton(CCObject*)
{
	if (ListFetcher::get().is_fetching)
		return;

	auto winSize = CCDirector::sharedDirector()->getWinSize();

	RouletteManager::get().reset();

	m_level = geode::Err("No level fetched");

	setupForNextLevel();

	main_menu->setVisible(true);
	playing_menu->setVisible(false);
	finished_menu->setVisible(false);
	error_menu->setVisible(false);

	main_bg->getChildByID("border-title")->setVisible(true);
	sorShowFrame(1);
	info_menu->getChildByID("info-button")->setVisible(true);
	info_menu->getChildByID("stats-button")->setPosition(
		{ winSize.width - 20.f, winSize.height - 20.f }
	);
	info_menu->getChildByID("stats-button")->setVisible(false);

	static_cast<CCLabelBMFont*>(playing_menu->getChildByID("percentage-text"))->setString(
		fmt::format("{}%", RouletteManager::get().currentPercentageGoal).c_str()
	);

	static_cast<RLProgressBar*>(
		playing_menu->getChildByID("progress-bar")
	)->setProgress(.0f);

	main_menu->getChildByID("demon-plus-button")->setPositionY(-20.f); // (X stays parked off-screen)
	m_sorHome = false;   // a finished/reset run lands back on the Solo setup
	this->sorRefreshSolo();
	main_menu->getChildByID("start-button")->setVisible(true);

	static_cast<RLRepeatingBackground*>(
		m_backgroundMenu->getChildByID("repeating-bg")
	)->start();

	m_loading_circle->positionCenter();
}

void RLRouletteLayer::onSkipButton(CCObject*)
{
	if (ListFetcher::get().is_fetching)
		return;

	auto& rlm = RouletteManager::get();

	if (rlm.gameState.levelPercentage == 100 || rlm.gameState.hasReachedGoal)
	{
		rl::utils::createNotificationToast(this, "Skip not used, you have already reached the goal!", .5f, 85.f);

		onNextButton(nullptr);

		return;
	}

	if (rlm.gameState.skipsUsed < Mod::get()->getSettingValue<int64_t>("max-skips"))
	{
		rlm.gameState.skipsUsed++;
		rlm.gameState.hasReachedGoal = false;
		rlm.currentPercentageGoal = rlm.gameState.levelPercentageGoal;

		setupForNextLevel(false, true, 40.f);

		static_cast<CCLabelBMFont*>(
			playing_menu->getChildByID("attempt-count-label")
		)->setString("Attempt 0");

		if (rlm.gameState.playedLevels.size())
			rlm.gameState.playedLevels.pop_back();

		getRandomListLevel();
	}
	else
		rl::utils::createNotificationToast(this, "You don't have any skips left!", .5f, 85.f);
}

void RLRouletteLayer::finishLevelRoulette()
{
	if (m_level.isErr())
	{
		setupForNextLevel();

		static_cast<TextArea*>(error_menu->getChildByID("reason-label"))->setString(m_level.unwrapErr());

		playing_menu->setVisible(false);
		error_menu->setVisible(true);

		static_cast<RLRepeatingBackground*>(
			m_backgroundMenu->getChildByID("repeating-bg")
		)->stop();

		return;
	}

	setupForNextLevel(true, false, 40.f);
	main_menu->setVisible(false);
	info_menu->getChildByID("info-button")->setVisible(false);
	info_menu->getChildByID("stats-button")->setVisible(true);

	const auto& [level, creator] = m_level.unwrap();
	auto& rlm = RouletteManager::get();
	rlm.gameState.levelID = level.levelID;

	// (Multiplayer rooms roll their own levels on the bot - the solo
	// roulette never touches room state.)

	static_cast<CCLabelBMFont*>(
		playing_menu->getChildByID("level-name-button")->getChildByID("button-label")
	)->setString(level.name.c_str());
	static_cast<CCLabelBMFont*>(
		playing_menu->getChildByID("level-creator-button")->getChildByID("button-label")
	)->setString(fmt::format("by {}", creator.name).c_str());

	static_cast<RLDifficultyNode*>(playing_menu->getChildByID("difficulty-node"))->setDifficulty({
		rl::utils::getDifficultyFromResponse(level), rl::utils::getFeatureStateFromResponse(level)
	});
	playing_menu->getChildByID("difficulty-node")->setVisible(true);
	playing_menu->getChildByID("difficulty-node")->setPositionY(40.f);

	playing_menu->setVisible(true);

	rlm.saveState();
}

void RLRouletteLayer::setupForNextLevel(bool levelTextVisible, bool enableLoadingCircle, float loadingCirclePosYOffset)
{
	// hides PlayingMenu's RLDifficultyNode and level text
	for (int i = 1; i < 3; i++)
		playing_menu->getChildByTag(i)->setVisible(levelTextVisible);
	playing_menu->getChildByTag(5)->setVisible(false);

	if (enableLoadingCircle)
		m_loading_circle->show();
	else
		m_loading_circle->stopAndHide();
	m_loading_circle->setPositionY(
		CCDirector::sharedDirector()->getWinSize().height / 2.f + loadingCirclePosYOffset
	);
}

void RLRouletteLayer::onListChanged()
{
	sorSyncBottomBar(); // room code may have changed (join/leave in popup)
	const int selectedList = rl::utils::getIndexOf(
		DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(),
		true
	);

	for (const auto difficulty : {
			GJDifficulty::Easy, GJDifficulty::Normal, GJDifficulty::Hard,
			GJDifficulty::Harder, GJDifficulty::Insane, GJDifficulty::Demon
	}) {
		auto button = getDifficultyButton(difficulty);

		button->setColor({ 125, 125, 125 });
		button->setEnabled(selectedList == 0);
	}

	GJDifficulty visualDifficulty;

	switch (selectedList)
	{
		case 0:
			onDifficultyButton(
				getDifficultyButton(m_selected_difficulty)
			);
		break;

		case 1:
			visualDifficulty = GJDifficulty::Demon;
		break;

		case 2:
			visualDifficulty = GJDifficulty::Insane;
		break;

		case 3:
			visualDifficulty = GJDifficulty::Easy;
		break;

		default:
			break;
	};

	if (selectedList != 0)
	{
		getDifficultyButton(visualDifficulty)->setColor({ 255, 255, 255 });

		static_cast<RLDifficultyNode*>(
			getDifficultyButton(GJDifficulty::Demon)->getChildByID("sprite-node")
		)->setDifficulty(
			visualDifficulty == GJDifficulty::Demon
				? GJDifficulty::DemonExtreme
				: static_cast<GJDifficulty>(-2)
		);

		main_menu->getChildByID("demon-plus-button")->setVisible(false);
	}
}

// set roulette button exclamation mark
void RLRouletteLayer::onExitTransitionDidStart()
{
	BaseCustomLayer::onExitTransitionDidStart();

	if (auto nextScene = static_cast<CCScene*>(CCDirector::sharedDirector()->m_pobScenesStack->firstObject()))
	{
		CCMenuItemSpriteExtra* rouletteButton = nullptr;

		if (Loader::get()->isModLoaded("ninxout.redash"))
		{
			if (auto menuLayer = nextScene->getChildByID("MenuLayer"))
			{
				rouletteButton = static_cast<CCMenuItemSpriteExtra*>(
					menuLayer->querySelector(
						fmt::format("right-side-menu > {}", "roulette-button"_spr)
					)
				);
			}
		}
		else if (auto creatorLayer = nextScene->getChildByID("CreatorLayer"))
		{
			bool hasBigButton = Mod::get()->getSettingValue<bool>("use-big-button");

			rouletteButton = static_cast<CCMenuItemSpriteExtra*>(
				creatorLayer->querySelector(fmt::format(
					"{} > {}",
					hasBigButton ? "creator-buttons-menu" : "center-left-menu"_spr,
					"roulette-button"_spr
				))
			);
		}

		// where tf my button go??
		if (!rouletteButton)
			return;

		if (RouletteManager::get().isPaused)
			RouletteManager::get().addExclamationMark(rouletteButton);
		else if (auto exMark = rouletteButton->getChildByID("exclamation-mark"_spr))
			exMark->removeFromParentAndCleanup(true);
	}
}

void RLRouletteLayer::onEnterTransitionDidFinish()
{
	BaseCustomLayer::onEnterTransitionDidFinish();

	auto& rlm = RouletteManager::get();
	this->sorCountRooms();
	if (s_sorOpenRoomsOnEnter) { s_sorOpenRoomsOnEnter = false; this->onRoomsButton(nullptr); }

	if (!rlm.isPlaying || !rlm.hasEnteredPlayLayer)
		return;

	auto progressBar = static_cast<RLProgressBar*>(
		playing_menu->getChildByID("progress-bar")
	);

	if (static_cast<int>(progressBar->getProgress()) != rlm.gameState.levelPercentage)
	{
		progressBar->setProgress(rlm.gameState.levelPercentage);
		progressBar->playFillingAnimation();
	}

	static_cast<CCLabelBMFont*>(
		playing_menu->getChildByID("attempt-count-label")
	)->setString(fmt::format("Attempt {}", rlm.gameState.levelAttempts).c_str());

	rlm.hasEnteredPlayLayer = false;
}


void RLRouletteLayer::getRandomListLevel()
{
	int listType = rl::utils::getIndexOf(
		DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(),
		true
	);

	auto& listFetcher = ListFetcher::get();

	switch (listType)
	{
	case 0:
		listFetcher.getRandomNormalListLevel(
			m_selected_difficulty == GJDifficulty::Demon
				? m_selected_demon_difficulty
				: m_selected_difficulty,
			m_level
		);
		break;

	case 1:
		listFetcher.getRandomDemonListLevel(m_level);
		break;

	case 2:
		listFetcher.getRandomChallengeListLevel(m_level);
		break;

	case 3:
		listFetcher.getRandomGDListLevel(RouletteManager::get().gdListID, m_level);
		break;

	default:
		m_level = geode::Err("Invalid list type");
		return;
	}
}

CCMenuItemSpriteExtra* RLRouletteLayer::createDifficultyButton(
	GJDifficulty difficulty,
	CCNode* sprite,
	CCPoint point,
	float scale,
	bool isDemon,
	bool visible
) {
	sprite->setScale(scale);
	sprite->setID("sprite-node");
	auto button = CCMenuItemSpriteExtra::create(
		sprite,
		this,
		menu_selector(RLRouletteLayer::onDifficultyButton)
	);
	button->setCascadeColorEnabled(true);
	if (
		rl::utils::getIndexOf(DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(), true) != 0 ||
		m_selected_difficulty != difficulty
		)
		button->setColor({ 125, 125, 125 });
	button->setPosition(point);
	button->setTag(static_cast<int>(difficulty));
	button->setVisible(visible);

	return button;
}

// ===================================================================
// Sorrow: Home (Solo | Multiplayer) + Solo setup screen.
// The vendored GD-Roulette main_menu stays as the game-state container
// (its logic still reads/writes it), but its old difficulty row, plus
// button, start button and frame title are parked off-screen; the new
// screens below drive the same DataManager state.
// ===================================================================

namespace {
constexpr float SOR_CARD = 150.f;

cocos2d::extension::CCScale9Sprite* sorPanel(float w, float h, cocos2d::ccColor3B c, GLubyte a) {
	return sorrow::ui::panel(w, h, c, a);
}

const char* sorFaceName(GJDifficulty d) {
	switch (d) {
		case GJDifficulty::Easy: return "Easy";
		case GJDifficulty::Normal: return "Normal";
		case GJDifficulty::Hard: return "Hard";
		case GJDifficulty::Harder: return "Harder";
		case GJDifficulty::Insane: return "Insane";
		case GJDifficulty::DemonEasy: return "Easy";
		case GJDifficulty::DemonMedium: return "Medium";
		case GJDifficulty::Demon: return "Hard";
		case GJDifficulty::DemonInsane: return "Insane";
		case GJDifficulty::DemonExtreme: return "Extreme";
		default: return "Any";
	}
}
const std::array<GJDifficulty, 5> SOR_FACES { GJDifficulty::Easy, GJDifficulty::Normal, GJDifficulty::Hard, GJDifficulty::Harder, GJDifficulty::Insane };
const std::array<GJDifficulty, 6> SOR_DEMONS { static_cast<GJDifficulty>(-2), GJDifficulty::DemonEasy, GJDifficulty::DemonMedium, GJDifficulty::Demon, GJDifficulty::DemonInsane, GJDifficulty::DemonExtreme };
const std::array<const char*, 4> SOR_LISTS { "Levels", "Demonlist", "Challenge", "GD List" };
const std::array<const char*, 5> SOR_RARITIES { "any", "featured", "epic", "legendary", "mythic" };
const std::array<const char*, 5> SOR_RARITY_LABELS { "Any", "Featured", "Epic", "Legendary", "Mythic" };
}

// A square card: dark translucent body, coloured accent strip, icon, title, subtitle.
cocos2d::CCNode* RLRouletteLayer::sorMakeCard(cocos2d::CCNode* icon, const char* title, cocos2d::ccColor3B accent, CCLabelBMFont** subOut)
{
	auto* card = cocos2d::CCNode::create();
	card->setContentSize({ SOR_CARD, SOR_CARD });
	auto* bg = sorPanel(SOR_CARD, SOR_CARD, { 0, 0, 0 }, 150);
	bg->setPosition({ SOR_CARD / 2.f, SOR_CARD / 2.f });
	card->addChild(bg, -2);
	auto* strip = cocos2d::CCLayerColor::create({ accent.r, accent.g, accent.b, 230 }, SOR_CARD - 16.f, 3.f);
	strip->setPosition({ 8.f, SOR_CARD - 12.f });
	card->addChild(strip);
	if (icon) { icon->setPosition({ SOR_CARD / 2.f, 92.f }); card->addChild(icon, 3); }
	auto* t = CCLabelBMFont::create(title, "bigFont.fnt");
	t->limitLabelWidth(SOR_CARD - 24.f, .75f, .3f);
	t->setPosition({ SOR_CARD / 2.f, 42.f });
	card->addChild(t);
	auto* sub = CCLabelBMFont::create("", "chatFont.fnt");
	sub->setColor({ 200, 200, 200 });
	sub->setPosition({ SOR_CARD / 2.f, 20.f });
	card->addChild(sub);
	if (subOut) *subOut = sub;
	return card;
}

void RLRouletteLayer::sorBuildHome()
{
	auto winSize = CCDirector::sharedDirector()->getWinSize();
	const float cx = winSize.width / 2.f, cy = winSize.height / 2.f - 4.f;

	m_homeRoot = CCMenu::create();
	m_homeRoot->setPosition({ 0.f, 0.f });
	m_homeRoot->setID("sor-home");
	this->addChild(m_homeRoot, 13);

	// Solo: an Insane face
	auto* soloIcon = RLDifficultyNode::create(GJDifficulty::Insane);
	soloIcon->setScale(1.25f);
	auto* solo = CCMenuItemSpriteExtra::create(
		sorMakeCard(soloIcon, "Solo", { 90, 220, 120 }, &m_soloSub), this, menu_selector(RLRouletteLayer::onSorSolo));
	solo->setPosition({ cx - SOR_CARD / 2.f - 14.f, cy });
	m_homeRoot->addChild(solo);

	// Multiplayer: your cube + a second cube in swapped colours
	auto* duo = cocos2d::CCNode::create();
	if (auto gm = GameManager::get()) {
		for (int i = 0; i < 2; ++i) {
			auto* sp = SimplePlayer::create(std::max(1, static_cast<int>(gm->m_playerFrame)));
			if (!sp) continue;
			sp->setColor(gm->colorForIdx(i == 0 ? gm->m_playerColor : gm->m_playerColor2));
			sp->setSecondColor(gm->colorForIdx(i == 0 ? gm->m_playerColor2 : gm->m_playerColor));
			sp->setScale(1.f);
			sp->setPosition({ i == 0 ? -17.f : 17.f, i == 0 ? 0.f : -4.f });
			if (i == 1) sp->setFlipX(true);
			duo->addChild(sp, i == 0 ? 1 : 0);
		}
	}
	auto* multi = CCMenuItemSpriteExtra::create(
		sorMakeCard(duo, "Multiplayer", { 90, 170, 255 }, &m_multiSub), this, menu_selector(RLRouletteLayer::onSorMulti));
	multi->setPosition({ cx + SOR_CARD / 2.f + 14.f, cy });
	m_homeRoot->addChild(multi);

	// balance: top-right corner
	m_homeBalance = CCLabelBMFont::create("", "goldFont.fnt");
	m_homeBalance->setAnchorPoint({ 1.f, .5f });
	m_homeBalance->setPosition({ winSize.width - 12.f, winSize.height - 20.f });
	m_homeRoot->addChild(m_homeBalance);

	// daily / weekly challenges
	m_chalSpr = ButtonSprite::create("Challenges", 200, true, "bigFont.fnt", "GJ_button_02.png", 30.f, .6f);
	auto* chal = CCMenuItemSpriteExtra::create(m_chalSpr, this, menu_selector(RLRouletteLayer::onSorChallenges));
	chal->setPosition({ cx, cy - SOR_CARD / 2.f - 26.f });
	m_homeRoot->addChild(chal);
	if (!sorrow::chal::state().loaded) sorrow::chal::fetch();

	this->sorRefreshHome();
}

void RLRouletteLayer::sorRefreshHome()
{
	if (!m_homeRoot) return;
	const auto& rlm = RouletteManager::get();
	if (m_soloSub) {
		m_soloSub->setString(rlm.isPaused ? "Resume your run" : "Classic level roulette");
		m_soloSub->limitLabelWidth(SOR_CARD - 20.f, .6f, .3f);
	}
	if (m_multiSub) {
		const auto& st = sorrow::rooms::state();
		std::string sub;
		if (st.active && st.state == "playing") sub = fmt::format("Room {} - round {}/{}", st.code, st.round, st.rounds);
		else if (st.active) sub = fmt::format("In room {}", st.code);
		else if (m_openRooms > 0) sub = fmt::format("{} open room{}", m_openRooms, m_openRooms == 1 ? "" : "s");
		else sub = "Rooms, bets, Globed";
		m_multiSub->setString(sub.c_str());
		m_multiSub->limitLabelWidth(SOR_CARD - 20.f, .6f, .3f);
	}
	const auto bal = sorrow::glimmerBalance();
	m_homeBalance->setString(bal >= 0 ? fmt::format("Glimmer: {}", sorrow::formatGlimmer(bal)).c_str() : "");
	m_homeBalance->limitLabelWidth(130.f, .45f, .2f);
	if (m_chalSpr) {
		const auto& cs = sorrow::chal::state();
		m_chalSpr->m_label->setString(cs.loaded
			? fmt::format("Challenges  {}/{}", sorrow::chal::doneCount(), sorrow::chal::totalCount()).c_str()
			: "Challenges");
		m_chalSpr->m_label->limitLabelWidth(186.f, .6f, .2f);
	}
}

void RLRouletteLayer::sorCountRooms()
{
	geode::Ref<RLRouletteLayer> self = this;
	sorrow::rooms::fetchList([self](bool ok, std::vector<sorrow::rooms::BrowseItem> rooms) {
		if (!self->getParent()) return;
		int open = 0;
		for (const auto& r : rooms) if (r.state != "playing" && r.players < std::max(1, r.maxPlayers)) open++;
		self->m_openRooms = ok ? open : 0;
		self->sorRefreshHome();
	});
}

void RLRouletteLayer::onSorSolo(CCObject*)
{
	m_sorHome = false;
	this->sorRefreshSolo();
	this->sorTick(0.f);
}

void RLRouletteLayer::onSorChallenges(CCObject*)
{
	if (auto* p = sorrow::chal::ChallengesPopup::create()) p->show();
}

void RLRouletteLayer::onSorMulti(CCObject*)
{
	this->onRoomsButton(nullptr);
}

void RLRouletteLayer::sorBuildSolo()
{
	auto winSize = CCDirector::sharedDirector()->getWinSize();
	const float cx = winSize.width / 2.f;

	m_soloRoot = cocos2d::CCNode::create();
	m_soloRoot->setID("sor-solo");
	this->addChild(m_soloRoot, 12);
	m_soloMenu = CCMenu::create();
	m_soloMenu->setPosition({ 0.f, 0.f });
	m_soloRoot->addChild(m_soloMenu, 5);

	auto* title = CCLabelBMFont::create("Solo Roulette", "goldFont.fnt");
	title->setScale(.7f);
	title->setPosition({ cx, 266.f });
	m_soloRoot->addChild(title);

	// list tabs - laid out from their real rendered widths (no overlap)
	{
		std::vector<cocos2d::CCNode*> tabs;
		for (int i = 0; i < 4; ++i) {
			auto* spr = ButtonSprite::create(SOR_LISTS[i], 66, true, "bigFont.fnt", "GJ_button_04.png", 26.f, .5f);
			auto* b = CCMenuItemSpriteExtra::create(spr, this, menu_selector(RLRouletteLayer::onSorList));
			b->setTag(i);
			m_soloMenu->addChild(b);
			m_listTabs.push_back(spr);
			tabs.push_back(b);
		}
		sorrow::ui::row(tabs, cx, 240.f, 8.f);
	}

	// difficulty picker: 5 faces, then the 6 demon tiers
	m_gridRoot = cocos2d::CCNode::create();
	m_soloRoot->addChild(m_gridRoot);
	auto makeCell = [&](GJDifficulty d, float w, cocos2d::CCPoint pos, int tag, bool demon) {
		auto* cell = cocos2d::CCNode::create();
		cell->setContentSize({ w, 58.f });
		// box BEHIND the face (z -2), face on top (z 3); the GD face sprites
		// already carry their own name text, so no extra label.
		auto* bg = sorPanel(w, 58.f, { 0, 0, 0 }, 110);
		bg->setPosition({ w / 2.f, 29.f });
		bg->setID("bg");
		cell->addChild(bg, -2);
		auto* icon = RLDifficultyNode::create(d);
		icon->setScale(.92f);
		icon->setPosition({ w / 2.f, 29.f });
		cell->addChild(icon, 3);
		(void)demon;
		auto* item = CCMenuItemSpriteExtra::create(cell, this, menu_selector(RLRouletteLayer::onSorDiff));
		item->setTag(tag);
		item->setPosition(pos);
		auto* menu = CCMenu::create();
		menu->setPosition({ 0.f, 0.f });
		menu->addChild(item);
		m_gridRoot->addChild(menu);
		m_gridCells.push_back(item);
	};
	for (int i = 0; i < 5; ++i) makeCell(SOR_FACES[i], 56.f, { cx + (i - 2) * 64.f, 192.f }, 100 + i, false);
	for (int i = 0; i < 6; ++i) makeCell(SOR_DEMONS[i], 52.f, { cx + (i - 2.5f) * 58.f, 126.f }, 200 + i, true);

	// other lists: a short description (+ list id button for GD lists)
	m_listInfo = CCLabelBMFont::create("", "chatFont.fnt");
	m_listInfo->setAlignment(kCCTextAlignmentCenter);
	m_listInfo->setPosition({ cx, 172.f });
	m_soloRoot->addChild(m_listInfo);
	m_listIdBtn = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Set List ID", 100, true, "bigFont.fnt", "GJ_button_02.png", 28.f, .55f),
		this, menu_selector(RLRouletteLayer::onSorListId));
	m_listIdBtn->setPosition({ cx, 122.f });
	m_soloMenu->addChild(m_listIdBtn);

	// rarity chips
	auto* rarLab = CCLabelBMFont::create("Rarity", "bigFont.fnt");
	rarLab->setScale(.35f);
	rarLab->setAnchorPoint({ 1.f, .5f });
	m_soloRoot->addChild(rarLab);
	m_rarityLabel = rarLab;
	{
		std::vector<cocos2d::CCNode*> chips;
		for (int i = 0; i < 5; ++i) {
			auto* spr = ButtonSprite::create(SOR_RARITY_LABELS[i], 46, true, "bigFont.fnt", "GJ_button_04.png", 24.f, .45f);
			auto* b = CCMenuItemSpriteExtra::create(spr, this, menu_selector(RLRouletteLayer::onSorRarity));
			b->setTag(i);
			m_soloMenu->addChild(b);
			m_rarityChips.push_back(spr);
			m_rarityItems.push_back(b);
			chips.push_back(b);
		}
		sorrow::ui::row(chips, cx + 22.f, 82.f, 5.f);
		rarLab->setPosition({ chips.front()->getPositionX() - chips.front()->getScaledContentWidth() / 2.f - 8.f, 82.f });
	}

	// bottom: skips stepper | START
	auto* skipsLab = CCLabelBMFont::create("Skips", "bigFont.fnt");
	skipsLab->setScale(.35f);
	skipsLab->setPosition({ cx - 150.f, 58.f });
	m_soloRoot->addChild(skipsLab);
	auto* minus = CCMenuItemSpriteExtra::create(CCSprite::createWithSpriteFrameName("edit_leftBtn_001.png"), this, menu_selector(RLRouletteLayer::onSorSkips));
	minus->setTag(-1);
	minus->setPosition({ cx - 180.f, 40.f });
	m_soloMenu->addChild(minus);
	auto* plus = CCMenuItemSpriteExtra::create(CCSprite::createWithSpriteFrameName("edit_rightBtn_001.png"), this, menu_selector(RLRouletteLayer::onSorSkips));
	plus->setTag(1);
	plus->setPosition({ cx - 120.f, 40.f });
	m_soloMenu->addChild(plus);
	m_skipsValue = CCLabelBMFont::create("3", "goldFont.fnt");
	m_skipsValue->setScale(.6f);
	m_skipsValue->setPosition({ cx - 150.f, 40.f });
	m_soloRoot->addChild(m_skipsValue);

	auto* start = CCMenuItemSpriteExtra::create(
		ButtonSprite::create("Start", 110, true, "bigFont.fnt", "GJ_button_01.png", 34.f, .9f),
		this, menu_selector(RLRouletteLayer::onSorStart));
	start->setPosition({ cx, 44.f });
	m_soloMenu->addChild(start);

	// (the top-left back arrow returns Solo -> Home)

	this->sorRefreshSolo();
}

void RLRouletteLayer::sorRefreshSolo()
{
	if (!m_soloRoot) return;
	const int list = rl::utils::getIndexOf(DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(), true);
	for (int i = 0; i < 4; ++i)
		m_listTabs[i]->updateBGImage(i == list ? "GJ_button_01.png" : "GJ_button_04.png");

	const bool levels = list == 0;
	m_gridRoot->setVisible(levels);
	for (auto* c : m_gridCells) c->setEnabled(levels);
	m_listInfo->setVisible(!levels);
	m_listIdBtn->setVisible(list == 3);
	if (!levels) {
		std::string text = list == 1 ? "A random level from the Pointercrate\nmain demonlist (top 75)."
			: list == 2 ? "A random level from the Challenge List\n(challengelist.gd)."
			: RouletteManager::get().gdListID > 0
				? fmt::format("A random level from GD List {}.", RouletteManager::get().gdListID)
				: std::string("Pick a GD level list by its ID.");
		m_listInfo->setString(text.c_str());
		m_listInfo->setScale(.7f);
	}

	// highlight the chosen difficulty
	for (auto* item : m_gridCells) {
		const int tag = item->getTag();
		bool on = false;
		if (tag < 200) on = m_selected_difficulty == SOR_FACES[tag - 100];
		else on = m_selected_difficulty >= GJDifficulty::Demon && m_selected_demon_difficulty == SOR_DEMONS[tag - 200];
		auto* cell = item->getNormalImage();
		if (auto* bg = static_cast<cocos2d::extension::CCScale9Sprite*>(cell->getChildByID("bg"))) {
			bg->setColor(on ? cocos2d::ccColor3B { 255, 200, 70 } : cocos2d::ccColor3B { 0, 0, 0 });
			bg->setOpacity(on ? 120 : 110);
		}
		cell->setScale(on ? 1.06f : 1.f);
		for (auto* ch : geode::cocos::CCArrayExt<cocos2d::CCNode*>(cell->getChildren()))
			if (ch->getID() != "bg") if (auto* rgba = typeinfo_cast<cocos2d::CCRGBAProtocol*>(ch)) rgba->setOpacity(on ? 255 : 150);
	}

	// rarity only filters RobTop searches (the Levels list)
	const auto& rar = RouletteManager::get().sorRarity;
	for (int i = 0; i < 5; ++i) {
		const bool on = rar == SOR_RARITIES[i] || (i == 0 && rar.empty());
		m_rarityChips[i]->updateBGImage(on ? "GJ_button_02.png" : "GJ_button_04.png");
		m_rarityChips[i]->setOpacity(levels ? 255 : 90);
		m_rarityItems[i]->setEnabled(levels);
	}
	m_rarityLabel->setOpacity(levels ? 255 : 90);

	m_skipsValue->setString(std::to_string(Mod::get()->getSettingValue<int64_t>("max-skips")).c_str());
}

void RLRouletteLayer::onSorList(CCObject* sender)
{
	const int idx = sender->getTag();
	const int prev = rl::utils::getIndexOf(DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(), true);
	DataManager::set<DMArrayKey::SELECTED_LIST_ARRAY>(prev, false);
	DataManager::set<DMArrayKey::SELECTED_LIST_ARRAY>(idx, true);
	this->onListChanged();
	this->sorRefreshSolo();
}

void RLRouletteLayer::onSorDiff(CCObject* sender)
{
	const int tag = sender->getTag();
	if (tag < 200) {
		this->onDifficultyButton(getDifficultyButton(SOR_FACES[tag - 100]));
	} else {
		const auto tier = SOR_DEMONS[tag - 200];
		this->onDifficultyButton(getDifficultyButton(GJDifficulty::Demon));
		auto array = DataManager::get<DMArrayKey::DEMON_DIFFICULTY_ARRAY>();
		array.set(rl::constants::save_data::DEMON_DIFF_TO_IDX.at(m_selected_demon_difficulty), false);
		array.set(rl::constants::save_data::DEMON_DIFF_TO_IDX.at(tier), true);
		m_selected_demon_difficulty = tier;
		RouletteManager::get().previousDemonDifficulty = tier;
		static_cast<RLDifficultyNode*>(getDifficultyButton(GJDifficulty::Demon)->getChildByID("sprite-node"))->setDifficulty(tier);
	}
	this->sorRefreshSolo();
}

void RLRouletteLayer::onSorRarity(CCObject* sender)
{
	RouletteManager::get().sorRarity = SOR_RARITIES[sender->getTag()];
	this->sorRefreshSolo();
}

void RLRouletteLayer::onSorSkips(CCObject* sender)
{
	const auto cur = Mod::get()->getSettingValue<int64_t>("max-skips");
	Mod::get()->setSettingValue<int64_t>("max-skips", std::clamp<int64_t>(cur + sender->getTag(), 0, 99));
	this->sorRefreshSolo();
}

void RLRouletteLayer::onSorListId(CCObject*)
{
	auto& rlm = RouletteManager::get();
	geode::Ref<RLRouletteLayer> self = this;
	auto* layer = RLIntegerInputAlertLayer::create({
		"List ID", "ID", 0, 0, INT_MAX,
		rlm.gdListID == 0 ? std::nullopt : std::optional<int>(rlm.gdListID),
		10, false,
		[self](auto iil) {
			auto& r = RouletteManager::get();
			r.gdListID = iil->m_integer;
			Mod::get()->setSavedValue<uint64_t>(
				DataManager::values::getKeyString<DMMiscKey::GD_LIST_ID>(),
				static_cast<uint64_t>(iil->m_integer));
			self->sorRefreshSolo();
		}
	});
	if (layer) layer->show();
}

void RLRouletteLayer::onSorStart(CCObject*)
{
	const int list = rl::utils::getIndexOf(DataManager::get<DMArrayKey::SELECTED_LIST_ARRAY>().asVector(), true);
	if (list == 3 && RouletteManager::get().gdListID <= 0) {
		Notification::create("Set a GD List ID first", NotificationIcon::Warning, 1.5f)->show();
		return;
	}
	this->onStartButton(nullptr);
}

void RLRouletteLayer::onSorBack(CCObject*)
{
	m_sorHome = true;
	this->sorRefreshHome();
	this->sorTick(0.f);
}
