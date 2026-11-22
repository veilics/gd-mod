// Sorrow Integration - the gray "(goal%)" label next to GD's percentage.
//
// GD-Roulette draws it on the level page, but only if the percentage label
// already exists at init - on a level that is not downloaded yet it does not,
// so the goal never appeared. This node keeps re-attaching/re-positioning
// itself on a short schedule instead, and also covers multiplayer room
// round targets and the in-level percentage.
#pragma once

#include <Geode/Geode.hpp>
#include "managers/RouletteManager.hpp"
#include "SorRooms.hpp"

namespace sorrow {

// Goal % for this level: room round target first, then solo roulette goal.
inline int goalPercentFor(int levelId) {
    if (int t = rooms::roundTargetFor(levelId); t > 0) return t;
    const auto& rlm = RouletteManager::get();
    if (rlm.isPlaying && rlm.gameState.levelID == levelId) return rlm.currentPercentageGoal;
    return 0;
}

class SorGoalLabel : public cocos2d::CCNode {
public:
    // `findLabel` returns the GD percentage label to sit next to (may be null for a while).
    static SorGoalLabel* create(int levelId, float scale, std::function<CCLabelBMFont*()> findLabel) {
        auto* n = new SorGoalLabel();
        n->m_levelId = levelId;
        n->m_scale = scale;
        n->m_find = std::move(findLabel);
        n->autorelease();
        n->setID("sor-goal-label"_spr);
        n->m_label = CCLabelBMFont::create("", "bigFont.fnt");
        n->m_label->setColor({ 125, 125, 125 });
        n->m_label->setAnchorPoint({ 0.f, .5f });
        n->addChild(n->m_label);
        n->schedule(schedule_selector(SorGoalLabel::tick), 0.f);
        return n;
    }

    void tick(float) {
        auto* target = m_find ? m_find() : nullptr;
        const int goal = goalPercentFor(m_levelId);
        const bool show = target && target->isVisible() && goal > 0 && this->getParent();
        m_label->setVisible(show);
        if (!show) return;
        const auto text = fmt::format("({}%)", goal);
        if (text != m_text) { m_text = text; m_label->setString(text.c_str()); }
        m_label->setScale(m_scale > 0.f ? m_scale : target->getScale() * 0.8f);
        // right edge of GD's label, in our parent's space
        const auto box = target->boundingBox();
        auto right = target->getParent()->convertToWorldSpace({ box.getMaxX(), box.getMidY() });
        this->setPosition(this->getParent()->convertToNodeSpace(right) + cocos2d::CCPoint { 5.f, 0.f });
    }

private:
    int m_levelId = 0;
    float m_scale = 0.f;
    std::string m_text;
    std::function<CCLabelBMFont*()> m_find;
    CCLabelBMFont* m_label = nullptr;
};

} // namespace sorrow
