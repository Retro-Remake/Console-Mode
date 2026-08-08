// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <memory>

#include "State.h"
#include "utils.h"


#define BLACK       "Black"
#define RED         "Red"
#define BLUE        "Blue"
#define GREEN       "Green"
#define YELLOW      "Yellow"
#define GRAY        "Gray"
#define WHITE       "White"


struct ConsoleData {
    std::string name;
    std::vector<std::string> execs;
    std::vector<std::string> romExts;
    std::vector<std::string> romDirs;
};

class Configuration {
private:

    std::string configIniFilepath;
    std::string stateFilepath;
    std::string gamelistpath;

    boost::property_tree::ptree mainPt;

    boost::property_tree::ptree gameListPt;

    // saved as the device /media/fat paths, not the mac-remapped runtime ones
    std::map<std::string, std::string> m_macOrigPaths;

public:

    // config.ini
    static const std::string GLOBAL;
    static const std::string APPLICATION;
    static const std::string SYSTEM;
    static const std::string GAME;
    static const std::string DEVTOOLS;

    static const std::string LOG_ENABLE;

    static const std::string ALIAS_PATH;
    static const std::string HOME_PATH;
    static const std::string ROMS_PATH;    // games root e.g. /media/fat/games
    static const std::string SCREEN_WIDTH;
    static const std::string SCREEN_HEIGHT;
    static const std::string SCREEN_DEPTH;
    static const std::string SCREEN_RES;   // "Default"/"" sentinel or "WxH"
    static const std::string MISTER_INI_RES;
    static const std::string MISTER_INI_HDR;
    static const std::string MISTER_INI_SD;
    static const std::string CUSTOM_BG_COLOR;
    static const std::string CRT_OVERSCAN;   // CRT-only: % trimmed per edge, 0 = off
    static const std::string CRT_FONT;
    static const std::string GAME_STYLE;
    static const std::string BUTTON_STYLE;
    static const std::string CLOCK_FORMAT;
    static const std::string BOOT_TO_GAMES;
    static const std::string HIDE_SECTIONS;
    static const std::string HIDDEN_SECTIONS;  // comma list

    static const std::string VOLUME;
    static const std::string SCREEN_REFRESH;
    static const std::string HISTORY_SIZE;
    static const std::string SHOW_FPS;
    static const std::string SS4_ANIM;
    static const std::string USE_MIRROR;   // self-update via codeload mirror

    static const std::string NET;
    static const std::string LOADCONFIG;
    static const std::string UPDATE_TIME;
    static const std::string BG_COLOR;
    static const std::string BG_ART;
    static const std::string SEL_COLOR;
    static const std::string CONTROLS;
    static const std::string INPUT_TESTER;
    static const std::string KB_MAPPING;
    static const std::string AB_SWAP;        // nav only, leaves the core mapping alone
    static const std::string XY_SWAP;        // nav only
    static const std::string PROMPT_SWAP;    // display only
    static const std::string CD_TESTER;
    static const std::string BGM;
    static const std::string BGM_FILE;
    static const std::string BGM_VOLUME;
    static const std::string BGM_ENABLED;
    static const std::string BGM_MODE;         // "Static" / "Dynamic"
    static const std::string BGM_MENU;
    static const std::string SFX_MENU;
    static const std::string SFX_CONFIRM;
    static const std::string SFX_BACK;
    static const std::string SFX_CONFIRM_FILE;
    static const std::string SFX_BACK_FILE;
    static const std::string SFX_NAV;
    static const std::string SFX_NAV_FILE;
    static const std::string LOADSCRIPT;
    static const std::string BLUETOOTH;
    static const std::string BT_RECONNECT;   // host-initiated reconnect policy (bt_boot.sh)
    static const std::string BT_REPAIR;

    static const std::string CLEAR_CACHE;
    static const std::string UPDATE;
    static const std::string UPDATE_CM;
    static const std::string SCRAPE_ARTWORK;
    static const std::string OPTIMIZE_ARTWORK;
    static const std::string IMPORT_GAMELIST;
    static const std::string MANAGE_HIDDEN;
    static const std::string SHOW_HOMEBREW;
#ifdef HAS_PS1
    static const std::string PS1_FAST;
    static const std::string DISC_OPTIONS;
#endif

    static const std::string DEV_TOOLS;

    static const std::string AUTOBOOT;
    static const std::string BOOT_MAIN_MENU;
    static const std::string TITLE_FILTER;
    static const std::string LOCK_FLIX_TITLE;
    static const std::string CENTER_LIST;
    static const std::string SHOW_RESUME;
    static const std::string BOOT_SPEED;     // "Unsafe" / "Safe" / "Safest"
    static const std::string PERF_MODE;      // "Off" / "Text Only"

    static const std::string LANGUAGE;
    static const std::string QUIT;

    static const std::string CORE_SELECTION;

    static const std::string ROM_OVERCLOCK;
    static const std::string ROM_AUTOSTART;
    static const std::string CORE_OVERRIDE;

    static const std::string GLOBAL_VOLUME;

    // theme.ini
    static const std::string SEL_ITEM_FONT_COLOR;
    static const std::string ITEMS_FONT_COLOR;

    static const std::string ART_X;
    static const std::string ART_Y;
    static const std::string ART_MAX_W;
    static const std::string ART_MAX_H;
    static const std::string ART_TXT_DIST_FROM_PIC;
    static const std::string ART_TXT_LINE_SEP;
    static const std::string GAME_COUNT_FONT_COLOR;
    static const std::string GAME_COUNT_X;
    static const std::string GAME_COUNT_Y;
    static const std::string GAME_COUNT_ALIGNMENT;
    static const std::string GAME_LIST_X;
    static const std::string GAME_LIST_Y;
    static const std::string ITEMS;
    static const std::string ITEMS_SEPARATION;
    static const std::string TEXT1_X;
    static const std::string TEXT1_Y;
    static const std::string TEXT1_ALIGNMENT;
    static const std::string TEXT2_X;
    static const std::string TEXT2_Y;
    static const std::string TEXT2_ALIGNMENT;
    static const std::string THEME_FONT;

    static const std::string TOP_Y;

    static const std::string FOLDER_FONT_SIZE;
    static const std::string FOLDER_LIST_Y;
    static const std::string FOLDER_ITEMS;
    static const std::string FOLDER_ITEMS_STEP;
    static const std::string FOLDER_FONT_COLOR;

    static const std::string HISTORY;
    static const std::string FAVORITES;
    static const std::string HIDDEN;


    Configuration(const std::string& configIniFilepath,
                  const std::string& gamelistpath, 
                  const std::string& stateFilepath);

    void set(const std::string& id, const std::string& value);

    std::string get(const std::string& id) const;
    bool getBool(const std::string& id) const;
    int getInt(const std::string& id) const;

    std::set<std::string> getList(const std::string& id, 
                                  const char delimiter = ',') const;
    std::map<std::string, ConsoleData> parseIniFile(const std::string& iniPath);

    void saveConfigIni();

    void saveGameListIni();

    State loadState();
    void saveState(const State& state);

    void getHistoryOrFavoList(const std::string& name, std::vector<std::string>& list);
    void setHistoryOrFavoList(const std::string& name, std::vector<std::string>& list);
};
