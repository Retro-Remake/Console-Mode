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
#include <unistd.h>
#include <cstring>
#include <sys/resource.h>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#endif
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
    if (FW != PH || FH != PW) return;
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

    m_listArtIsLogo = false;
    if (menu == MENU_FOLDER && currentRomIndex >= 0 && currentRomIndex < (int)romData.size())
        m_listArtIsLogo = romData[currentRomIndex].second.rfind("syslogo:", 0) == 0;

    {
        int itemsPerPage = theme.getIntValue(Configuration::ITEMS) == 0 ? 10 : theme.getIntValue(Configuration::ITEMS);
        bool samePage = m_prevRomSel >= 0 &&
                        listStartIndex(m_prevRomSel, itemsPerPage, (int)romData.size())
                            == listStartIndex(currentRomIndex, itemsPerPage, (int)romData.size());
        if (!m_forceFull && !romData.empty()
            && folderName == m_prevRomFolder
            && romData.size() == m_prevRomCount
            && samePage) {
            int startX = theme.getIntValue(Configuration::GAME_LIST_X);
            int baseY  = theme.getIntValue(Configuration::GAME_LIST_Y);
            int stepY  = theme.getIntValue(Configuration::ITEMS_SEPARATION);
            int startIndex = listStartIndex(currentRomIndex, itemsPerPage, (int)romData.size());
            int clipWidth = theme.getIntValue("GENERAL.game_list_w");

            if (currentRomIndex != m_prevRomSel) {

                int oldY = baseY + (m_prevRomSel  - startIndex) * stepY;
                int newY = baseY + (currentRomIndex - startIndex) * stepY;
                std::string oldAlias = disp(romData[m_prevRomSel].first);
                std::string newAlias = disp(romData[currentRomIndex].first);
                {
                    drawRomRowUnselected(cachedText(font, oldAlias, theme.getColor(Configuration::ITEMS_FONT_COLOR)),
                                         startX, oldY, clipWidth);
                    scrollPixelPosition = 0;                 // restart marquee
                    selectTime = SDL_GetTicks();
                    drawRomRowSelected(cachedText(font, newAlias, theme.getColor(Configuration::SEL_ITEM_FONT_COLOR)),
                                       startX, newY, clipWidth);
                }
                if (!m_textOnly) {    // Performance Mode: no art
                    SDL_Surface* shown = thumbnail;
                    requestThumbnail(romData[currentRomIndex].second, exactArt);
                    // the box already shows `shown` until the new art arrives
                    if (thumbnail != shown) drawListArt(thumbnail, m_listArtIsLogo);
                }
                drawListPlaytime(romData[currentRomIndex].second, menu);
                m_prevRomSel    = currentRomIndex;
                m_settlePending = true;                  // refresh art/footer once scrolling stops
                m_needsRedraw   = true;
                return;
            } else if (!m_settlePending) {
                int rowY = baseY + (currentRomIndex - startIndex) * stepY;
                drawRomRowSelected(cachedText(font, disp(romData[currentRomIndex].first),
                                   theme.getColor(Configuration::SEL_ITEM_FONT_COLOR)),
                                   startX, rowY, clipWidth);
                return;
            }

            m_settlePending = false;
            if (refreshPageInfo(std::to_string(currentRomIndex + 1) + " / " + std::to_string(romData.size()),
                                folderName, menu))
                return;
        } else if (!m_forceFull && !romData.empty() && m_prevRomSel >= 0
                   && folderName == m_prevRomFolder && romData.size() == m_prevRomCount) {
            int startX = theme.getIntValue(Configuration::GAME_LIST_X);
            int baseY  = theme.getIntValue(Configuration::GAME_LIST_Y);
            int stepY  = theme.getIntValue(Configuration::ITEMS_SEPARATION);
            int clipWidth = theme.getIntValue("GENERAL.game_list_w");
            int startIndex = listStartIndex(currentRomIndex, itemsPerPage, (int)romData.size());
            const bool stars = menu == MENU_ROM || menu == MENU_SEARCH_RESULTS;
            const int th = font ? TTF_FontHeight(font) : 0;
            const int rr = std::max(6, th * 3 / 10);
            for (int s = 0; s < itemsPerPage; s++) {
                const int i = startIndex + s;
                const int y = baseY + s * stepY;
                if (stars) {
                    SDL_Rect star = {(Sint16)(startX - 12 - 2 * rr - 3), (Sint16)(y + th / 2 - rr - 3),
                                     (Uint16)(2 * rr + 6), (Uint16)(2 * rr + 6)};
                    SDL_FillRect(screen, &star, bgFillColor());
                    addDamage(star);
                }
                if (i >= (int)romData.size()) {
                    drawRomRowUnselected(nullptr, startX, y, clipWidth);
                    continue;
                }
                const bool sel = i == currentRomIndex;
                SDL_Surface* text = cachedText(font, disp(romData[i].first),
                                               theme.getColor(sel ? Configuration::SEL_ITEM_FONT_COLOR : Configuration::ITEMS_FONT_COLOR));
                if (sel) { scrollPixelPosition = 0; selectTime = SDL_GetTicks(); drawRomRowSelected(text, startX, y, clipWidth); }
                else     drawRomRowUnselected(text, startX, y, clipWidth);
                if (stars && text && isFav(romData[i].second)) {
                    int cx = startX - 12 - rr;
                    if (cx - rr > 2) drawFavStar(cx, y + text->h / 2, rr, false);
                }
            }
            if (!m_textOnly) {
                SDL_Surface* shown = thumbnail;
                requestThumbnail(romData[currentRomIndex].second, exactArt);
                if (thumbnail != shown) drawListArt(thumbnail, m_listArtIsLogo);
            }
            drawListPlaytime(romData[currentRomIndex].second, menu);
            m_prevRomSel = currentRomIndex;
            m_settlePending = false;
            if (refreshPageInfo(std::to_string(currentRomIndex + 1) + " / " + std::to_string(romData.size()),
                                folderName, menu))
                return;
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

    int startIndex = listStartIndex(currentRomIndex, itemsPerPage, (int)romData.size());
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

        if ((menu == MENU_ROM || menu == MENU_SEARCH_RESULTS) && isFav(romData[i].second)) {
            int rr = std::max(6, textSurface->h * 3 / 10);
            int cx = startX - 12 - rr;
            if (cx - rr > 2) drawFavStar(cx, startY + textSurface->h / 2, rr, false);
        }

        startY += stepY;
    }

    if (!m_textOnly) {
        requestThumbnail(romData[currentRomIndex].second, exactArt);
        drawListArt(thumbnail, m_listArtIsLogo);   // card behind baked wordmarks only
    }
    drawListPlaytime(romData[currentRomIndex].second, menu);

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

    drawCommonInfo("", settingsTitle);
}

