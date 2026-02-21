#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>

using namespace geode::prelude;

namespace ryxu {
	static double nowSeconds() {
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}

	struct Config {
		static bool showCounters() {
			return Mod::get()->getSettingValue<bool>("show-counters");
		}

		static bool rgbFps() {
			return Mod::get()->getSettingValue<bool>("rgb-fps");
		}

		static bool rgbCps() {
			return Mod::get()->getSettingValue<bool>("rgb-cps");
		}

		static bool fpsBypassEnabled() {
			return Mod::get()->getSettingValue<bool>("fps-bypass-enabled");
		}

		static int fpsBypassValue() {
			return static_cast<int>(Mod::get()->getSettingValue<int64_t>("fps-bypass"));
		}

		static bool unlockAllIcons() {
			return Mod::get()->getSettingValue<bool>("unlock-all-icons");
		}

		static bool practiceMusicSync() {
			return Mod::get()->getSettingValue<bool>("practice-music-sync");
		}

		static bool noclipEnabled() {
			return Mod::get()->getSettingValue<bool>("noclip");
		}

		static bool showHitboxes() {
			return Mod::get()->getSettingValue<bool>("show-hitboxes");
		}

		static bool autoCheckpoints() {
			return Mod::get()->getSettingValue<bool>("auto-checkpoints");
		}
	};

	static void applyFpsBypass() {
		static bool s_lastEnabled = false;
		static int s_lastTargetFps = 0;

		bool enabled = Config::fpsBypassEnabled();
		int targetFps = std::max(1, Config::fpsBypassValue());
		if (!enabled) {
			targetFps = 60;
		}

		if (enabled == s_lastEnabled && targetFps == s_lastTargetFps) {
			return;
		}

		s_lastEnabled = enabled;
		s_lastTargetFps = targetFps;

		double animationInterval = 1.0 / static_cast<double>(targetFps);
		CCDirector::sharedDirector()->setAnimationInterval(animationInterval);
		log::info("[Ryxu Mod] FPS target set to {}", targetFps);
	}

	static std::deque<double> s_clickTimes;

	static void recordClick() {
		s_clickTimes.push_back(nowSeconds());
	}

	static int getCps() {
		double now = nowSeconds();
		while (!s_clickTimes.empty() && (now - s_clickTimes.front()) > 1.0) {
			s_clickTimes.pop_front();
		}
		return static_cast<int>(s_clickTimes.size());
	}

	static ccColor3B rainbowColor(float timeScale, float phaseOffset = 0.0f) {
		float phase = std::fmod(static_cast<float>(nowSeconds()) * timeScale + phaseOffset, 1.0f);
		float h = phase * 6.0f;
		float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;

		if (h < 1.0f) {
			r = 1.0f;
			g = x;
		}
		else if (h < 2.0f) {
			r = x;
			g = 1.0f;
		}
		else if (h < 3.0f) {
			g = 1.0f;
			b = x;
		}
		else if (h < 4.0f) {
			g = x;
			b = 1.0f;
		}
		else if (h < 5.0f) {
			r = x;
			b = 1.0f;
		}
		else {
			r = 1.0f;
			b = x;
		}

		return ccc3(
			static_cast<GLubyte>(r * 255.0f),
			static_cast<GLubyte>(g * 255.0f),
			static_cast<GLubyte>(b * 255.0f)
		);
	}

	// Practice music sync logic removed; only unlock item

}

class CounterOverlay : public CCLayer {
protected:
	CCLabelBMFont* m_fpsLabel = nullptr;
	CCLabelBMFont* m_cpsLabel = nullptr;
	float m_smoothedFps = 60.0f;

	bool init() override {
		if (!CCLayer::init()) {
			return false;
		}

		this->setID("ryxu-counter-overlay"_spr);

		auto winSize = CCDirector::sharedDirector()->getWinSize();

		m_fpsLabel = CCLabelBMFont::create("FPS: 0", "bigFont.fnt");
		m_fpsLabel->setAnchorPoint({0.f, 1.f});
		m_fpsLabel->setScale(0.35f);
		m_fpsLabel->setPosition({8.f, winSize.height - 8.f});
		this->addChild(m_fpsLabel);

		m_cpsLabel = CCLabelBMFont::create("CPS: 0", "bigFont.fnt");
		m_cpsLabel->setAnchorPoint({0.f, 1.f});
		m_cpsLabel->setScale(0.35f);
		m_cpsLabel->setPosition({8.f, winSize.height - 24.f});
		this->addChild(m_cpsLabel);

		this->scheduleUpdate();
		return true;
	}

public:
	static CounterOverlay* create() {
		auto ret = new CounterOverlay();
		if (ret && ret->init()) {
			ret->autorelease();
			return ret;
		}

		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	void update(float dt) override {
		float instantFps = dt > 0.0f ? (1.0f / dt) : 0.0f;
		m_smoothedFps = m_smoothedFps * 0.9f + instantFps * 0.1f;

		m_fpsLabel->setString(fmt::format("FPS: {}", static_cast<int>(m_smoothedFps + 0.5f)).c_str());
		m_cpsLabel->setString(fmt::format("CPS: {}", ryxu::getCps()).c_str());

		m_fpsLabel->setColor(ryxu::Config::rgbFps() ? ryxu::rainbowColor(0.18f, 0.00f) : ccWHITE);
		m_cpsLabel->setColor(ryxu::Config::rgbCps() ? ryxu::rainbowColor(0.18f, 0.33f) : ccWHITE);
	}
};

class $modify(RyxuBaseGameLayer, GJBaseGameLayer) {
	void handleButton(bool down, int button, bool isPlayer1) {
		GJBaseGameLayer::handleButton(down, button, isPlayer1);

		if (down && ryxu::Config::showCounters()) {
			ryxu::recordClick();
		}
	}
};

class $modify(RyxuMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) {
			return false;
		}

