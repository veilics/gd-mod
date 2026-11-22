#pragma once

#include <Geode/cocos/layers_scenes_transitions_nodes/CCLayer.h>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

class BaseCustomLayer : public cocos2d::CCLayer
{
public:
	bool init(const char*);

	virtual void onClose(cocos2d::CCObject*) = 0;
	virtual void keyDown(cocos2d::enumKeyCodes key, double timestamp) override;
	virtual void keyBackClicked() override;

public:
	cocos2d::CCMenu* m_backMenu;
	cocos2d::CCMenu* m_sideArtMenu;
	cocos2d::CCMenu* m_backgroundMenu;

	CCMenuItemSpriteExtra* m_backBtn;

	// true when the photo background (sorrow_bg.jpg) loaded - custom layers
	// then skip their own repeating stripe background.
	bool m_hasPhotoBg = false;
};
