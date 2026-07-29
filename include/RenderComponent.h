// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <map>
#include <deque>
#include <set>
#include <unordered_set>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_rotozoom.h>
#include <iostream>
#include "Configuration.h"
#include "ILayout.h"
#include "Settings.h"

#include "Menu.h"

#include "JoystickMapper.h"
#include "crt_video.h"
#include "crashlog.h"
#include "fonts_data.h"

class RenderComponent {
#ifdef HAS_PS1

    friend class Ps1Disc;
#endif
public:
    SDL_Surface* screen;

    SDL_Surface* m_fbScreen = nullptr;
    int          m_rotation = 0;
    TTF_Font* font = nullptr;
    std::string  bgcolor;

    enum GameStyle { GS_LIST, GS_GRID, GS_FLIX, GS_SS4 };   // list / grid / coverflow / PS4-row

    GameStyle gameStyle() const { return m_textOnly ? GS_LIST : m_gameStyle; }

    void setOptionsHint(bool b) { m_optionsHint = b; }
    void setGameStyle(const std::string& v) {
        m_gridBig   = (v == "Big Grid View");
        m_gameStyle = (v == "Grid View" || m_gridBig) ? GS_GRID
                    : (v == "NX View" || v == "Flix View") ? GS_FLIX
                    : (v == "SS4") ? GS_SS4 : GS_LIST;
    }
    int gridCols() const { return m_gridBig ? 3 : 5; }
    int gridRows() const { return m_gridBig ? 2 : 3; }

    void setTextOnly(bool b) { m_textOnly = b; }
    bool textOnly() const { return m_textOnly; }

    enum ButtonStyle { BS_LETTERS, BS_LETTERS_COLOR, BS_PLAYSTATION, BS_PLAYSTATION_COLOR, BS_PLAYSTATION_GRAY };
    void setButtonStyle(const std::string& v) {
        m_buttonStyle = (v == "PlayStation Gray")  ? BS_PLAYSTATION_GRAY
                      : (v == "PlayStation Color") ? BS_PLAYSTATION_COLOR
                      : (v == "PlayStation")       ? BS_PLAYSTATION
                      : (v == "ABXY Color")        ? BS_LETTERS_COLOR
                      : BS_LETTERS;
    }
    ButtonStyle buttonStyle() const { return m_buttonStyle; }

    void setClock12(bool b) { m_clock12 = b; }
    bool clock12() const { return m_clock12; }

    // EXT strips the extension, REGION strips "(USA)"-style tags (games only)
    enum TitleFilter { TF_EXT, TF_REGION, TF_EXT_REGION };
    void setTitleFilter(const std::string& v) {
        m_titleFilter = (v == "Region") ? TF_REGION
                      : (v == "Ext. + Region") ? TF_EXT_REGION : TF_EXT;
    }
    virtual std::string applyTitleFilter(const std::string& name) const = 0;

    // ~88% of a 4:3 frame, clamped to the CRT safe area
    int menuBgWidth() const {
        int w = screenHeight * 4 * 88 / (3 * 100);
        int safe = screenWidth - 2 * theme.marginX();
        return (safe > 0 && w > safe) ? safe : w;
    }

protected:
    GameStyle m_gameStyle = GS_LIST;
    bool m_gridBig = false;
    bool m_gridColorFallback = false;
    bool m_clock12 = false;
    bool      m_textOnly  = false;
    int       m_preloadCount = 0;
    ButtonStyle m_buttonStyle = BS_LETTERS;
    bool m_optionsHint = true;
    TitleFilter m_titleFilter = TF_EXT;

    SDL_Surface* imgBack;
    SDL_Surface* imgBack2;

    TTF_Font* artFont = nullptr;
    TTF_Font* btnFont = nullptr;
    TTF_Font* glyphFont = nullptr;
    std::map<std::string, SDL_Surface*> m_psGlyphCache;   // key "A|c"/"A|m"
    TTF_Font* headerFont = nullptr;
    TTF_Font* footerFont = nullptr;

    TTF_Font* settingsFont = nullptr;
    TTF_Font* titleFont = nullptr;
    TTF_Font* gridTitleFont = nullptr;

    // optional CJK fallback loaded from SD, absent = tofu not a crash
    std::string m_cjkPath;
    bool m_cjkChecked = false;
    bool m_cjkAvail = false;
    std::map<int, TTF_Font*> m_cjkBySize;
    std::map<TTF_Font*, int> m_fontPt;

