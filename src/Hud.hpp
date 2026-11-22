// Sorrow Integration - on-screen HUD.
// A small draggable panel under the top-center of the screen:
//   Level Name
//   Target 43% - Best 31% - Deaths 12
//   [gem] Glimmer 12,345
// Fades in when a session becomes active, fades out when it ends.
//
// NOTE: never use operator""_spr here. _spr requires GEODE_MOD_ID at compile
// time; without it the runtime fallback returns nullptr and every use
// segfaults in strlen (observed on Android builds without the macro).
#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <fmt/format.h>
#include <string>

#include "Store.hpp"
#include "Bridge.hpp"
#include "Glimmer.hpp"
#include "GlobedLink.hpp"

using namespace geode::prelude;

namespace sorrow {

class SorrowHud : public cocos2d::CCLayer {
public:
    static SorrowHud* get() {
        if (auto scene = cocos2d::CCScene::get()) {
            return scene->getChildByType<SorrowHud>(0);
        }
        return nullptr;
    }

    static SorrowHud* create() {
        auto ret = new SorrowHud();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        this->setID("sorrow-hud");
        this->setZOrder(150);

        const auto win = cocos2d::CCDirector::get()->getWinSize();
        this->setPosition({win.width / 2.f, win.height - 52.f});

        m_panel = cocos2d::CCLayerColor::create(cocos2d::ccc4(10, 10, 14, 165), 260.f, 48.f);
        m_panel->setAnchorPoint({0.5f, 0.5f});
        m_panel->setPosition({0.f, 0.f});
        m_panel->setID("sorrow-panel");
        this->addChild(m_panel);

        m_line1 = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        m_line1->setScale(0.42f);
        m_line1->setPosition({0.f, 13.f});
        m_line1->setID("sorrow-line1");
        this->addChild(m_line1);

        m_line2 = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        m_line2->setScale(0.30f);
        m_line2->setPosition({0.f, 1.f});
        m_line2->setOpacity(210);
        m_line2->setID("sorrow-line2");
        this->addChild(m_line2);

        m_glimLine = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
        m_glimLine->setScale(0.28f);
        m_glimLine->setPosition({0.f, -12.f});
        m_glimLine->setOpacity(230);
        m_glimLine->setID("sorrow-line3");
        this->addChild(m_glimLine);

        // Glimmer gem icon, bundled next to mod.json in the package. Optional:
        // if resources are not reachable the balance line stays text-only.
        const auto iconPath = Mod::get()->getResourcesDir() / "glimmer.png";
        if (cocos2d::CCFileUtils::get()->isFileExist(iconPath.string())) {
            m_glimIcon = cocos2d::CCSprite::create(iconPath.string().c_str());
            if (m_glimIcon) {
                const float target = 18.f;
                const float w = m_glimIcon->getContentSize().width;
                if (w > 0.f) m_glimIcon->setScale(target / w);
                m_glimIcon->setPosition({-116.f, -12.f});
                m_glimIcon->setOpacity(230);
                this->addChild(m_glimIcon);
            }
        }

        this->fade(false);
        this->setVisible(false);

        // Dragging: a targeted delegate so taps that miss the panel fall through.
        cocos2d::CCTouchDispatcher::get()->addTargetedDelegate(this, -150, true);

        this->schedule(schedule_selector(SorrowHud::pollSession), 1.0f);
        return true;
    }

    void updateDisplay() {
        auto& s = currentSession();
        if (s.kind == Session::Kind::None || !Mod::get()->getSettingValue<bool>("show-hud")) {
            this->fade(false);
            return;
        }
        const std::string title = s.kind == Session::Kind::Group
            ? ("Group " + s.groupCode + " - " + s.levelName)
            : s.levelName;
        m_line1->setString(clipText(title, 46).c_str());
        m_line2->setString(fmt::format("Target {}%  Best {}%  Deaths {}",
            s.target, s.bestThisLevel, s.deathsThisLevel).c_str());
        const auto bal = glimmerBalance();
        m_glimLine->setString(fmt::format("Glimmer  {}",
            bal >= 0 ? commaNum(bal) : std::string("--")).c_str());
        this->fade(true);
    }

    // Floating "+1,500" toast when the bot credits a payout for a run.
    void showGlimmerDelta(std::int64_t delta) {
        if (delta == 0) return;
        const std::string text = (delta > 0 ? "+" : "-") + commaNum(std::llabs(delta));
        auto toast = cocos2d::CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        toast->setScale(0.32f);
        toast->setPosition({0.f, 28.f});
        toast->setColor(delta > 0 ? cocos2d::ccc3(120, 255, 170) : cocos2d::ccc3(255, 120, 130));
        this->addChild(toast);
        toast->runAction(cocos2d::CCSequence::create(
            cocos2d::CCSpawn::create(
                cocos2d::CCEaseOut::create(cocos2d::CCMoveBy::create(1.3f, {0.f, 16.f}), 1.6f),
                cocos2d::CCFadeOut::create(1.3f),
                nullptr),
            cocos2d::CCCallFuncN::create(this, callfuncN_selector(SorrowHud::removeToast)),
            nullptr));
        // Pulse the balance line so the change reads clearly.
        if (m_glimLine) {
            m_glimLine->stopAllActions();
            m_glimLine->setScale(0.28f);
            m_glimLine->runAction(cocos2d::CCSequence::create(
                cocos2d::CCScaleTo::create(0.12f, 0.36f),
                cocos2d::CCScaleTo::create(0.25f, 0.28f),
                nullptr));
        }
    }

