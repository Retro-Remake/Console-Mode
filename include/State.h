// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once

#include <string>

enum MenuLevel {
    MENU_MAIN,
    MENU_SECTION,
    MENU_FOLDER,
    MENU_ROM,
    MENU_RBF,
    MENU_FAVORITES,
    MENU_HISTORY,
    MENU_HIDDEN_GAMES,
    MENU_HIDDEN_SYSTEM,
    MENU_VOLUME,
    MENU_SCRIPT,
    MENU_CONTROLS,
    MENU_BLUETOOTH,
    MENU_WIFI,
    MENU_WIFI_OPTIONS,
#ifdef HAS_PS1
    MENU_CD,
#endif
    APP_SETTINGS,
    DEVTOOLS_SETTINGS,
    MENU_SYSTEMCONFIG,
    MENU_HOMEBREW_FOLDER,
    MENU_HOMEBREW_GAME,
#ifdef HAS_PS1
    MENU_DISC_OPTIONS,
    MENU_DUMP_DEVICE,
#endif
    MENU_SUBROM,
    MENU_SEARCH_RESULTS,
    MENU_CD_TESTER,
    MENU_BGM,

    MENU_CONTROLS_JOY,
    MENU_CONTROLS_JOY1,
    MENU_CONTROLS_JOY2,
    MENU_JOY_TESTER,

    MENU_OTHER_CD,

    MENU_SELECT_CORE,
    MENU_GAME_OPTIONS,

    MENU_SCREENSHOTS,
    MENU_SCREENSHOT_GRID,
    MENU_SCREENSHOT_VIEW,

    MENU_SCRAPE_SELECT,
    MENU_HIDE_SECTIONS,
    MENU_SCRAPE_PROGRESS,
    MENU_SCRAPE_DONE,
    MENU_OPTIMIZE_PROGRESS,
    MENU_UPDATE_CM_CONFIRM,
    MENU_UPDATE_CM_PROGRESS,

    MENU_INPUT_TESTER,
    MENU_ABOUT,
    MENU_CREDITS,
    MENU_STORAGE,
    MENU_SET_TIME,
    MENU_LICENSE,
    MENU_LICENSE_TEXT,
    MENU_KB_MAP,
    MENU_FEEDBACK,

    MENU_NONE

};

struct State {
    MenuLevel currentMenuLevel;
    int currentSectionIndex;
    int currentFolderIndex;
    int currentRomIndex;

    int currentMainIndex;
    int currentRbfIndex;

    int currentFavoritesIndex;
    int currentHistoryIndex;

    int currentHomebrewFolderIndex;
    int currentHomebrewGameIndex;

    int currentSearchIndex;

    std::string resumePath;
    State()
    {
        currentMenuLevel = MENU_MAIN;
        currentSectionIndex = 0;
        currentFolderIndex = 0;
        currentRomIndex = 0;

        currentMainIndex = 0;
        currentRbfIndex = 0;

        currentFavoritesIndex = 0;
        currentHistoryIndex = 0;

        currentHomebrewFolderIndex = 0;
        currentHomebrewGameIndex = 0;

        currentSearchIndex = 0;

        resumePath = "";

    }
};