    Configuration& cfg;
    ILayout& theme;
    SDL_Surface* thumbnail = nullptr;

    std::unordered_map<std::string, SDL_Surface*> thumbCache;   // key "path@WxH", null = no art
    std::vector<std::string> thumbCacheOrder;
    std::unordered_set<std::string> m_pinnedThumbs;
    std::string m_loadedThumbKey;
    std::string m_currentSelKey;

    static const size_t THUMB_CACHE_MAX = 256;   // must exceed the pinned icon count or eviction thrashes

    static const long ART_SOURCE_MAX_BYTES = 2L * 1024 * 1024;   // reject by source file size, not dimensions

    struct ThumbReq    { std::string key, path; int w, h; bool exact; bool bg; };
    struct ThumbResult { std::string key; SDL_Surface* scaled; };
    std::vector<std::thread> m_thumbThreads;
    std::mutex              m_thumbMutex;
    std::condition_variable m_thumbCv;
    std::deque<ThumbReq>    m_thumbQueue;
    std::deque<ThumbResult> m_thumbResults;
    std::set<std::string>   m_thumbInflightKeys;
    bool m_thumbStop     = false;
    bool m_thumbStarted  = false;

    struct GridCell { SDL_Rect rect; std::string label; bool fav = false; };
    std::map<std::string, GridCell> m_gridCells;

    std::set<std::string> m_ss4VisibleKeys;
    std::string m_gridSelKey;
    SDL_Rect    m_gridSelRect = {0,0,0,0};
    std::string m_gridSelLabel;
    bool        m_gridSelFav  = false;
    bool        m_gridHasSel = false;

    TTF_Font*   m_gridTitleFont = nullptr;
    std::string m_gridTitleText;
    int         m_gridTitleX = 0, m_gridTitleY = 0, m_gridTitleW = 0;
    bool        m_gridTitleScrolls = false;
    int         m_titleScroll = 0;
    Uint32      m_titleScrollTime = 0, m_titleScrollEnd = 0;
    std::string m_titleScrollText;
    int m_gridCellW = 0, m_gridCellH = 0;

    std::unordered_map<std::string, SDL_Surface*> m_bgCache;   // key "BG|<path>"
    std::vector<std::string> m_bgCacheOrder;
    std::set<std::string> m_bgMissing;
    static const size_t BG_CACHE_MAX = 8;   // ~8 MB each at 1080p, LRU-evicted
    SDL_Surface* m_gridBg = nullptr;
    SDL_Surface* m_shotSurf = nullptr;
    std::string  m_shotSurfPath;

    std::unordered_map<std::string,std::string> m_gamelistArt;
    std::unordered_map<std::string,std::string> m_gamelistBg;
    std::mutex m_gamelistMutex;   // workers read the maps while a re-import rebuilds them
    std::string  m_gridBgPath;

    bool   m_bgSettlePending = false;
    Uint32 m_bgSettleTime = 0;
    static const Uint32 BG_SETTLE_MS = 700;

    bool   m_bgDecodePending = false;

    bool m_searchActive = false;
    bool m_searchNeedsFull = false;
    std::string m_searchQuery;
    int m_kbRow = 0, m_kbCol = 0;
    size_t m_queryCursor = 0;
    bool m_caps = false;
    bool m_textActive = false;
    bool m_kbSym = false;
    std::string m_textTitle;
    SDL_Surface* m_qrPatreon = nullptr;
    SDL_Surface* m_knifeIcon = nullptr;


    std::unordered_map<std::string, SDL_Surface*> textCache;
    std::vector<std::string> textCacheOrder;
    static const size_t TEXT_CACHE_MAX = 512;
    virtual SDL_Surface* cachedText(TTF_Font* f, const std::string& text, SDL_Color color) = 0;


