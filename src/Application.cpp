// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Core run loop and command/state dispatch. Derived from simplermenu_plus
// (rg35xx-cfw), MPL-2.0.
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <filesystem>
#ifdef __linux__
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif
#include <algorithm>
#include <array>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include "Application.h"
#include "IniSafe.h"
#include "UpdateManager.h"
#include "boot_timing.h"
#include "build_info.h"
#include "scraper.h"
#include <boost/property_tree/xml_parser.hpp>
#include "Exception.h"
#include "misterini.h"
#include "system_icons.h"
#ifdef __linux__
#include <endian.h>
#endif
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <regex>
#include <exception>
#include <utility>
#include <cctype>
#include "miniz.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <string>
#include <libudev.h>
#include <SDL/SDL.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include "rp2040_led_watch_linux.h"
#include <sys/wait.h>
#include <sys/time.h>
#include <cstdlib>
#include <sys/stat.h>
using namespace appdetail;

void Application::drawCurrentState() {

    renderComponent.clearRedrawFlag();

    static MenuLevel s_prevDrawLevel = MENU_NONE;
    if (state.currentMenuLevel != s_prevDrawLevel) {
        renderComponent.forceFullRedraw();
        s_prevDrawLevel = state.currentMenuLevel;
    }

    auto drawStyledGameList = [&](const std::string& title, std::vector<Rom>& vec,
                                  int idx, MenuLevel m) {
        RenderComponent::GameStyle gs = renderComponent.gameStyle();
        if (gs == RenderComponent::GS_GRID || gs == RenderComponent::GS_FLIX || gs == RenderComponent::GS_SS4) {
            std::vector<std::pair<std::string,std::string>> cells;
            cells.reserve(vec.size());
            for (const Rom& r : vec) cells.push_back({ r.getTitle(), r.getPath() });
            if (gs == RenderComponent::GS_GRID)      renderComponent.drawGrid(title, cells, idx, m);
            else if (gs == RenderComponent::GS_SS4)  renderComponent.drawSS4(title, cells, idx, m);
            else                                     renderComponent.drawFlix(title, cells, idx, m);
        } else {
            renderComponent.drawHistoryOrFavoritesList(title, vec, idx, m);
        }
    };

    auto drawStyledCells = [&](const std::string& title,
                               std::vector<std::pair<std::string,std::string>>& cells,
                               int idx, MenuLevel m) {
        switch (renderComponent.gameStyle()) {
            case RenderComponent::GS_FLIX: renderComponent.drawFlix(title, cells, idx, m); break;
            case RenderComponent::GS_SS4:  renderComponent.drawSS4(title, cells, idx, m); break;
            case RenderComponent::GS_LIST: renderComponent.drawRomList(title, cells, idx, m, false, true); break;
            default:                       renderComponent.drawGrid(title, cells, idx, m); break;
        }
    };

    switch (state.currentMenuLevel) {
        case MENU_MAIN:
        {

            if (Resume >= 0 && Resume < (int)vecMainMenu.size()) {
                if (state.resumePath.size() > 4) {
                    std::string nm = renderComponent.getAlias(state.resumePath);
                    if (nm.size() > 24) {
                        size_t cut = 23;
                        while (cut > 0 && (nm[cut] & 0xC0) == 0x80) cut--;   // don't split a UTF-8 code point
                        nm = nm.substr(0, cut) + "...";
                    }
                    vecMainMenu[Resume] = "Resume Game - " + nm;
                } else {
                    vecMainMenu[Resume] = "Resume Game";
                }
            }
#ifdef HAS_PS1
            renderComponent.drawMainMenu(vecMainMenu,state.currentMainIndex,m_ps1.haveDock());
#else
            renderComponent.drawMainMenu(vecMainMenu,state.currentMainIndex,false);
#endif
            break;
        }
        case MENU_SEARCH_RESULTS:
        {
            if (renderComponent.searchActive()) {
                renderComponent.drawSearchOverlay();
                break;
            }
            if (state.currentSearchIndex < 0 || state.currentSearchIndex >= (int)m_searchResultCells.size())
                state.currentSearchIndex = 0;
            std::string title = "Search \"" + m_searchResultsQuery + "\"";

            RenderComponent::GameStyle gs = renderComponent.gameStyle();
            if (gs == RenderComponent::GS_GRID)
                renderComponent.drawGrid(title, m_searchResultCells, state.currentSearchIndex, MENU_SEARCH_RESULTS);
            else if (gs == RenderComponent::GS_SS4)
                renderComponent.drawSS4(title, m_searchResultCells, state.currentSearchIndex, MENU_SEARCH_RESULTS);
            else if (gs == RenderComponent::GS_FLIX)
                renderComponent.drawFlix(title, m_searchResultCells, state.currentSearchIndex, MENU_SEARCH_RESULTS);
            else
                renderComponent.drawRomList(title, m_searchResultCells, state.currentSearchIndex, MENU_SEARCH_RESULTS, false, false);
            break;
        }
        case MENU_RBF:
        case MENU_FAVORITES:
        case MENU_HISTORY:
        case MENU_HOMEBREW_GAME:
        {
            if (state.currentMenuLevel==MENU_FAVORITES){
                if (state.currentFavoritesIndex>=(int)vecFavoritesFile.size())
                    state.currentFavoritesIndex = 0;
                drawStyledGameList("Favorites",vecFavoritesFile,state.currentFavoritesIndex,MENU_FAVORITES);
            }
            else if (state.currentMenuLevel==MENU_HISTORY){
                std::vector<Rom> tmpVecHistoryFile;
                tmpVecHistoryFile.insert(tmpVecHistoryFile.end(),vecHistoryFile.begin(),vecHistoryFile.end());
                std::reverse(tmpVecHistoryFile.begin(), tmpVecHistoryFile.end());

                if (state.currentHistoryIndex>=(int)tmpVecHistoryFile.size())
                    state.currentHistoryIndex = 0;

                drawStyledGameList("History",tmpVecHistoryFile,state.currentHistoryIndex,MENU_HISTORY);
            }
            else if (state.currentMenuLevel==MENU_RBF){
                if (state.currentRbfIndex>=vecRbfFile.size())
                    state.currentRbfIndex = 0;

                if (renderComponent.searchActive())
                    renderComponent.drawSearchOverlay();
                else
                    renderComponent.drawHistoryOrFavoritesList("Load Core",vecRbfFile,state.currentRbfIndex,MENU_RBF);
            }
            else if (state.currentMenuLevel==MENU_HOMEBREW_GAME){

                std::vector<Rom>* roms = getCurrentHomebrewRomList();
                std::vector<Rom> empty;
                std::vector<Rom>& list = roms ? *roms : empty;
                if (currentHomebrewGameIndex < 0 ||
                    currentHomebrewGameIndex >= static_cast<int>(list.size()))
                    currentHomebrewGameIndex = 0;

                std::string hbTitle = getCurrentHomebrewListTitle();
                RenderComponent::GameStyle gs = renderComponent.gameStyle();
                if (gs == RenderComponent::GS_GRID || gs == RenderComponent::GS_FLIX || gs == RenderComponent::GS_SS4) {
                    std::vector<std::pair<std::string,std::string>> cells;
                    for (const Rom& r : list) {
                        std::string t = r.getTitle();
                        if (r.IsFolder() && !r.IsCue()) t += " [DIR]";   // homebrew tree is eager so IsCue is accurate
                        cells.push_back({t, r.getPath()});
                    }
                    if (gs == RenderComponent::GS_GRID)
                        renderComponent.drawGrid(hbTitle, cells, currentHomebrewGameIndex, MENU_HOMEBREW_GAME);
                    else if (gs == RenderComponent::GS_SS4)
                        renderComponent.drawSS4(hbTitle, cells, currentHomebrewGameIndex, MENU_HOMEBREW_GAME);
                    else
                        renderComponent.drawFlix(hbTitle, cells, currentHomebrewGameIndex, MENU_HOMEBREW_GAME);
                } else {
                    renderComponent.drawHistoryOrFavoritesList(hbTitle, list, currentHomebrewGameIndex, MENU_HOMEBREW_GAME);
                }
            }
            break;
        }
#ifdef HAS_PS1
        case MENU_CD:
        case MENU_OTHER_CD:
        {
            m_ps1.draw(state.currentMenuLevel);
            break;
        }
#endif
        case MENU_SECTION:
        {
            auto& sections = menu.getSections();
            if (state.currentSectionIndex < 0 || state.currentSectionIndex >= (int)sections.size())
                state.currentSectionIndex = 0;

            RenderComponent::GameStyle gs = renderComponent.gameStyle();
            std::vector<std::pair<std::string,std::string>> cells;
            for (Section& section : sections) {
                std::string g = section.getGroupname();
                std::string disp = lowerCopy(g), low = lowerCopy(g);
                if (!disp.empty()) disp[0] = toupper(disp[0]);
                std::string iconPath = cfg.get(Configuration::HOME_PATH)
                    + "ConsoleMode/themeconfig/section_groups/" + low + "1.png";
                cells.push_back({disp, iconPath});
            }

            std::string title = "Load Game";
            renderComponent.setOptionsHint(false);
            if (gs == RenderComponent::GS_FLIX)
                renderComponent.drawFlix(title, cells, state.currentSectionIndex, MENU_FOLDER);
            else if (gs == RenderComponent::GS_SS4)
                renderComponent.drawSS4(title, cells, state.currentSectionIndex, MENU_FOLDER);
            else if (gs == RenderComponent::GS_GRID)
                renderComponent.drawGrid(title, cells, state.currentSectionIndex, MENU_FOLDER);
            else
                renderComponent.drawFolderEx(title, cells, state.currentSectionIndex);
            renderComponent.setOptionsHint(true);
            break;
        }
        case MENU_SCREENSHOTS:
        {
            if (g_shotCores.empty()) { state.currentMenuLevel = MenuLevel::MENU_MAIN; break; }
            if (g_shotCoreIndex < 0 || g_shotCoreIndex >= (int)g_shotCores.size()) g_shotCoreIndex = 0;
            std::vector<std::pair<std::string,std::string>> cells;
            for (auto& c : g_shotCores) cells.push_back({c.first, c.second});
            drawStyledCells(g_shotPickBg ? "Set BG Image" : "Screenshots", cells, g_shotCoreIndex, MENU_SCREENSHOTS);
            break;
        }
        case MENU_SCREENSHOT_GRID:
        {
            if (g_shotFiles.empty()) { state.currentMenuLevel = MenuLevel::MENU_SCREENSHOTS; break; }
            if (g_shotFileIndex < 0 || g_shotFileIndex >= (int)g_shotFiles.size()) g_shotFileIndex = 0;
            std::vector<std::pair<std::string,std::string>> cells;
            for (auto& f : g_shotFiles)
                cells.push_back({std::filesystem::path(f).stem().string(), f});
            drawStyledCells((g_shotPickBg ? "Set BG Image / " : "Screenshots / ") + g_shotCore, cells, g_shotFileIndex, MENU_SCREENSHOT_GRID);
            break;
        }
        case MENU_SCREENSHOT_VIEW:
        {
            if (g_shotFiles.empty()) { state.currentMenuLevel = MenuLevel::MENU_SCREENSHOTS; break; }
            if (g_shotFileIndex < 0 || g_shotFileIndex >= (int)g_shotFiles.size()) g_shotFileIndex = 0;
            const std::string& f = g_shotFiles[g_shotFileIndex];

            std::string caption = "Screenshots / " + g_shotCore;
            renderComponent.drawScreenshot(f, g_shotFileIndex, (int)g_shotFiles.size(), caption);
            break;
        }
        case MENU_SCRAPE_SELECT:
        {
            std::vector<std::pair<std::string,bool>> items;
            int checked = 0;
            for (auto& r : g_scrapeSystems) {
                items.push_back({systemDisplayName(r.id), r.checked});
                if (r.checked) checked++;
            }
            std::string title   = (g_scrapeMode == SCRAPE_LIBRETRO) ? "Scrape Artwork" : "Import gamelist.xml";

            std::vector<std::string> actionRows;
            if (g_scrapeMode == SCRAPE_LIBRETRO) {
                actionRows.push_back(std::string("Source:  ") +
                    (g_scrapeSource == SCRAPE_SRC_TGDB ? "TheGamesDB" : "Default (libretro)"));
                actionRows.push_back(std::string("Force re-scrape:  ") +
                    (g_scrapeForce ? "On (overwrite existing)" : "Off (skip games with art)"));
            }

            bool allSel = !g_scrapeSystems.empty() && checked == (int)g_scrapeSystems.size();
            std::string allRow = allSel ? "Deselect All" : "Select All";
            if (!allSel && g_scrapeMode == SCRAPE_LIBRETRO) allRow += "  (Very Slow)";
            actionRows.push_back(allRow);
            actionRows.push_back("> Start (" + std::to_string(checked) + " selected)");
            renderComponent.drawCheckList(title, actionRows, items, g_scrapeCursor);
            break;
        }
        case MENU_SCRAPE_PROGRESS:
        {
            ScrapeStatus st = g_scraper.status();
            if (st.finished) {
                if (st.wroteAny) renderComponent.invalidateArtCache();
                state.currentMenuLevel = g_scrapeFinishMenu;
                renderComponent.forceFullRedraw();
                if (!st.note.empty())
                    SetTip(st.note);
                else
                    SetTip("Artwork: " + std::to_string(st.found) + " found, "
                           + std::to_string(st.skipped) + " skipped, "
                           + std::to_string(st.missing) + " missing"

                           + (st.missing > 0 && g_scrapeFinishMenu == APP_SETTINGS
                                  ? " - listed in ConsoleMode/ScrapeLogs" : ""));
            } else {
                renderComponent.drawScrapeProgress(g_scrapeTitle, st.currentGame,
                                                   st.system, st.phase, st.opTimeoutSec, st.opStartMs,
                                                   st.done, st.total, st.found, st.skipped, st.missing,
                                                   false, st.dest);
            }
            break;
        }
        case MENU_OPTIMIZE_PROGRESS:
        {
            bool done = processOptimizeBatch();
            if (done) {
                renderComponent.invalidateArtCache();
                state.currentMenuLevel = APP_SETTINGS;
                renderComponent.forceFullRedraw();
                SetTip("Optimized " + std::to_string(g_optOk) + " of "
                       + std::to_string(g_optTotal) + " artwork files");
            } else {
                renderComponent.drawScrapeProgress("Optimizing Artwork", g_optCurrent,
                                                   "", "", 0, 0,
                                                   g_optDone, g_optTotal, g_optOk, 0, g_optDone - g_optOk);
            }
            break;
        }
        case MENU_UPDATE_CM_CONFIRM:
        {
            renderComponent.drawUpdateConfirm(cmupdate::currentVersion(), cmupdate::latestVersion(),
                                              cmupdate::assetSizeBytes(), cmupdate::notes(),
                                              m_updNotesScroll);
            break;
        }
        case MENU_UPDATE_CM_PROGRESS:
        {

            cmupdate::installIfReady();
            long dl = cmupdate::downloadedBytes() / 1024, tot = cmupdate::totalBytes() / 1024;
            renderComponent.drawScrapeProgress("Update Console Mode", cmupdate::phaseText(),
                                               "", "", 0, 0, (int)dl, (int)tot, 0, 0, 0, true);
            break;
        }
        case MENU_FOLDER:
        {
            auto& sections = menu.getSections();
            if (sections.empty()) {
                renderComponent.drawFolderEx("Load Game", {}, 0);
                break;
            }

            if (state.currentSectionIndex < 0 || state.currentSectionIndex >= static_cast<int>(sections.size()))
                state.currentSectionIndex = 0;

            Section& section = sections[state.currentSectionIndex];
            auto& folders = section.getFolders();
            if (state.currentFolderIndex < 0 || state.currentFolderIndex >= static_cast<int>(folders.size()))
                state.currentFolderIndex = 0;

            RenderComponent::GameStyle gs = renderComponent.gameStyle();
            bool isList = (gs != RenderComponent::GS_GRID && gs != RenderComponent::GS_FLIX && gs != RenderComponent::GS_SS4);
            std::vector<std::pair<std::string,std::string>> cells;
            for (Folder& folder : folders)
                cells.push_back({systemDisplayName(folder.getTitle()), systemLogoPath(folder, isList)});

            std::string sectionName = section.getTitle();
            sectionName =  sectionName.substr(0, sectionName.size() - 4);
            sectionName = "Load Game / "+sectionName;
            if (gs == RenderComponent::GS_FLIX)
                renderComponent.drawFlix(sectionName, cells, state.currentFolderIndex, MENU_FOLDER);
            else if (gs == RenderComponent::GS_SS4)
                renderComponent.drawSS4(sectionName, cells, state.currentFolderIndex, MENU_FOLDER);
            else if (gs == RenderComponent::GS_GRID)
                renderComponent.drawGrid(sectionName, cells, state.currentFolderIndex, MENU_FOLDER);
            else
                renderComponent.drawFolderEx(sectionName, cells, state.currentFolderIndex);

            break;
        }
        case MENU_SELECT_CORE:
        {
            renderComponent.drawSelectCore(vCores, currentCoreIndex, selectCoreIndex,
                                           "Select Core Override", &vCoresSelectable);
            break;
        }
        case MENU_GAME_OPTIONS:
        {
            renderComponent.drawSelectCore(g_gameOptItems, g_gameOptIndex, -1, "Game Options");
            break;
        }
        case MENU_HIDDEN_GAMES:
        {

            if (g_hiddenSysIndex < 0) g_hiddenSysIndex = 0;
            if (g_hiddenSysIndex >= (int)g_hiddenSystems.size())
                g_hiddenSysIndex = g_hiddenSystems.empty() ? 0 : (int)g_hiddenSystems.size() - 1;
            renderComponent.drawSelectCore(g_hiddenSystems, g_hiddenSysIndex, -1, "Hidden Games");
            break;
        }
        case MENU_HIDDEN_SYSTEM:
        {

            std::vector<std::string> labels;
            labels.reserve(g_hiddenSystemGames.size());
            for (const Rom& r : g_hiddenSystemGames) labels.push_back(r.getTitle());
            if (g_hiddenGameIndex < 0) g_hiddenGameIndex = 0;
            if (g_hiddenGameIndex >= (int)labels.size())
                g_hiddenGameIndex = labels.empty() ? 0 : (int)labels.size() - 1;
            renderComponent.drawSelectCore(labels, g_hiddenGameIndex, -1, g_hiddenSelectedSystem);
            break;
        }
        case MENU_ROM:
        {
            std::vector<std::pair<std::string, std::string>> romData;
            std::vector<Rom>* roms = getCurrentRomList();

            std::set<std::string> favLaunch, favParents;
            for (auto& f : vecFavoritesFile) {
                favLaunch.insert(f.getPath());

                // lazy folders can't resolve a launch path yet so match by parent dir
                const std::string& p = f.getPath();
                size_t s = p.find_last_of('/');
                if (s != std::string::npos) favParents.insert(p.substr(0, s));
            }
            std::set<std::string> favCells;
            if (roms) {
                for (Rom& rom : *roms)
                {
                    std::string title = rom.getTitle();
                    if (displaysAsFolder(rom))
                        title += " [DIR]";
                    romData.push_back({title, rom.getPath()});
                    if (!favLaunch.empty()) {
                        std::string lp = resolveLaunchPathForRom(rom);
                        if (!lp.empty() && favLaunch.count(lp)) favCells.insert(rom.getPath());
                        else if (rom.IsFolder() && rom.isLazy() && favParents.count(rom.getPath()))
                            favCells.insert(rom.getPath());
                    }
                }
            }
            renderComponent.setFavoritePaths(std::move(favCells));

            std::string name = getCurrentRomListTitle();
            if (state.currentRomIndex<0||state.currentRomIndex>=romData.size()) state.currentRomIndex = 0;
            if (renderComponent.searchActive()) {

                renderComponent.drawSearchOverlay();
            } else if (renderComponent.gameStyle() == RenderComponent::GS_GRID)
                renderComponent.drawGrid(name, romData, state.currentRomIndex, MENU_ROM);
            else if (renderComponent.gameStyle() == RenderComponent::GS_FLIX)
                renderComponent.drawFlix(name, romData, state.currentRomIndex, MENU_ROM);
            else if (renderComponent.gameStyle() == RenderComponent::GS_SS4)
                renderComponent.drawSS4(name, romData, state.currentRomIndex, MENU_ROM);
            else
                renderComponent.drawRomList(name, romData, state.currentRomIndex);

            break;
        }
        case MENU_SUBROM:
        {
            auto& subSecs = menu.getSections();
            if (state.currentSectionIndex < 0 || state.currentSectionIndex >= (int)subSecs.size()) break;
            auto& subFolders = subSecs[state.currentSectionIndex].getFolders();
            if (state.currentFolderIndex < 0 || state.currentFolderIndex >= (int)subFolders.size()) break;
            auto& subRoms = subFolders[state.currentFolderIndex].getRoms();
            if (state.currentRomIndex < 0 || state.currentRomIndex >= (int)subRoms.size()) break;
            Rom rom = subRoms[state.currentRomIndex];
            std::vector<std::pair<std::string, std::string>> romData;
            for (const Rom& rom : rom.getRoms())
            {
                romData.push_back({rom.getTitle(), rom.getPath()});
            }

            if (currentSubRomIndex<0||currentSubRomIndex>=romData.size()) currentSubRomIndex = 0;

            std::string firstName = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getTitle();

            std::string name = firstName +" / "+ rom.getTitle();

            std::set<std::string> favLaunch, favCells;
            for (auto& f : vecFavoritesFile) favLaunch.insert(f.getPath());
            for (auto& c : romData) if (favLaunch.count(c.second)) favCells.insert(c.second);
            renderComponent.setFavoritePaths(std::move(favCells));

            renderComponent.drawSubRomList(name, romData, currentSubRomIndex);

            break;
        }
        case APP_SETTINGS:
        {
            if (ip == "No network"){
                nGetIpCount++;
                if(nGetIpCount > 100){
                    ip = getNet();
                    nGetIpCount = 0;
                    if (ip != "No network")
                        appSettings.updateString(Configuration::NET,ip);
                }
            }

            if (g_settingsGroupIdx < 0) {
                std::vector<Settings::SettingRow> rows;
                for (const auto& g : settingsGroups()) rows.push_back({g.name, ">"});
                rows.push_back({"Developer Tools", ">"});
                rows.push_back({"About", ">"});
                rows.push_back({"Reboot", ""});
                rows.push_back({"Quit", ""});
                renderComponent.drawDevToolsSettings("Home / " + languages.get(Languages::APP_SETTINGS), rows, g_settingsTopIndex);
            } else {
                appSettings.setTransientValue(Configuration::UPDATE_CM,
                    cmupdate::updateAvailable() ? cmupdate::latestVersion() + " available" : "");
                renderComponent.drawAppSettings(std::string("Settings / ") + settingsGroups()[g_settingsGroupIdx].name,
                                                appSettings.getAppSettings(), currentSettingsIndex,
                                                (uint8_t)((vol & 7) | (isMute ? 0x10 : 0)));
            }
            break;
        }
        case DEVTOOLS_SETTINGS:
        {
            if (g_devGroupIdx < 0) {
                std::vector<Settings::SettingRow> rows;
                for (const auto& g : devGroups()) rows.push_back({g.name, ">"});
                renderComponent.drawDevToolsSettings("Settings / Developer Tools", rows, g_devTopIndex);
            } else {
                renderComponent.drawDevToolsSettings(std::string("Developer Tools / ") + devGroups()[g_devGroupIdx].name,
                                                     devSettings.getDevSettings(), currentDevToolsIndex);
            }
            break;
        }
        case MENU_ABOUT:
            renderComponent.drawAbout(g_aboutIndex);
            break;

        case MENU_STORAGE:
            renderComponent.drawStorageList();
            break;

        case MENU_SET_TIME:
            renderComponent.drawSetTime(g_timeEdit.tm_year + 1900, g_timeEdit.tm_mon + 1,
                                        g_timeEdit.tm_mday, g_timeEdit.tm_hour,
                                        g_timeEdit.tm_min, g_timeField);
            break;

        case MENU_CREDITS:
            renderComponent.drawCredits();
            break;

        case MENU_LICENSE:
            renderComponent.drawLicenseList();
            break;

        case MENU_LICENSE_TEXT:
            renderComponent.drawLicenseText();
            break;

        case MENU_KB_MAP:
            renderComponent.drawKbMap(cfg.getBool(Configuration::AB_SWAP));
            break;

        case MENU_INPUT_TESTER:
        {
            std::vector<std::string> devLines;
            for (int i = 0; i < m_evCount; i++) {
                if (!m_evfd[i].isOpen) continue;
                char l[224];
                const char* node = strrchr(m_evfd[i].path, '/');
                snprintf(l, sizeof l, "%s %04x:%04x  %s", node ? node + 1 : m_evfd[i].path,
                         m_evfd[i].vid, m_evfd[i].pid,
                         m_evfd[i].name[0] ? m_evfd[i].name : "?");
                devLines.push_back(l);
            }
            renderComponent.drawInputTester(devLines, m_inputTestLog);
            break;
        }
        case MENU_VOLUME:
        {
            break;
        }
        case MENU_SCRIPT:
        {
            renderComponent.drawScriptList(vecScriptList,currentScriptIndex);
            break;
        }
        case MENU_CD_TESTER:
        {
            std::vector<std::pair<std::string,int>> rows;
            for (int i = 0; i < (int)cdtest::items().size(); i++)
                rows.push_back({cdtest::items()[i].label, cdtest::durationSec(i)});
            int cur = cdtest::currentIndex();
            renderComponent.drawCdTester(rows, m_cdIdx, cur, cdtest::paused(),
                                         cdtest::elapsedSec(),
                                         cur >= 0 ? cdtest::durationSec(cur) : 0);
            break;
        }
        case MENU_CONTROLS:
        {
            renderComponent.drawCtrols(currentControlIndex);
            break;
        }
        case MENU_CONTROLS_JOY:
        {
            renderComponent.drawJoyStickMap(JsMapper.getVecDevices(), currentJoystickIndex);
            break;
        }
        case MENU_CONTROLS_JOY1:
        {
            if(currentJoystickIndex < JsMapper.getVecDevices().size()){
                std::string devName = JsMapper.getVecDevices()[currentJoystickIndex].vid_pid;
                renderComponent.drawJoyStickMap1(JsMapper.getKeys(), devName, currentJoystickKeyIndex);

                if (joystickKeyState == 1){
                    const std::vector<KeyInfo> vecKeys = JsMapper.getKeys();
                    if (currentJoystickKeyIndex < (int)vecKeys.size())
                        renderComponent.showTip((currentJoystickKeyIndex >= JoystickMapper::AXIS_LX_ROW
                                                 ? "Tilt the stick for " : "Press a button for ")
                                                + vecKeys[currentJoystickKeyIndex].key
                                                + " (hold B to cancel)");
                }
            }
            break;
        }
        case MENU_CONTROLS_JOY2:
        {
            std::string dev = (currentJoystickIndex < (int)JsMapper.getVecDevices().size())
                ? JsMapper.getVecDevices()[currentJoystickIndex].vid_pid : std::string();
            renderComponent.drawJoyStickOptions(dev, m_joyOptItems, m_joyOptIndex);
            break;
        }
        case MENU_JOY_TESTER:
        {

            std::vector<bool> lit(12, false);
            for (int i = 0; i < 12; i++) {
                int code = JsMapper.buttonCode(i);
                if (code && m_joyPressed.count(code)) lit[i] = true;
            }
            if (m_joyHatX < 0) lit[1] = true;  else if (m_joyHatX > 0) lit[0] = true;   // Left / Right
            if (m_joyHatY < 0) lit[3] = true;  else if (m_joyHatY > 0) lit[2] = true;   // Up / Down
            std::string dev = (currentJoystickIndex < (int)JsMapper.getVecDevices().size())
                ? JsMapper.getVecDevices()[currentJoystickIndex].name : std::string();
            renderComponent.drawJoyTester(lit, dev);

            if (m_joyEscSince && SDL_GetTicks() - m_joyEscSince >= 3000) {
                state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY;
                m_joyTesterVid = m_joyTesterPid = 0;
                m_joyPressed.clear();
                m_joyEscSince = 0;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            break;
        }
        case MENU_BLUETOOTH:
        {
            readBlueBoothLog();

            // bt_pair.sh writes this status, "idle" means hide it
            static std::string s_pairMsg;
            static Uint32 s_pairReadAt = 0;
            if (s_pairReadAt == 0 || SDL_GetTicks() - s_pairReadAt > 500) {
                s_pairReadAt = SDL_GetTicks();
                std::ifstream sf("/tmp/consolemode/bt_pair_status.txt");
                std::string m;
                std::getline(sf, m);
                s_pairMsg = (m == "idle") ? std::string() : m;
            }
            renderComponent.drawBluetooth(btDevices,currentBtIndex,s_pairMsg);
            break;
        }
        case MENU_WIFI:
        {
            m_wifi.tick();

            if (renderComponent.textInputActive()) {
                renderComponent.drawSearchOverlay();
                break;
            }

            const auto& nets = m_wifi.networks();
            if (currentWifiIndex >= (int)nets.size())
                currentWifiIndex = nets.empty() ? 0 : (int)nets.size() - 1;

            std::string sig = m_wifi.statusText() + "|" + std::to_string(currentWifiIndex) + "|";
            for (const auto& n : nets)
                sig += n.enc + (n.active ? "*" : "") + std::to_string(n.bars) + ";";
            static std::string s_wifiSig;
            if (m_wifiRepaint || sig != s_wifiSig) {
                m_wifiRepaint = false;
                s_wifiSig = sig;
                renderComponent.drawWifi(nets, currentWifiIndex, m_wifi.statusText());
            }
            break;
        }
        case MENU_WIFI_OPTIONS:
            renderComponent.drawWifiOptions(wifiOptNet.ssid, wifiOptItems, currentWifiOptIndex);
            break;
        case MENU_SYSTEMCONFIG:
        {
            renderComponent.drawSystemConfig(appSettings.MisterInis,currentIniIndex,appSettings.iniIndex);
            break;
        }
        case MENU_HOMEBREW_FOLDER:
        {
            if (currentHomebrewFolderIndex < 0 ||
                currentHomebrewFolderIndex >= static_cast<int>(homebrewFolders.size()))
                currentHomebrewFolderIndex = 0;

            RenderComponent::GameStyle gs = renderComponent.gameStyle();
            bool isList = (gs != RenderComponent::GS_GRID && gs != RenderComponent::GS_FLIX && gs != RenderComponent::GS_SS4);
            std::vector<std::pair<std::string,std::string>> cells;
            for (const HomeBrewFolder& folder : homebrewFolders) {
                std::string sys = homebrewConsoleName(folder.getTitle());
                cells.push_back({ systemDisplayName(sys),
                                  systemLogoPathFor(sys, folder.getRoms(), isList) });
            }
            if (gs == RenderComponent::GS_FLIX)
                renderComponent.drawFlix("Homebrew", cells, currentHomebrewFolderIndex, MENU_FOLDER);
            else if (gs == RenderComponent::GS_SS4)
                renderComponent.drawSS4("Homebrew", cells, currentHomebrewFolderIndex, MENU_FOLDER);
            else if (gs == RenderComponent::GS_GRID)
                renderComponent.drawGrid("Homebrew", cells, currentHomebrewFolderIndex, MENU_FOLDER);
            else
                renderComponent.drawFolderEx("Homebrew", cells, currentHomebrewFolderIndex);
            break;
        }
#ifdef HAS_PS1
        case MENU_DISC_OPTIONS:
        case MENU_DUMP_DEVICE:
        {
            m_ps1.draw(state.currentMenuLevel);
            break;
        }
#endif
    }

    if (tipStartTime > 0){
        int timeSpace = SDL_GetTicks() - tipStartTime;
        if (timeSpace > m_tipDurationMs){
            tipStartTime = 0;
            strtip = "";
            renderComponent.forceFullRedraw();
            m_dirty = true;
        }
        else
            renderComponent.showTip(strtip);
    }
}

void Application::handleCommand(ControlMap cmd) {
    m_dirty = true;

    // CRT 180deg flips the whole frame so invert the d-pad to match the tube
    if (crt::rotation() == 3) {
        switch (cmd) {
            case CMD_UP:    cmd = CMD_DOWN;  break;
            case CMD_DOWN:  cmd = CMD_UP;    break;
            case CMD_LEFT:  cmd = CMD_RIGHT; break;
            case CMD_RIGHT: cmd = CMD_LEFT;  break;
            default: break;
        }
    }

    adoptRbfScanResult();

    if (renderComponent.textInputActive()) {
        switch (cmd) {
            case CMD_LEFT:  renderComponent.searchMove(-1, 0); break;
            case CMD_RIGHT: renderComponent.searchMove( 1, 0); break;
            case CMD_UP:    renderComponent.searchMove( 0,-1); break;
            case CMD_DOWN:  renderComponent.searchMove( 0, 1); break;
            case CMD_ENTER:
                if (renderComponent.searchSelect() == 2) submitTextOverlay();
                break;
            case CMD_BACK:
                cancelTextOverlay();
                break;
            default: break;
        }
        return;
    }

    if (renderComponent.searchActive()) {
        switch (cmd) {
            case CMD_LEFT:  renderComponent.searchMove(-1, 0); break;
            case CMD_RIGHT: renderComponent.searchMove( 1, 0); break;
            case CMD_UP:    renderComponent.searchMove( 0,-1); break;
            case CMD_DOWN:  renderComponent.searchMove( 0, 1); break;
            case CMD_ENTER: {
                int r = renderComponent.searchSelect();
                if (r == 1) jumpSearchMatch();
                else if (r == 2) submitTextOverlay();
                break;
            }
            case CMD_X:
            case CMD_BACK:  cancelTextOverlay(); break;
            default: break;
        }
        return;
    }

    // gate the settings dispatch on the entry level so a transition cmd isn't also run as nav
    MenuLevel entryLevel = state.currentMenuLevel;
    int entrySettingsGroup = g_settingsGroupIdx;
    int entryDevGroup      = g_devGroupIdx;

    switch (state.currentMenuLevel) {
        case MenuLevel::MENU_MAIN:
            if (cmd == CMD_ENTER) {
                handleMainMenu(state.currentMainIndex);
                return;
            } else if (cmd == CMD_UP) {
                if (state.currentMainIndex > 0) {
                    state.currentMainIndex--;
#ifdef HAS_PS1
                    if (vecMainMenu[state.currentMainIndex] == "Other Disc")
                        state.currentMainIndex--;
                    if (!m_ps1.haveDock() && vecMainMenu[state.currentMainIndex] == "PS1 Disc")
                        state.currentMainIndex--;
#endif
                }
                else state.currentMainIndex = vecMainMenu.size() - 1;
            } else if (cmd == CMD_DOWN) {
                state.currentMainIndex = (state.currentMainIndex + 1) % vecMainMenu.size();
#ifdef HAS_PS1
                if (!m_ps1.haveDock() && vecMainMenu[state.currentMainIndex] == "PS1 Disc")
                    state.currentMainIndex++;
                if (vecMainMenu[state.currentMainIndex] == "Other Disc")
                    state.currentMainIndex++;
#endif
            }
            break;

        case MenuLevel::MENU_RBF:
            if (cmd == CMD_ENTER) {
                if (!isRbfScanReady()) {
                    parseAllRbf();
                    SetTip("Scanning cores...");
                    return;
                }
                if (vecRbfFile.empty()) {
                    SetTip("No cores found.");
                    return;
                }
                if (state.currentRbfIndex < 0 || state.currentRbfIndex >= static_cast<int>(vecRbfFile.size()))
                    state.currentRbfIndex = 0;

                logMessage(INFO,"MenuLevel::MENU_RBF","load rbf");
                std::string path = vecRbfFile[state.currentRbfIndex].getPath();
                launchRomWithPath(path,false);
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                if (vecRbfFile.size() > 0){
                    if (state.currentRbfIndex > 0) state.currentRbfIndex--;
                    else state.currentRbfIndex = vecRbfFile.size() - 1;
                }
            } else if (cmd == CMD_DOWN) {
                if (vecRbfFile.size() > 0){
                    state.currentRbfIndex = (state.currentRbfIndex + 1) % vecRbfFile.size();
                }
            } else if (cmd == CMD_X) {
                if (!vecRbfFile.empty()) renderComponent.openSearch();
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_MAIN;
                renderComponent.resetValues();
            }
            break;

        case MenuLevel::MENU_SEARCH_RESULTS:
            if (cmd == CMD_ENTER) {
                if (!m_searchResults.empty() &&
                    state.currentSearchIndex >= 0 &&
                    state.currentSearchIndex < (int)m_searchResults.size()) {
                    std::string path = resolveLaunchPathForRom(m_searchResults[state.currentSearchIndex]);
                    if (!path.empty()) {
                        launchRomWithPath(path);
                        renderComponent.resetValues();
                    }
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                state.currentSearchIndex = styleMoveIndex(cmd, state.currentSearchIndex,
                    (int)m_searchResults.size(), renderComponent.gameStyle(),
                    theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_X) {
                renderComponent.openSearch();
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_ROM;
                renderComponent.resetValues();
            }
            break;

        case MenuLevel::MENU_FAVORITES:

            if (state.currentFavoritesIndex < 0) state.currentFavoritesIndex = 0;
            if (state.currentFavoritesIndex >= (int)vecFavoritesFile.size())
                state.currentFavoritesIndex = vecFavoritesFile.empty() ? 0 : (int)vecFavoritesFile.size()-1;
            if (cmd == CMD_ENTER) {
                if (vecFavoritesFile.size()>0){
                    std::string path = vecFavoritesFile[state.currentFavoritesIndex].getPath();
                    launchRomWithPath(path);
                    renderComponent.resetValues();
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                state.currentFavoritesIndex = styleMoveIndex(cmd, state.currentFavoritesIndex,
                    (int)vecFavoritesFile.size(), renderComponent.gameStyle(),
                    theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_MAIN;
                renderComponent.resetValues();
            } else if(cmd == CMD_X){
                if (vecFavoritesFile.size()>0){
                    std::string romName = vecFavoritesFile[state.currentFavoritesIndex].getTitle();
                    std::string romPath = vecFavoritesFile[state.currentFavoritesIndex].getPath();
                    romName = renderComponent.getAlias(romName);
                    delGameFromList(Configuration::FAVORITES,romName,romPath);
                    if (state.currentFavoritesIndex< vecFavoritesFile.size())
                        ;
                    else if (state.currentFavoritesIndex > 0) state.currentFavoritesIndex--;
                    renderComponent.resetValues();
                }
            }

            break;

        case MenuLevel::MENU_HISTORY:

            if (state.currentHistoryIndex < 0) state.currentHistoryIndex = 0;
            if (state.currentHistoryIndex >= (int)vecHistoryFile.size())
                state.currentHistoryIndex = vecHistoryFile.empty() ? 0 : (int)vecHistoryFile.size()-1;
            if (cmd == CMD_ENTER) {
                if (vecHistoryFile.size()>0){
                    int index = vecHistoryFile.size() - state.currentHistoryIndex - 1;
                    std::string path = vecHistoryFile[index].getPath();
                    launchRomWithPath(path);
                    renderComponent.resetValues();
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                state.currentHistoryIndex = styleMoveIndex(cmd, state.currentHistoryIndex,
                    (int)vecHistoryFile.size(), renderComponent.gameStyle(),
                    theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_MAIN;
                renderComponent.resetValues();
            } else if(cmd == CMD_X){
                if (vecHistoryFile.size()>0){
                    int index = vecHistoryFile.size() - state.currentHistoryIndex - 1;
                    std::string romName = vecHistoryFile[index].getTitle();
                    std::string romPath = vecHistoryFile[index].getPath();
                    romName = renderComponent.getAlias(romName);
                    delGameFromList(Configuration::HISTORY,romName,romPath);
                    if (state.currentHistoryIndex< vecHistoryFile.size())
                        ;
                    else if (state.currentHistoryIndex > 0) state.currentHistoryIndex--;
                    renderComponent.resetValues();
                }
            }
            break;
        case MenuLevel::MENU_SECTION:
            if (cmd == CMD_ENTER) {
                resetRomFolderNavigation();
                state.currentFolderIndex = 0;

                auto& secs = menu.getSections();
                bool single = state.currentSectionIndex >= 0
                    && state.currentSectionIndex < static_cast<int>(secs.size())
                    && secs[state.currentSectionIndex].getFolders().size() == 1;
                if (single) {
                    state.currentRomIndex = 0;
                    state.currentMenuLevel = MenuLevel::MENU_ROM;
                } else {
                    state.currentMenuLevel = MenuLevel::MENU_FOLDER;
                }
                renderComponent.resetValues();
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {

                int n = (int)menu.getSections().size();
                if (n > 0)
                    state.currentSectionIndex = styleMoveIndex(cmd, state.currentSectionIndex,
                        n, renderComponent.gameStyle(), theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_MAIN;
                renderComponent.resetValues();
            }
            break;

        case MenuLevel::MENU_SCREENSHOTS:
            if (cmd == CMD_ENTER) {
                if (g_shotCoreIndex >= 0 && g_shotCoreIndex < (int)g_shotCores.size()) {
                    g_shotCore = g_shotCores[g_shotCoreIndex].first;
                    scanShotFiles(cfg.get(Configuration::HOME_PATH) + "screenshots", g_shotCore);
                    if (!g_shotFiles.empty()) {
                        g_shotFileIndex = 0;
                        state.currentMenuLevel = MenuLevel::MENU_SCREENSHOT_GRID;
                        renderComponent.resetValues();
                    }
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                g_shotCoreIndex = styleMoveIndex(cmd, g_shotCoreIndex, (int)g_shotCores.size(),
                    renderComponent.gameStyle(), theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = g_shotReturnMenu;
                g_shotPickBg = false;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            }
            break;

        case MenuLevel::MENU_SCREENSHOT_GRID:
            if (cmd == CMD_ENTER) {
                if (!g_shotFiles.empty() && g_shotFileIndex >= 0 && g_shotFileIndex < (int)g_shotFiles.size()) {
                    if (g_shotPickBg) {
                        bool ok = setGameBgFromShot(g_shotPickRomPath, g_shotFiles[g_shotFileIndex]);
                        if (ok) renderComponent.invalidateArtCache();
                        SetTip(ok ? "Background set." : "Could not set background.");
                        g_shotPickBg = false;
                        state.currentMenuLevel = g_gameOptReturnMenu;
                        renderComponent.resetValues();
                        renderComponent.forceFullRedraw();
                    } else {
                        state.currentMenuLevel = MenuLevel::MENU_SCREENSHOT_VIEW;
                        renderComponent.forceFullRedraw();
                    }
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                g_shotFileIndex = styleMoveIndex(cmd, g_shotFileIndex, (int)g_shotFiles.size(),
                    renderComponent.gameStyle(), theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_X) {
                deleteSelectedScreenshot();
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_SCREENSHOTS;
                renderComponent.resetValues();
            }
            break;

        case MenuLevel::MENU_SCREENSHOT_VIEW:
            if (cmd == CMD_LEFT || cmd == CMD_UP) {
                if (g_shotFileIndex > 0) { g_shotFileIndex--; renderComponent.forceFullRedraw(); }
            } else if (cmd == CMD_RIGHT || cmd == CMD_DOWN) {
                if (g_shotFileIndex + 1 < (int)g_shotFiles.size()) { g_shotFileIndex++; renderComponent.forceFullRedraw(); }
            } else if (cmd == CMD_X) {
                deleteSelectedScreenshot();
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_SCREENSHOT_GRID;
                renderComponent.forceFullRedraw();
            }
            break;

        case MenuLevel::MENU_SCRAPE_SELECT:
        {

            bool online      = (g_scrapeMode == SCRAPE_LIBRETRO);
            int  idxSource   = online ? 0 : -1;
            int  idxForce    = online ? 1 : -1;
            int  idxAll      = online ? 2 : 0;
            int  idxStart    = online ? 3 : 1;
            int  special     = idxStart + 1;
            int  sys         = (int)g_scrapeSystems.size();
            int  N           = special + sys;
            auto toggleSource = [&]() {
                g_scrapeSource = (g_scrapeSource == SCRAPE_SRC_TGDB) ? SCRAPE_SRC_LIBRETRO : SCRAPE_SRC_TGDB;
                buildScrapeSystems();
                renderComponent.forceFullRedraw();
            };
            auto toggleAll = [&]() {
                bool anyUnchecked = false;
                for (auto& r : g_scrapeSystems) if (!r.checked) { anyUnchecked = true; break; }
                for (auto& r : g_scrapeSystems) r.checked = anyUnchecked;
                renderComponent.forceFullRedraw();
            };
            if (cmd == CMD_UP) {
                g_scrapeCursor = (g_scrapeCursor > 0) ? g_scrapeCursor - 1 : N - 1;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_DOWN) {
                g_scrapeCursor = (g_scrapeCursor < N - 1) ? g_scrapeCursor + 1 : 0;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                if (g_scrapeCursor == idxSource)      toggleSource();
                else if (g_scrapeCursor == idxForce) { g_scrapeForce = !g_scrapeForce; renderComponent.forceFullRedraw(); }
                else if (g_scrapeCursor == idxAll)     toggleAll();
            } else if (cmd == CMD_ENTER) {
                if (g_scrapeCursor == idxSource)        toggleSource();
                else if (g_scrapeCursor == idxForce)  { g_scrapeForce = !g_scrapeForce; renderComponent.forceFullRedraw(); }
                else if (g_scrapeCursor == idxAll)      toggleAll();
                else if (g_scrapeCursor == idxStart)    startScrapeSelected();
                else {
                    int si = g_scrapeCursor - special;
                    if (si >= 0 && si < sys) {
                        g_scrapeSystems[si].checked = !g_scrapeSystems[si].checked;
                        renderComponent.forceFullRedraw();
                    }
                }
            } else if (cmd == CMD_Y) {
                startScrapeSelected();
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
            }
            break;
        }
        case MenuLevel::MENU_SCRAPE_PROGRESS:
            if (cmd == CMD_BACK) {
                g_scraper.cancel();
                renderComponent.invalidateArtCache();
                state.currentMenuLevel = g_scrapeCancelMenu;
                renderComponent.forceFullRedraw();
            }
            break;

        case MenuLevel::MENU_OPTIMIZE_PROGRESS:
            if (cmd == CMD_BACK) {
                g_optIndex = g_optJobs.size();
                renderComponent.invalidateArtCache();
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.forceFullRedraw();
            }
            break;

        case MenuLevel::MENU_UPDATE_CM_CONFIRM:
            if (cmd == CMD_ENTER) {
                cmupdate::startDownload(cfg.getBool(Configuration::USE_MIRROR));
                state.currentMenuLevel = MenuLevel::MENU_UPDATE_CM_PROGRESS;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP)   { if (m_updNotesScroll > 0) m_updNotesScroll--; }
            else if (cmd == CMD_DOWN)   { m_updNotesScroll++; }
            else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.forceFullRedraw();
            }
            break;

        case MenuLevel::MENU_UPDATE_CM_PROGRESS:
            if (cmd == CMD_BACK) {
                cmupdate::cancel();
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.forceFullRedraw();
            }
            break;

        case MenuLevel::MENU_FOLDER:
        {
            auto& sections = menu.getSections();
            if (sections.empty() || state.currentSectionIndex < 0 || state.currentSectionIndex >= static_cast<int>(sections.size())) {
                state.currentMenuLevel = MenuLevel::MENU_SECTION;
                state.currentSectionIndex = 0;
                state.currentFolderIndex = 0;
                renderComponent.resetValues();
                break;
            }

            Section& section = sections[state.currentSectionIndex];
            auto& folders = section.getFolders();
            if (folders.empty()) {
                if (cmd == CMD_BACK) {
                    state.currentMenuLevel = MenuLevel::MENU_SECTION;
                    renderComponent.resetValues();
                } else {
                    SetTip("No systems found.");
                }
                break;
            }

            if (state.currentFolderIndex < 0 || state.currentFolderIndex >= static_cast<int>(folders.size()))
                state.currentFolderIndex = 0;

            if (cmd == CMD_ENTER) {
                logMessage(INFO,"MenuLevel::MENU_FOLDER","CMD_ENTER");

                resetRomFolderNavigation();
                state.currentMenuLevel = MenuLevel::MENU_ROM;
                state.currentRomIndex = 0;
                renderComponent.resetValues();
            } else if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_SECTION;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                int n = (int)folders.size();
                if (n > 0)
                    state.currentFolderIndex = styleMoveIndex(cmd, state.currentFolderIndex,
                        n, renderComponent.gameStyle(), theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_OPTIONS){

                currentCoreIndex = 0;
                selectCoreIndex = -1;
                std::string folderName = folders[state.currentFolderIndex].getTitle();

                if (!isRbfScanReady()) {
                    parseAllRbf();
                    SetTip("Scanning cores...");
                    return;
                }

                buildSystemCoreRows(folderName);

                bool anySelectable = false;
                for (char s : vCoresSelectable) if (s) { anySelectable = true; break; }
                if (!anySelectable) {
                    SetTip("No cores found for " + folderName);
                    return;
                }

                selectCoreIndex = findSelectedCoreIndex(folderName, vCores);
                currentCoreIndex = (selectCoreIndex >= 0) ? selectCoreIndex : 0;
                while (currentCoreIndex < (int)vCores.size() && !vCoresSelectable[currentCoreIndex])
                    currentCoreIndex++;

                coreSelectScope = CORE_SELECT_SYSTEM;
                coreSelectReturnMenu = MENU_FOLDER;
                coreSelectFolderName = folderName;
                coreSelectRomPath.clear();

                state.currentMenuLevel = MenuLevel::MENU_SELECT_CORE;
                renderComponent.resetValues();
            }

            break;
        }

        case MenuLevel::MENU_SELECT_CORE:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = coreSelectReturnMenu;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP || cmd == CMD_DOWN) {
                int n = (int)vCores.size();
                if (n == 0) return;
                int dir = (cmd == CMD_UP) ? -1 : 1;
                int i = currentCoreIndex;
                for (int k = 0; k < n; k++) {
                    i = (i + dir + n) % n;
                    if (vCoresSelectable.empty() || i >= (int)vCoresSelectable.size() || vCoresSelectable[i]) {
                        currentCoreIndex = i;
                        break;
                    }
                }

            } else if (cmd == CMD_ENTER) {
                if (currentCoreIndex < 0 || currentCoreIndex >= static_cast<int>(vCores.size()))
                    return;

                if (currentCoreIndex < (int)vCoresSelectable.size() && !vCoresSelectable[currentCoreIndex])
                    return;

                if (coreSelectScope == CORE_SELECT_GAME) {
                    if (coreSelectRomPath.empty())
                        return;

                    // RA override chain-loads MiSTer_RA and a per-game core can't escape it
                    {
                        std::string coreName = coreNameFromRbfPath(vCores[currentCoreIndex]);
                        if (!coreName.empty()
                            && appSettings.misterIniMainOverrideEnabled(coreName, kRaMainBinary)) {
                            SetTip("RA active - set in system options.");
                            return;
                        }
                    }

                    if (getSelectedCoreForRom(coreSelectRomPath) == vCores[currentCoreIndex]) {
                        setSelectedCoreForRom(coreSelectRomPath, "");
                        saveCoreOverrideCache();
                        SetTip("Override cleared - default core.");
                        selectCoreIndex = -1;
                        return;
                    }
                    setSelectedCoreForRom(coreSelectRomPath, vCores[currentCoreIndex]);
                    saveCoreOverrideCache();
                    logMessage(INFO,"Core override saved",coreSelectRomPath.c_str());
                } else {
                    std::string folderName = coreSelectFolderName;
                    if (folderName.empty()) {
                        auto& sections = menu.getSections();
                        if (state.currentSectionIndex < 0 || state.currentSectionIndex >= static_cast<int>(sections.size()))
                            return;

                        auto& folders = sections[state.currentSectionIndex].getFolders();
                        if (state.currentFolderIndex < 0 || state.currentFolderIndex >= static_cast<int>(folders.size()))
                            return;

                        folderName = folders[state.currentFolderIndex].getTitle();
                    }

                    const std::string chosen = vCores[currentCoreIndex];

                    if (getSelectedCoreForSystem(folderName) == chosen) {
                        setSelectedCoreForSystem(folderName, "");
                        saveCoreSelectionCache();
                        std::string ccName = coreNameFromRbfPath(chosen);
                        if (!ccName.empty() && rbfPathIsRaCore(chosen)
                            && appSettings.misterIniMainOverrideEnabled(ccName, kRaMainBinary)) {
                            appSettings.setMisterIniMainOverride(ccName, kRaMainBinary, false);
                            SetTip("Override cleared - RA disabled.");
                        } else {
                            SetTip("Override cleared - default core.");
                        }
                        buildSystemCoreRows(folderName);
                        selectCoreIndex = -1;
                        if (currentCoreIndex >= (int)vCores.size())
                            currentCoreIndex = vCores.empty() ? 0 : (int)vCores.size() - 1;
                        return;
                    }

                    setSelectedCoreForSystem(folderName, chosen);
                    saveCoreSelectionCache();
                    logMessage(INFO,"Core override saved",folderName.c_str());

                    // an RA core turns on the chain-load ini line, a normal core comments it out
                    std::string coreName = coreNameFromRbfPath(chosen);
                    if (!coreName.empty()) {
                        if (rbfPathIsRaCore(chosen)) {
                            std::error_code ec;
                            if (!std::filesystem::exists(cfg.get(Configuration::HOME_PATH) + "/" + kRaMainBinary, ec)) {
                                SetTip(std::string(kRaMainBinary) + " missing.");
                            } else if (appSettings.setMisterIniMainOverride(coreName, kRaMainBinary, true)) {
                                SetTip("RA enabled [" + coreName + "]");
                            } else {
                                SetTip("Ini update failed.");
                            }
                        } else if (appSettings.misterIniMainOverrideEnabled(coreName, kRaMainBinary)) {
                            appSettings.setMisterIniMainOverride(coreName, kRaMainBinary, false);
                            SetTip("RA disabled [" + coreName + "]");
                        }
                    }

                    buildSystemCoreRows(folderName);
                    selectCoreIndex = -1;
                    for (int i = 0; i < (int)vCores.size(); i++)
                        if (vCores[i] == chosen) { selectCoreIndex = i; break; }
                    currentCoreIndex = (selectCoreIndex >= 0) ? selectCoreIndex : 0;
                    return;
                }

                selectCoreIndex = currentCoreIndex;
            }

            break;

        case MenuLevel::MENU_GAME_OPTIONS:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = g_gameOptReturnMenu;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP) {
                int n = (int)g_gameOptItems.size();
                g_gameOptIndex = (g_gameOptIndex > 0) ? g_gameOptIndex - 1 : n - 1;
            } else if (cmd == CMD_DOWN) {
                int n = (int)g_gameOptItems.size();
                g_gameOptIndex = (g_gameOptIndex + 1) % n;
            } else if (cmd == CMD_ENTER) {
                if (g_gameOptIndex < 0 || g_gameOptIndex >= (int)g_gameOptItems.size())
                    break;
                const std::string& choice = g_gameOptItems[g_gameOptIndex];
                if (choice == "Set BG Image") {
                    scanShotCores(cfg.get(Configuration::HOME_PATH) + "screenshots");
                    if (g_shotCores.empty()) {
                        SetTip("No screenshots found.");
                        break;
                    }
                    g_shotPickBg      = true;
                    g_shotPickRomPath = g_gameOptCellPath;
                    g_shotReturnMenu  = MENU_GAME_OPTIONS;
                    g_shotCoreIndex   = 0;
                    state.currentMenuLevel = MenuLevel::MENU_SCREENSHOTS;
                    renderComponent.resetValues();
                    renderComponent.forceFullRedraw();
                } else if (choice == "Scrape Art (Default)" || choice == "Scrape Art (TheGamesDB)") {

                    // art keys on the displayed cell path not the launch path
                    ScrapeSource src = (choice == "Scrape Art (TheGamesDB)") ? SCRAPE_SRC_TGDB : SCRAPE_SRC_LIBRETRO;
                    scrapeSingleGame(coreSelectFolderName, g_gameOptCellPath, src);
                } else if (choice == "Hide Game") {

                    // hide keys on the launch path, the same identity favorites/history use
                    std::string path = coreSelectRomPath;
                    if (!path.empty()) {
                        std::filesystem::path pp(path);
                        std::string name = renderComponent.getAlias(pp.stem().string());
                        addGame2List(Configuration::HIDDEN, name, path);
                        std::vector<Rom>* roms = getCurrentRomList();
                        if (roms)
                            roms->erase(std::remove_if(roms->begin(), roms->end(),
                                        [this](const Rom& r){ return isHidden(r); }), roms->end());
                        loadHistoryAndFavorites();
                        SetTip("Game hidden.");
                    }
                    state.currentMenuLevel = g_gameOptReturnMenu;
                    renderComponent.resetValues();
                    renderComponent.forceFullRedraw();
                } else {
                    if (!isRbfScanReady()) {
                        parseAllRbf();
                        SetTip("Scanning cores...");
                        break;
                    }
                    currentCoreIndex = 0;
                    selectCoreIndex  = -1;

                    buildSystemCoreRows(coreSelectFolderName, false);
                    {
                        bool anySel = false;
                        for (char s : vCoresSelectable) if (s) { anySel = true; break; }
                        if (!anySel) {
                            SetTip("No cores found for " + coreSelectFolderName);
                            break;
                        }
                    }
                    selectCoreIndex = findSelectedCoreIndexForRom(coreSelectRomPath, vCores);
                    if (selectCoreIndex < 0)
                        selectCoreIndex = findSelectedCoreIndex(coreSelectFolderName, vCores);
                    currentCoreIndex = (selectCoreIndex >= 0) ? selectCoreIndex : 0;
                    while (currentCoreIndex < (int)vCores.size() && !vCoresSelectable[currentCoreIndex])
                        currentCoreIndex++;
                    coreSelectScope = CORE_SELECT_GAME;
                    coreSelectReturnMenu = MENU_GAME_OPTIONS;
                    state.currentMenuLevel = MenuLevel::MENU_SELECT_CORE;
                    renderComponent.resetValues();
                    renderComponent.forceFullRedraw();
                }
            }
            break;

        case MenuLevel::MENU_HIDDEN_GAMES:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP) {
                int n = (int)g_hiddenSystems.size();
                if (n > 0) g_hiddenSysIndex = (g_hiddenSysIndex > 0) ? g_hiddenSysIndex - 1 : n - 1;
            } else if (cmd == CMD_DOWN) {
                int n = (int)g_hiddenSystems.size();
                if (n > 0) g_hiddenSysIndex = (g_hiddenSysIndex + 1) % n;
            } else if (cmd == CMD_ENTER) {
                if (g_hiddenSysIndex >= 0 && g_hiddenSysIndex < (int)g_hiddenSystems.size()) {
                    g_hiddenSelectedSystem = g_hiddenSystems[g_hiddenSysIndex];
                    rebuildHiddenSystemGames(g_hiddenSelectedSystem);
                    g_hiddenGameIndex = 0;
                    state.currentMenuLevel = MenuLevel::MENU_HIDDEN_SYSTEM;
                    renderComponent.resetValues();
                    renderComponent.forceFullRedraw();
                }
            }
            break;

        case MenuLevel::MENU_HIDDEN_SYSTEM:
            if (g_hiddenGameIndex < 0) g_hiddenGameIndex = 0;
            if (g_hiddenGameIndex >= (int)g_hiddenSystemGames.size())
                g_hiddenGameIndex = g_hiddenSystemGames.empty() ? 0 : (int)g_hiddenSystemGames.size() - 1;
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_HIDDEN_GAMES;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP) {
                int n = (int)g_hiddenSystemGames.size();
                if (n > 0) g_hiddenGameIndex = (g_hiddenGameIndex > 0) ? g_hiddenGameIndex - 1 : n - 1;
            } else if (cmd == CMD_DOWN) {
                int n = (int)g_hiddenSystemGames.size();
                if (n > 0) g_hiddenGameIndex = (g_hiddenGameIndex + 1) % n;
            } else if (cmd == CMD_X || cmd == CMD_ENTER) {
                if (!g_hiddenSystemGames.empty()) {
                    std::string nm  = g_hiddenSystemGames[g_hiddenGameIndex].getTitle();
                    std::string pth = g_hiddenSystemGames[g_hiddenGameIndex].getPath();
                    delGameFromList(Configuration::HIDDEN, nm, pth);
                    loadHistoryAndFavorites();
                    reloadMenuTree();
                    rebuildHiddenSystems();
                    if (g_hiddenSystems.empty()) {
                        state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                    } else {
                        rebuildHiddenSystemGames(g_hiddenSelectedSystem);
                        if (g_hiddenSystemGames.empty()) {
                            if (g_hiddenSysIndex >= (int)g_hiddenSystems.size())
                                g_hiddenSysIndex = (int)g_hiddenSystems.size() - 1;
                            state.currentMenuLevel = MenuLevel::MENU_HIDDEN_GAMES;
                        } else if (g_hiddenGameIndex >= (int)g_hiddenSystemGames.size()) {
                            g_hiddenGameIndex = (int)g_hiddenSystemGames.size() - 1;
                        }
                    }
                    renderComponent.resetValues();
                    renderComponent.forceFullRedraw();
                }
            }
            break;

        case MenuLevel::MENU_ROM:
        {
            std::vector<Rom>* roms = getCurrentRomList();
            if (!roms) {
                resetRomFolderNavigation();
                state.currentMenuLevel = MenuLevel::MENU_FOLDER;
                renderComponent.resetValues();
                break;
            }

            if (cmd == CMD_BACK) {
                if (leaveRomFolder()) {
                    renderComponent.resetValues();
                } else {
                    resetRomFolderNavigation();

                    auto& secs = menu.getSections();
                    bool single = state.currentSectionIndex >= 0
                        && state.currentSectionIndex < static_cast<int>(secs.size())
                        && secs[state.currentSectionIndex].getFolders().size() == 1;
                    state.currentMenuLevel = single ? MenuLevel::MENU_SECTION : MenuLevel::MENU_FOLDER;
                    renderComponent.resetValues();
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                if (roms->empty())
                    return;
                state.currentRomIndex = styleMoveIndex(cmd, state.currentRomIndex,
                    (int)roms->size(), renderComponent.gameStyle(),
                    theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_ENTER) {
                Rom* rom = getCurrentSelectedRom();
                if (!rom)
                    return;

                if (rom->IsFolder() && !rom->IsCue()) {
                    if (enterSelectedRomFolder())
                        logMessage(INFO,"MENU_ROM","enter folder");
                    else if (rom->IsCue())
                        launchRom();   // lazy folder collapsed to a single CD game so launch it now
                } else {
                    launchRom();
                }
                renderComponent.resetValues();
            } else if (cmd == CMD_X){
                renderComponent.openSearch();
                renderComponent.forceFullRedraw();
                m_dirty = true;
            } else if (cmd == CMD_Y){
                Rom* rom = getCurrentSelectedRom();
                if (!rom)
                    return;

                // IsCue is only accurate once a lazy folder is expanded
                ensureRomExpanded(*rom);
                if (rom->IsFolder() && !rom->IsCue())
                    return;

                std::string romPath = resolveLaunchPathForRom(*rom);
                if (romPath.empty())
                    return;

                std::string romName = renderComponent.getAlias(resolveLaunchTitleForRom(*rom));
                toggleFavorite(romName, romPath);
                renderComponent.forceFullRedraw();
                m_dirty = true;
            } else if (cmd == CMD_OPTIONS) {
                auto& sections = menu.getSections();
                if (state.currentSectionIndex < 0 || state.currentSectionIndex >= static_cast<int>(sections.size()))
                    return;

                auto& folders = sections[state.currentSectionIndex].getFolders();
                if (state.currentFolderIndex < 0 || state.currentFolderIndex >= static_cast<int>(folders.size()))
                    return;

                Folder& folder = folders[state.currentFolderIndex];
                Rom* rom = getCurrentSelectedRom();
                if (!rom)
                    return;

                ensureRomExpanded(*rom);
                if (rom->IsFolder() && !rom->IsCue()) {
                    SetTip("Open folder first.");
                    return;
                }

                std::string romPath = resolveLaunchPathForRom(*rom);
                if (romPath.empty()) {
                    SetTip("No games found.");
                    return;
                }

                coreSelectScope = CORE_SELECT_GAME;
                coreSelectFolderName = folder.getTitle();
                coreSelectRomPath = romPath;                 // launch path keys the core override
                g_gameOptCellPath = rom->getPath();          // cell path keys the BG lookup
                g_gameOptItems = { "Set BG Image", "Scrape Art (Default)", "Scrape Art (TheGamesDB)", "Select Core Override", "Hide Game" };
                g_gameOptReturnMenu = MENU_ROM;
                g_gameOptIndex = 0;

                state.currentMenuLevel = MenuLevel::MENU_GAME_OPTIONS;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            }

            break;
        }

        case MenuLevel::MENU_SUBROM:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_ROM;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                Rom rom = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getRoms()[state.currentRomIndex];
                if (rom.getRoms().size()==0)
                    return;
                if (currentSubRomIndex > 0) currentSubRomIndex--;
                else currentSubRomIndex = rom.getRoms().size() - 1;
            } else if (cmd == CMD_DOWN) {
                Rom rom = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getRoms()[state.currentRomIndex];
                if (rom.getRoms().size()==0)
                    return;
                currentSubRomIndex = (currentSubRomIndex + 1) % rom.getRoms().size();
            }else if (cmd == CMD_ENTER) {
                Rom rom = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getRoms()[state.currentRomIndex];
                std::vector<Rom> roms = rom.getRoms();
                if (roms.size()==0)
                    return;

                std::string path = roms[currentSubRomIndex].getPath();
                std::string folderName = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getTitle();
                launchRomWithPath(path,true,"",folderName);
            } else if (cmd == CMD_Y){

                Rom rom = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getRoms()[state.currentRomIndex];
                std::vector<Rom> roms = rom.getRoms();
                if (roms.size()==0)
                    return;

                std::string romName = renderComponent.getAlias(roms[currentSubRomIndex].getTitle());
                std::string romPath = roms[currentSubRomIndex].getPath();
                toggleFavorite(romName, romPath);
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }

            break;

        case MENU_HOMEBREW_FOLDER:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_MAIN;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {

                int n = (int)homebrewFolders.size();
                if (n > 0) {
                    currentHomebrewFolderIndex = styleMoveIndex(cmd, currentHomebrewFolderIndex,
                        n, renderComponent.gameStyle(), theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
                }
            } else if (cmd == CMD_ENTER) {
                state.currentMenuLevel = MenuLevel::MENU_HOMEBREW_GAME;
                currentHomebrewGameIndex = 0;
                homebrewRomIndexStack.clear();
                renderComponent.resetValues();
            }

            break;

        case MENU_HOMEBREW_GAME:
        {
            std::vector<Rom>* roms = getCurrentHomebrewRomList();
            if (!roms) {
                homebrewRomIndexStack.clear();
                state.currentMenuLevel = MenuLevel::MENU_HOMEBREW_FOLDER;
                renderComponent.resetValues();
                break;
            }
            if (cmd == CMD_BACK) {
                if (leaveHomebrewFolder()) {
                    renderComponent.resetValues();
                } else {
                    state.currentMenuLevel = MenuLevel::MENU_HOMEBREW_FOLDER;
                    renderComponent.resetValues();
                }
            } else if (cmd == CMD_UP || cmd == CMD_DOWN || cmd == CMD_LEFT || cmd == CMD_RIGHT) {

                int n = (int)roms->size();
                if (n > 0)
                    currentHomebrewGameIndex = styleMoveIndex(cmd, currentHomebrewGameIndex,
                        n, renderComponent.gameStyle(), theme.getIntValue(Configuration::ITEMS), renderComponent.gridCols());
            } else if (cmd == CMD_ENTER) {
                if (roms->size() > 0 &&
                    currentHomebrewGameIndex >= 0 &&
                    currentHomebrewGameIndex < static_cast<int>(roms->size())){
                    const Rom& sel = (*roms)[currentHomebrewGameIndex];
                    bool isFolder = sel.IsFolder();
                    bool IsCue = sel.IsCue();
                    if (!isFolder){
                        std::string path = sel.getPath();
                        logMessage(INFO,"run homebrew",path.c_str());
                        launchRomWithPath(path,true,sel.getlauncher());
                    }else if (isFolder && IsCue){
                        const std::vector<Rom>& tmpRoms = sel.getRoms();
                        if (tmpRoms.size()>0){
                            std::string path = tmpRoms[0].getPath();
                            launchRomWithPath(path,true,tmpRoms[0].getlauncher());
                        }
                    }
                    else{
                        if (enterSelectedHomebrewFolder())
                            renderComponent.resetValues();
                    }
                }
            } else if (cmd == CMD_OPTIONS) {
                if (roms->size() > 0 &&
                    currentHomebrewGameIndex >= 0 &&
                    currentHomebrewGameIndex < static_cast<int>(roms->size())) {
                    const Rom& sel = (*roms)[currentHomebrewGameIndex];
                    if (sel.IsFolder() && !sel.IsCue()) {
                        SetTip("Open folder first.");
                    } else {
                        g_gameOptCellPath  = sel.getPath();        // BG lookup path
                        g_gameOptItems     = { "Set BG Image" };   // homebrew has no selectable cores
                        g_gameOptReturnMenu = MENU_HOMEBREW_GAME;
                        g_gameOptIndex     = 0;
                        state.currentMenuLevel = MenuLevel::MENU_GAME_OPTIONS;
                        renderComponent.resetValues();
                        renderComponent.forceFullRedraw();
                    }
                }
            }

            break;
        }

        case APP_SETTINGS:
            if (g_settingsGroupIdx < 0) {
                int nGroups = (int)settingsGroups().size();
                int total = nGroups + 4;
                if (cmd == CMD_BACK) {
                    state.currentMenuLevel = MenuLevel::MENU_MAIN;
                    try {
                        state = cfg.loadState();
                    } catch (const StateNotFoundException&) {

                        state.currentMenuLevel = MenuLevel::MENU_MAIN;
                        cfg.saveState(state);
                    }
                    renderComponent.resetValues();
                } else if (cmd == CMD_UP) {
                    g_settingsTopIndex = (g_settingsTopIndex > 0) ? g_settingsTopIndex - 1 : total - 1;
                    renderComponent.forceFullRedraw();
                } else if (cmd == CMD_DOWN) {
                    g_settingsTopIndex = (g_settingsTopIndex + 1) % total;
                    renderComponent.forceFullRedraw();
                } else if (cmd == CMD_ENTER) {
                    if (g_settingsTopIndex < nGroups) {
                        appSettings.setVisibleGroup(settingsGroups()[g_settingsTopIndex].keys);
                        g_settingsGroupIdx = g_settingsTopIndex;
                        currentSettingsIndex = 0;
                        renderComponent.forceFullRedraw();
                    } else if (g_settingsTopIndex == nGroups) {
                        state.currentMenuLevel = MenuLevel::DEVTOOLS_SETTINGS;
                        g_devGroupIdx = -1; g_devTopIndex = 0;
                        renderComponent.forceFullRedraw();
                    } else if (g_settingsTopIndex == nGroups + 1) {
                        g_aboutIndex = 0;
                        state.currentMenuLevel = MenuLevel::MENU_ABOUT;
                        renderComponent.forceFullRedraw();
                    } else if (g_settingsTopIndex == nGroups + 2) {
#if !defined(MACOS) && !defined(X86)
                        SDL_Quit();
                        rebootViaHost(-1);   // warm reboot via host, does not return
#endif
                        exit(EXIT_CONSOLE_QUIT);
                    } else {
                        SDL_Quit();
                        flushConsoleTtyInput();   // no type-ahead on the login prompt
                        exit(EXIT_CONSOLE_QUIT);
                    }
                }
                break;
            }
            if (cmd == CMD_BACK) {
                g_settingsGroupIdx = -1;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP) {
                int n = appSettings.settingCount();
                if (currentSettingsIndex > 0) currentSettingsIndex--;
                else if (n > 0) currentSettingsIndex = n - 1;
            } else if (cmd == CMD_DOWN) {
                int n = appSettings.settingCount();
                if (n > 0) currentSettingsIndex = (currentSettingsIndex + 1) % n;
            }else if (cmd == CMD_ENTER) {
                if (appSettings.getCurrentKey() == Configuration::DEV_TOOLS)
                    state.currentMenuLevel = MenuLevel::DEVTOOLS_SETTINGS;
                else if (appSettings.getCurrentKey() == Configuration::BLUETOOTH){
                    state.currentMenuLevel = MenuLevel::MENU_BLUETOOTH;
                    logMessage(INFO,"Configuration::BLUETOOTH","IN");

                    #ifndef X86
                        btDevices.clear();
                        FileManager fileManager(cfg);
                        fileManager.LoadBlueToothLog(btDevices);

                        namespace fs = std::filesystem;
                        fs::remove("/tmp/consolemode/bluetooth_devices.log");
                        fs::remove("/tmp/consolemode/bt_pair_status.txt");

                        // kill a stale instance orphaned by a crash while this menu was open
                        std::string command = "[ -f /tmp/btstatus.pid ] && kill \"$(cat /tmp/btstatus.pid)\" 2>/dev/null; "
                                              "/tmp/consolemode/btstatus.sh & echo $! > /tmp/btstatus.pid";
                        int ret = system(command.c_str());
                        if(ret == -1) {
                            logMessage(INFO,"Configuration::BLUETOOTH error","btstatus.sh");
                        } else if (ret != 0) {
                            char status_msg[50];
                            snprintf(status_msg, sizeof(status_msg), "system() update.sh returned status: %d", ret);
                            logMessage(INFO,"Configuration::BLUETOOTH error 1",status_msg);
                        } else {
                            logMessage(INFO,"Configuration::BLUETOOTH OK","successfully");
                        }

                    #endif

                    logMessage(INFO,"Configuration::BLUETOOTH","OUT");
                }
                else if (appSettings.getCurrentKey() == Configuration::NET){
                    state.currentMenuLevel = MenuLevel::MENU_WIFI;
                    currentWifiIndex = 0;
                    m_wifiRepaint = true;
                    m_wifi.open();
                }
                else if (appSettings.getCurrentKey() == Configuration::CONTROLS){
                    JsMapper.findDevices();
                    state.currentMenuLevel = MenuLevel::MENU_CONTROLS;
                }
                else if (appSettings.getCurrentKey() == Configuration::KB_MAPPING){
                    state.currentMenuLevel = MenuLevel::MENU_KB_MAP;
                    renderComponent.forceFullRedraw();
                    m_dirty = true;
                }
                else if (appSettings.getCurrentKey() == Configuration::INPUT_TESTER){
                    m_inputTestLog.clear();
                    state.currentMenuLevel = MenuLevel::MENU_INPUT_TESTER;
                    renderComponent.forceFullRedraw();
                    m_dirty = true;
                }
                else if (appSettings.getCurrentKey() == Configuration::VOLUME){

                    isMute = !isMute;
                    saveVolumeInfo();
                }
                else if (appSettings.getCurrentKey() == Configuration::LOADCONFIG){
                    currentIniIndex = appSettings.iniIndex;
                    state.currentMenuLevel = MenuLevel::MENU_SYSTEMCONFIG;
                }
                else if (appSettings.getCurrentKey() == Configuration::MISTER_INI_RES){

                    // write the video_mode preset into the ini then warm reboot to apply
                    if (appSettings.applyMisterIniVideoMode()) {
#if !defined(MACOS) && !defined(X86)
                        SDL_Quit();
                        rebootViaHost(-1);
#endif
                        SetTip("Video mode written to the ini.");
                    }
                }
                else if  (appSettings.getCurrentKey() == Configuration::CLEAR_CACHE){
                    rebuildGameDatabase();
                }
                else if  (appSettings.getCurrentKey() == Configuration::UPDATE){
                    updateSystem();
                }
                else if  (appSettings.getCurrentKey() == Configuration::SCRAPE_ARTWORK){
                    g_scrapeMode = SCRAPE_LIBRETRO;
                    buildScrapeSystems();
                    state.currentMenuLevel = MenuLevel::MENU_SCRAPE_SELECT;
                    renderComponent.forceFullRedraw();
                }
                else if  (appSettings.getCurrentKey() == Configuration::IMPORT_GAMELIST){
                    g_scrapeMode = SCRAPE_GAMELIST;
                    buildScrapeSystems();
                    state.currentMenuLevel = MenuLevel::MENU_SCRAPE_SELECT;
                    renderComponent.forceFullRedraw();
                }
                else if  (appSettings.getCurrentKey() == Configuration::OPTIMIZE_ARTWORK){
                    startOptimizeArtwork();
                }
                else if  (appSettings.getCurrentKey() == Configuration::UPDATE_CM){
                    if (cmupdate::updateAvailable()) {

                        m_updNotesScroll = 0;
                        state.currentMenuLevel = MenuLevel::MENU_UPDATE_CM_CONFIRM;
                        renderComponent.forceFullRedraw();
                    } else if (cmupdate::phase() == cmupdate::UP_TO_DATE) {
                        SetTip("Up to date (v" + cmupdate::currentVersion() + ")");
                    } else {

                        // no check result yet, never download blind
                        cmupdate::startCheck(cfg.getBool(Configuration::USE_MIRROR));
                        SetTip("Checking for updates...");
                    }
                }
                else if  (appSettings.getCurrentKey() == Configuration::MANAGE_HIDDEN){
                    if (vecHiddenFile.empty()) {
                        SetTip("No hidden games.");
                    } else {
                        rebuildHiddenSystems();
                        g_hiddenSysIndex = 0;
                        state.currentMenuLevel = MenuLevel::MENU_HIDDEN_GAMES;
                        renderComponent.forceFullRedraw();
                    }
                }

                renderComponent.resetValues();
            }else if  (cmd == CMD_LEFT) {
                if (appSettings.getCurrentKey() == Configuration::VOLUME){
                    if (isMute){
                        isMute = false;
                        saveVolumeInfo();
                    }else{
                        uint8_t oldVol = vol;
                        if (vol >= 7) vol= 7;
                        else vol += 1;
                        if (oldVol != vol) saveVolumeInfo();
                    }
                }
            }else if  (cmd == CMD_RIGHT) {
                if (appSettings.getCurrentKey() == Configuration::VOLUME){
                    if (isMute){
                        isMute = false;
                        saveVolumeInfo();
                    }else{
                        uint8_t oldVol = vol;
                        if (vol <= 0 ) vol= 0;
                        else vol -= 1;
                        if (oldVol != vol) saveVolumeInfo();
                    }
                }
            }
            break;

        case DEVTOOLS_SETTINGS:
            if (g_devGroupIdx < 0) {
                int total = (int)devGroups().size();
                if (cmd == CMD_BACK) {
                    state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                    renderComponent.resetValues();
                } else if (cmd == CMD_UP) {
                    g_devTopIndex = (g_devTopIndex > 0) ? g_devTopIndex - 1 : total - 1;
                    renderComponent.forceFullRedraw();
                } else if (cmd == CMD_DOWN) {
                    g_devTopIndex = (g_devTopIndex + 1) % total;
                    renderComponent.forceFullRedraw();
                } else if (cmd == CMD_ENTER) {
                    devSettings.setVisibleGroup(devGroups()[g_devTopIndex].keys);
                    g_devGroupIdx = g_devTopIndex;
                    currentDevToolsIndex = 0;
                    renderComponent.forceFullRedraw();
                }
                break;
            }
            if (cmd == CMD_BACK) {
                g_devGroupIdx = -1;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP) {
                if (currentDevToolsIndex > 0 )currentDevToolsIndex--;
                else currentDevToolsIndex = devSettings.getDevToolSize() - 1;
            } else if (cmd == CMD_DOWN) {
                currentDevToolsIndex = (currentDevToolsIndex + 1) % devSettings.getDevToolSize();
            } else if (cmd == CMD_LEFT) {
            } else if (cmd == CMD_RIGHT) {
            }else if (cmd == CMD_ENTER) {
                if (devSettings.getCurrentKey() == Configuration::LOADSCRIPT)
                    state.currentMenuLevel = MenuLevel::MENU_SCRIPT;
                else if (devSettings.getCurrentKey() == Configuration::CD_TESTER){
                    if (!cdtest::driveExists()) {
                        SetTip("No CD drive found.");
                    } else if (!cdtest::open()) {
                        SetTip("No playable CD found.");
                    } else {
                        m_cdIdx = 0;
                        state.currentMenuLevel = MenuLevel::MENU_CD_TESTER;
                        renderComponent.resetValues();
                        renderComponent.forceFullRedraw();
                        m_dirty = true;
                    }
                }
#ifdef HAS_PS1
                else if (devSettings.getCurrentKey() == Configuration::DISC_OPTIONS){
                    m_ps1.openDiscOptions();
                }
#endif
            }
            break;

        case MENU_ABOUT:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                g_settingsGroupIdx = -1;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            } else if (cmd == CMD_UP || cmd == CMD_DOWN) {
                g_aboutIndex ^= 1;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            } else if (cmd == CMD_ENTER) {
                if (g_aboutIndex == 0) {
                    state.currentMenuLevel = MenuLevel::MENU_STORAGE;
                } else {
                    renderComponent.creditsReset();
                    state.currentMenuLevel = MenuLevel::MENU_CREDITS;
                }
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            break;

        case MENU_STORAGE:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_ABOUT;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            break;

        case MenuLevel::MENU_SET_TIME:
        {
            auto daysIn = [](int y, int m) {
                static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
                if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) return 29;
                return d[m - 1];
            };
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_LEFT || cmd == CMD_RIGHT) {
                int nFields = renderComponent.clock12() ? 6 : 5;   // 12h adds an AM/PM field
                g_timeField = (g_timeField + (cmd == CMD_RIGHT ? 1 : nFields - 1)) % nFields;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP || cmd == CMD_DOWN) {
                int dir = (cmd == CMD_UP) ? 1 : -1;
                int y = g_timeEdit.tm_year + 1900;
                switch (g_timeField) {
                    case 0: y += dir; if (y < 2020) y = 2099; if (y > 2099) y = 2020;
                            g_timeEdit.tm_year = y - 1900; break;
                    case 1: g_timeEdit.tm_mon = (g_timeEdit.tm_mon + dir + 12) % 12; break;
                    case 2: { int dim = daysIn(y, g_timeEdit.tm_mon + 1);
                              g_timeEdit.tm_mday = ((g_timeEdit.tm_mday - 1 + dir + dim) % dim) + 1; break; }
                    case 3:
                        if (renderComponent.clock12())   // cycle 12..11 within the same half-day
                            g_timeEdit.tm_hour = (g_timeEdit.tm_hour / 12) * 12
                                               + ((g_timeEdit.tm_hour % 12 + dir + 12) % 12);
                        else
                            g_timeEdit.tm_hour = (g_timeEdit.tm_hour + dir + 24) % 24;
                        break;
                    case 4: g_timeEdit.tm_min  = (g_timeEdit.tm_min  + dir + 60) % 60; break;
                    case 5: g_timeEdit.tm_hour = (g_timeEdit.tm_hour + 12) % 24; break;   // AM <-> PM
                }
                int dim = daysIn(g_timeEdit.tm_year + 1900, g_timeEdit.tm_mon + 1);
                if (g_timeEdit.tm_mday > dim) g_timeEdit.tm_mday = dim;   // a Y/M change can shrink the month
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_ENTER) {
                g_timeEdit.tm_sec = 0;
                g_timeEdit.tm_isdst = -1;
                time_t t = mktime(&g_timeEdit);
                if (t != (time_t)-1) {
                    struct timeval tv = { t, 0 };
                    settimeofday(&tv, nullptr);
                    std::error_code rec;
                    bool rtc = std::filesystem::exists("/dev/rtc0", rec)
                            || std::filesystem::exists("/dev/rtc", rec);
                    system("hwclock -w >/dev/null 2>&1 &");
                    SetTip(rtc ? "Time set (saved to RTC)." : "Time set (no RTC found).");
                } else {
                    SetTip("Invalid date.");
                }
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            }
            break;
        }

        case MENU_CREDITS:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_ABOUT;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            else if (cmd == CMD_ENTER) {
                renderComponent.licenseListReset();
                state.currentMenuLevel = MenuLevel::MENU_LICENSE;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            else if (cmd == CMD_UP)   { renderComponent.creditsScroll(-1); m_dirty = true; }
            else if (cmd == CMD_DOWN) { renderComponent.creditsScroll(+1); m_dirty = true; }
            break;

        case MENU_LICENSE:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_CREDITS;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            else if (cmd == CMD_ENTER) {
                renderComponent.licenseTextReset();
                state.currentMenuLevel = MenuLevel::MENU_LICENSE_TEXT;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            else if (cmd == CMD_UP)   { renderComponent.licenseListMove(-1); m_dirty = true; }
            else if (cmd == CMD_DOWN) { renderComponent.licenseListMove(+1); m_dirty = true; }
            break;

        case MENU_LICENSE_TEXT:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_LICENSE;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            else if (cmd == CMD_UP)   { renderComponent.licenseTextScroll(-1); m_dirty = true; }
            else if (cmd == CMD_DOWN) { renderComponent.licenseTextScroll(+1); m_dirty = true; }
            break;

        case MENU_INPUT_TESTER:
            // modal, only Back exits so the pad under test can't drive the menu
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            break;

        case MENU_KB_MAP:
            if (cmd == CMD_BACK || cmd == CMD_ENTER) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            }
            break;

        case MENU_CD_TESTER:

            if (cmd == CMD_BACK) {
                cdtest::close();
                state.currentMenuLevel = MenuLevel::DEVTOOLS_SETTINGS;
                renderComponent.forceFullRedraw();
                m_dirty = true;
            } else if (cmd == CMD_UP || cmd == CMD_DOWN) {
                int n = (int)cdtest::items().size();
                if (n > 0)
                    m_cdIdx = (m_cdIdx + (cmd == CMD_UP ? -1 : 1) + n) % n;
            } else if (cmd == CMD_ENTER) {
                if (cdtest::playing() && m_cdIdx == cdtest::currentIndex())
                    cdtest::togglePause();
                else
                    cdtest::play(m_cdIdx);
            } else if (cmd == CMD_LEFT) {
                cdtest::seek(-10);
            } else if (cmd == CMD_RIGHT) {
                cdtest::seek(10);
            } else if (cmd == CMD_X) {
                cdtest::stop();
            }
            break;

        case MENU_SYSTEMCONFIG:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                if (currentIniIndex > 0) currentIniIndex--;
                else currentIniIndex = appSettings.MisterInis.size() - 1;
            } else if (cmd == CMD_DOWN) {
                if (appSettings.MisterInis.size() > 0)
                    currentIniIndex = (currentIniIndex + 1) % appSettings.MisterInis.size();
            } else if (cmd == CMD_ENTER) {
                appSettings.iniIndex = currentIniIndex;

                int ini_num = currentIniIndex;
                logMessage(INFO,"Configuration::LOADCONFIG",std::to_string(ini_num).c_str());

                #ifdef X86
                #else

                    // persist so a cold boot restores it, the host reads last_ini on init
                    {
                        std::ofstream lf(std::string(getHomePath()) + "/ConsoleMode/last_ini", std::ios::trunc);
                        if (lf) lf << ini_num << "\n";
                    }

                    logMessage(INFO,"Configuration::LOADCONFIG reboot via host",std::to_string(ini_num).c_str());
                    SDL_Quit();
                    rebootViaHost(ini_num);   // host applies the ini and warm reboots, does not return
                #endif
            }
            break;

        case MENU_SCRIPT:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::DEVTOOLS_SETTINGS;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                if (vecScriptList.empty()) break;
                if (currentScriptIndex > 0) currentScriptIndex--;
                else currentScriptIndex = vecScriptList.size() - 1;
            } else if (cmd == CMD_DOWN) {
                if (vecScriptList.empty()) break;   // guard % 0 on an empty Scripts folder
                currentScriptIndex = (currentScriptIndex + 1) % vecScriptList.size();
            } else if (cmd == CMD_ENTER) {
                if (islaunchedByMiSTer)
                    execShell(currentScriptIndex);
                else
                    execShellNoMister(currentScriptIndex);
            }
            break;

        case MENU_CONTROLS:
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP) {
                currentControlIndex = currentControlIndex == 0 ? 1: 0;
            } else if (cmd == CMD_DOWN) {
                currentControlIndex = currentControlIndex == 0 ? 1: 0;
            } else if (cmd == CMD_ENTER) {
                if (currentControlIndex==0){
                    JsMapper.findDevices();
                    refreshHostDeviceMaps();
                    state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY;
                    currentJoystickIndex = 0;
                    renderComponent.resetValues();
                }
                else if (currentControlIndex==1){
                    if (islaunchedByMiSTer)
                        execShellWithName("/media/fat/ConsoleMode/remap.sh");
                    else
                        execShellWithNameNoMister("/media/fat/ConsoleMode/remap.sh");
                }
            }
            break;

        case MENU_CONTROLS_JOY:{

            const std::vector<DeviceInfo> vecDev = JsMapper.getVecDevices();
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_CONTROLS;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                if(vecDev.size()>0){
                    if (currentJoystickIndex > 0) currentJoystickIndex--;
                    else currentJoystickIndex = vecDev.size() - 1;
                }
            } else if (cmd == CMD_DOWN) {
                if(vecDev.size()>0){
                    currentJoystickIndex = (currentJoystickIndex + 1) % vecDev.size();
                }
            } else if (cmd == CMD_ENTER) {
                if(vecDev.size()>0){
                    JsMapper.loadFile(vecDev[currentJoystickIndex].path, vecDev[currentJoystickIndex].type);

                    currentJoystickKeyIndex = 0;
                    joystickKeyState = 0;
                    state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY1;
                    renderComponent.resetValues();
                }
            } else if (cmd == CMD_OPTIONS) {
                if (vecDev.size() > 0) {
                    m_joyOptItems = {"Input Tester", "Delete Mapping"};
                    m_joyOptIndex = 0;
                    state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY2;
                    renderComponent.forceFullRedraw();
                }
            }
            break;
        }

        case MENU_CONTROLS_JOY1:{
            if (joystickKeyState == 0){
                std::vector<KeyInfo> vecKey = JsMapper.getKeys();
                if (cmd == CMD_BACK) {
                    state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY;
                    renderComponent.resetValues();
                } else if (cmd == CMD_UP) {
                    if(vecKey.size()>0){
                        if (currentJoystickKeyIndex > 0) currentJoystickKeyIndex--;
                        else currentJoystickKeyIndex = vecKey.size() - 1;
                    }
                } else if (cmd == CMD_DOWN) {
                    if(vecKey.size()>0){
                        currentJoystickKeyIndex = (currentJoystickKeyIndex + 1) % vecKey.size();
                    }
                } else if (cmd == CMD_ENTER) {
                    joystickKeyState = 1;
                    tipStartTime = 0;   // drop the previous "X set" tip, the prompt replaces it
                    strtip.clear();
                    renderComponent.forceFullRedraw();
                    m_dirty = true;
                }
            }else{
                if (cmd == CMD_BACK) {
                    state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY;
                    renderComponent.resetValues();
                } else if (cmd == CMD_ENTER) {
                    joystickKeyState = 0;
                }
            }

            break;
        }

        case MENU_CONTROLS_JOY2: {
            int nOpt = (int)m_joyOptItems.size();
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY;
                renderComponent.forceFullRedraw();
            } else if (cmd == CMD_UP && nOpt > 0) {
                m_joyOptIndex = (m_joyOptIndex + nOpt - 1) % nOpt;
            } else if (cmd == CMD_DOWN && nOpt > 0) {
                m_joyOptIndex = (m_joyOptIndex + 1) % nOpt;
            } else if (cmd == CMD_ENTER && m_joyOptIndex < nOpt) {
                const std::vector<DeviceInfo> vecDev = JsMapper.getVecDevices();
                if (currentJoystickIndex < (int)vecDev.size()) {
                    const DeviceInfo& d = vecDev[currentJoystickIndex];
                    std::string act = m_joyOptItems[m_joyOptIndex];
                    if (act == "Input Tester") {
                        JsMapper.loadFile(d.path, d.type);   // load its codes so presses highlight
                        m_joyTesterVid = m_joyTesterPid = 0;
                        sscanf(d.vid_pid.c_str(), "%hx_%hx", &m_joyTesterVid, &m_joyTesterPid);
                        m_joyPressed.clear(); m_joyHatX = 0; m_joyHatY = 0; m_joyEscSince = 0;
                        state.currentMenuLevel = MenuLevel::MENU_JOY_TESTER;
                        renderComponent.forceFullRedraw();
                        m_dirty = true;
                    } else if (act == "Delete Mapping") {
                        JsMapper.deleteMap(d.path);
                        launchMiSTerRR("@inputreload");
                        SetTip("Mapping deleted");
                        JsMapper.findDevices();
                        if (currentJoystickIndex >= (int)JsMapper.getVecDevices().size())
                            currentJoystickIndex = 0;
                        state.currentMenuLevel = MenuLevel::MENU_CONTROLS_JOY;
                        renderComponent.forceFullRedraw();
                    }
                }
            }
            break;
        }

        case MENU_JOY_TESTER:
            // modal, Select+Start held 3s exits (checked in the draw)
            break;

        case MENU_BLUETOOTH:
            if (cmd == CMD_BACK) {

                logMessage(INFO,"MENU_BLUETOOTH","stop pairing + scan");

                // group kill so the run's scan child dies with the script
                system("[ -f /tmp/bt_pair.pid ] && kill -- -$(cat /tmp/bt_pair.pid) 2>/dev/null; "
                       "bluetoothctl scan off >/dev/null 2>&1; "
                       "rm -f /tmp/consolemode/bt_pair_status.txt /tmp/bt_pair.pid; "
                       "rmdir /tmp/bt_pair.lock.d 2>/dev/null");

                FILE* fp = fopen("/tmp/btstatus.pid", "r");
                if (fp) {
                    pid_t pid = 0;
                    int got = fscanf(fp, "%d", &pid);
                    fclose(fp);

                    // only signal a real positive pid: 0 hits our group, -1 the whole uid. TERM lets its cleanup trap run
                    if (got == 1 && pid > 0) kill(pid, SIGTERM);
                    remove("/tmp/btstatus.pid");
                }

                btInfo.clear();

                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                if (currentBtIndex > 0) currentBtIndex--;
                else currentBtIndex = MAX_BT_DEVS - 1;
            } else if (cmd == CMD_DOWN) {
                currentBtIndex = (currentBtIndex + 1) % MAX_BT_DEVS;
            } else if (cmd == CMD_ENTER) {

                #ifndef X86
                    // bt_pair.sh single-instances itself so a repeat press is a harmless no-op
                    logMessage(INFO,"MENU_BLUETOOTH","start pair (bt_pair.sh bg)");
                    system("setsid /tmp/consolemode/bt_pair.sh >/dev/null 2>&1 &");
                #endif
            } else if (cmd == CMD_OPTIONS) {
                #ifndef X86
                    logMessage(INFO,"MENU_BLUETOOTH","clear pairings (bt_clear.sh)");
                    system("[ -f /tmp/bt_pair.pid ] && kill -- -$(cat /tmp/bt_pair.pid) 2>/dev/null; "
                           "setsid /tmp/consolemode/bt_clear.sh >/dev/null 2>&1 &");
                #endif
            }
            break;

        case MENU_WIFI:
            if (cmd == CMD_BACK) {
                m_wifi.close();   // aborts an in-flight connect and restores network state
                state.currentMenuLevel = MenuLevel::APP_SETTINGS;
                renderComponent.resetValues();
            } else if (cmd == CMD_UP) {
                if (currentWifiIndex > 0) currentWifiIndex--;
                else if (!m_wifi.networks().empty()) currentWifiIndex = (int)m_wifi.networks().size() - 1;
            } else if (cmd == CMD_DOWN) {
                if (!m_wifi.networks().empty())
                    currentWifiIndex = (currentWifiIndex + 1) % (int)m_wifi.networks().size();
            } else if (cmd == CMD_X) {
                m_wifi.rescan();
            } else if (cmd == CMD_OPTIONS) {
                const auto& nets = m_wifi.networks();
                if (currentWifiIndex < (int)nets.size()) {
                    wifiOptNet = nets[currentWifiIndex];
                    currentWifiOptIndex = 0;

                    bool saved = m_wifi.isSaved(wifiOptNet);
                    wifiOptItems.clear();
                    wifiOptItems.push_back("Connect");
                    if (saved && wifiOptNet.sec != "Open") wifiOptItems.push_back("Change Password");
                    if (saved)                             wifiOptItems.push_back("Forget");
                    state.currentMenuLevel = MenuLevel::MENU_WIFI_OPTIONS;
                }
            } else if (cmd == CMD_ENTER) {
                const auto& nets = m_wifi.networks();
                if (nets.empty())
                    m_wifi.rescan();
                else if (currentWifiIndex < (int)nets.size())
                    connectWifiNet(nets[currentWifiIndex]);
            }
            break;

        case MENU_WIFI_OPTIONS: {
            int nOpt = (int)wifiOptItems.size();
            if (cmd == CMD_BACK) {
                state.currentMenuLevel = MenuLevel::MENU_WIFI;
                m_wifiRepaint = true;
            } else if (cmd == CMD_UP && nOpt > 0) {
                currentWifiOptIndex = (currentWifiOptIndex + nOpt - 1) % nOpt;
            } else if (cmd == CMD_DOWN && nOpt > 0) {
                currentWifiOptIndex = (currentWifiOptIndex + 1) % nOpt;
            } else if (cmd == CMD_ENTER && currentWifiOptIndex < nOpt) {
                std::string act = wifiOptItems[currentWifiOptIndex];
                state.currentMenuLevel = MenuLevel::MENU_WIFI;
                m_wifiRepaint = true;
                if (act == "Connect") {
                    connectWifiNet(wifiOptNet);
                } else if (act == "Change Password") {
                    wifiPending = wifiOptNet;
                    renderComponent.openTextInput("Password for " + wifiOptNet.ssid);
                } else if (act == "Forget") {
                    m_wifi.forget(wifiOptNet);
                }
            }
            break;
        }
#ifdef HAS_PS1
        case MenuLevel::MENU_CD:
        case MenuLevel::MENU_OTHER_CD:
        case MENU_DISC_OPTIONS:
        case MENU_DUMP_DEVICE:
            m_ps1.nav(state.currentMenuLevel, cmd);
            break;
#endif
    }

    // these menus need their own Back teardown so no settings shortcut into them
    if (cmd == CMD_SYS_SETTINGS &&
        state.currentMenuLevel != MenuLevel::MENU_SCRAPE_PROGRESS &&
        state.currentMenuLevel != MenuLevel::MENU_OPTIMIZE_PROGRESS &&
        state.currentMenuLevel != MenuLevel::MENU_UPDATE_CM_PROGRESS &&
        state.currentMenuLevel != MenuLevel::MENU_INPUT_TESTER &&
        state.currentMenuLevel != MenuLevel::MENU_JOY_TESTER &&
        state.currentMenuLevel != MenuLevel::MENU_CD_TESTER &&
        state.currentMenuLevel != MenuLevel::MENU_BLUETOOTH &&
        state.currentMenuLevel != MenuLevel::MENU_WIFI) {
        if(state.currentMenuLevel != MenuLevel::APP_SETTINGS) {
            cfg.saveState(state);
        }
        state.currentMenuLevel = MenuLevel::APP_SETTINGS;
        renderComponent.resetValues();
    }

    if(entryLevel == MenuLevel::APP_SETTINGS && state.currentMenuLevel == MenuLevel::APP_SETTINGS && entrySettingsGroup >= 0) {

        std::string currentKey = appSettings.getCurrentKey();
        bool onlyEnter = false;
        if (currentKey == Configuration::NET || currentKey == Configuration::VOLUME ||
            currentKey == Configuration::LOADCONFIG || currentKey == Configuration::DEV_TOOLS ||
            currentKey == Configuration::BLUETOOTH || currentKey == Configuration::UPDATE_TIME ||
            currentKey == Configuration::CLEAR_CACHE || currentKey == Configuration::QUIT ||
            currentKey == Configuration::UPDATE ||
            currentKey == Configuration::SCRAPE_ARTWORK || currentKey == Configuration::IMPORT_GAMELIST ||
            currentKey == Configuration::OPTIMIZE_ARTWORK || currentKey == Configuration::MANAGE_HIDDEN
            ){
                onlyEnter = true;
            }

        if (cmd == CMD_UP) {
            appSettings.navigateUp();
        } else if (cmd == CMD_DOWN) {
            appSettings.navigateDown();
        } else if (cmd == CMD_LEFT && !onlyEnter) {
            appSettings.navigateLeft();
        } else if (cmd == CMD_RIGHT && !onlyEnter) {
            appSettings.navigateRight();
        } else if (cmd == CMD_ENTER) {
            appSettings.navigateEnter();
            if  (appSettings.getCurrentKey() == Configuration::UPDATE_TIME){
                time_t now = time(nullptr);
                localtime_r(&now, &g_timeEdit);
                g_timeField = 0;
                state.currentMenuLevel = MenuLevel::MENU_SET_TIME;
                renderComponent.forceFullRedraw();
            }
        }

        // a L/R value change keeps the same row so force a repaint to show the new value
        if (cmd == CMD_LEFT || cmd == CMD_RIGHT)
            renderComponent.forceFullRedraw();

        std::string currentValue = appSettings.getCurrentValue();
    }

    if(entryLevel == MenuLevel::DEVTOOLS_SETTINGS && state.currentMenuLevel == MenuLevel::DEVTOOLS_SETTINGS && entryDevGroup >= 0) {
        std::string currentKey = devSettings.getCurrentKey();
        bool onlyEnter = false;
        if (currentKey == Configuration::CONTROLS || currentKey == Configuration::LOADSCRIPT
             || currentKey == Configuration::INPUT_TESTER ){
            onlyEnter = true;
        }
#ifdef HAS_PS1
        if (currentKey == Configuration::DISC_OPTIONS)
            onlyEnter = true;
#endif

        if (cmd == CMD_UP) {
            devSettings.navigateUp();
        } else if (cmd == CMD_DOWN) {
            devSettings.navigateDown();
        } else if (cmd == CMD_LEFT && !onlyEnter) {
            devSettings.navigateLeft();
        } else if (cmd == CMD_RIGHT && !onlyEnter) {
            devSettings.navigateRight();
        } else if (cmd == CMD_ENTER) {
            devSettings.navigateEnter();
        }

        if (cmd == CMD_LEFT || cmd == CMD_RIGHT)
            renderComponent.forceFullRedraw();
    }
}

void Application::run() {

    islaunchedByMiSTer = consolemodeConsumeMisterLaunchMarker();
    if(islaunchedByMiSTer)
        logMessage(INFO,"begin run","islaunchedByMiSTer is true");
    else
        logMessage(INFO,"begin run","islaunchedByMiSTer is false");

#ifndef LITE_BUILD

    {
        // deploy check reads this to confirm the new binary actually ran
        std::string tp = std::string(getHomePath()) + "/ConsoleMode/Logs/input_evtrace.log";
        FILE* tf = fopen(tp.c_str(), "w");
        if (tf) { fprintf(tf, "%u BOOT %s\n", SDL_GetTicks(), CONSOLEMODE_COMMIT); fclose(tf); }
    }
#endif

    if (inisafe::g_fileReset.load())
        SetTip("A settings file was corrupt and was reset (kept as .bad).");

    // restores the .bak and re-execs if a self-update is boot-looping
    cmupdate::armUpdateWatchdog();

    logMessage(INFO,"run", "start");

    int last_count = count_event_devices();

    SDL_Event event;

    int fps = 0;
    int frameCount = 0;
    Uint32 fpsTimer = 0;

    Uint32 frameStart = 0;

    openInputDev();
    struct input_event ev;

    int screenRefresh = cfg.getInt(Configuration::SCREEN_REFRESH);
    if (screenRefresh <= 0) screenRefresh = 60;
    Uint32 frameDelay = 1000 / screenRefresh;
    Uint32 runStartMs = SDL_GetTicks();
    bootMark("run-enter (event loop starting)");
    int lastClockMin = -1;
    int prevDrawMenu = -1;
#ifdef MACOS
    bool macConsumedNav = false;
#endif

#ifdef __linux__

    {
        // pin the render thread to core 0 and briefly raise priority through boot contention
        cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
        sched_setaffinity(0, sizeof(set), &set);
    }
    pid_t renderTid = (pid_t)syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, renderTid, -10);
    bool prioReverted = false;
#endif

    long long perfRenderSumUs = 0, perfPresentSumUs = 0;
    long perfRenderMaxUs = 0, perfPresentMaxUs = 0;
    int perfRenderedFrames = 0;

#ifdef MACOS

    {
        // mac only: env vars render the splash/glyphs to a BMP for design preview
        const char* gshot = getenv("CONSOLEMODE_GLYPH_SHOT");
        if (gshot) {
            for (int f = 0; f < 4; f++) { renderComponent.debugGlyphSheet(); renderComponent.update(); SDL_Delay(16); }
            renderComponent.saveScreenshot(gshot);
            isRunning = false;
        }
        const char* sshot = getenv("CONSOLEMODE_SPLASH_SHOT");
        if (sshot) {
            const char* penv = getenv("CONSOLEMODE_SPLASH_PROGRESS");
            float prog = penv ? (float)atof(penv) : 0.55f;
            for (int f = 0; f < 6; f++) { renderComponent.drawBootSplash(prog, "Loading..."); renderComponent.update(); SDL_Delay(16); }
            renderComponent.saveScreenshot(sshot);
            isRunning = false;
        }
    }
#endif

    {
        // hold the menu behind a splash until icons decode, draining input so evdev can't overflow
        std::string bootSpeed = cfg.get(Configuration::BOOT_SPEED);
        if (bootSpeed.empty()) bootSpeed = "Safe";
        if (bootSpeed != "Unsafe" && bootSpeed != "Fast") {
            Uint32 SLOW_MS = 10000;         // Slow holds at least this long
            { const char* e = getenv("CONSOLEMODE_SPLASH_MS"); if (e && atoi(e) > 0) SLOW_MS = (Uint32)atoi(e); }
            Uint32 MOD_MS = 3000;           // Moderate minimum hold
            Uint32 CAP_MS = std::max(SLOW_MS, MOD_MS) + 10000;   // never gate longer than this
            bool slow = (bootSpeed == "Safest" || bootSpeed == "Slow");
            int total = renderComponent.thumbPreloadCount();
            Uint32 gateStart = SDL_GetTicks();
            bootMark("splash-gate-enter (" + bootSpeed + ")");
            while (isRunning) {
#if defined(__linux__)
                for (int i = 0; i < m_evCount; i++) {
                    if (!m_evfd[i].isOpen || m_evfd[i].fd < 0) continue;
                    struct input_event iev;
                    while (read(m_evfd[i].fd, &iev, sizeof(iev)) == (ssize_t)sizeof(iev)) {}
                }
#endif
                SDL_PumpEvents();
                { SDL_Event ev; while (SDL_PollEvent(&ev)) {} }
                renderComponent.pollThumbResult();
                int pending = (int)renderComponent.thumbWorkPending();
                Uint32 elapsed = SDL_GetTicks() - gateStart;
                bool decodeDone = (pending == 0);

                Uint32 minMs = slow ? SLOW_MS : MOD_MS;
                bool ready = decodeDone && elapsed >= minMs;
                if (ready || elapsed >= CAP_MS) break;
                float decodeFrac = (total > 0) ? (float)(total - pending) / (float)total : 1.0f;
                float prog = std::min(decodeFrac, (float)elapsed / (float)minMs);
                renderComponent.drawBootSplash(prog, "Loading...");
                renderComponent.update();
                SDL_Delay(16);
            }
            renderComponent.forceFullRedraw();
            bootMark("splash-gate-released");
            // re-open pads that enumerated while the splash held
            openInputDev();
        }
    }

    // load USB caches before the first frame so last-used games show immediately
    loadUsbRom();

    while (isRunning) {
        frameStart = SDL_GetTicks();

#ifdef __linux__

        if (!prioReverted && (SDL_GetTicks() - runStartMs) > 8000) {
            setpriority(PRIO_PROCESS, renderTid, 0);
            prioReverted = true;
        }
#endif

#if !defined(MACOS) && !defined(X86)

        {
            // a 90deg TATE change needs a full re-init so warm reboot, debounced ~0.5s
            static int s_lastRot = -1, s_rotStable = 0;
            int curRot = crt::rotation();
            if (curRot == s_lastRot) { if (s_rotStable < 1000) s_rotStable++; }
            else { s_lastRot = curRot; s_rotStable = 0; }
            bool portraitInvolved = (curRot == 1 || curRot == 2) ||
                                    (renderComponent.m_rotation == 1 || renderComponent.m_rotation == 2);
            if (curRot != renderComponent.m_rotation && portraitInvolved && s_rotStable >= 30) {
                rebootViaHost(-1);
            }
        }
#endif

        // a pre-2023 clock fails every TLS cert so kick NTP instead of spending an attempt
        { static int s_cmTries = 0; static Uint32 s_cmNext = 6000;
          if ((SDL_GetTicks() - runStartMs) > s_cmNext && !cmupdate::checkSettled() &&
              cmupdate::phase() != cmupdate::CHECKING && cmupdate::phase() < cmupdate::DOWNLOADING) {
              if (time(nullptr) < 1672531200) {
#if !defined(MACOS) && !defined(X86)

                  system("ntpdate -b -u pool.ntp.org >/dev/null 2>&1 &");
#endif
                  s_cmNext += 15000;
              } else {
                  s_cmTries++;
                  s_cmNext += (s_cmTries < 6) ? 30000 : 300000;
                  cmupdate::startCheck(cfg.getBool(Configuration::USE_MIRROR));
              }
          } }

        { static bool s_greeted = false;
          if (!s_greeted && (SDL_GetTicks() - runStartMs) > 8000) {
              s_greeted = true;
              std::string m = cmupdate::takeBootMessage();
              if (!m.empty()) SetTip(m, 6000);
          } }

        adoptRbfScanResult();

        // while a direction is held, poll instead of blocking so repeats aren't quantised to the frame
        bool held = false;
        for (int i = 0; i < m_evCount; i++)
            if (m_evfd[i].isOpen && (m_evfd[i].hat_x != 0 || m_evfd[i].hat_y != 0)) { held = true; break; }

        // block in select() for input unless something changed or is animating
        bool active = m_dirty || held || tipStartTime > 0 ||
                      renderComponent.needsContinuousRedraw() ||
#ifdef HAS_PS1
                      state.currentMenuLevel == MENU_CD ||
#endif
                      state.currentMenuLevel == MENU_CD_TESTER ||
                      state.currentMenuLevel == MENU_BLUETOOTH ||
                      state.currentMenuLevel == MENU_WIFI ||
                      state.currentMenuLevel == MENU_OPTIMIZE_PROGRESS ||
                      state.currentMenuLevel == MENU_UPDATE_CM_PROGRESS ||
                      (SDL_GetTicks() - runStartMs) < 5000;

        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = active ? 0 : (frameDelay * 1000);

        for (int i = 0; i < m_evCount; i++) {
            if (m_evfd[i].isOpen && m_evfd[i].fd >= 0) {
                FD_SET(m_evfd[i].fd, &rfds);
                if (m_evfd[i].fd > maxfd) maxfd = m_evfd[i].fd;
            }
        }

        int ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        bool isHandlerInput = false;
        if (ret > 0) {
            for (int i = 0; i < m_evCount; i++) {

                if (!m_evfd[i].isOpen || m_evfd[i].fd < 0)
                    continue;

                if (!FD_ISSET(m_evfd[i].fd, &rfds))
                    continue;

                while (1) {
                    ssize_t n = read(m_evfd[i].fd, &ev, sizeof(ev));

                    if (n == sizeof(ev)) {

                        // opt-in: log key press/release edges to debug dropped KEYUPs
                        if (ev.type == EV_KEY && ev.value != 2 && isLoggingEnabled()) {
                            static int evTrace = 0;
                            if (evTrace < 12000) { evTrace++;
                                const char* kn = (ev.code==KEY_UP)?"UP":(ev.code==KEY_DOWN)?"DOWN":
                                                 (ev.code==KEY_LEFT)?"LEFT":(ev.code==KEY_RIGHT)?"RIGHT":"key";
                                std::string tp = std::string(getHomePath()) + "/ConsoleMode/Logs/input_evtrace.log";
                                FILE* tf = fopen(tp.c_str(), "a");
                                if (tf) { fprintf(tf, "%u EV %s %04x:%04x %s code=%d(0x%x) val=%d\n",
                                                  SDL_GetTicks(), m_evfd[i].path, m_evfd[i].vid, m_evfd[i].pid, kn, ev.code, ev.code, ev.value); fclose(tf); }
                            }
                        }

                        // capture buttons + d-pad from every device for a VID:PID readout
                        if (state.currentMenuLevel == MENU_INPUT_TESTER &&
                            ((ev.type == EV_KEY && ev.value != 2) ||
                             (ev.type == EV_ABS && (ev.code == ABS_HAT0X || ev.code == ABS_HAT0Y ||
                                                    ev.code == ABS_X || ev.code == ABS_Y ||
                                                    ev.code == ABS_Z || ev.code == ABS_RZ ||
                                                    ev.code == ABS_GAS || ev.code == ABS_BRAKE)))) {
                            char line[160];
                            line[0] = 0;
                            // node + id: two pads can share a vid:pid, the node is what differs
                            const char* nd = strrchr(m_evfd[i].path, '/');
                            char evTag[48];
                            snprintf(evTag, sizeof evTag, "%s %04x:%04x",
                                     nd ? nd + 1 : m_evfd[i].path, m_evfd[i].vid, m_evfd[i].pid);
                            if (ev.type == EV_KEY)
                                snprintf(line, sizeof line, "%s BTN 0x%x (%d)  %s",
                                         evTag, ev.code, ev.code,
                                         ev.value ? "press" : "release");
                            else if (ev.code == ABS_HAT0X || ev.code == ABS_HAT0Y)
                                snprintf(line, sizeof line, "%s HAT %s  val=%d",
                                         evTag, ev.code == ABS_HAT0X ? "X" : "Y", ev.value);
                            else if (ev.code == ABS_X || ev.code == ABS_Y) {
                                // digital pads report the dpad here, log direction edges
                                int dir = absAxisDir(&m_evfd[i], ev.code, ev.value);
                                snprintf(line, sizeof line, "%s AXIS %s  dir=%+d",
                                         evTag, ev.code == ABS_X ? "X" : "Y", dir);
                            }
                            else {
                                // analog triggers (xinput pads), shown as edges not a value stream
                                struct input_absinfo ai;
                                if (ioctl(m_evfd[i].fd, EVIOCGABS(ev.code), &ai) == 0 && ai.maximum > ai.minimum) {
                                    int span = ai.maximum - ai.minimum;
                                    const char* nm = ev.code == ABS_Z ? "Z" : ev.code == ABS_RZ ? "RZ"
                                                   : ev.code == ABS_GAS ? "GAS" : "BRAKE";
                                    if (ev.value >= ai.minimum + span * 3 / 4)
                                        snprintf(line, sizeof line, "%s TRIG %s (axis)  press", evTag, nm);
                                    else if (ev.value <= ai.minimum + span / 4)
                                        snprintf(line, sizeof line, "%s TRIG %s (axis)  release", evTag, nm);
                                }
                            }
                            if (line[0] && (m_inputTestLog.empty() || m_inputTestLog.back() != line)) {
                                m_inputTestLog.push_back(line);
                                if (m_inputTestLog.size() > 16) m_inputTestLog.erase(m_inputTestLog.begin());
                                m_dirty = true;
                            }
                        }

                        if (state.currentMenuLevel == MENU_JOY_TESTER &&
                            m_evfd[i].vid == m_joyTesterVid && m_evfd[i].pid == m_joyTesterPid) {
                            if (ev.type == EV_KEY && ev.value != 2) {
                                if (ev.value) m_joyPressed.insert(ev.code);
                                else          m_joyPressed.erase(ev.code);
                                m_dirty = true;
                            } else if (ev.type == EV_ABS && ev.code == ABS_HAT0X) { m_joyHatX = ev.value; m_dirty = true; }
                            else if (ev.type == EV_ABS && ev.code == ABS_HAT0Y) { m_joyHatY = ev.value; m_dirty = true; }
                            int selc = JsMapper.buttonCode(10), stac = JsMapper.buttonCode(11);
                            bool both = selc && stac && m_joyPressed.count(selc) && m_joyPressed.count(stac);
                            if (both && !m_joyEscSince) m_joyEscSince = SDL_GetTicks();
                            if (!both) m_joyEscSince = 0;
                        }

                        if (ev.type == EV_SYN) continue;
                        if (ev.type == EV_MSC) continue;

                        // the MiSTer mirror doesn't forward Select/Start so grab those from the physical node
                        if (m_evfd[i].ignoreForInput) {
                            if (ev.type == EV_KEY && ev.value == 1) {

                                int sel = m_evfd[i].selectCode ? m_evfd[i].selectCode : BTN_SELECT;
                                int sta = m_evfd[i].startCode  ? m_evfd[i].startCode  : BTN_START;
                                if      (ev.code == sel) handleCommand(CMD_OPTIONS);
                                else if (ev.code == sta) handleCommand(CMD_SYS_SETTINGS);
                            }
                            continue;
                        }

                        // Sony pad: hat only, except while a capture is armed
                        if (m_evfd[i].vid == 0x054c && ev.type == EV_ABS &&
                            !(state.currentMenuLevel == MENU_CONTROLS_JOY1 && joystickKeyState == 1)){
                            if (!(ev.code == ABS_HAT0X || ev.code == ABS_HAT0Y))
                                continue;
                        }

                        // 0=release (keyboard arrow tracking), 1=press
                        if (((ev.value == 0 || ev.value == 1) && ev.type == EV_KEY) ||
                            (ev.type == EV_ABS && (ev.code == ABS_HAT0X || ev.code == ABS_HAT0Y)) ||
                            ( (m_evfd[i].vid == 0x0ca3 || m_evfd[i].vid == 0x2dc8 || m_evfd[i].vid == 0x045e) && ev.type == EV_ABS && (ev.code == ABS_X || ev.code == ABS_Y)) ||
                            (ev.type == EV_ABS && (ev.code == m_evfd[i].mapAx1X || ev.code == m_evfd[i].mapAx1Y)) ||
                            (ev.type == EV_ABS && state.currentMenuLevel == MENU_CONTROLS_JOY1 && joystickKeyState == 1) ){

                            // no logging here, it fires per event and each line fflushes to the SD
                            bool isHander = process_evdev_event(&m_evfd[i], &ev);
                            if (isHander)
                                isHandlerInput = isHander;
                        }

                        continue;
                    }

                    if (n == -1) {

                        if (errno == EAGAIN)
                            break;

                        printf("device %s disconnected, closing fd\n", m_evfd[i].path);
                        close(m_evfd[i].fd);
                        m_evfd[i].fd = -1;
                        m_evfd[i].isOpen = 0;
                        break;
                    }

                    break;
                }
            }
        }

        unsigned int sdlNow = SDL_GetTicks();

        for (int i = 0; i < m_evCount; i++) {

            if (!m_evfd[i].isOpen || m_evfd[i].fd < 0)
                continue;

            EvDev* d = &m_evfd[i];

            // force-release a kernel-stuck d-pad after ~5s of dead-repeat
            if (d->hat_x == 0 && d->hat_y == 0) { d->repeatRun = 0; d->stuckLogged = false; }
            else {
                bool due = (d->hat_x == -1 && sdlNow >= d->next_repeat_left)  ||
                           (d->hat_x ==  1 && sdlNow >= d->next_repeat_right) ||
                           (d->hat_y == -1 && sdlNow >= d->next_repeat_up)    ||
                           (d->hat_y ==  1 && sdlNow >= d->next_repeat_down);
                if (due) {
                    d->repeatRun++;
                    if (d->repeatRun == 20 && !d->stuckLogged) { d->stuckLogged = true; logStuckDpad(d); }
                    if (d->repeatRun >= 100) {
                        logStuckDpad(d);
                        d->hat_x = 0; d->hat_y = 0; d->repeat_ready_down = false; d->repeatRun = 0; d->kbHeld = 0;
                    }
                }
            }

            if (d->hat_x == -1 && sdlNow >= d->next_repeat_left) {
                if (!dpadActuallyHeld(d, true)) { d->hat_x = 0; d->kbHeld &= ~0xCu; }
                else {
                    Transevdev2SDLevent(MY_KEY_LEFT);
                    d->next_repeat_left = sdlNow + REPEAT_INTERVAL;
                    isHandlerInput = true;
                }
            }

            if (d->hat_x == 1 && sdlNow >= d->next_repeat_right) {
                if (!dpadActuallyHeld(d, true)) { d->hat_x = 0; d->kbHeld &= ~0xCu; }
                else {
                    Transevdev2SDLevent(MY_KEY_RIGHT);
                    d->next_repeat_right = sdlNow + REPEAT_INTERVAL;
                    isHandlerInput = true;
                }
            }

            if (d->hat_y == -1 && sdlNow >= d->next_repeat_up) {
                if (!dpadActuallyHeld(d, false)) { d->hat_y = 0; d->kbHeld &= ~0x3u; }
                else {
                    Transevdev2SDLevent(MY_KEY_UP);
                    d->next_repeat_up = sdlNow + REPEAT_INTERVAL;
                    isHandlerInput = true;
                }
            }

            if (d->hat_y == 1) {

                if (!d->repeat_ready_down) {
                    if (sdlNow >= d->next_repeat_down) {
                        if (!dpadActuallyHeld(d, false)) { d->hat_y = 0; d->repeat_ready_down = false; d->kbHeld &= ~0x3u; }
                        else {
                            d->repeat_ready_down = true;
                            d->next_repeat_down = sdlNow + REPEAT_INTERVAL;
                        }
                    }
                } else {
                    if (sdlNow >= d->next_repeat_down) {
                        if (!dpadActuallyHeld(d, false)) { d->hat_y = 0; d->repeat_ready_down = false; d->kbHeld &= ~0x3u; }
                        else {
                            Transevdev2SDLevent(MY_KEY_DOWN);
                            isHandlerInput = true;
                            d->next_repeat_down = sdlNow + REPEAT_INTERVAL;
                        }
                    }
                }
            }

        }

        captureEscapeTick();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    isRunning = false;
                    break;
                case SDL_KEYDOWN:
                case SDL_JOYAXISMOTION:
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYHATMOTION:
                    {
#ifndef MACOS
                        // device: evdev owns all input, drop SDL's duplicate to avoid double-fires
                        break;
#endif
                        if (isHandlerInput)
                            break;
                        isButtonHeld = true;
                        lastHeldEvent = event;
                        repeatStartTime = SDL_GetTicks() + 500;
                        repeatInterval = 100;
                        ControlMap key = controlMapping.convertCommand(event);
                        handleCommand(key);
                    }
                    break;
                case SDL_JOYBUTTONUP:
                case SDL_KEYUP:
                    isButtonHeld = false;
                    break;
            }
        }

        if (isHandlerInput) m_dirty = true;

#ifdef MACOS

        {
            // mac: $CONSOLEMODE_KEYS feeds scripted commands for headless screenshots
            static const char* navKeys = getenv("CONSOLEMODE_KEYS");
            static size_t navIdx = 0;
            static int    navTick = 0;
            if (navKeys && navKeys[navIdx]) {
                if (navTick++ % 10 == 0) {
                    ControlMap cmd = CMD_NONE;
                    switch (navKeys[navIdx]) {
                        case 'u': cmd = CMD_UP;      break;
                        case 'd': cmd = CMD_DOWN;    break;
                        case 'l': cmd = CMD_LEFT;    break;
                        case 'r': cmd = CMD_RIGHT;   break;
                        case 'e': cmd = CMD_ENTER;   break;
                        case 'b': cmd = CMD_BACK;    break;
                        case 'o': cmd = CMD_OPTIONS; break;
                        case 'x': cmd = CMD_X;       break;
                        case 'y': cmd = CMD_Y;       break;
                        case 's': cmd = CMD_SYS_SETTINGS; break;
                        default:  break;
                    }
                    navIdx++;
                    if (cmd != CMD_NONE) { handleCommand(cmd); m_dirty = true; }
                }
            } else if (navKeys) {
                macConsumedNav = true;
            }
        }
#endif

        if (SDL_GetTicks() - fpsTimer >= 1000) {
            fps = frameCount;
            frameCount = 0;
            fpsTimer = SDL_GetTicks();

#ifdef __linux__

            // opt-in: 1Hz kernel key-state vs our latch, to catch stuck/grabbed devices
            if (isLoggingEnabled()) { static int snapN = 0;
              if (snapN < 180) { snapN++;
                std::string sp = std::string(getHomePath()) + "/ConsoleMode/Logs/input_evtrace.log";
                FILE* sf = fopen(sp.c_str(), "a");
                if (sf) {
                    for (int s = 0; s < m_evCount; s++) {
                        EvDev* dd = &m_evfd[s];
                        if (!dd->isOpen || dd->fd < 0) continue;
                        unsigned char kb[KEY_MAX/8 + 1]; memset(kb, 0, sizeof(kb));
                        if (ioctl(dd->fd, EVIOCGKEY(sizeof(kb)), kb) < 0) continue;
                        int u=(kb[KEY_UP/8]>>(KEY_UP%8))&1,    dn=(kb[KEY_DOWN/8]>>(KEY_DOWN%8))&1;
                        int l=(kb[KEY_LEFT/8]>>(KEY_LEFT%8))&1, r=(kb[KEY_RIGHT/8]>>(KEY_RIGHT%8))&1;
                        if (!(u||dn||l||r) && dd->hat_x==0 && dd->hat_y==0 && !dd->isKeyboard) continue;
                        fprintf(sf, "%u SNAP %s kern[U%d D%d L%d R%d] hat_x=%d hat_y=%d kbHeld=%u kbd=%d\n",
                                SDL_GetTicks(), dd->path, u, dn, l, r, dd->hat_x, dd->hat_y, dd->kbHeld, dd->isKeyboard?1:0);
                    }
                    fclose(sf);
                }
              }
            }
#endif

            if (cfg.getBool(Configuration::SHOW_FPS) && perfRenderedFrames > 0) {
                char pbuf[192];
                snprintf(pbuf, sizeof(pbuf),
                    "fps=%d rendered=%d/s | render avg=%lldus max=%ldus | clear avg=%ldus | present avg=%lldus max=%ldus | thumbCache=%zu textCache=%zu",
                    fps, perfRenderedFrames,
                    perfRenderSumUs / perfRenderedFrames, perfRenderMaxUs,
                    renderComponent.clearAvgUs(),
                    perfPresentSumUs / perfRenderedFrames, perfPresentMaxUs,
                    renderComponent.thumbCacheSize(), renderComponent.textCacheSize());
                logMessage(INFO, "PERF", pbuf);
            }
            perfRenderSumUs = perfPresentSumUs = 0;
            perfRenderMaxUs = perfPresentMaxUs = 0;
            perfRenderedFrames = 0;

            int now = count_event_devices();
            if (now != last_count) {
                last_count = now;
                openInputDev();
            }
        }

#ifdef HAS_PS1

        {
            static Uint32 s_dockPollStart = 0, s_lastDockPoll = 0;
            Uint32 nowMs = SDL_GetTicks();
            if (s_dockPollStart == 0) s_dockPollStart = nowMs;
            if (nowMs - s_dockPollStart <= 4000 && nowMs - s_lastDockPoll >= 250) {
                s_lastDockPoll = nowMs;
                m_ps1.pollDock();
            }
        }
#endif

        {
            // footer clock ticks once a minute even when idle
            time_t t = time(NULL);
            struct tm lt;
            localtime_r(&t, &lt);
            if (lt.tm_min != lastClockMin) { lastClockMin = lt.tm_min; m_dirty = true; renderComponent.forceFullRedraw(); }
        }

        bool thumbDrew = renderComponent.pollThumbResult();

        if (state.currentMenuLevel != prevDrawMenu) {
            renderComponent.forceFullRedraw();
            prevDrawMenu = state.currentMenuLevel;
        }
        if (tipStartTime > 0 ||
#ifdef HAS_PS1
            state.currentMenuLevel == MENU_CD ||
#endif
            state.currentMenuLevel == MENU_CD_TESTER ||
            state.currentMenuLevel == MENU_BLUETOOTH ||
            state.currentMenuLevel == MENU_WIFI ||
            (SDL_GetTicks() - runStartMs) < 5000)
            renderComponent.forceFullRedraw();

        bool needRender = m_dirty || tipStartTime > 0 ||
#ifdef HAS_PS1
                          state.currentMenuLevel == MENU_CD ||
#endif
                          state.currentMenuLevel == MENU_CD_TESTER ||
                          state.currentMenuLevel == MENU_BLUETOOTH ||
                          state.currentMenuLevel == MENU_WIFI ||
                          state.currentMenuLevel == MENU_JOY_TESTER ||
                          (SDL_GetTicks() - runStartMs) < 5000;
        bool bgAdopted = renderComponent.takeBgAdopted();   // finished BG decode forces one full repaint
        if (needRender || renderComponent.needsContinuousRedraw() || bgAdopted) {
            struct timespec _t0, _t1, _t2;
            clock_gettime(CLOCK_MONOTONIC, &_t0);
            drawCurrentState();
            clock_gettime(CLOCK_MONOTONIC, &_t1);
            renderComponent.printFPS(fps);
            renderComponent.update();
            clock_gettime(CLOCK_MONOTONIC, &_t2);
            long _rUs = (_t1.tv_sec - _t0.tv_sec) * 1000000L + (_t1.tv_nsec - _t0.tv_nsec) / 1000;
            long _pUs = (_t2.tv_sec - _t1.tv_sec) * 1000000L + (_t2.tv_nsec - _t1.tv_nsec) / 1000;
            perfRenderSumUs  += _rUs;
            perfPresentSumUs += _pUs;
            if (_rUs > perfRenderMaxUs)  perfRenderMaxUs  = _rUs;
            if (_pUs > perfPresentMaxUs) perfPresentMaxUs = _pUs;
            perfRenderedFrames++;
            frameCount++;

            // after 100 rendered frames the new binary is good, disarm the rollback
            { static long s_healthFrames = 0;
              if (++s_healthFrames == 100) cmupdate::updateHealthy(); }
            m_dirty = false;
#ifdef MACOS

            // mac: $CONSOLEMODE_SHOT writes a BMP then exits
            { static const char* shot = getenv("CONSOLEMODE_SHOT");
              static const char* keys = getenv("CONSOLEMODE_KEYS");
              static int doneAt = -1;
              static int shotFrames = 0;
              shotFrames++;
              if (keys && macConsumedNav && doneAt < 0) {
                  int d = 3; const char* de = getenv("CONSOLEMODE_SHOT_DELAY"); if (de && atoi(de) > 0) d = atoi(de);
                  doneAt = shotFrames + d;
              }
              bool ready = keys ? (doneAt >= 0 && shotFrames >= doneAt)
                                : (shotFrames >= 3);
              if (shot && !ready) m_dirty = true;

              if (shot && ready) { renderComponent.saveScreenshot(shot); isRunning = false; } }
#endif
        } else if (thumbDrew) {
            renderComponent.update();
        }

        // boot instrumentation: one-shot milestones + 500ms samples for the first 30s
        if (bootTimingActive()) {
            static bool s_firstFrame = false, s_decodeDone = false, s_sawPending = false;
            static int  s_prevDev = -1;
            static Uint32 s_lastSample = 0;
            Uint32 nowT = SDL_GetTicks();
            if (!s_firstFrame && perfRenderedFrames >= 1) {
                s_firstFrame = true; bootMark("first-frame (menu visible)");
            }
            if ((nowT - runStartMs) < 30000 && nowT - s_lastSample >= 500) {
                s_lastSample = nowT;
                size_t pend = renderComponent.thumbWorkPending();
                int dev = count_event_devices();
                if (pend > 0) s_sawPending = true;
                if (!s_decodeDone && s_sawPending && pend == 0) {
                    s_decodeDone = true; bootMark("decode-done (icon cache warm)");
                }
                if (dev != s_prevDev) {
                    s_prevDev = dev; bootMark("input-devices=" + std::to_string(dev));
                }
                bootMark("sample q=" + std::to_string(pend) + " dev=" + std::to_string(dev)
                         + " frames=" + std::to_string(perfRenderedFrames));
            }
        }

        // held-scroll needs a 2ms floor so key-repeats aren't quantised to the frame
        bool fastPace = held || tipStartTime > 0;
        Uint32 pace = fastPace ? 2 : frameDelay;
        Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < pace)
            SDL_Delay(pace - elapsed);
    }

    for(int i = 0 ; i < m_evCount; i++){
        if(m_evfd[i].fd >= 0 && m_evfd[i].isOpen == 1 ){
            close(m_evfd[i].fd);
        }
    }
}

