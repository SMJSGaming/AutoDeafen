#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>

#include <stdio.h>
#include <locale>
#include <codecvt>
#include <string.h>
#include <windows.h> // screw macos lmao
#include <WinUser.h>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/LoadingLayer.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include <cocos2d.h>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;
using namespace std;

struct AutoDeafenLevel {
	bool enabled = false;
	short levelType; // 0 = Normal, 1 = Local/Editor, 2 = Daily/Weekly, 3 = gauntlet
	int id = 0;
	short percentage = 50;
	AutoDeafenLevel(bool a, short b, int c, short d) { // I am lazy lmao
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
vector<uint32_t> deafenKeybind = {};

short getLevelType(GJGameLevel* level) {

	if (level -> m_levelType != GJLevelType::Saved) return 1;
	if (level -> m_dailyID > 0) return 2;
	if (level -> m_gauntletLevel) return 3;

	return 0;

}

void saveFile() {

	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";

	// log::info("{}", "Saving .autodeafen file to " + path.string());

	ofstream file(path);
	if (file.is_open()) {
		file.write("ad1", sizeof("ad1")); // File Header - autodeafen file version 1



		int size = deafenKeybind.size();

		std::string keycodes = "";
		for (const uint32_t key : deafenKeybind)
			keycodes = keycodes + to_string(key) + ", ";
		keycodes.pop_back();keycodes.pop_back();

		// log::info("{}{}", "Keys: ", keycodes);
		for (uint32_t key : deafenKeybind)
			file.write(reinterpret_cast<const char*>(&key), sizeof(uint32_t));
		uint32_t filler = 0xFFFFFFFF;
		for (int i = 0; i < (4 - size); i++)
			file.write(reinterpret_cast<const char*>(&filler), sizeof(uint32_t)); // Fillers



		for (AutoDeafenLevel const& a : loadedAutoDeafenLevels) {
			file.write(reinterpret_cast<const char*>(&a.enabled), sizeof(bool));
			file.write(reinterpret_cast<const char*>(&a.levelType), sizeof(short));
			file.write(reinterpret_cast<const char*>(&a.id), sizeof(int));
			file.write(reinterpret_cast<const char*>(&a.percentage), sizeof(short));
		}
		file.close();
		log::debug("Successfully saved .autodeafen file.");

	} else {
		log::error("AutoDeafen file failed when trying to open and save.");
	}

}

void loadFile() {
	auto path = Mod::get() -> getSaveDir();
	path /= ".autodeafen";

	log::info("{}", "Loading .autodeafen file from " + path.string());

	ifstream file(path, std::ios::binary);
	if (file.is_open()) {

		char header[4]; // Why on earth is the length of "ad1" 4??? wtf c++
		file.read(header, sizeof("ad1"));

		if (strncmp(header, "ad1", 4) == 0) {
			log::info("Loading autodeafen file version 1.");
			for (int i = 0; i < 4; i++) {
				uint32_t r;
				file.read(reinterpret_cast<char*>(&r), sizeof(uint32_t));
				if (r == 0xFFFFFFFF) continue;
				else deafenKeybind.push_back(r);
			}
			while (file.good()) {
				AutoDeafenLevel level;
				file.read(reinterpret_cast<char*>(&level.enabled), sizeof(bool));
				file.read(reinterpret_cast<char*>(&level.levelType), sizeof(short));
				file.read(reinterpret_cast<char*>(&level.id), sizeof(int));
				file.read(reinterpret_cast<char*>(&level.percentage), sizeof(short));
				loadedAutoDeafenLevels.push_back(level);
				// log::debug("{} {} {} {}", level.id, level.levelType, level.enabled, level.percentage);
			}
		}

		log::info("Successfully loaded .autodeafen file.");

		file.close();

	} else {
		log::warn("AutoDeafen file failed when trying to open and load (probably just doesn't exist). Will create a new one on exit.");
		deafenKeybind = {0xA1, 0x48};
	}
}

void saveLevel(AutoDeafenLevel lvl) {

	for (auto level : loadedAutoDeafenLevels) {
		if (level.id == lvl.id) {
			level = lvl;
			return;
		}
	}
	if (lvl.percentage != 50 || lvl.enabled) // Don't bother wasting file size if it's the default already
		loadedAutoDeafenLevels.push_back(lvl);
	saveFile();

}

void sendKeyEvent(uint32_t key, int state) {
	INPUT inputs[1];
	inputs[0].type = INPUT_KEYBOARD;
	if (key == 163 || key == 165) inputs[0].ki.dwFlags = state | KEYEVENTF_EXTENDEDKEY; 
	else inputs[0].ki.dwFlags = state;
	inputs[0].ki.wScan = 0;
	inputs[0].ki.wVk = key;

	SendInput(1, inputs, sizeof(INPUT));
}

void triggerDeafenKeybind() {

	if (currentlyLoadedLevel.enabled) {
		log::info("Triggered deafen keybind.");

		const int size = deafenKeybind.size();

		for (int i = 0; i < size - 1; i++) {
			uint32_t key = deafenKeybind[i];
			sendKeyEvent(key, 0);
		}

		// The non-modifier key will always be at the end because of how I coded it.
		uint32_t nm = deafenKeybind[size - 1];
		sendKeyEvent(nm, 0);
		sendKeyEvent(nm, 2);

		for (int i = 0; i < size - 1; i++) {
			uint32_t key = deafenKeybind[i];
			sendKeyEvent(key, 2);
		}

	}



}


class $modify(LoadingLayer) {
	bool init(bool p0) {
		if (!LoadingLayer::init(p0)) return false;
		loadFile();
		return true;
	}
};

class $modify(PlayerObject) {
	void playerDestroyed(bool p0) {
		if (this != nullptr) {
			auto playLayer = PlayLayer::get();
			if (playLayer != nullptr) {
				auto level = playLayer->m_level;
				if (level != nullptr) {
					
					if (	playLayer->m_player1 != nullptr &&
							this == (playLayer->m_player1) &&
							!(level->isPlatformer()) &&
							!(playLayer->m_isPracticeMode)) {

						if (hasDeafenedThisAttempt && !hasDied) {
							hasDied = true;
							triggerDeafenKeybind();
						}}}}}
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
	
	void postUpdate(float p0) {

		PlayLayer::postUpdate(p0);
		if (this->m_isPracticeMode) return;

		int percent = PlayLayer::getCurrentPercentInt();
		// log::info("{}", currentlyLoadedLevel.percentage);
		// log::info("{}", percent);
		if (percent >= currentlyLoadedLevel.percentage && percent != 100 && !hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = true;
			triggerDeafenKeybind();
		}

	}

	void levelComplete() {
		PlayLayer::levelComplete();
		if (hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = false;
			triggerDeafenKeybind();
		}
	}

	void onQuit() {
		PlayLayer::onQuit();
		saveLevel(currentlyLoadedLevel);
		currentlyLoadedLevel = AutoDeafenLevel();
		if (hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = false;
			triggerDeafenKeybind();
		}
	}
	
};



// Shamelessly copied from https://gist.github.com/radj307/201e82048751713eb522386b46d94955
std::wstring KeyNameFromScanCode(const unsigned scanCode) {
    wchar_t buf[32]{};
    GetKeyNameTextW(scanCode << 16, buf, sizeof(buf));
    return{ buf };
}
std::wstring KeyNameFromVirtualKeyCode(const unsigned virtualKeyCode){
    return KeyNameFromScanCode(MapVirtualKeyW(virtualKeyCode, MAPVK_VK_TO_VSC));
}

// Shamelessly copied from chatgpt (i am lazy)
std::string wstring_to_string(const std::wstring& wstr) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(wstr);
}

bool currentlyInMenu = false;

class EditKeybindLayer : public geode::Popup<std::string const&> {

	public:
		CCLabelBMFont* primaryLabel;
		CCLabelBMFont* secondaryLabel;
		CCLabelBMFont* confirmationLabel;
		bool alreadyUsed = false;
	protected:
		virtual void keyDown(cocos2d::enumKeyCodes key) {
			
			if (alreadyUsed) return;

			vector<uint32_t> keys = {}; // Max 4 keys

			for (uint32_t i = 0; i < 6; i++)
				if (GetKeyState(160 + i) < 0 && keys.size() < 3) 
					keys.push_back(160 + i);
			keys.push_back(key);



			std::string str = "";
			std::string keycodes = "";

			for (const uint32_t key : keys) {
				str = str + wstring_to_string(KeyNameFromVirtualKeyCode(key)) + " + ";
				keycodes = keycodes + to_string(key) + ", ";
			}
			str.pop_back();str.pop_back();str.pop_back();
			keycodes.pop_back();keycodes.pop_back();

			

			log::debug("{}{}{}{}", "Keys: ", str, " ||  Keycodes: ", keycodes );

			char* strarray = new char[str.length() + 1];
			strcpy(strarray, str.c_str());

			for (uint32_t key : keys) {
				if (key > 0x404) {
					secondaryLabel->setString("Unsupported key in keybind!");
					confirmationLabel->setString("Change your keybind and try again!");
					return;
				}
			}

			secondaryLabel->setString( strarray, true );
			confirmationLabel->setString("Saved Keybind. You may exit this menu.");
			deafenKeybind = keys;
			alreadyUsed = true;

		}
		bool setup(std::string const& value) override {
			this -> setKeyboardEnabled(true);
			currentlyInMenu = true;

			auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
			CCPoint topLeftCorner = winSize/2.f-ccp(m_size.width/2.f,-m_size.height/2.f);

			primaryLabel = CCLabelBMFont::create("Edit Keybind", "goldFont.fnt");
			primaryLabel->setAnchorPoint({0.5, 0.5});
			primaryLabel->setScale(1.0f);
			primaryLabel->setPosition(topLeftCorner + ccp(142, -20)); // 80 = center

			std::string str = "";
			for (const uint32_t key : deafenKeybind)
				str = str + wstring_to_string(KeyNameFromVirtualKeyCode(key)) + " + ";

			secondaryLabel = CCLabelBMFont::create("Press your deafen keybind...", "bigFont.fnt"); 
			secondaryLabel->setAnchorPoint({0.5, 0.5});
			secondaryLabel->setScale(0.5f);
			secondaryLabel->setPosition(topLeftCorner + ccp(150, -100));

			confirmationLabel = CCLabelBMFont::create("", "goldFont.fnt"); 
			confirmationLabel->setAnchorPoint({0.5, 0.5});
			confirmationLabel->setScale(0.5f);
			confirmationLabel->setPosition(topLeftCorner + ccp(150, -150));

			m_mainLayer -> addChild(primaryLabel);
			m_mainLayer -> addChild(secondaryLabel);
			m_mainLayer -> addChild(confirmationLabel);

			return true;
		}
		void onClose(CCObject* a) override {
			Popup::onClose(a);
			currentlyInMenu = false;
		}
		static EditKeybindLayer* create() {
			auto ret = new EditKeybindLayer();
			if (ret && ret->init(300, 200, "", "GJ_square02.png")) {
				ret->autorelease();
				return ret;
			}
			CC_SAFE_DELETE(ret);
			return nullptr;
		}
	public:
		static EditKeybindLayer* openMenu() {
			auto layer = create();
			layer -> show();
			return layer;
		}

};

CCMenuItemToggler* enabledButton;
class ButtonLayer : public CCLayer {
	public:
		void toggleEnabled(CCObject* sender) {
			currentlyLoadedLevel.enabled = !currentlyLoadedLevel.enabled;
			enabledButton -> toggle(!currentlyLoadedLevel.enabled);
		};
};


TextInput* percentageInput;
class ConfigLayer : public geode::Popup<std::string const&> {
	public:
		void editKeybind(CCObject*) {
			this->onClose(nullptr);

			EditKeybindLayer::openMenu();
			currentlyInMenu = true;

			// this -> setTouchEnabled(true);
			// this -> setKeyboardEnabled(true);
			// popup -> setTouchEnabled(true);
			// popup -> setKeyboardEnabled(true); // may as well just enable all of them
		};
	protected:
		bool setup(std::string const& value) override {

			this->setKeyboardEnabled(true);
			currentlyInMenu = true;

			auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

			CCPoint topLeftCorner = winSize/2.f-ccp(m_size.width/2.f,-m_size.height/2.f);

			auto topLabel = CCLabelBMFont::create("AutoDeafen", "goldFont.fnt"); 
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



			auto editKeybindButton = CCMenuItemSpriteExtra::create(
				ButtonSprite::create("Edit Keybind"),
				this, 
				menu_selector(ConfigLayer::editKeybind)
			);
			editKeybindButton->setAnchorPoint({0.5, 0.5});
			editKeybindButton->setPosition(topLeftCorner + ccp(142, -150));



			auto menu = CCMenu::create();
			menu -> setPosition( {0, 0} );

			
			menu -> addChild(enabledButton);
			menu -> addChild(percentageInput);
			menu -> addChild(editKeybindButton);

			m_mainLayer -> addChild(topLabel);
			m_mainLayer -> addChild(enabledLabel);
			m_mainLayer -> addChild(percentageLabel);

			m_mainLayer -> addChild(menu);

			return true;
		}
		void onClose(CCObject* a) override {
			Popup::onClose(a);
			currentlyLoadedLevel.percentage = stoi(percentageInput -> getString());
			currentlyInMenu = false;
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

	void keyDown(cocos2d::enumKeyCodes p0) {   if (!currentlyInMenu) PauseLayer::keyDown(p0);   }
	void onResume(CCObject* sender)        {   if (!currentlyInMenu) PauseLayer::onResume(sender);   }
	void onRestart(CCObject* sender)       {   if (!currentlyInMenu) PauseLayer::onRestart(sender);     if (hasDeafenedThisAttempt) triggerDeafenKeybind(); }
	void onRestartFull(CCObject* sender)   {   if (!currentlyInMenu) PauseLayer::onRestartFull(sender); if (hasDeafenedThisAttempt) triggerDeafenKeybind(); }
	void onQuit(CCObject* sender)          {   if (!currentlyInMenu) PauseLayer::onQuit(sender);        if (hasDeafenedThisAttempt) triggerDeafenKeybind(); }
	void onPracticeMode(CCObject* sender)  {   if (!currentlyInMenu) PauseLayer::onPracticeMode(sender);   }
	void onSettings(CCObject* sender)      {   if (!currentlyInMenu) PauseLayer::onSettings(sender);   }


};