void RenderComponent::printFPS(int fps) {
    if(cfg.getBool(Configuration::SHOW_FPS)) {

        std::string fpsText = "FPS: " + std::to_string(fps);

        SDL_Color color = theme.getColor(Configuration::ITEMS_FONT_COLOR);

        int topy = theme.getIntValue(Configuration::TOP_Y);

        int w = 0, h = 0;
        TTF_SizeUTF8(headerFont, fpsText.c_str(), &w, &h);
        // fixed max width so a shrinking number leaves no old digits
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

    std::string noExt = base;
    size_t dot = romExtDotPos(noExt);   // strip a real extension only not "G. Darius"'s period
    if (dot != std::string::npos) noExt.erase(dot);
    auto it = aliasMap.find(noExt);
    // romset names carry (...) junk
    if (it != aliasMap.end()) return applyTitleFilter(it->second);
    auto cn = coreNameMap.find(noExt);
    if (cn != coreNameMap.end()) return cn->second;
    // keep the datecode visible
    size_t us = noExt.find_last_of('_');
    if (us != std::string::npos && noExt.size() - us == 9 &&
        noExt.find_first_not_of("0123456789", us + 1) == std::string::npos) {
        auto cd = coreNameMap.find(noExt.substr(0, us));
        if (cd != coreNameMap.end())
            return cd->second + "  " + noExt.substr(us + 1, 4) + "-"
                 + noExt.substr(us + 5, 2) + "-" + noExt.substr(us + 7, 2);
    }

    return applyTitleFilter(base);
}

static bool waitForVsync() {
#if defined(__linux__) && defined(FBIO_WAITFORVSYNC)
    static int fd = -2;
    if (fd == -2) fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fd < 0) return false;
    __u32 arg = 0;
    if (ioctl(fd, FBIO_WAITFORVSYNC, &arg) == 0) return true;
    close(fd);
    fd = -1;
    bootMark("present: vsync not answered");
#endif
    return false;
}

void RenderComponent::flushSpans() {
    if (m_spans.empty()) return;
    if (!(m_fbFront->flags & SDL_HWSURFACE)) SDL_UpdateRects(m_fbFront, (int)m_spans.size(), m_spans.data());
    m_spans.clear();
}

