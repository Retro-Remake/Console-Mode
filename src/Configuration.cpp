// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include "Configuration.h"
#include "Exception.h"
#include "utils.h"
#include "IniSafe.h"        // atomic writes + corrupt-file recovery
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <cstring>

// config.ini
const std::string Configuration::GLOBAL = std::string("GLOBAL");
const std::string Configuration::APPLICATION = std::string("APPLICATION");
const std::string Configuration::SYSTEM = std::string("SYSTEM");
const std::string Configuration::GAME = std::string("GAME");
const std::string Configuration::DEVTOOLS = std::string("DEVTOOLS");

const std::string Configuration::ALIAS_PATH = std::string("GLOBAL.aliasPath");
const std::string Configuration::HOME_PATH = std::string("GLOBAL.homePath");
const std::string Configuration::ROMS_PATH = std::string("GLOBAL.romsPath");
const std::string Configuration::SCREEN_WIDTH = std::string("GLOBAL.screenWidth");
const std::string Configuration::SCREEN_HEIGHT = std::string("GLOBAL.screenHeight");
const std::string Configuration::SCREEN_DEPTH = std::string("GLOBAL.screenDepth");
const std::string Configuration::SCREEN_RES = std::string("APPLICATION.screenRes");
const std::string Configuration::MISTER_INI_RES = std::string("APPLICATION.misterIniRes");   // editable, Enter writes video_mode to the active ini
const std::string Configuration::MISTER_INI_HDR = std::string("APPLICATION.misterIniHdr");
const std::string Configuration::MISTER_INI_SD = std::string("APPLICATION.misterIniScandoubler");
const std::string Configuration::CUSTOM_BG_COLOR = std::string("APPLICATION.customBgColor");
const std::string Configuration::CRT_OVERSCAN = std::string("APPLICATION.crtOverscan");
const std::string Configuration::CRT_FONT = std::string("APPLICATION.crtFont");
const std::string Configuration::GAME_STYLE = std::string("APPLICATION.gameStyle");
const std::string Configuration::BUTTON_STYLE = std::string("APPLICATION.buttonStyle");
const std::string Configuration::CLOCK_FORMAT = std::string("APPLICATION.clockFormat");
const std::string Configuration::BOOT_TO_GAMES = std::string("APPLICATION.bootToGames");
const std::string Configuration::HIDE_SECTIONS = std::string("APPLICATION.hideSections");
const std::string Configuration::HIDDEN_SECTIONS = std::string("APPLICATION.hiddenSections");
const std::string Configuration::LOG_ENABLE = std::string("GLOBAL.loggingEnabled");

const std::string Configuration::VOLUME = std::string("APPLICATION.volume");
const std::string Configuration::QUIT = std::string("APPLICATION.quit");
const std::string Configuration::UPDATE_TIME = std::string("APPLICATION.updateTime");
const std::string Configuration::BG_COLOR = std::string("APPLICATION.bgColor");
const std::string Configuration::BG_ART = std::string("APPLICATION.bgArt");
const std::string Configuration::SEL_COLOR = std::string("APPLICATION.selColor");
const std::string Configuration::NET = std::string("APPLICATION.net");
const std::string Configuration::LOADCONFIG = std::string("APPLICATION.loadConfig");
const std::string Configuration::BLUETOOTH = std::string("APPLICATION.bluetooth");
const std::string Configuration::BT_RECONNECT = std::string("APPLICATION.btReconnect");
const std::string Configuration::BT_REPAIR = std::string("APPLICATION.btRePair");
const std::string Configuration::CLEAR_CACHE = std::string("APPLICATION.clearcache");
const std::string Configuration::UPDATE = std::string("APPLICATION.update");
const std::string Configuration::UPDATE_CM = std::string("APPLICATION.updateCM");
const std::string Configuration::SCRAPE_ARTWORK = std::string("APPLICATION.scrapeArtwork");
const std::string Configuration::OPTIMIZE_ARTWORK = std::string("APPLICATION.optimizeArtwork");
const std::string Configuration::IMPORT_GAMELIST = std::string("APPLICATION.importGamelist");
const std::string Configuration::MANAGE_HIDDEN = std::string("APPLICATION.manageHidden");
const std::string Configuration::DEV_TOOLS = std::string("APPLICATION.devtools");