    bool m_fullFrame = true;
    bool m_forceFull = true;
    std::vector<SDL_Rect> m_damage;
    void addDamage(const SDL_Rect& r) {
        if (m_fullFrame) return;
        // clamp to fb: SDL_UpdateRects doesn't clip and OOB rects corrupt the FPGA scanout
        int x = r.x, y = r.y, w = (int)r.w, h = (int)r.h;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > screenWidth)  w = screenWidth  - x;
        if (y + h > screenHeight) h = screenHeight - y;
        if (w <= 0 || h <= 0) return;
        SDL_Rect c = { (Sint16)x, (Sint16)y, (Uint16)w, (Uint16)h };
        m_damage.push_back(c);
    }

    std::string m_prevRomFolder;
    int    m_prevRomSel    = -1;
    size_t m_prevRomCount  = 0;
    bool   m_settlePending = false;
    virtual void drawRomRowSelected(SDL_Surface* textSurface, int startX, int startY, int clipWidth) = 0;
    virtual void drawRomRowUnselected(SDL_Surface* textSurface, int startX, int startY, int clipWidth) = 0;

    int    m_prevMainSel    = -1;  size_t m_prevMainCount    = 0;


    virtual void drawAppSettingRow(const Settings::SettingRow& item, bool selected,
                           int startX, int rowY, int bgWidth, uint8_t vol, bool clearStrip) = 0;

    struct ListPrev { int sel = 0; size_t count = 0; int aux = 0; int scroll = 0; };
    std::map<int, ListPrev> m_listPrev;

    virtual bool incrementalRows(int key, size_t count, int currentIndex, int itemsPerPage,
                         int baseY, int stepY,
                         const std::function<void(int idx, bool selected, int rowY)>& drawRow,
                         int aux = 0,
                         const std::function<void()>& footerCb = nullptr,
                         int lookAhead = -1) = 0;

    int listStart(int key) { return m_listPrev[key].scroll; }

    virtual void drawListScrollBar(int key, int baseY, int stepY, int itemsPerPage, int total) = 0;



    Uint32 bgFillColor() {
        if (bgcolor == RED)    return 0xcf3a1f;
        if (bgcolor == BLUE)   return 0x3d5f93;
        if (bgcolor == GREEN)  return 0x2a988d;
        if (bgcolor == YELLOW) return 0xd0a001;
        if (bgcolor == GRAY)   return 0x4e4e4e;
        return SDL_MapRGB(screen->format, 0, 0, 0);
    }

    Uint32 selColor() {

        if (selcolor == RED)    return 0xff3030;
        if (selcolor == BLUE)   return 0x33aaff;
        if (selcolor == GREEN)  return 0x33ff66;
        if (selcolor == YELLOW) return 0xffe000;
        if (selcolor == GRAY)   return 0xc8c8c8;
        return 0xFFFFFFFF;
    }
    std::string selcolor = WHITE;
    float m_ss4Aspect = 0.72f;

    float  m_ss4Pos     = 0.0f;
    float  m_ss4Target  = 0.0f;
    int    m_ss4LastSel = -1;
    Uint32 m_ss4AnimMs  = 0;
    SDL_Surface* m_ss4Scratch = nullptr;

    std::string  m_ss4UpKey;
    SDL_Surface* m_ss4Up = nullptr;

    SDL_Rect    m_ss4SlotRect{};
    std::string m_ss4SlotArt;
    int         m_ss4StateN = -1, m_ss4StateMenu = -1;

    Uint32 listCardColor() {
        if (bgcolor == RED)    return 0x7a2010;
        if (bgcolor == BLUE)   return 0x24395a;
        if (bgcolor == GREEN)  return 0x195c55;
        if (bgcolor == YELLOW) return 0x6b5300;
        if (bgcolor == GRAY)   return 0x2e2e2e;
        return 0x303030;
    }

    int scrollPixelPosition = 0;
    Uint32 scrollEndTime = 0;
    Uint32 selectTime = 0;
    static const Uint32 SCROLL_TIMEOUT = 2000;

    bool m_needsRedraw = false;

    bool m_bgAdoptRepaint = false;
public:
    bool needsContinuousRedraw() const { return m_needsRedraw; }
    void clearRedrawFlag()             { m_needsRedraw = false; }
    bool takeBgAdopted()               { bool b = m_bgAdoptRepaint; m_bgAdoptRepaint = false; return b; }
    size_t thumbCacheSize() const      { return thumbCache.size(); }
    size_t textCacheSize() const       { return textCache.size(); }



    void forceFullRedraw() {
        m_forceFull = true;

        m_gridCells.clear();
        m_gridSelKey.clear();
        m_currentSelKey.clear();
        m_ss4VisibleKeys.clear();
    }

    long clearAvgUs() { long a = m_clearCount ? m_clearUsSum / m_clearCount : 0; m_clearUsSum = 0; m_clearCount = 0; return a; }