void RenderComponent::presentDamage(bool all) {
    m_clipped.clear();
    if (all) m_clipped.push_back({0, 0, (Uint16)screen->w, (Uint16)screen->h});
    else for (const SDL_Rect& r : m_damage) {
        const int x0 = std::max(0, (int)r.x), y0 = std::max(0, (int)r.y);
        const int x1 = std::min(screen->w, (int)r.x + (int)r.w), y1 = std::min(screen->h, (int)r.y + (int)r.h);
        if (x0 < x1 && y0 < y1) m_clipped.push_back({(Sint16)x0, (Sint16)y0, (Uint16)(x1 - x0), (Uint16)(y1 - y0)});
    }
    if (m_clipped.empty()) return;
    int ymin = screen->h, ymax = 0, xmin = screen->w, xmax = 0;
    for (const SDL_Rect& r : m_clipped) {
        ymin = std::min(ymin, (int)r.y); ymax = std::max(ymax, r.y + r.h);
        xmin = std::min(xmin, (int)r.x); xmax = std::max(xmax, r.x + r.w);
    }
    for (int y = ymin; y < ymax; y++) {
        m_runs.clear();
        for (const SDL_Rect& r : m_clipped)
            if (y >= r.y && y < r.y + r.h) m_runs.push_back({r.x, r.x + r.w});
        std::sort(m_runs.begin(), m_runs.end());
        for (size_t i = 0; i < m_runs.size();) {
            int x0 = m_runs[i].first, x1 = m_runs[i].second;
            for (i++; i < m_runs.size() && m_runs[i].first <= x1; i++) x1 = std::max(x1, m_runs[i].second);
            const size_t n = (size_t)(x1 - x0) * 4;
            memcpy((Uint8*)m_fbFront->pixels + (size_t)y * m_fbFront->pitch + (size_t)x0 * 4,
                   (const Uint8*)screen->pixels + (size_t)y * screen->pitch + (size_t)x0 * 4, n);
        }
    }
    if (!(m_fbFront->flags & SDL_HWSURFACE))
        m_spans.push_back({(Sint16)xmin, (Sint16)ymin, (Uint16)(xmax - xmin), (Uint16)(ymax - ymin)});
}

void RenderComponent::update() {
    if (!screen) return;
    if (m_fbFront) {
        if (!m_fullFrame && m_damage.empty()) return;
        waitForVsync();
        if (SDL_MUSTLOCK(m_fbFront)) SDL_LockSurface(m_fbFront);
        presentDamage(m_fullFrame);
        flushSpans();
        if (SDL_MUSTLOCK(m_fbFront)) SDL_UnlockSurface(m_fbFront);
    } else if (m_fbScreen) {
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
std::unordered_map<std::string, std::string> RenderComponent::coreNameMap;
// MiSTer names.txt, "CoreName: Display Name" per line
void RenderComponent::loadCoreNames(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t c = line.find(':');
        if (c == std::string::npos || c == 0) continue;
        std::string key = trimCopy(line.substr(0, c));
        std::string val = trimCopy(line.substr(c + 1));
        if (!key.empty() && !val.empty()) coreNameMap.emplace(key, val);
    }
}

const std::string* RenderComponent::coreNameLookup(const std::string& key) {
    auto it = coreNameMap.find(key);
    return (it != coreNameMap.end()) ? &it->second : nullptr;
}
// alias.txt wins
void RenderComponent::loadRomsetAliases(const std::string& xmlPath) {
    std::ifstream f(xmlPath);
    if (!f) return;
    std::stringstream ss; ss << f.rdbuf();
    const std::string x = ss.str();
    auto attr = [](const std::string& tag, const std::string& key) -> std::string {
        size_t p = tag.find(" " + key + "=\"");
        if (p == std::string::npos) return "";
        p += key.size() + 3;
        size_t e = tag.find('"', p);
        return (e == std::string::npos) ? "" : tag.substr(p, e - p);
    };
    size_t pos = 0;
    while ((pos = x.find('<', pos)) != std::string::npos) {
        size_t end = x.find('>', pos);
        if (end == std::string::npos) break;
        std::string tag = x.substr(pos, end - pos);
        pos = end + 1;
        std::string name = attr(tag, "name");
        std::string alt  = attr(tag, "altname");
        if (name.empty() || alt.empty()) continue;
        // the five standard entities
        static const std::pair<const char*, const char*> ents[] =
            {{"&amp;","&"},{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},{"&apos;","'"}};
        for (auto& e : ents) {
            size_t q;
            while ((q = alt.find(e.first)) != std::string::npos)
                alt.replace(q, strlen(e.first), e.second);
        }
        aliasMap.emplace(name, alt);
    }
}
// stem \t title
void RenderComponent::loadNameAliases(const std::string& tsvPath) {
    std::ifstream f(tsvPath);
    std::string line;
    while (std::getline(f, line)) {
        size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        aliasMap.emplace(line.substr(0, tab), line.substr(tab + 1));
    }
}

const std::string* RenderComponent::aliasLookup(const std::string& key) {
    auto it = aliasMap.find(key);
    return (it != aliasMap.end()) ? &it->second : nullptr;
}