const std::string Configuration::SCREEN_REFRESH = std::string("DEVTOOLS.screenRefresh");
const std::string Configuration::HISTORY_SIZE = std::string("DEVTOOLS.historySize");
const std::string Configuration::SHOW_FPS = std::string("DEVTOOLS.showFPS");
const std::string Configuration::SS4_ANIM = std::string("DEVTOOLS.ss4Anim");
const std::string Configuration::USE_MIRROR = std::string("DEVTOOLS.useMirror");
const std::string Configuration::LANGUAGE = std::string("DEVTOOLS.language");
#ifdef HAS_PS1
const std::string Configuration::PS1_FAST = std::string("DEVTOOLS.ps1fast");
const std::string Configuration::DISC_OPTIONS = std::string("DEVTOOLS.discOptions");
#endif
const std::string Configuration::CONTROLS = std::string("DEVTOOLS.controls");
const std::string Configuration::INPUT_TESTER = std::string("DEVTOOLS.inputTester");
const std::string Configuration::KB_MAPPING = std::string("APPLICATION.kbMapping");
const std::string Configuration::AB_SWAP = std::string("CONTROLS.abSwap");
const std::string Configuration::XY_SWAP = std::string("CONTROLS.xySwap");
const std::string Configuration::PROMPT_SWAP = std::string("CONTROLS.promptSwap");
const std::string Configuration::CD_TESTER = std::string("DEVTOOLS.cdTester");
const std::string Configuration::BGM = std::string("APPLICATION.bgm");
const std::string Configuration::BGM_FILE = std::string("APPLICATION.bgmFile");
const std::string Configuration::BGM_VOLUME = std::string("APPLICATION.bgmVolume");
const std::string Configuration::BGM_ENABLED = std::string("APPLICATION.bgmEnabled");
const std::string Configuration::BGM_MODE = std::string("APPLICATION.bgmMode");
const std::string Configuration::BGM_MENU = std::string("APPLICATION.bgmMenu");
const std::string Configuration::SFX_MENU = std::string("APPLICATION.sfxMenu");
const std::string Configuration::SFX_CONFIRM = std::string("APPLICATION.confirmSound");
const std::string Configuration::SFX_BACK = std::string("APPLICATION.backSound");
const std::string Configuration::SFX_CONFIRM_FILE = std::string("APPLICATION.sfxConfirm");
const std::string Configuration::SFX_BACK_FILE = std::string("APPLICATION.sfxBack");
const std::string Configuration::SFX_NAV = std::string("APPLICATION.navSound");
const std::string Configuration::SFX_NAV_FILE = std::string("APPLICATION.sfxNav");
const std::string Configuration::LOADSCRIPT = std::string("DEVTOOLS.loadScript");
const std::string Configuration::SHOW_HOMEBREW = std::string("DEVTOOLS.showHomebrew");
const std::string Configuration::AUTOBOOT = std::string("DEVTOOLS.autoboot");
const std::string Configuration::BOOT_MAIN_MENU = std::string("DEVTOOLS.bootMainMenu");
const std::string Configuration::TITLE_FILTER = std::string("DEVTOOLS.titleFiltering");
const std::string Configuration::LOCK_FLIX_TITLE = std::string("DEVTOOLS.lockFlixTitle");
const std::string Configuration::CENTER_LIST = std::string("DEVTOOLS.centerList");
const std::string Configuration::SHOW_RESUME = std::string("DEVTOOLS.showResume");
const std::string Configuration::BOOT_SPEED = std::string("DEVTOOLS.bootSpeed");
const std::string Configuration::PERF_MODE = std::string("DEVTOOLS.perfMode");


const std::string Configuration::CORE_SELECTION = std::string("SYSTEM.coreSelection");

const std::string Configuration::ROM_OVERCLOCK = std::string("GAME.romOverclock");
const std::string Configuration::ROM_AUTOSTART = std::string("GAME.romAutostart");
const std::string Configuration::CORE_OVERRIDE = std::string("GAME.coreOverride");

const std::string Configuration::GLOBAL_VOLUME = std::string("VOLUME.globalVolume");