protected:
    long m_clearUsSum = 0;
    int  m_clearCount = 0;
    static const Uint32 SCROLL_SPEED = 600;
    static const Uint32 END_SCROLL_PAUSE = 3000;

    int screenHeight;
    int screenWidth;

    int mainmenustartY;

    static std::unordered_map<std::string, std::string> aliasMap;

    int space = 0;
    int gDotCount = 0;

    void renderText(const std::string& text, Sint16 x, Sint16 y, SDL_Color color, int align = 0) {
        renderTextEx(font, text, x, y, color, align);
    }

    void renderTextEx(TTF_Font* pFont, const std::string& text, Sint16 x, Sint16 y, SDL_Color color, int align = 0) {
        SDL_Surface* textSurface = cachedText(pFont, text, color);
        if (!textSurface) return;

        SDL_Rect destRect;
        switch(align) {
            case 1:
                destRect.x = x - textSurface->w / 2;
                destRect.y = y - textSurface->h / 2;
                break;
            case 2:
                destRect.x = x - textSurface->w;
                destRect.y = y - textSurface->h / 2;
                break;
            case 0:
            default:
                destRect.x = x;
                destRect.y = y - textSurface->h / 2;
                break;
        }

        SDL_BlitSurface(textSurface, NULL, screen, &destRect);

    }

    void clearScreen() {
        struct timespec _c0; clock_gettime(CLOCK_MONOTONIC, &_c0);
        m_fullFrame = true;
        m_damage.clear();

        if (bgcolor == RED)
            SDL_FillRect(screen, nullptr, 0xcf3a1f);
        else if (bgcolor == BLUE)
            SDL_FillRect(screen, nullptr, 0x3d5f93);
        else if (bgcolor == GREEN)
            SDL_FillRect(screen, nullptr, 0x2a988d);
        else if (bgcolor == YELLOW)
            SDL_FillRect(screen, nullptr, 0xd0a001);
        else if (bgcolor == GRAY)
            SDL_FillRect(screen, nullptr, 0x4e4e4e);
        else
            SDL_FillRect(screen, nullptr, SDL_MapRGB(screen->format, 0, 0, 0));
        struct timespec _c1; clock_gettime(CLOCK_MONOTONIC, &_c1);
        m_clearUsSum += (_c1.tv_sec - _c0.tv_sec) * 1000000L + (_c1.tv_nsec - _c0.tv_nsec) / 1000;
        m_clearCount++;
    }

