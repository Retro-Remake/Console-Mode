// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Base list/settings rendering, aliases and frame present. Derived from
// simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include <string>
#include <vector>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>
#include <filesystem>
#include <cctype>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>
#include <SDL/SDL_image.h>
#include <fstream>
#include "utils.h"
#include "build_info.h"
#include "licenses_data.h"
#include "UpdateManager.h"
#include "miniz.h"
#include <sched.h>
#include <sys/resource.h>
#include "RenderComponent.h"
#include "scraper.h"
#include "Configuration.h"

static size_t romExtDotPos(const std::string& s) {
    size_t dot = s.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return std::string::npos;
    std::string ext = s.substr(dot + 1);
    if (ext.empty() || ext.size() > 4) return std::string::npos;
    bool hasAlpha = false;
    for (char c : ext) {
        if (!isalnum((unsigned char)c)) return std::string::npos;
        if (isalpha((unsigned char)c)) hasAlpha = true;
    }
    return hasAlpha ? dot : std::string::npos;   // all-digit suffix like ".98" isn't an extension
}

static void rotateBlit90(SDL_Surface* src, SDL_Surface* dst, int dir) {
    if (!src || !dst) return;
    const int PW = src->w, PH = src->h;
    const int FW = dst->w, FH = dst->h;           // FW == PH, FH == PW
    if (FW != PH || FH != PW) return;             // geometry mismatch
    if (SDL_MUSTLOCK(src) && SDL_LockSurface(src) < 0) return;
    if (SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) { if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src); return; }
    const uint8_t* sp = (const uint8_t*)src->pixels;
    uint8_t*       dp = (uint8_t*)dst->pixels;
    const int sPitch = src->pitch, dPitch = dst->pitch;
    for (int dy = 0; dy < FH; ++dy) {
        uint32_t* drow = (uint32_t*)(dp + (size_t)dy * dPitch);
        for (int dx = 0; dx < FW; ++dx) {
            int sx, sy;
            if (dir == 1) { sx = dy;          sy = PH - 1 - dx; }   // 90 CW
            else          { sx = PW - 1 - dy; sy = dx;          }   // 90 CCW
            drow[dx] = *(const uint32_t*)(sp + (size_t)sy * sPitch + (size_t)sx * 4);
        }
    }
    if (SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
    if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
}

#include "system_icons.h"
#include <algorithm>
#include "stb_image.h"
#include "stb_image_write.h"
#include <cmath>
#include "RenderComponent.h"
#include "Configuration.h"