// theme.ini
const std::string Configuration::SEL_ITEM_FONT_COLOR = std::string("DEFAULT.selected_item_font_color");
const std::string Configuration::ITEMS_FONT_COLOR = std::string("DEFAULT.items_font_color");

const std::string Configuration::ART_X = std::string("GENERAL.art_x");
const std::string Configuration::ART_Y = std::string("GENERAL.art_y");
const std::string Configuration::ART_MAX_W = std::string("GENERAL.art_max_w");
const std::string Configuration::ART_MAX_H = std::string("GENERAL.art_max_h");
const std::string Configuration::ART_TXT_DIST_FROM_PIC = std::string("GENERAL.art_text_distance_from_picture");
const std::string Configuration::ART_TXT_LINE_SEP = std::string("GENERAL.art_text_line_separation");
const std::string Configuration::GAME_COUNT_ALIGNMENT = std::string("GENERAL.game_count_alignment");
const std::string Configuration::GAME_COUNT_FONT_COLOR = std::string("GENERAL.game_count_font_color");
const std::string Configuration::GAME_COUNT_X = std::string("GENERAL.game_count_x");
const std::string Configuration::GAME_COUNT_Y = std::string("GENERAL.game_count_y");
const std::string Configuration::GAME_LIST_X = std::string("GENERAL.game_list_x");
const std::string Configuration::GAME_LIST_Y = std::string("GENERAL.game_list_y");
const std::string Configuration::ITEMS = std::string("GENERAL.items");
const std::string Configuration::ITEMS_SEPARATION = std::string("GENERAL.items_separation");
const std::string Configuration::TEXT1_X= std::string("GENERAL.text1_x");
const std::string Configuration::TEXT1_Y= std::string("GENERAL.text1_y");
const std::string Configuration::TEXT1_ALIGNMENT= std::string("GENERAL.text1_alignment");
const std::string Configuration::TEXT2_X= std::string("GENERAL.text2_x");
const std::string Configuration::TEXT2_Y = std::string("GENERAL.text2_y");
const std::string Configuration::TEXT2_ALIGNMENT = std::string("GENERAL.text2_alignment");
const std::string Configuration::THEME_FONT = std::string("GENERAL.font");
const std::string Configuration::TOP_Y = std::string("GENERAL.top_y");

const std::string Configuration::FOLDER_FONT_SIZE  = std::string("GENERAL.folder_font_size");
const std::string Configuration::FOLDER_LIST_Y = std::string("GENERAL.folder_list_y");
const std::string Configuration::FOLDER_ITEMS = std::string("GENERAL.folder_items");
const std::string Configuration::FOLDER_ITEMS_STEP = std::string("GENERAL.folder_items_separation");
const std::string Configuration::FOLDER_FONT_COLOR = std::string("GENERAL.folder_sel_item_font_color");

const std::string Configuration::HISTORY = std::string("HISTORY");
const std::string Configuration::FAVORITES = std::string("FAVORITES");
const std::string Configuration::HIDDEN = std::string("HIDDEN");

