#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>

#include <stdio.h>
#include <string.h>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/LoadingLayer.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include "../build/bindings/bindings/Geode/binding/GJGameLevel.hpp"

#include <cocos2d.h>
#include "../../../LocalStuff/Geode/loader/include/Geode/ui/TextInput.hpp"

using namespace geode::prelude;
using namespace std;

struct AutoDeafenLevel {
	bool enabled = false;
	short levelType; // 0 = Normal, 1 = Local/Editor, 2 = Daily/Weekly, 3 = gauntlet
	int id = 0;
	int percentage = 50;
	AutoDeafenLevel(bool a, short b, int c, int d) { // I am lazy lmao
		enabled = a;
		levelType = b;
		id = c;
		percentage = d;
	}
	AutoDeafenLevel() {}
};


AutoDeafenLevel currentlyLoadedLevel;
vector<AutoDeafenLevel> loadedAutoDeafenLevels;

bool hasDeafenedThisAttempt = false;
bool hasDied = false;
uint32_t deafenKeybind[] = {0xA1, 0x48};

void saveLevel(AutoDeafenLevel lvl) {

	loadedAutoDeafenLevels.push_back(lvl);

}

short getLevelType(GJGameLevel* level) {

	if (level -> m_levelType != GJLevelType::Saved) return 1;
	if (level -> m_dailyID > 0) return 2;
	if (level -> m_gauntletLevel) return 3;

	return 0;

}

void saveFile() {

	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";

	log::info("{}", "Saving file to " + path.string());

	ofstream file(path);
	if (file.is_open()) {

		file.write("ad1", sizeof("ad1")); // Autodeafen file version 1
		for (AutoDeafenLevel const& a : loadedAutoDeafenLevels) {
			file.write(reinterpret_cast<const char*>(&a.enabled), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.levelType), sizeof(short));
			file.write(reinterpret_cast<const char*>(&a.id), sizeof(int));
			file.write(reinterpret_cast<const char*>(&a.percentage), sizeof(int));
		}
		file.close();
		log::info("Successfully saved .autodeafen file.");

	} else {
		log::error("AutoDeafen file failed when trying to open and save.");
	}

}

void loadFile() {
	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";

	log::info("{}", "Loading file from " + path.string());

	ifstream file(path, std::ios::binary);
	if (file.is_open()) {

		char header[4]; // Why tf is this 4??? wtf c++
		file.read(header, sizeof("ad1"));

		if (strncmp(header, "ad1", 4) == 0) {
			log::info("Loading autodeafen file version 1.");
			while (file.good()) {
				AutoDeafenLevel level;
				file.read(reinterpret_cast<char*>(&level.enabled), sizeof(bool));
				file.read(reinterpret_cast<char*>(&level.levelType), sizeof(short));
				file.read(reinterpret_cast<char*>(&level.id), sizeof(int));
				file.read(reinterpret_cast<char*>(&level.percentage), sizeof(int));
				loadedAutoDeafenLevels.push_back(level);
				log::debug("{} {} {} {}", level.id, level.levelType, level.enabled, level.percentage);
			}
		}

		log::info("Successfully loaded .autodeafen file.");

		file.close();

	} else {
		log::warn("AutoDeafen file failed when trying to open and load. Will create a new one on exit.");
	}
}

class $modify(LoadingLayer) {
	bool init(bool p0) {
		if (!LoadingLayer::init(p0)) return false;
		loadFile();
		return true;
	}
};

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


class $modify(PlayerObject) {
	void playerDestroyed(bool p0) {

		if (this == nullptr) return;
		
		auto playLayer = PlayLayer::get();
		if (playLayer == nullptr) return;

		auto level = playLayer->m_level;
		if (level == nullptr) return;

		if (playLayer->m_player1 == nullptr) return; // Christ, really gotta do every null check possible for it not to crash
		if (this != (playLayer->m_player1) ) return;

		if (level->m_levelType == GJLevelType::Editor) return;
		
		if (level->isPlatformer()) return;
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
		short levelType = getLevelType(level);
		if (levelType == 1) id = m_level -> m_M_ID;
		for (AutoDeafenLevel level : loadedAutoDeafenLevels)
			if (level.id == id && level.levelType == levelType) { currentlyLoadedLevel = level; return true; }

		currentlyLoadedLevel = AutoDeafenLevel(false, levelType, id, 50);
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

		int percent = PlayLayer::getCurrentPercentInt();
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
		saveFile();
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
		void updatePercentage(CCObject* sender) {
			
		}
};

TextInput* percentageInput;
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
			enabledLabel->setPosition(topLeftCorner + ccp(60, -60)); // 80 = center

			enabledButton = CCMenuItemToggler::create(
				CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
				CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
				this,
				menu_selector(ButtonLayer::toggleEnabled)
			);

			enabledButton -> setPosition(enabledLabel->getPosition() + ccp(140,0));
			enabledButton -> setScale(0.85f);
			enabledButton -> setClickable(true);
			enabledButton -> toggle(currentlyLoadedLevel.enabled);



			percentageInput = TextInput::create(100.f, "%");

			percentageInput -> setFilter("0123456789");
			percentageInput -> setPosition(enabledButton->getPosition() + ccp(0, -40));
			percentageInput -> setScale(0.85f);
			percentageInput -> setMaxCharCount(2);
			percentageInput -> setEnabled(true);
			percentageInput -> setString(to_string(currentlyLoadedLevel.percentage));

			auto percentageLabel = CCLabelBMFont::create("Percent", "bigFont.fnt"); 
			percentageLabel->setAnchorPoint({0, 0.5});
			percentageLabel->setScale(0.7f);
			percentageLabel->setPosition(topLeftCorner + ccp(60, -100));

			menu -> addChild(topLabel);
			menu -> addChild(enabledLabel);
			menu -> addChild(enabledButton);
			menu -> addChild(percentageLabel);
			menu -> addChild(percentageInput);

			return true;
		}
		void onClose(CCObject* a) override {
			Popup::onClose(a);
			currentlyLoadedLevel.percentage = stoi(percentageInput -> getString());
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