void Application::launchRom() {

    Rom* selectedRom = getCurrentSelectedRom();
    if (!selectedRom)
        return;

    std::string romName = resolveLaunchTitleForRom(*selectedRom);
    std::string romPath = resolveLaunchPathForRom(*selectedRom);
    if (romPath.empty()) {
        SetTip("No games found.");
        return;
    }

    std::string folderName = menu.getSections()[state.currentSectionIndex].getFolders()[state.currentFolderIndex].getTitle();

    state.resumePath = romPath;
    cfg.saveState(state);

    romName = renderComponent.getAlias(romName);
    addGame2List(Configuration::HISTORY,romName,romPath);

    logMessage(INFO,"launchRom",romPath.c_str());

    std::string launchPath = romPath;
    appendSelectedCoreToLaunchPath(launchPath, folderName);
    logMessage(INFO,"MiSTer_RR_cmd final",launchPath.c_str());

    if (!launchMiSTerRR(launchPath)) {
        logMessage(ERROR,"launchRom","launchMiSTerRR false");
    } else {
        logMessage(INFO,"launchRom","launchMiSTerRR successfully");
    }

    SDL_Quit();
    exit(EXIT_CONSOLE_RUN_GAME);
}

void Application::settingsChanged(const std::string& key, const std::string& value) {
    if (key == Configuration::LANGUAGE) {
        languages.setLang(value);
        notifyLanguageChange();

    } else if (key == Configuration::CORE_OVERRIDE) {
        MenuCache menuCache;

        std::string cacheFilePath = globalCachePath();

        if (menuCache.cacheExists(cacheFilePath)) {
            allCachedItems = menuCache.loadFromCache(cacheFilePath);
        }
    }
    else if (key == Configuration::BG_COLOR) {
        // bg color affects the whole screen so force a full redraw
        renderComponent.setBgColor(value);
        appSettings.currentValue = value;

        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::SEL_COLOR) {
        renderComponent.setSelColor(value);
        appSettings.currentValue = value;
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::GAME_STYLE) {

        renderComponent.setGameStyle(value);
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::BUTTON_STYLE) {

        renderComponent.setButtonStyle(value);
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::CLOCK_FORMAT) {
        renderComponent.setClock12(value == "12-Hour");
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::BT_RECONNECT || key == Configuration::BT_REPAIR) {

        // cfg still holds the old value here so take the changed key from the notification
        std::string rec = (key == Configuration::BT_RECONNECT) ? value : cfg.get(Configuration::BT_RECONNECT);
        std::string rep = (key == Configuration::BT_REPAIR)    ? value : cfg.get(Configuration::BT_REPAIR);
        if (rec.empty()) rec = "Balanced";
        if (rep.empty()) rep = "Auto-Accept";
        writeBtConfig(rec, rep);
    }
    else if (key == Configuration::CRT_OVERSCAN) {

        // overscan is a layout inset, the theme has to re-lay out
        if (m_fontsReady) {
            theme.setResolution(cfg.getInt(Configuration::SCREEN_WIDTH), cfg.getInt(Configuration::SCREEN_HEIGHT));
            renderComponent.loadFonts();
            renderComponent.forceFullRedraw();
            m_dirty = true;
        }
    }
    else if (key == Configuration::CRT_FONT) {

        // the ctor notify fires before renderComponent exists so guard on m_fontsReady, not a member
        if (m_fontsReady) {
            renderComponent.loadFonts();
            renderComponent.forceFullRedraw();
            m_dirty = true;
        }
    }
    else if (key == Configuration::PERF_MODE) {

        renderComponent.setTextOnly(value == "Text Only");
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::TITLE_FILTER) {

        renderComponent.setTitleFilter(value);
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
    else if (key == Configuration::QUIT) {
        if(value != "INTERNAL") {
            SDL_Quit();
            flushConsoleTtyInput();   // no type-ahead on the login prompt
            exit(EXIT_CONSOLE_QUIT);
        }
    }

    // rewrite only on a real change (observers fire ~40x at boot), and never persist the computed MISTER_INI_RES row
    if (key != Configuration::MISTER_INI_RES) {
        bool cfgChanged = (cfg.get(key) != value);
        cfg.set(key, value);
        if (cfgChanged) cfg.saveConfigIni();
    }

    if (key == Configuration::HISTORY_SIZE) {

        // trim the visible history now, don't wait for the next launch
        int histCap = atoi(value.c_str());
        if (histCap < 5 || histCap > 100) histCap = 20;
        bool trimmed = false;
        while ((int)vecHistoryFile.size() > histCap) {
            vecHistoryFile.erase(vecHistoryFile.begin());
            trimmed = true;
        }
        if (trimmed) {
            std::vector<std::string> list;
            for (auto item : vecHistoryFile)
                list.push_back(item.getPath());
            cfg.setHistoryOrFavoList(Configuration::HISTORY, list);
            cfg.saveGameListIni();
            renderComponent.forceFullRedraw();
            m_dirty = true;
        }
    }

    if (key == Configuration::SHOW_HOMEBREW) {
        InitMainMenu();
    }

    if (key == Configuration::SHOW_RESUME) {   // add/remove the Resume Game entry
        InitMainMenu();
        renderComponent.forceFullRedraw();
        m_dirty = true;
    }
}

void Application::attach(ILanguageObserver *observer) {
    langObservers.push_back(observer);
}

void Application::detach(ILanguageObserver *observer) {
    langObservers.erase(std::remove(langObservers.begin(),
                                    langObservers.end(),
                                    observer),
                        langObservers.end());
}

void Application::notifyLanguageChange() {
    for (ILanguageObserver *observer : langObservers) {
        observer->languageChanged();
    }
}

std::string Application::getName() {
    return "Application::" + std::to_string((unsigned long long)(void**)this);
}