Configuration::Configuration(const std::string& configIniFilepath, 
                             const std::string& gamelistpath, 
                             const std::string& stateFilepath) 
    : configIniFilepath(configIniFilepath), gamelistpath(gamelistpath),stateFilepath(stateFilepath) {

    // corrupt config.ini -> boot on defaults but seed homePath, nothing else regenerates it
    if (!inisafe::readIni(configIniFilepath, mainPt)) {
        if (mainPt.get_optional<std::string>("GLOBAL.homePath") == boost::none)
            mainPt.put("GLOBAL.homePath", std::string(getHomePath()) + "/");
    }

#ifdef MACOS
    // remap the device /media/fat paths onto the SD mount ($CONSOLEMODE_HOME)
    {
        std::string home = getHomePath();
        const char* pathKeys[] = {
            "GLOBAL.homePath", "GLOBAL.themePath", "GLOBAL.romsPath",
            "GLOBAL.aliasPath", "GLOBAL.systemMenuJSON", "GLOBAL.romMenuJSON",
            "GLOBAL.imagesPath",
        };
        for (const char* k : pathKeys) {
            auto v = mainPt.get_optional<std::string>(k);
            if (!v) continue;
            std::string s = *v;
            m_macOrigPaths[k] = s;   // keep the device path so saves stay device-correct
            if (s.rfind("/media/fat", 0) == 0)
                s = home + s.substr(std::strlen("/media/fat"));
            else if (s.rfind("/media/images", 0) == 0)
                s = home + "/media/images" + s.substr(std::strlen("/media/images"));
            mainPt.put(k, s);
        }
        // keep the device's configured dims so a save doesn't clobber them with the mac window size
        for (const char* k : { "GLOBAL.screenWidth", "GLOBAL.screenHeight" }) {
            auto v = mainPt.get_optional<std::string>(k);
            if (v) m_macOrigPaths[k] = *v;
        }
    }
#endif

    if (!std::filesystem::exists(gamelistpath)) {
        std::ofstream file(gamelistpath);
        if (file) {
            file << "[FAVORITES]\n";
            file << "[HISTORY]\n";
            file << "[HIDDEN]\n";
            file.close();
        }
    }

    // corrupt gamelist.ini -> back up, start with empty favorites/history
    if (!inisafe::readIni(gamelistpath, gameListPt)) {
        gameListPt.put_child("FAVORITES", boost::property_tree::ptree());
        gameListPt.put_child("HISTORY",   boost::property_tree::ptree());
        gameListPt.put_child("HIDDEN",    boost::property_tree::ptree());
    }

    enableLogging(getInt(Configuration::LOG_ENABLE));
}

void Configuration::set(const std::string& id, const std::string& value) {
    mainPt.put(id, value);
}

std::string Configuration::get(const std::string& id) const {
    // missing key -> "" instead of throwing
    return mainPt.get<std::string>(id, std::string());
}

bool Configuration::getBool(const std::string& id) const {
    // missing key -> false, never throws
    return mainPt.get<bool>(id, false);
}

int Configuration::getInt(const std::string& id) const {
    // missing key -> 0, never throws
    return mainPt.get<int>(id, 0);
}

std::set<std::string> Configuration::getList(const std::string& id, 
                                             const char delimiter) const {
    std::set<std::string> result;
    std::string value = get(id);
    if (value.empty()) return result;   // don't insert an empty "" token

    size_t start = 0;
    size_t end = value.find(delimiter);
    while (end != std::string::npos) {
        result.insert(value.substr(start, end - start));
        start = end + 1;
        end = value.find(delimiter, start);
    }
    result.insert(value.substr(start, end));

    return result;
}


void Configuration::getHistoryOrFavoList(const std::string& name, std::vector<std::string>& list)
{
    if (auto his = gameListPt.get_child_optional(name))  {
        for (auto& child : *his) {
            std::string value = child.second.data();
            // keep every entry, don't prune paths absent now (unplugged/late USB) or they're lost
            if (!value.empty()) list.push_back(value);
        }
    }
}

void Configuration::setHistoryOrFavoList(const std::string& name, std::vector<std::string>& list)
{
    auto ptree = gameListPt.get_child_optional(name);
    if (!ptree)
        ptree = gameListPt.put_child(name, {});
    
    if (ptree)
    {       
        ptree->clear();
        std::string key = "";
        int count = 0;
        for (auto item:list){
            key = "PATH" + std::to_string(count);
            count++;
            ptree->put(key,item);
        }
    }
}


std::map<std::string, ConsoleData> Configuration::parseIniFile(const std::string& iniPath) {
    std::map<std::string, ConsoleData> consoleDataMap;
    boost::property_tree::ptree pt;
    // skip a stray/incomplete section_groups file instead of throwing ptree_bad_path
    try {
        if (!inisafe::readIni(iniPath, pt)) return consoleDataMap;
        auto consoleList = pt.get<std::string>("CONSOLES.consoleList");
        std::stringstream ss(consoleList);
        std::string consoleName;

        while (std::getline(ss, consoleName, ',')) {
            ConsoleData data;
            data.name = consoleName;

            std::string execs_str = pt.get<std::string>(consoleName + ".execs");
            std::stringstream fieldSs(execs_str);
            std::string exec;
            while (std::getline(fieldSs, exec, ',')) {
                data.execs.push_back(remapMediaFat(exec));
            }
            std::string romExts_str = pt.get<std::string>(consoleName + ".romExts");
            fieldSs = std::stringstream(romExts_str);
            std::string romExt;
            while (std::getline(fieldSs, romExt, ',')) {
                data.romExts.push_back(romExt);
            }
            std::string romDirs_str = pt.get<std::string>(consoleName + ".romDirs");
            fieldSs = std::stringstream(romDirs_str);
            std::string romDir;
            while (std::getline(fieldSs, romDir, ',')) {
                data.romDirs.push_back(remapMediaFat(romDir));
            }

            consoleDataMap[consoleName] = data;
        }
    } catch (const std::exception& e) {
        logMessage(ERROR, "parseIniFile", (iniPath + ": " + e.what()).c_str());
        inisafe::g_fileReset = true;   // surface the one-time "file skipped/reset" notice
    }
    return consoleDataMap;
}