		ryxu::applyFpsBypass();

		CCNode* buttonIcon = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
		if (!buttonIcon) {
			buttonIcon = ButtonSprite::create("Ryxu", "goldFont.fnt", "GJ_button_04.png", 0.55f);
		}

		if (!buttonIcon) {
			log::warn("[Ryxu Mod] Could not create menu button icon");
			return true;
		}

		auto myButton = CCMenuItemSpriteExtra::create(
			buttonIcon,
			this,
			menu_selector(RyxuMenuLayer::onOpenMiniMenu)
		);

		auto menu = this->getChildByID("bottom-menu");
		if (menu) {
			menu->addChild(myButton);
			myButton->setID("ryxu-menu-button"_spr);
			menu->updateLayout();
		}
		else {
			auto fallbackMenu = CCMenu::create();
			fallbackMenu->setPosition({0.f, 0.f});
			fallbackMenu->setID("ryxu-fallback-menu"_spr);
			this->addChild(fallbackMenu, 1000);

			auto winSize = CCDirector::sharedDirector()->getWinSize();
			myButton->setPosition({winSize.width - 28.f, 26.f});
			myButton->setID("ryxu-menu-button"_spr);
			fallbackMenu->addChild(myButton);
		}

		log::info("[Ryxu Mod] Menu hook initialized");

		return true;
	}

	void onOpenMiniMenu(CCObject*) {
		openSettingsPopup(Mod::get());
	}
};

class $modify(RyxuGameManager, GameManager) {
	bool getGameVariable(char const* key) {
		if (ryxu::Config::practiceMusicSync() && key && std::strcmp(key, "0052") == 0) {
			return true;
		}

		return GameManager::getGameVariable(key);
	}

	bool isIconUnlocked(int id, IconType type) {
		if (ryxu::Config::unlockAllIcons()) {
			return true;
		}

		return GameManager::isIconUnlocked(id, type);
	}
};

class $modify(RyxuPlayLayer, PlayLayer) {
	struct Fields {
		CounterOverlay* counterOverlay = nullptr;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
			return false;
		}

		ryxu::applyFpsBypass();
		// syncPracticeMusic removed

		if (ryxu::Config::showCounters()) {
			auto overlay = CounterOverlay::create();
			if (overlay) {
				this->addChild(overlay, 9999);
				m_fields->counterOverlay = overlay;
			}
		}

		return true;
	}

	void update(float dt) {
		PlayLayer::update(dt);

		ryxu::applyFpsBypass();

		if (ryxu::Config::showCounters()) {
			if (!m_fields->counterOverlay) {
				auto overlay = CounterOverlay::create();
				if (overlay) {
					this->addChild(overlay, 9999);
					m_fields->counterOverlay = overlay;
				}
			}
		}
		else if (m_fields->counterOverlay) {
			m_fields->counterOverlay->removeFromParent();
			m_fields->counterOverlay = nullptr;
		}

		if (ryxu::Config::showHitboxes()) {
			// Hook your preferred collider renderer here.
			// Keep this lightweight; avoid rebuilding draw geometry each frame.
		}
	}

	void startMusic() {
		PlayLayer::startMusic();
		// No music sync; unlock logic only
	}

	void loadFromCheckpoint(CheckpointObject* object) {
		PlayLayer::loadFromCheckpoint(object);
		// No music sync; unlock logic only
	}

	void destroyPlayer(PlayerObject* player, GameObject* source) {
		if (ryxu::Config::noclipEnabled()) {
			return;
		}

		PlayLayer::destroyPlayer(player, source);
		// Removed practice music sync here to prevent song restart on death
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		if (ryxu::Config::autoCheckpoints()) {
			this->createCheckpoint();
		}
		// Removed practice music sync here to prevent song restart on respawn
	}
};

class $modify(RyxuGameStatsManager, GameStatsManager) {
	bool isItemUnlocked(UnlockType type, int id) {
		if (GameStatsManager::isItemUnlocked(type, id)) {
			return true;
		}

		if (ryxu::Config::practiceMusicSync() && type == UnlockType::GJItem && id == 17) {
			return true;
		}

		return false;
	}
};