    void fade(bool show) {
        if (m_fadedIn == show) return;
        m_fadedIn = show;
        this->setVisible(true);
        const auto to = show ? static_cast<unsigned char>(255) : 0;
        m_panel->setOpacity(show ? 165 : 0);
        m_panel->runAction(cocos2d::CCFadeTo::create(show ? 0.35f : 0.5f, 165 * to / 255));
        m_line1->setOpacity(to);
        m_line1->runAction(cocos2d::CCFadeTo::create(show ? 0.35f : 0.5f, to));
        m_line2->setOpacity(show ? 210 : 0);
        m_line2->runAction(cocos2d::CCFadeTo::create(show ? 0.35f : 0.5f, 210 * to / 255));
        m_glimLine->setOpacity(show ? 230 : 0);
        m_glimLine->runAction(cocos2d::CCFadeTo::create(show ? 0.35f : 0.5f, 230 * to / 255));
        if (m_glimIcon) {
            m_glimIcon->setOpacity(show ? 230 : 0);
            m_glimIcon->runAction(cocos2d::CCFadeTo::create(show ? 0.35f : 0.5f, 230 * to / 255));
        }
        if (!show) {
            this->runAction(cocos2d::CCSequence::create(
                cocos2d::CCDelayTime::create(0.5f),
                cocos2d::CCHide::create(),
                nullptr));
        }
    }

private:
    static std::string clipText(const std::string& t, size_t n) {
        return t.size() > n ? t.substr(0, n - 1) + ".." : t;
    }

    static std::string commaNum(std::int64_t v) {
        std::string s = std::to_string(v);
        int insert = static_cast<int>(s.length()) - 3;
        while (insert > 0) { s.insert(static_cast<size_t>(insert), ","); insert -= 3; }
        return s;
    }

    void removeToast(cocos2d::CCNode* node) {
        if (node) node->removeFromParent();
    }

    // Poll the bridge so sessions started in Discord (or by a Globed room-mate) appear in game.
    void pollSession(float) {
        auto& s = currentSession();
        const bool inLevel = PlayLayer::get() != nullptr;
        if (!inLevel) { this->fade(false); return; }

        if (s.kind != Session::Kind::None) {
            this->updateDisplay();
            // Periodic balance refresh while a session is running (payouts from
            // Globed room-mates' wins land on the bot side, not on our run path).
            m_pollCount += 1;
            if (m_pollCount % 15 == 0) {
                sorrow::refreshBalance();
                this->updateDisplay();
            }
            return;
        }

        // Ask the bot whether this player has a solo session waiting.
        Bridge::fetchSession([this](bool ok, int, matjson::Value body) {
            if (!ok || !body.isObject()) return;
            if (!body.contains("active") || !body["active"].asBool().unwrapOr(false)) return;
            auto& st = currentSession();
            if (st.kind != Session::Kind::None) return;
            st.kind = body.contains("groupCode") ? Session::Kind::Group : Session::Kind::Solo;
            st.sid = body["sid"].asString().unwrapOr("");
            st.groupCode = body["groupCode"].asString().unwrapOr("");
            st.levelId = static_cast<int>(body["levelId"].asInt().unwrapOr(0));
            st.levelName = body["levelName"].asString().unwrapOr("");
            st.target = static_cast<int>(body["target"].asInt().unwrapOr(0));
            st.progressive = body["progressive"].asBool().unwrapOr(true);
            st.startedAt = static_cast<std::int64_t>(std::time(nullptr));
            const auto bal = body["balance"].asInt().unwrapOr(-1);
            if (bal >= 0) setGlimmer(bal, 0);
            this->updateDisplay();

            // Group session picked up in game: if we are the Globed room owner we are the
            // relay host - broadcast the start to room-mates over Server Events.
            if (st.kind == Session::Kind::Group) {
                if (!st.groupCode.empty()) {
                    announceStart(st.groupCode, st.levelId, st.target);
                }
            }
        });
    }

    // Drag support (CCTouchDelegate virtuals inherited through CCLayer).
    bool ccTouchBegan(cocos2d::CCTouch* t, cocos2d::CCEvent*) override {
        auto rect = m_panel->boundingBox();
        rect.origin = this->getPosition() - cocos2d::CCPoint{m_panel->getScaledContentSize().width * 0.5f, 0.f};
        if (rect.containsPoint(t->getLocation())) {
            m_dragging = true;
            m_dragStart = t->getLocation();
            return true;
        }
        return false;
    }
    void ccTouchMoved(cocos2d::CCTouch* t, cocos2d::CCEvent*) override {
        if (!m_dragging) return;
        auto d = t->getLocation() - m_dragStart;
        this->setPosition(this->getPosition() + d);
        m_dragStart = t->getLocation();
    }
    void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*) override { m_dragging = false; }

    cocos2d::CCLayerColor* m_panel = nullptr;
    cocos2d::CCLabelBMFont* m_line1 = nullptr;
    cocos2d::CCLabelBMFont* m_line2 = nullptr;
    cocos2d::CCLabelBMFont* m_glimLine = nullptr;
    cocos2d::CCSprite* m_glimIcon = nullptr;
    bool m_dragging = false;
    cocos2d::CCPoint m_dragStart;
    bool m_fadedIn = false;
    int m_pollCount = 0;
};

} // namespace sorrow
