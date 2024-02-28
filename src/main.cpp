#include <iostream>
#include <fstream>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include "../build/bindings/bindings/Geode/binding/GJGameLevel.hpp"

#include <cocos2d.h>

using namespace geode::prelude;
using namespace std;

struct AutoDeafenLevel {
	bool enabled = false;
	bool editor = false;
	int id = 0;
	float percentage = 50;
	AutoDeafenLevel(bool a, bool b, int c, float d) { // I am lazy lmao
		enabled = a;
		editor = b;
		id = c;
		percentage = d;
	}
	AutoDeafenLevel() {}
};


AutoDeafenLevel currentlyLoadedLevel;
list<AutoDeafenLevel> loadedAutoDeafenLevels;

bool hasDeafenedThisAttempt = false;
bool hasDied = false;
uint32_t deafenKeybind[] = {0xA1, 0x48};

ghc::filesystem::path getFilePath(GJGameLevel* lvl) {

	auto path = Mod::get() -> getSaveDir();
	int id = lvl -> m_levelID.value();

	path /= (id == 0) ? ("editorlevels" + to_string(lvl -> m_M_ID)) : to_string(id);
	path /= ".autodeafen";

	return path;
}

void saveLevel(AutoDeafenLevel lvl) {

	loadedAutoDeafenLevels.push_back(lvl);
	lvl = AutoDeafenLevel();

}

void saveFile() {

	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";
	
	ofstream file(path);
	if (file.is_open()) {

		file.write("ad1", sizeof("ad1")); // Autodeafen file version 1
		for (AutoDeafenLevel const& a : loadedAutoDeafenLevels) {
			file.write(reinterpret_cast<const char*>(&a.enabled), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.editor), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.id), sizeof(int));
			file.write(reinterpret_cast<const char*>(&a.percentage), sizeof(float));
		}
		file.close();

	} else {
		log::error("AutoDeafen file failed when trying to open and save.");
	}

}

void playDeafenKeybind() {

	if (currentlyLoadedLevel.enabled) {
		log::info("Played deafen keybind.");

		// TODO - make this configurable

		keybd_event(deafenKeybind[0], 0, 0x0000, 0);

		keybd_event(deafenKeybind[1], 0, 0x0000, 0);
		keybd_event(deafenKeybind[1], 0, 0x0002, 0);


		keybd_event(deafenKeybind[0], 0, 0x0002, 0);
	}



}


void loadFromFile(GJGameLevel* lvl) {

	auto path = getFilePath(lvl);

	if (!ghc::filesystem::exists(path)) {
		return;
	}

}


class $modify(PlayerObject) {
	TodoReturn playerDestroyed(bool p0) {
		
		auto playLayer = PlayLayer::get();

		if (this != (playLayer->m_player1) ) return;
		if (playLayer == nullptr) return;
		if (playLayer->m_isPracticeMode) return;

		if (hasDeafenedThisAttempt && !hasDied) {
			hasDied = true;
			playDeafenKeybind();
		}
		PlayerObject::playerDestroyed(p0);
	}
};

class $modify(PlayLayer) {

	bool init(GJGameLevel* level, bool p1, bool p2) {

		if (!PlayLayer::init(level, p1, p2)) return false;

		int id = m_level -> m_levelID.value();
		bool editor = (id == 0);
		if (editor) id = m_level -> m_M_ID;
		for (AutoDeafenLevel level : loadedAutoDeafenLevels)
			if (level.id == id && level.editor == editor) { currentlyLoadedLevel = level; return true; }

		currentlyLoadedLevel = AutoDeafenLevel(false, editor, id, 50);
		hasDeafenedThisAttempt = false;

		return true;

	}

	void resetLevel() {
		PlayLayer::resetLevel();
		hasDied = false;
		hasDeafenedThisAttempt = false;
	}
	
	void updateProgressbar() {

		PlayLayer::updateProgressbar();
		if (this->m_isPracticeMode) return;

		float percent = static_cast<float>(PlayLayer::getCurrentPercentInt());
		// log::info("{}", currentlyLoadedLevel.percentage);
		// log::info("{}", percent);
		if (percent >= currentlyLoadedLevel.percentage && percent != 100 && !hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = true;
			playDeafenKeybind();
		}

	}

	void levelComplete() {
		PlayLayer::levelComplete();
		if (hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = false;
			playDeafenKeybind();
		}
	}

	void onQuit() {
		PlayLayer::onQuit();
		saveLevel(currentlyLoadedLevel);
		currentlyLoadedLevel = AutoDeafenLevel();
	}
	
};

CCMenuItemToggler* enabledButton;
class ButtonLayer : public CCLayer {
	public:
		void toggleEnabled(CCObject* sender) {
			currentlyLoadedLevel.enabled = !currentlyLoadedLevel.enabled;
			enabledButton -> toggle(!currentlyLoadedLevel.enabled);
			// log::info("{}", currentlyLoadedLevel.enabled);
		}
};

class ConfigLayer : public geode::Popup<std::string const&> {
	protected:
		bool setup(std::string const& value) override {

			auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
			auto menu = CCMenu::create();

			CCPoint topLeftCorner = winSize/2.f-ccp(m_size.width/2.f,-m_size.height/2.f);

			menu -> setPosition( {0, 0} );

			m_mainLayer -> addChild(menu);

			auto topLabel = CCLabelBMFont::create("AutoDeafen", "bigFont.fnt"); 
			topLabel->setAnchorPoint({0.5, 0.5});
			topLabel->setScale(1.0f);
			topLabel->setPosition(topLeftCorner + ccp(142, 5));

			auto enabledLabel = CCLabelBMFont::create("Enabled", "bigFont.fnt"); 
			enabledLabel->setAnchorPoint({0, 0.5});
			enabledLabel->setScale(0.7f);
			enabledLabel->setPosition(topLeftCorner + ccp(80, -60));

			enabledButton = CCMenuItemToggler::create(
				CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
				CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
				this,
				menu_selector(ButtonLayer::toggleEnabled)
			);

			enabledButton -> setPosition(enabledLabel->getPosition() + ccp(120,0));
			enabledButton -> setScale(0.85f);
			enabledButton -> setClickable(true);
			enabledButton -> toggle(currentlyLoadedLevel.enabled);


			menu -> addChild(topLabel);
			menu -> addChild(enabledLabel);
			menu -> addChild(enabledButton);

			return true;
		}
		static ConfigLayer* create() {
			auto ret = new ConfigLayer();
			if (ret && ret->init(300, 200, "", "GJ_square02.png")) {
				ret->autorelease();
				return ret;
			}
			CC_SAFE_DELETE(ret);
			return nullptr;
		}
	public:
		void openMenu(CCObject*) {
			auto layer = create();
			layer -> show();
		}
};


class $modify(PauseLayer) {

	void customSetup() {
		PauseLayer::customSetup();
		auto winSize = CCDirector::sharedDirector() -> getWinSize();

		CCSprite* sprite = CCSprite::createWithSpriteFrameName("GJ_musicOffBtn_001.png");

		auto btn = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ConfigLayer::openMenu) );
		auto menu = this -> getChildByID("right-button-menu");

		menu->addChild(btn);
		menu->updateLayout();

		
	}
};