void Configuration::saveConfigIni() {
#ifdef MACOS
    // persist the device /media/fat paths, not the mac-remapped runtime ones
    std::map<std::string, std::string> runtime;
    for (const auto& kv : m_macOrigPaths) {
        auto cur = mainPt.get_optional<std::string>(kv.first);
        if (cur) runtime[kv.first] = *cur;
        mainPt.put(kv.first, kv.second);
    }
    inisafe::writeIniAtomic(configIniFilepath, mainPt);
    for (const auto& kv : runtime)
        mainPt.put(kv.first, kv.second);
#else
    inisafe::writeIniAtomic(configIniFilepath, mainPt);
#endif
}

void Configuration::saveGameListIni(){
    inisafe::writeIniAtomic(gamelistpath, gameListPt);   // atomic so a power cut can't truncate the list
}

State Configuration::loadState() {

    boost::property_tree::ptree statePt;
    try {
        boost::property_tree::json_parser::read_json(stateFilepath, statePt);
    } catch (const boost::property_tree::json_parser_error& e) {
        throw StateNotFoundException(
            "Error loading state: " + std::string(e.what()));
    }

    State state;
    try {
        std::string currentMenuLevelStr = 
            statePt.get<std::string>("currentMenuLevel");

        if (currentMenuLevelStr == "MENU_MAIN") {
            state.currentMenuLevel = MenuLevel::MENU_MAIN;
        } else if (currentMenuLevelStr == "MENU_SECTION") {
            state.currentMenuLevel = MenuLevel::MENU_SECTION;
        } else if (currentMenuLevelStr == "MENU_FOLDER") {
            state.currentMenuLevel = MenuLevel::MENU_FOLDER;
        } else if (currentMenuLevelStr == "MENU_ROM") {
            state.currentMenuLevel = MenuLevel::MENU_ROM;
        } else if (currentMenuLevelStr == "APP_SETTINGS") {
            state.currentMenuLevel = MenuLevel::APP_SETTINGS;
        } else if (currentMenuLevelStr == "MENU_RBF") {
            state.currentMenuLevel = MenuLevel::MENU_RBF;
        } else if (currentMenuLevelStr == "MENU_FAVORITES") {
            state.currentMenuLevel = MenuLevel::MENU_FAVORITES;
        } else if (currentMenuLevelStr == "MENU_HISTORY") {
            state.currentMenuLevel = MenuLevel::MENU_HISTORY;
#ifdef HAS_PS1
        } else if (currentMenuLevelStr == "MENU_CD") {
            state.currentMenuLevel = MenuLevel::MENU_CD;
#endif
        } else if (currentMenuLevelStr == "MENU_HOMEBREW_FOLDER") {
            state.currentMenuLevel = MenuLevel::MENU_HOMEBREW_FOLDER;
        } else if (currentMenuLevelStr == "MENU_HOMEBREW_GAME") {
            state.currentMenuLevel = MenuLevel::MENU_HOMEBREW_GAME;
        } else if (currentMenuLevelStr == "MENU_SUBROM") {
            state.currentMenuLevel = MenuLevel::MENU_MAIN;
        } else if (currentMenuLevelStr == "MENU_SELECT_CORE") {
            state.currentMenuLevel = MenuLevel::MENU_MAIN;
        }
        else {
            // unknown level string -> land on the main menu
            state.currentMenuLevel = MenuLevel::MENU_MAIN;
            logMessage(INFO, "loadState",
                       ("unknown currentMenuLevel: " + currentMenuLevelStr).c_str());
        }
            
        state.currentSectionIndex = statePt.get<int>("currentSectionIndex");
        state.resumePath = statePt.get<std::string>("resumePath");  

        state.currentFolderIndex = statePt.get<int>("currentFolderIndex");
        state.currentRomIndex = statePt.get<int>("currentRomIndex");

        state.currentMainIndex = 0;

        state.currentRbfIndex = statePt.get<int>("currentRbfIndex");

        state.currentFavoritesIndex = statePt.get<int>("currentFavoritesIndex");
        state.currentHistoryIndex = statePt.get<int>("currentHistoryIndex");

        // defaulted so older state files without these keys still load
        state.currentHomebrewFolderIndex = statePt.get<int>("currentHomebrewFolderIndex", 0);
        state.currentHomebrewGameIndex   = statePt.get<int>("currentHomebrewGameIndex", 0);

    
    } catch (const std::exception&) {
        // partial/old state.json, keep whatever parsed
    }

    return state;
}