void RenderComponent::drawRomList(const std::string& folderName, const std::vector<std::pair<std::string, std::string>>& romData, int currentRomIndex,
                                  MenuLevel menu, bool useAlias, bool exactArt) {
    // folder names pass through as-is (getAlias would strip after a '/', e.g. "TI-99/4A")
    auto disp = [&](const std::string& s) -> std::string { return useAlias ? getAlias(s) : s; };
    m_listArtIsLogo = (menu == MENU_FOLDER);   // folder logos get a backing card

    // same-page selection/scroll change: repaint only the affected rows
    {
        int itemsPerPage = theme.getIntValue(Configuration::ITEMS) == 0 ? 10 : theme.getIntValue(Configuration::ITEMS);
        bool samePage = m_prevRomSel >= 0 &&
                        (m_prevRomSel / itemsPerPage) == (currentRomIndex / itemsPerPage);
        if (!m_forceFull && !romData.empty()
            && folderName == m_prevRomFolder
            && romData.size() == m_prevRomCount
            && samePage) {
            int startX = theme.getIntValue(Configuration::GAME_LIST_X);
            int baseY  = theme.getIntValue(Configuration::GAME_LIST_Y);
            int stepY  = theme.getIntValue(Configuration::ITEMS_SEPARATION);
            int startIndex = (currentRomIndex / itemsPerPage) * itemsPerPage;
            int clipWidth = theme.getIntValue("GENERAL.game_list_w");

            if (currentRomIndex != m_prevRomSel) {
                // selection moved within page: redraw old and new rows
                int oldY = baseY + (m_prevRomSel  - startIndex) * stepY;
                int newY = baseY + (currentRomIndex - startIndex) * stepY;
                std::string oldAlias = disp(romData[m_prevRomSel].first);
                std::string newAlias = disp(romData[currentRomIndex].first);
                drawRomRowUnselected(cachedText(font, oldAlias, theme.getColor(Configuration::ITEMS_FONT_COLOR)),
                                     startX, oldY, clipWidth);
                scrollPixelPosition = 0;                 // restart marquee
                selectTime = SDL_GetTicks();
                drawRomRowSelected(cachedText(font, newAlias, theme.getColor(Configuration::SEL_ITEM_FONT_COLOR)),
                                   startX, newY, clipWidth);
                if (!m_textOnly) {    // Performance Mode: no art
                    requestThumbnail(romData[currentRomIndex].second, exactArt);
                    // redraw box so a smaller icon doesn't leave the previous showing through
                    drawListArt(thumbnail, menu == MENU_FOLDER);
                }
                m_prevRomSel    = currentRomIndex;
                m_settlePending = true;                  // refresh art/footer once scrolling stops
                m_needsRedraw   = true;
                return;
            } else if (!m_settlePending) {
                // stationary: marquee the selected row
                int rowY = baseY + (currentRomIndex - startIndex) * stepY;
                drawRomRowSelected(cachedText(font, disp(romData[currentRomIndex].first),
                                   theme.getColor(Configuration::SEL_ITEM_FONT_COLOR)),
                                   startX, rowY, clipWidth);
                return;
            }
            // settle pending: fall through to full redraw so art and footer catch up
            m_settlePending = false;
        }
    }

    clearScreen();
    m_forceFull = false;
    m_settlePending = false;
    m_prevRomFolder = folderName;
    m_prevRomSel    = currentRomIndex;
    m_prevRomCount  = romData.size();

    if (romData.size() == 0) {
        SDL_Color colorTxt = theme.getColor(Configuration::ITEMS_FONT_COLOR);

        std::string tip = "No Games found in ["+folderName+"]";
        renderText(tip,screenWidth/2,screenHeight/2,colorTxt, 1);

        drawCommonInfo("",folderName,menu);
        return;
    }

    int startX = theme.getIntValue(Configuration::GAME_LIST_X);
    int startY = theme.getIntValue(Configuration::GAME_LIST_Y);
    int stepY = theme.getIntValue(Configuration::ITEMS_SEPARATION);

    int itemsPerPage = theme.getIntValue(Configuration::ITEMS) == 0 ? 10 : theme.getIntValue(Configuration::ITEMS);

    int currentPage = currentRomIndex / itemsPerPage;
    int startIndex = currentPage * itemsPerPage;
    int endIndex = std::min<int>(startIndex + itemsPerPage, romData.size());

    SDL_Color selColor  = theme.getColor(Configuration::SEL_ITEM_FONT_COLOR);
    SDL_Color normColor = theme.getColor(Configuration::ITEMS_FONT_COLOR);
    int clipWidth = theme.getIntValue("GENERAL.game_list_w");

    for (int i = startIndex; i < endIndex; i++) {
        SDL_Color color = (i == currentRomIndex) ? selColor : normColor;
        std::string alias = disp(romData[i].first);

        SDL_Surface* textSurface = cachedText(font, alias, color);   // cached, do not free
        // TTF returns NULL for an empty string, skip the row
        if (!textSurface) {
            startY += stepY;
            continue;
        }

        if(i == currentRomIndex) {
            drawRomRowSelected(textSurface, startX, startY, clipWidth);
        } else {
            SDL_Rect clipRect = {startX, startY, clipWidth, static_cast<Uint16>(textSurface->h)}; // clip so text doesn't spill

            SDL_SetClipRect(screen, &clipRect);
            SDL_BlitSurface(textSurface, nullptr, screen, &clipRect);
        }

        SDL_SetClipRect(screen, NULL);

        // fav star sits left of the pill so row repaints never touch it
        if (menu == MENU_ROM && isFav(romData[i].second)) {
            int rr = std::max(6, textSurface->h * 3 / 10);
            int cx = startX - 12 - rr;
            if (cx - rr > 2) drawFavStar(cx, startY + textSurface->h / 2, rr, false);
        }

        startY += stepY;
    }

    // deferred + cached, never blocks scrolling (Performance Mode skips it)
    if (!m_textOnly) {
        requestThumbnail(romData[currentRomIndex].second, exactArt);
        drawListArt(thumbnail, menu == MENU_FOLDER);   // card behind system logos
    }

    std::string pageInfo = std::to_string(currentRomIndex + 1) + " / " + std::to_string(romData.size());

    drawCommonInfo(pageInfo,folderName,menu);
}

