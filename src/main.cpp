#include <iostream>
#include <fstream>

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/ui/GeodeUI.hpp>
#include "../build/bindings/bindings/Geode/binding/GJGameLevel.hpp"

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
uint32_t deafenKeybind[] = {0xA1, 0x76};

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

	log::info("Played deafen keybind.");

	// TODO - make this configurable

	keybd_event(deafenKeybind[0], 0, 0x0000, 0);

	keybd_event(deafenKeybind[1], 0, 0x0000, 0);
	keybd_event(deafenKeybind[1], 0, 0x0002, 0);


	keybd_event(deafenKeybind[0], 0, 0x0002, 0);



}


void loadFromFile(GJGameLevel* lvl) {

	auto path = getFilePath(lvl);

	if (!ghc::filesystem::exists(path)) {
		return;
	}

}


class $modify(PlayerObject) {
	TodoReturn playerDestroyed(bool p0) {
		PlayerObject::playerDestroyed(p0);
		if (hasDeafenedThisAttempt) {
			hasDeafenedThisAttempt = false;
			playDeafenKeybind();
		}
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
	
	void updateProgressbar() {

		PlayLayer::updateProgressbar();

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
	
};

class $modify(MyPauseMenuLayer, PauseLayer) {

	void customSetup() {
		PauseLayer::customSetup();
		auto winSize = CCDirector::sharedDirector() -> getWinSize();

		CCSprite* sprite = CCSprite::createWithSpriteFrameName("GJ_musicOffBtn_001.png");

		auto btn = CCMenuItemSpriteExtra::create(sprite, this, nullptr);
		auto menu = this -> getChildByID("right-button-menu");

		
	}
};