void Configuration::saveState(const State& state) {
    
        boost::property_tree::ptree statePt;
    
        std::string currentMenuLevelStr;
        switch (state.currentMenuLevel) {            
            case MenuLevel::MENU_SECTION:
                currentMenuLevelStr = "MENU_SECTION";
                break;
            case MenuLevel::MENU_FOLDER:
                currentMenuLevelStr = "MENU_FOLDER";
                break;
            case MenuLevel::MENU_ROM:
                currentMenuLevelStr = "MENU_ROM";
                break;
            case MenuLevel::APP_SETTINGS:
                currentMenuLevelStr = "APP_SETTINGS";
                break;
            case MenuLevel::MENU_MAIN:
                currentMenuLevelStr = "MENU_MAIN";
                break;
            case MenuLevel::MENU_RBF:
                currentMenuLevelStr = "MENU_RBF";
                break;
            case MenuLevel::MENU_FAVORITES:
                currentMenuLevelStr = "MENU_FAVORITES";
                break;
            case MenuLevel::MENU_HISTORY:
                currentMenuLevelStr = "MENU_HISTORY";
                break;
#ifdef HAS_PS1
            case MenuLevel::MENU_CD:
                currentMenuLevelStr = "MENU_CD";
                break;
#endif
            case MenuLevel::MENU_HOMEBREW_FOLDER:
                currentMenuLevelStr = "MENU_HOMEBREW_FOLDER";
                break; 
            case MenuLevel::MENU_HOMEBREW_GAME:
                currentMenuLevelStr = "MENU_HOMEBREW_GAME";
                break;
            case MenuLevel::MENU_SUBROM:
                currentMenuLevelStr = "MENU_ROM";
                break; 
            case MenuLevel::MENU_SELECT_CORE:
                currentMenuLevelStr = "MENU_ROM";
                break;
            case MenuLevel::MENU_SEARCH_RESULTS:   // alternate view of the system's rom list
                currentMenuLevelStr = "MENU_ROM";
                break;

            default:
                // any non-persistable level persists as MENU_MAIN instead of throwing
                logMessage(INFO, "saveState default->MENU_MAIN",
                           std::to_string((int)state.currentMenuLevel).c_str());
                currentMenuLevelStr = "MENU_MAIN";
                break;
        }
    
        statePt.put("currentMenuLevel", currentMenuLevelStr);
        
        statePt.put("currentSectionIndex", state.currentSectionIndex);
        statePt.put("currentFolderIndex", state.currentFolderIndex);
        statePt.put("currentRomIndex", state.currentRomIndex);

        statePt.put("currentMainIndex", state.currentMainIndex);
        statePt.put("currentRbfIndex", state.currentRbfIndex);

        statePt.put("currentFavoritesIndex", state.currentFavoritesIndex);
        statePt.put("currentHistoryIndex", state.currentHistoryIndex);

        statePt.put("currentHomebrewFolderIndex", state.currentHomebrewFolderIndex);
        statePt.put("currentHomebrewGameIndex", state.currentHomebrewGameIndex);

        statePt.put("resumePath", state.resumePath);
    
        inisafe::writeJsonAtomic(stateFilepath, statePt);
    
}