public:

    RenderComponent(Configuration& cfg, ILayout& theme);
    virtual ~RenderComponent();


    void resetValues() {
        selectTime = SDL_GetTicks();
        scrollPixelPosition = 0;
        scrollEndTime = 0;

        thumbnail = nullptr;
        space = 0;
        gDotCount = 0;
    }

    void loadFonts() {
        TTF_Font* olds[] = { font, artFont, btnFont, footerFont, headerFont,
                             settingsFont, titleFont, gridTitleFont, glyphFont };
        for (TTF_Font* o : olds) if (o) TTF_CloseFont(o);
        font = artFont = btnFont = footerFont = headerFont =
            settingsFont = titleFont = gridTitleFont = glyphFont = nullptr;

        for (auto& kv : m_cjkBySize) if (kv.second) TTF_CloseFont(kv.second);
        m_cjkBySize.clear();
        m_fontPt.clear();

        auto openBaked = [this](const std::string& id, int size) -> TTF_Font* {
            const unsigned char* d = nullptr; std::size_t n = 0;
            if (!fontData(id, &d, &n) || !d || !n) return nullptr;
            SDL_RWops* rw = SDL_RWFromConstMem(d, (int)n);
            TTF_Font* fp = rw ? TTF_OpenFontRW(rw, 1, size) : nullptr;
            if (fp) m_fontPt[fp] = size;
            return fp;
        };

        bool crtF = crt::active() || getenv("CONSOLEMODE_SAFE_AREA");
        std::string mode = crtF ? cfg.get(Configuration::CRT_FONT) : std::string();
        bool legible = (mode == "Legible");
        bool sharp   = crtF && !legible && mode != "Smooth";

        const char* uiFace   = legible ? "dejavusans-bold" : (sharp ? "akrobat-bold" : "akrobat-semibold");
        const char* boldFace = legible ? "dejavusans-bold" : "akrobat-bold";
        int uiHint   = legible ? TTF_HINTING_NORMAL : (sharp ? TTF_HINTING_MONO : TTF_HINTING_NORMAL);
        int listHint = legible ? TTF_HINTING_NORMAL : (sharp ? TTF_HINTING_MONO : TTF_HINTING_LIGHT);
        int boldHint = legible ? TTF_HINTING_NORMAL : (sharp ? TTF_HINTING_MONO : TTF_HINTING_NORMAL);

        const bool legScale = legible && screenWidth <= 400;
        const int  LEG_FONT_PCT = (screenWidth > 336) ? 75 : 95;   // hardware-tuned for 352 vs 320
        auto sz = [&](const char* k){ int s = theme.getIntValue(k); return legScale ? std::max(1, s * LEG_FONT_PCT / 100) : s; };

        font = openBaked(uiFace, sz("GENERAL.font_size"));
        if (font) { TTF_SetFontHinting(font, listHint); TTF_SetFontKerning(font, 1); }
        artFont = openBaked(uiFace, sz("GENERAL.art_text_font_size"));
        if (artFont) { TTF_SetFontHinting(artFont, uiHint); TTF_SetFontKerning(artFont, 1); }
        btnFont = openBaked(uiFace, sz("GENERAL.btn_font_size"));
        if (btnFont) { TTF_SetFontHinting(btnFont, uiHint); TTF_SetFontKerning(btnFont, 1); }
        footerFont = openBaked(uiFace, sz("GENERAL.footer_font_size"));
        if (footerFont) { TTF_SetFontHinting(footerFont, uiHint); TTF_SetFontKerning(footerFont, 1); }
        headerFont = openBaked(uiFace, sz("GENERAL.header_font_size"));
        if (headerFont) { TTF_SetFontHinting(headerFont, uiHint); TTF_SetFontKerning(headerFont, 1); }

        settingsFont = openBaked(boldFace, sz("GENERAL.settings_font_size"));
        titleFont     = openBaked(boldFace, sz("GENERAL.title_font_size"));
        gridTitleFont = openBaked(boldFace, sz("GENERAL.grid_title_font_size"));
        if (settingsFont) TTF_SetFontHinting(settingsFont, boldHint);
        if (titleFont)     TTF_SetFontHinting(titleFont,     boldHint);
        if (gridTitleFont) TTF_SetFontHinting(gridTitleFont, boldHint);

        int glyphFontSize = (int)(theme.getIntValue("GENERAL.btn_font_size") * 3);
        if (glyphFontSize < 8) glyphFontSize = 8;
        glyphFont = openBaked("promptfont", glyphFontSize);
        if (glyphFont) TTF_SetFontHinting(glyphFont, TTF_HINTING_LIGHT);

        for (auto& kv : textCache) if (kv.second) SDL_FreeSurface(kv.second);
        textCache.clear();
        textCacheOrder.clear();

        for (auto& kv : m_psGlyphCache) if (kv.second) SDL_FreeSurface(kv.second);
        m_psGlyphCache.clear();
        m_gridTitleFont = nullptr;
        m_gridTitleScrolls = false;
    }

    void initialize() {
        // MUST switch vmode before SDL_Init: the fbcon driver reads fb geometry at init
        std::string res = cfg.get(Configuration::SCREEN_RES);
        int wantW = 0, wantH = 0;
        std::string customVmode;
        bool explicitRes = (!res.empty() && res != "Default"
                            && sscanf(res.c_str(), "%dx%d", &wantW, &wantH) == 2
                            && wantW > 0 && wantH > 0);

        // 1440p/1536p need presets 14/13, plain vmode -r is over the scaler Fmax
        if      (res == "2560x1440") customVmode = "vmode 14";
        else if (res == "2048x1536") customVmode = "vmode 13";

        {
            const char* crtEnv = getenv("CONSOLEMODE_CRT");
            if (crtEnv && std::string(crtEnv) == "1") explicitRes = false;
        }

#if !defined(MACOS) && !defined(X86)
        if (explicitRes) {
            char vmodeCmd[96];
            if (!customVmode.empty()) snprintf(vmodeCmd, sizeof(vmodeCmd), "%s", customVmode.c_str());
            else                      snprintf(vmodeCmd, sizeof(vmodeCmd), "vmode -r %d %d rgb32", wantW, wantH);
            system(vmodeCmd);
            usleep(300000);   // let the scaler settle before SDL reads geometry
        }
#endif

        {
            int sdlTries = 0;
            while (SDL_Init(SDL_INIT_VIDEO) < 0) {
                if (++sdlTries >= 60) {   // ~6s
                    std::cerr << "SDL_Init(VIDEO) failed after retries: " << SDL_GetError() << std::endl;
                    exit(EXIT_CONSOLE_QUIT);
                }
                usleep(100000);
            }
        }

        int resW, resH;
        if (explicitRes) {
#if !defined(MACOS) && !defined(X86)

            const SDL_VideoInfo* vi = customVmode.empty() ? nullptr : SDL_GetVideoInfo();
            if (vi && vi->current_w > 0 && vi->current_h > 0) { resW = vi->current_w; resH = vi->current_h; }
            else { resW = wantW; resH = wantH; }
#else
            resW = wantW; resH = wantH;
#endif
        } else {
#ifdef MACOS

            resW = cfg.getInt(Configuration::SCREEN_WIDTH);
            resH = cfg.getInt(Configuration::SCREEN_HEIGHT);
            if (resW <= 0 || resH <= 0) { resW = 1280; resH = 720; }
#else
            const SDL_VideoInfo* vi = SDL_GetVideoInfo();
            if (vi && vi->current_w > 0 && vi->current_h > 0) { resW = vi->current_w; resH = vi->current_h; }
            else { resW = 1920; resH = 1080; }
#endif
        }

        int depth = cfg.getInt(Configuration::SCREEN_DEPTH);
        if (depth <= 0) depth = 32;

        screen = SDL_SetVideoMode(resW, resH, depth, SDL_SWSURFACE);

        for (int vmTries = 0; !screen && vmTries < 60; ++vmTries) {
            usleep(100000);
            screen = SDL_SetVideoMode(resW, resH, depth, SDL_SWSURFACE);
        }
        if (!screen) {

            std::cerr << "Video mode " << resW << "x" << resH << " failed: "
                      << SDL_GetError() << " -- falling back." << std::endl;
#if !defined(MACOS) && !defined(X86)
            system("vmode -r 1920 1080 rgb32");
            usleep(300000);
#endif
            const SDL_VideoInfo* vi = SDL_GetVideoInfo();
            int fw = (vi && vi->current_w > 0) ? vi->current_w : 1280;
            int fh = (vi && vi->current_h > 0) ? vi->current_h : 720;
            screen = SDL_SetVideoMode(fw, fh, depth, SDL_SWSURFACE);
            if (screen) { resW = fw; resH = fh; }
        }
        if (!screen) {
            std::cerr << "Unable to set video mode: " << SDL_GetError() << std::endl;
            SDL_Quit();
            exit(EXIT_CONSOLE_QUIT);
        }

        crt::init();

        // TATE 1=CW 2=CCW: draw into a portrait surface, rotate-blit to landscape fb0 in update()
        m_rotation = crt::rotation();
#if defined(MACOS) || defined(X86)

        if (const char* r = getenv("CONSOLEMODE_ROTATE")) { int rv = atoi(r); if (rv >= 0 && rv <= 3) m_rotation = rv; }
#endif
        if (screen && (m_rotation == 1 || m_rotation == 2)) {
            const SDL_PixelFormat* pf = screen->format;
            SDL_Surface* portrait = SDL_CreateRGBSurface(
                SDL_SWSURFACE, screen->h, screen->w, pf->BitsPerPixel,
                pf->Rmask, pf->Gmask, pf->Bmask, pf->Amask);
            if (portrait) {
                m_fbScreen = screen;
                screen = portrait;
            } else {
                m_rotation = 0;
            }
        }

        resW = screen->w; resH = screen->h;
        cfg.set(Configuration::SCREEN_WIDTH,  std::to_string(resW));
        cfg.set(Configuration::SCREEN_HEIGHT, std::to_string(resH));
        theme.setResolution(resW, resH);
        screenWidth  = resW;
        screenHeight = resH;

        if (TTF_Init() == -1) {
            std::cerr << "Unable to initialize TTF: " << TTF_GetError() << std::endl;
            SDL_Quit();
            exit(EXIT_CONSOLE_QUIT);
        }

        loadFonts();

#ifdef MACOS
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#else
        SDL_EnableKeyRepeat(0, 0);
#endif

        SDL_ShowCursor(SDL_DISABLE);

        std::string imgPath = cfg.get(Configuration::HOME_PATH) + "ConsoleMode/themeconfig/resources/back22.png";
        imgBack = IMG_Load(imgPath.c_str());
        if (!imgBack) {
            std::cerr << "Unable to load imgBack" << std::endl;
            imgBack = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 32, 0, 0, 0, 0);   // never NULL
        }

        int imgHeight = imgBack ? imgBack->h : 8;
        int fontSize = theme.getIntValue("GENERAL.font_size");
        double imgSize = fontSize*1.5;
        double scale = imgSize/imgHeight;

        {
            SDL_Surface* z = zoomSurface(imgBack, scale, scale, SMOOTHING_ON);
            if (z) { SDL_FreeSurface(imgBack); imgBack = z; }
        }

        imgPath = cfg.get(Configuration::HOME_PATH) + "ConsoleMode/themeconfig/resources/back22.png";
        imgBack2 = IMG_Load(imgPath.c_str());
        if (!imgBack2) {
            std::cerr << "Unable to load imgBack2" << std::endl;
            imgBack2 = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, 32, 0, 0, 0, 0);   // never NULL
        }

        fontSize = theme.getIntValue("GENERAL.btn_font_size");
        imgSize = fontSize*1.5;
        scale = imgSize/imgHeight;
        {
            SDL_Surface* z = zoomSurface(imgBack2, scale, scale, SMOOTHING_ON);
            if (z) { SDL_FreeSurface(imgBack2); imgBack2 = z; }
        }
    }


    virtual void drawCommonInfo(std::string pageInfo = "", std::string folderName = "", MenuLevel menu = MENU_NONE, bool footerOnly = false) = 0;





    bool   m_netUp = false;
    bool   m_btUp  = false;
    Uint32 m_netCheckMs = 0;














    bool searchActive() const { return m_searchActive; }
    const std::string& searchQuery() const { return m_searchQuery; }

    bool textInputActive() const { return m_textActive; }
    const std::string& textInputValue() const { return m_searchQuery; }



    void drawRomList(const std::string& folderName, const std::vector<std::pair<std::string, std::string>>& romData, int currentRomIndex,
                     MenuLevel menu = MENU_ROM, bool useAlias = true, bool exactArt = false);


    void drawAppSettings(const std::string& settingsTitle, const std::vector<Settings::SettingRow>& settingList, int currentSettingIndex, uint8_t vol);











    virtual void requestThumbnail(const std::string& romPath, bool exact = false) = 0;

    virtual void drawListArt(SDL_Surface* art, bool isLogo) = 0;
    bool m_listArtIsLogo = false;

    void setFavoritePaths(std::set<std::string> paths) { m_favPaths = std::move(paths); }
    bool isFav(const std::string& path) const { return !path.empty() && m_favPaths.count(path) > 0; }
    std::set<std::string> m_favPaths;

    virtual void drawFavStar(int cx, int cy, int r, bool badge) = 0;
    void creditsReset() { m_creditsScroll = 0; }
    void licenseListReset() { m_licSel = 0; m_licScroll = 0; m_licWrapSel = -1; }
    int  licenseSel() const { return m_licSel; }
    void licenseTextReset() { m_licScroll = 0; }
protected:
    int m_creditsScroll = 0;
    int m_licSel = 0;
    int m_licScroll = 0;
    std::vector<std::string> m_licLines;
    int m_licWrapW = 0;
    int m_licWrapSel = -1;
public:
    void printFPS(int fps);
    void loadAliases();
    std::string getAlias(const std::string& title);

    void update();

    void setSelColor(const std::string& color) { selcolor = color; }








protected:
    float m_cdVis[32] = {};
    int  m_cdPrevSel = -1, m_cdPrevPlay = -1, m_cdPrevElapsed = -1;
    bool m_cdPrevPaused = false;
    size_t m_cdPrevN = 0;
};