void RenderComponent::drawAppSettings(const std::string& settingsTitle, const std::vector<Settings::SettingRow>& settingList, int currentSettingIndex, uint8_t vol) {

    int bgWidth = menuBgWidth();
    int startX = (screenWidth - bgWidth)/2;
    int stepY = theme.getIntValue(Configuration::FOLDER_ITEMS_STEP) == 0 ? 50: theme.getIntValue(Configuration::FOLDER_ITEMS_STEP);
    int startY0 = mainmenustartY;
    int itemsPerPage = (screenHeight - startY0*2)/stepY;
    if (itemsPerPage <= 0) itemsPerPage = 9;

    size_t n = settingList.size();
    auto rowCb = [&](int i, bool selected, int rowY) {
        drawAppSettingRow(settingList[i], selected, startX, rowY, bgWidth, vol, false);
    };
    auto sbar = [&]{ drawListScrollBar(1, startY0, stepY, itemsPerPage, (int)n); };

    // vol changes without the cursor moving, so pass it as aux
    if (incrementalRows(1, n, currentSettingIndex, itemsPerPage, startY0, stepY, rowCb, (int)vol, sbar, 2))
        return;

    clearScreen();
    m_forceFull = false;
    int startIndex = listStart(1);
    int endIndex = std::min<int>(startIndex + itemsPerPage, (int)n);
    int rowY = startY0;
    for (int i = startIndex; i < endIndex; i++) { rowCb(i, i == currentSettingIndex, rowY); rowY += stepY; }
    sbar();

    drawCommonInfo("", settingsTitle);   // caller passes the full breadcrumb
}

void RenderComponent::printFPS(int fps) {
    if(cfg.getBool(Configuration::SHOW_FPS)) {

        std::string fpsText = "FPS: " + std::to_string(fps);

        SDL_Color color = theme.getColor(Configuration::ITEMS_FONT_COLOR);

        int topy = theme.getIntValue(Configuration::TOP_Y);

        int w = 0, h = 0;
        TTF_SizeUTF8(headerFont, fpsText.c_str(), &w, &h);
        // fixed max-width region so a shrinking number leaves no old digits behind
        int maxW = 0, mh = 0;
        TTF_SizeUTF8(headerFont, "FPS: 9999", &maxW, &mh);

        // renderTextEx centers vertically on topy, so the rect must span y-h/2
        SDL_Rect r = {(Sint16)(screenWidth/2 - maxW/2 - 4), (Sint16)(topy - h/2 - 2),
                      (Uint16)(maxW + 8), (Uint16)(h + 4)};
        SDL_FillRect(screen, &r, bgFillColor());
        renderTextEx(headerFont, fpsText, screenWidth/2-w/2, topy, color, 0);
        addDamage(r);
    }
}

void RenderComponent::loadAliases() {
    std::ifstream infile(cfg.get(Configuration::ALIAS_PATH));
    std::string line;
    while (std::getline(infile, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string filename = line.substr(0, pos);
            std::string alias = line.substr(pos + 1);
            aliasMap[filename] = alias;
        }
    }
}

std::string RenderComponent::getAlias(const std::string& title) {
    // folder rows end in '/', so don't strip ext (stem() empties and segfaults the renderer)
    if (!title.empty() && title.back() == '/') {
        std::string folderName = title.substr(0, title.size() - 1);
        auto it = aliasMap.find(folderName);
        return (it != aliasMap.end()) ? it->second : folderName;
    }

    std::string base = title;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base.erase(0, slash + 1);

    // a custom alias wins verbatim, ignoring Title Filtering
    std::string noExt = base;
    size_t dot = romExtDotPos(noExt);   // strip a real extension only not "G. Darius"'s period
    if (dot != std::string::npos) noExt.erase(dot);
    auto it = aliasMap.find(noExt);
    if (it != aliasMap.end()) return it->second;

    return applyTitleFilter(base);
}

void RenderComponent::update() {
    if (!screen) return;
    if (m_fbScreen) {
        // TATE: rotate the whole portrait surface into landscape fb0 each present
        rotateBlit90(screen, m_fbScreen, m_rotation);
        SDL_UpdateRect(m_fbScreen, 0, 0, 0, 0);
    } else if (m_fullFrame) {
        SDL_UpdateRect(screen, 0, 0, 0, 0);
    } else if (!m_damage.empty()) {
        SDL_UpdateRects(screen, (int)m_damage.size(), m_damage.data());
    }
    m_fullFrame = false;
    m_damage.clear();

    // mirror fb0 into the FPGA DDR scanout, no-op unless CONSOLEMODE_CRT=1
    crt::pushFrame();
}

std::unordered_map<std::string, std::string> RenderComponent::aliasMap;
