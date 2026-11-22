#pragma once

#include "../listfetcher/ListFetcher.hpp"
#include "../custom_layers/base/BaseCustomLayer.hpp"
#include "../custom_layers/RLConfirmationAlertLayer.hpp"
#include "../custom_layers/RLDifficultySelectAlertLayer.hpp"
#include "../custom_layers/RLStatsAlertLayer.hpp"
#include "../custom_nodes/RLLoadingCircle.hpp"
#include "RLRouletteInfoAlertLayer.hpp"

class RLRouletteLayer : public BaseCustomLayer
{
public:
	RLRouletteLayer();

	static RLRouletteLayer* create();
	virtual bool init() override;

	virtual void onClose(cocos2d::CCObject*) override;

	void onInfoButton(cocos2d::CCObject*);
	void onRoomsButton(cocos2d::CCObject*);
	void onRarityButton(cocos2d::CCObject*);
	void onStatsButton(cocos2d::CCObject*);

	// Sorrow: updates the bottom bar labels (room code / rarity) live.
	void sorSyncBottomBar();
	void sorShowFrame(int which);
	void sorTick(float);
	// Home (Solo | Multiplayer) + Solo setup
	cocos2d::CCNode* sorMakeCard(cocos2d::CCNode* icon, const char* title, cocos2d::ccColor3B accent, cocos2d::CCLabelBMFont** subOut);
	void sorBuildHome();
	void sorRefreshHome();
	void sorCountRooms();
	void sorBuildSolo();
	void sorRefreshSolo();
	void onSorSolo(cocos2d::CCObject*);
	void onSorMulti(cocos2d::CCObject*);
	void onSorChallenges(cocos2d::CCObject*);
	void onSorList(cocos2d::CCObject*);
	void onSorDiff(cocos2d::CCObject*);
	void onSorRarity(cocos2d::CCObject*);
	void onSorSkips(cocos2d::CCObject*);
	void onSorListId(cocos2d::CCObject*);
	void onSorStart(cocos2d::CCObject*);
	void onSorBack(cocos2d::CCObject*);
	void onDifficultyButton(cocos2d::CCObject*);
	void onStartButton(cocos2d::CCObject*);
	void onPlusButton(cocos2d::CCObject*);
	void onLevelInfo(cocos2d::CCObject*);
	void onPlayButton(cocos2d::CCObject*);
	void onSkipButton(cocos2d::CCObject*);
	void onNextButton(cocos2d::CCObject*);
	void onRefreshButton(cocos2d::CCObject*);
	void onLevelsPlayedButton(cocos2d::CCObject*);
	void onResetButton(cocos2d::CCObject*);

	void onListChanged();

	virtual void onExitTransitionDidStart() override;
	virtual void onEnterTransitionDidFinish() override;

	inline CCMenuItemSpriteExtra* getDifficultyButton(GJDifficulty difficulty)
	{
		return static_cast<CCMenuItemSpriteExtra*>(main_menu->getChildByTag(static_cast<int>(difficulty)));
	}

public:
	cocos2d::CCLayer* main_bg;

	cocos2d::CCMenu* info_menu;
	cocos2d::CCMenu* main_menu;
	cocos2d::CCMenu* playing_menu;
	cocos2d::CCMenu* finished_menu;
	cocos2d::CCMenu* error_menu;

private:
	void finishLevelRoulette();
	void setupForNextLevel(bool = false, bool = false, float = .0f);

	void getRandomListLevel();
	CCMenuItemSpriteExtra* createDifficultyButton(GJDifficulty, cocos2d::CCNode*, cocos2d::CCPoint, float, bool = false, bool = true);

private:
	RLLoadingCircle* m_loading_circle;
	RLRouletteInfoAlertLayer* m_roulette_info_layer;
	RLConfirmationAlertLayer* m_confirmation_layer;
	RLDifficultySelectAlertLayer* m_demon_select_layer;
	RLStatsAlertLayer* m_stats_layer;

	GJDifficulty m_selected_difficulty;
	GJDifficulty m_selected_demon_difficulty;

	geode::Result<ListFetcher::level_pair_t> m_level;

	// Sorrow bottom action bar.
	cocos2d::CCMenu* m_sorBottomBar = nullptr;
	CCMenuItemSpriteExtra* m_roomsButton = nullptr;
	CCMenuItemSpriteExtra* m_rarityButton = nullptr;
	ButtonSprite* m_roomsBtnSprite = nullptr;
	ButtonSprite* m_rarityBtnSprite = nullptr;
	cocos2d::extension::CCScale9Sprite* m_photoPanel = nullptr;
	int m_sorFrame = 1;
	bool m_sorHome = true;
public:
	static bool s_sorOpenRoomsOnEnter;
private:
	int m_openRooms = 0;
	float m_homeTimer = 0.f;
	cocos2d::CCMenu* m_homeRoot = nullptr;
	cocos2d::CCLabelBMFont* m_soloSub = nullptr;
	cocos2d::CCLabelBMFont* m_multiSub = nullptr;
	cocos2d::CCLabelBMFont* m_homeBalance = nullptr;
	ButtonSprite* m_chalSpr = nullptr;
	cocos2d::CCNode* m_soloRoot = nullptr;
	cocos2d::CCMenu* m_soloMenu = nullptr;
	cocos2d::CCNode* m_gridRoot = nullptr;
	std::vector<CCMenuItemSpriteExtra*> m_gridCells;
	std::vector<ButtonSprite*> m_listTabs;
	std::vector<ButtonSprite*> m_rarityChips;
	std::vector<CCMenuItemSpriteExtra*> m_rarityItems;
	cocos2d::CCLabelBMFont* m_listInfo = nullptr;
	cocos2d::CCLabelBMFont* m_rarityLabel = nullptr;
	cocos2d::CCLabelBMFont* m_skipsValue = nullptr;
	CCMenuItemSpriteExtra* m_listIdBtn = nullptr;
};
