// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include <iostream>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <fcntl.h>
#include <unistd.h>
#include <SDL/SDL.h>

#include <boost/algorithm/string.hpp>

#include "Configuration.h"
#include "bgm.h"
#include "Languages.h"
#include "Settings.h"
#include "Exception.h"
#include "crt_video.h"
#include <unordered_map>

// English fallback labels used when languages.ini lacks a key
static std::string defaultSettingLabel(const std::string& id) {
    static const std::unordered_map<std::string,std::string> kDefaults = {
        // App settings
        {"volume","Volume"}, {"net","Network"}, {"bluetooth","Bluetooth"},
        {"bgColor","Background Color"}, {"bgArt","Background Artwork"}, {"selColor","Selection Color"}, {"gameStyle","Game Style"}, {"buttonStyle","Button Labels"}, {"clockFormat","Clock Format"}, {"bootToGames","Boot to Load Game"}, {"btReconnect","BT Reconnect"}, {"btRePair","BT Re-Pair"}, {"screenRes","Console Mode Resolution"}, {"misterIniRes","MiSTer INI Resolution"}, {"misterIniHdr","MiSTer INI HDR"}, {"misterIniScandoubler","MiSTer INI Scandoubler"}, {"crtOverscan","CRT Overscan"}, {"crtFont","CRT Font"},
        {"loadConfig","System Config"}, {"updateTime","Set Time"}, {"clearcache","Rebuild Game Database"},
        {"update","Update"}, {"updateCM","Update Console Mode"}, {"scrapeArtwork","Scrape Artwork"}, {"optimizeArtwork","Optimize Artwork"}, {"importGamelist","Import gamelist.xml"}, {"manageHidden","Manage Hidden Games"}, {"hideSections","Hide Categories"},
        {"devtools","Developer Tools"}, {"quit","Quit"}, {"inputTester","Input Tester"}, {"kbMapping","Console Mode Keyboard Mapping"}, {"cdTester","CD Tester"},
        {"abSwap","Console Mode A/B Swap"}, {"xySwap","Console Mode X/Y Swap"}, {"promptSwap","Swap Button Prompts"},
        // Developer tools
        {"showFPS","Show FPS"}, {"bootMainMenu","Boot To Main Menu"}, {"ss4Anim","SS4 Animation"}, {"useMirror","Console Mode Update Mirror"}, {"screenRefresh","CM Refresh Rate"}, {"showHomebrew","Show Homebrew"},
        {"showResume","Show Resume Game"}, {"historySize","History Size"}, {"titleFiltering","Title Filtering"}, {"controls","Controls"},
        {"lockFlixTitle","Lock NX Title"}, {"centerList","Center List Selection"}, {"bootSpeed","App Boot Speed"}, {"perfMode","Performance Mode"},
        {"autoboot","Console Mode Autoboot"}, {"loadScript","Load Script"},
#ifdef HAS_PS1
        {"discOptions","Disc Options"},
#endif
        {"bgm","Set Static BGM"}, {"bgmVolume","BGM Volume"},
        {"bgmEnabled","Enable BGM"}, {"bgmMode","Mode"},
        {"bgmMenu","Background Music"}, {"sfxMenu","System Sounds"}, {"confirmSound","Confirm Sound"}, {"backSound","Back Sound"}, {"navSound","Nav Sound"},
        // Rom settings
        {"romOverclock","ROM Overclock"}, {"romAutostart","ROM Autostart"}, {"coreOverride","Core Override"},
    };
    auto it = kDefaults.find(id);
    return it != kDefaults.end() ? it->second : id;   // unknown key -> raw id
}

void AppSettings::updateBgmVolume(bool increase) {
    int v = 70;
    try { v = std::stoi(settingsMap[Configuration::BGM_VOLUME].value); } catch (...) {}
    v += increase ? 5 : -5;
    v = v < 0 ? 0 : (v > 100 ? 100 : v);
    settingsMap[Configuration::BGM_VOLUME].value = std::to_string(v) + "%";
    currentValue = settingsMap[Configuration::BGM_VOLUME].value;
    bgm::setVolume(v);
}

void AppSettings::updateBgmMode(bool) {
    const bool dyn = settingsMap[Configuration::BGM_MODE].value == "Dynamic";
    currentValue = dyn ? "Static" : "Dynamic";
    settingsMap[Configuration::BGM_MODE].value = currentValue;
}

Settings::Settings(Configuration& cfg, Languages& languages,
                   int minValue, int maxValue, int delta) 
    : cfg(cfg), languages(languages), 
      minValue(minValue), maxValue(maxValue), delta(delta) {
}

AppSettings::AppSettings(Configuration& cfg, Languages& languages,
                               int minValue, int maxValue, int delta)
    : Settings(cfg, languages, minValue, maxValue, delta) {
    defaultKeys = {
        Configuration::CLEAR_CACHE,
        Configuration::SCRAPE_ARTWORK,
        Configuration::OPTIMIZE_ARTWORK,
        Configuration::IMPORT_GAMELIST,
        Configuration::MANAGE_HIDDEN,
        Configuration::HIDE_SECTIONS,
        Configuration::VOLUME,
        Configuration::BGM_MENU,
        Configuration::SFX_MENU,
        Configuration::BGM_ENABLED,
        Configuration::BGM_MODE,
        Configuration::BGM,
        Configuration::BGM_VOLUME,
        Configuration::SFX_CONFIRM,
        Configuration::SFX_BACK,
        Configuration::SFX_NAV,
        Configuration::GAME_STYLE,
        Configuration::BUTTON_STYLE,
        Configuration::CLOCK_FORMAT,
        Configuration::BG_COLOR,
        Configuration::BG_ART,
        Configuration::SEL_COLOR,
        Configuration::SCREEN_RES,
        Configuration::MISTER_INI_RES,
        Configuration::MISTER_INI_HDR,
        Configuration::MISTER_INI_SD,
        Configuration::CRT_OVERSCAN,
        Configuration::CRT_FONT,
        Configuration::NET,
        Configuration::BLUETOOTH,
        Configuration::BT_RECONNECT,
        Configuration::BT_REPAIR,
        Configuration::CONTROLS,
        Configuration::AB_SWAP,
        Configuration::XY_SWAP,
        Configuration::PROMPT_SWAP,
        Configuration::INPUT_TESTER,
        Configuration::KB_MAPPING,
        Configuration::BOOT_TO_GAMES,
        Configuration::LOADCONFIG,
        Configuration::UPDATE_TIME,
        Configuration::UPDATE,
        Configuration::UPDATE_CM,
        Configuration::DEV_TOOLS,
        Configuration::QUIT
    };

}

RomSettings::RomSettings(Configuration& cfg, Languages& languages,
                          int minValue, int maxValue, int delta)
        : Settings(cfg, languages, minValue, maxValue, delta) {
    defaultKeys = {
        Configuration::ROM_OVERCLOCK, Configuration::ROM_AUTOSTART, Configuration::CORE_OVERRIDE
    };    

}

void Settings::navigateUp() {
     if (!enabledKeys.empty()) {
        currentIndex--;
        if (currentIndex < 0) {
            currentIndex = enabledKeys.size() - 1;
        }
        currentKey = enabledKeys[currentIndex];
        currentValue = settingsMap[currentKey].value;   // sync so Left/Right cycles from this row
    }
}

void Settings::navigateDown() {
    if (!enabledKeys.empty()) {
        currentIndex++;
        if (currentIndex >= enabledKeys.size()) {
            currentIndex = 0;
        }
        currentKey = enabledKeys[currentIndex];
        currentValue = settingsMap[currentKey].value;   // sync so Left/Right cycles from this row
    }
}

std::vector<Settings::SettingRow> AppSettings::getAppSettings() {
    std::vector<SettingRow> settingRows;

    for (const auto& key : enabledKeys) {

        size_t pos = key.find_last_of(".");

        if (pos != std::string::npos) {
            try {
                settingRows.push_back({languages.getOr(key.substr(pos + 1), defaultSettingLabel(key.substr(pos + 1))),
                                        settingsMap[key].value
                                        });
            } catch (boost::property_tree::ptree_bad_path e) {
                throw ItemNotFoundException("getAppSettings Language translation not found for "
                + key + " in " + languages.getLang());

            }
        } else {
            throw ItemNotFoundException("Setting key format unknown: "
                + key);
        }
    }
    return settingRows;
}

std::vector<Settings::SettingRow> RomSettings::getRomSettings() {
    
    std::vector<SettingRow> settingRows;
    
    for (const auto& key : enabledKeys) {

        if (key.find("GAME.") == 0) {
        
            size_t pos = key.find_last_of(".");
            
            if (pos != std::string::npos) {
                try {
                    settingRows.push_back({languages.getOr(key.substr(pos + 1), defaultSettingLabel(key.substr(pos + 1))), 
                                            settingsMap[key].value
                                            });
                } catch (boost::property_tree::ptree_bad_path e) {
                    throw ItemNotFoundException("getRomSettings Language translation not found for " 
                    + key + " in " + languages.getLang());
                }
            } else {
                throw ItemNotFoundException("Setting key format unknown: " 
                    + key);
            }
        }
    }

    return settingRows;
}

#include "misterini.h"
// MiSTer video_mode presets, indices 0-14, 14 = pixel-repeat
static const char* kMisterVideoModes[] = {
    "1280x720@60",  "1024x768@60",  "720x480@60",   "720x576@50",   "1280x1024@60",
    "800x600@60",   "640x480@60",   "1280x720@50",  "1920x1080@60", "1920x1080@50",
    "1366x768@60",  "1024x600@60",  "1920x1440@60", "2048x1536@60", "2560x1440@60"
};
static const int kMisterVideoModeCount = 15;

// "Mode N - res" for a preset, bare "Mode N" if out of range
static std::string misterModeDesc(int mode) {
    std::string s = "Mode " + std::to_string(mode);
    if (mode >= 0 && mode < kMisterVideoModeCount) s += " - " + std::string(kMisterVideoModes[mode]);
    if (mode == 14) s += " (pr)";
    return s;
}

std::string Settings::parseMisterIniVideoMode() {
    std::string iniFile = activeMisterIniPath();
    std::ifstream f(iniFile);
    if (!f) return "no ini";
    std::string line, commented;
    bool inMister = true;                        // pre-header lines are global
    while (std::getline(f, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        std::string t = line.substr(s);
        if (t[0] == '[') {
            std::string low = t; for (auto& c : low) if (c >= 'A' && c <= 'Z') c += 32;
            inMister = (low.rfind("[mister]", 0) == 0);
            continue;
        }
        if (!inMister) continue;                 // ignore per-core sections' video_mode
        bool isComment = (t[0] == ';' || t[0] == '#');
        std::string body = t;
        if (isComment) { size_t b = body.find_first_not_of(";# \t"); body = (b == std::string::npos) ? "" : body.substr(b); }
        if (body.rfind("video_mode", 0) != 0) continue;
        char after = body.size() > 10 ? body[10] : '\0';         // not video_mode_ntsc / video_mode_pal
        if (after != '=' && after != ' ' && after != '\t') continue;
        size_t eq = body.find('=');
        if (eq == std::string::npos) continue;
        std::string v = body.substr(eq + 1);
        size_t a = v.find_first_not_of(" \t");
        v = (a == std::string::npos) ? "" : v.substr(a);
        size_t c = v.find_first_of(";#\r\n");                     // drop trailing inline comment
        if (c != std::string::npos) v = v.substr(0, c);
        size_t z = v.find_last_not_of(" \t");
        v = (z == std::string::npos) ? "" : v.substr(0, z + 1);
        std::string cls;
        if (!v.empty() && v.find_first_not_of("0123456789") == std::string::npos) cls = misterModeDesc(atoi(v.c_str()));
        else                                                                      cls = v.empty() ? "(empty)" : v;
        if (!isComment) return cls;                              // an active setting wins
        if (commented.empty()) commented = cls + " (commented)";
    }
    return commented.empty() ? "Not set" : commented;
}

// m_iniVmSel: 0 = keep current, 1..15 = write video_mode 0..14
std::string Settings::misterIniVmLabel(int sel) {
    if (sel <= 0) return m_iniVmCurrent;
    return misterModeDesc(sel - 1);
}

void Settings::updateMisterIniVideoMode(bool increase) {
    int n = kMisterVideoModeCount + 1;   // "keep current" + 15 presets
    m_iniVmSel = (m_iniVmSel + (increase ? 1 : -1) + n) % n;
    currentValue = misterIniVmLabel(m_iniVmSel);
    settingsMap[Configuration::MISTER_INI_RES].value = currentValue;
}

bool Settings::applyMisterIniVideoMode() {
    if (m_iniVmSel <= 0) return false;   // "keep current" -> nothing to write
    return writeMisterIniVideoMode(m_iniVmSel - 1);
}

// atomic rewrite (tmp + fsync + rename) that keeps the user's comments, unlike a ptree round-trip
static bool writeLinesAtomic(const std::string& path, const std::vector<std::string>& lines) {
    std::string tmp = path + ".cmtmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        for (auto& line : lines) out << line << "\n";
        out.flush();
        if (!out.good()) { std::error_code ec; std::filesystem::remove(tmp, ec); return false; }
    }
    int fd = open(tmp.c_str(), O_RDONLY);
    if (fd >= 0) { fsync(fd); close(fd); }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);      // atomic replace on the same fs
    if (ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}

// rewrite one ini's video_mode, preserving everything else
static bool applyVideoModeToIni(const std::string& iniFile, int mode) {
    std::ifstream in(iniFile);
    if (!in) return false;
    std::vector<std::string> lines; std::string l;
    while (std::getline(in, l)) lines.push_back(l);
    in.close();
    int activeIdx = -1, commentedIdx = -1, sectionIdx = -1;
    bool inMister = true;                        // pre-header lines are global
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = lines[i];
        size_t s = t.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        t = t.substr(s);
        if (t[0] == '[') { inMister = (lowerCopy(t).rfind("[mister]", 0) == 0); if (inMister) sectionIdx = (int)i; continue; }
        if (!inMister) continue;                 // never touch a per-core section's video_mode
        bool isComment = (t[0] == ';' || t[0] == '#');
        std::string body = t;
        if (isComment) { size_t b = body.find_first_not_of(";# \t"); body = (b == std::string::npos) ? "" : body.substr(b); }
        if (body.rfind("video_mode", 0) != 0) continue;
        char after = body.size() > 10 ? body[10] : '\0';   // not video_mode_ntsc/pal
        if (after != '=' && after != ' ' && after != '\t') continue;
        if (!isComment && activeIdx < 0) activeIdx = (int)i;
        else if (isComment && commentedIdx < 0) commentedIdx = (int)i;
    }
    std::string newline = "video_mode=" + std::to_string(mode);
    int replaceIdx = (activeIdx >= 0) ? activeIdx : commentedIdx;
    if (replaceIdx >= 0 && !lines[replaceIdx].empty() && lines[replaceIdx].back() == '\r')
        newline += '\r';                       // CRLF file, keep the line ending
    if      (replaceIdx  >= 0) lines[replaceIdx] = newline;
    else if (sectionIdx  >= 0) lines.insert(lines.begin() + sectionIdx + 1, newline);
    else                       lines.insert(lines.begin(), newline);
    return writeLinesAtomic(iniFile, lines);
}


static const char* kHdrLabels[3] = { "Off", "HDR10", "HDR10 DCI-P3" };

// an active line beats a commented one
std::string Settings::parseMisterIniHdr() {
    std::ifstream f(activeMisterIniPath());
    if (!f) return "no ini";
    std::string line, commented;
    bool inMister = true;
    while (std::getline(f, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        std::string t = line.substr(s);
        if (t[0] == '[') { inMister = (lowerCopy(t).rfind("[mister]", 0) == 0); continue; }
        if (!inMister) continue;
        bool isComment = (t[0] == ';' || t[0] == '#');
        std::string body = t;
        if (isComment) { size_t b = body.find_first_not_of(";# \t"); body = (b == std::string::npos) ? "" : body.substr(b); }
        if (lowerCopy(body).rfind("hdr", 0) != 0) continue;
        char after = body.size() > 3 ? body[3] : '\0';   // not hdr_max_nits / hdr_avg_nits
        if (after != '=' && after != ' ' && after != '\t') continue;
        size_t eq = body.find('=');
        if (eq == std::string::npos) continue;
        std::string v = trimCopy(body.substr(eq + 1));
        size_t c = v.find_first_of(";#\r\n");
        if (c != std::string::npos) v = trimCopy(v.substr(0, c));
        int n = (!v.empty() && v.find_first_not_of("0123456789") == std::string::npos) ? atoi(v.c_str()) : -1;
        std::string cls = (n >= 0 && n <= 2) ? kHdrLabels[n] : (v.empty() ? "(empty)" : v);
        if (!isComment) return cls;
        if (commented.empty()) commented = cls + " (commented)";
    }
    return commented.empty() ? "Not set" : commented;
}

std::string Settings::parseMisterIniScandoubler() {
    std::ifstream f(activeMisterIniPath());
    if (!f) return "no ini";
    std::string line, commented;
    bool inMister = true;
    while (std::getline(f, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        std::string t = line.substr(s);
        if (t[0] == '[') { inMister = (lowerCopy(t).rfind("[mister]", 0) == 0); continue; }
        if (!inMister) continue;
        bool isComment = (t[0] == ';' || t[0] == '#');
        std::string body = t;
        if (isComment) { size_t b = body.find_first_not_of(";# \t"); body = (b == std::string::npos) ? "" : body.substr(b); }
        if (lowerCopy(body).rfind("forced_scandoubler", 0) != 0) continue;
        char after = body.size() > 18 ? body[18] : '\0';
        if (after != '=' && after != ' ' && after != '\t') continue;
        size_t eq = body.find('=');
        if (eq == std::string::npos) continue;
        std::string v = trimCopy(body.substr(eq + 1));
        size_t c = v.find_first_of(";#\r\n");
        if (c != std::string::npos) v = trimCopy(v.substr(0, c));
        std::string cls = (v == "1") ? "On" : (v == "0") ? "Off" : (v.empty() ? "(empty)" : v);
        if (!isComment) return cls;
        if (commented.empty()) commented = cls + " (commented)";
    }
    return commented.empty() ? "Not set" : commented;
}

static bool applySdToIni(const std::string& iniFile, int sd) {
    std::ifstream in(iniFile);
    if (!in) return false;
    std::vector<std::string> lines; std::string l;
    while (std::getline(in, l)) lines.push_back(l);
    in.close();
    int activeIdx = -1, commentedIdx = -1, sectionIdx = -1;
    bool inMister = true;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = lines[i];
        size_t s = t.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        t = t.substr(s);
        if (t[0] == '[') { inMister = (lowerCopy(t).rfind("[mister]", 0) == 0); if (inMister) sectionIdx = (int)i; continue; }
        if (!inMister) continue;
        bool isComment = (t[0] == ';' || t[0] == '#');
        std::string body = t;
        if (isComment) { size_t b = body.find_first_not_of(";# \t"); body = (b == std::string::npos) ? "" : body.substr(b); }
        if (lowerCopy(body).rfind("forced_scandoubler", 0) != 0) continue;
        char after = body.size() > 18 ? body[18] : '\0';
        if (after != '=' && after != ' ' && after != '\t') continue;
        if (!isComment && activeIdx < 0) activeIdx = (int)i;
        else if (isComment && commentedIdx < 0) commentedIdx = (int)i;
    }
    std::string newline = "forced_scandoubler=" + std::to_string(sd);
    int replaceIdx = (activeIdx >= 0) ? activeIdx : commentedIdx;
    if (replaceIdx >= 0 && !lines[replaceIdx].empty() && lines[replaceIdx].back() == '\r')
        newline += '\r';
    if      (replaceIdx  >= 0) lines[replaceIdx] = newline;
    else if (sectionIdx  >= 0) lines.insert(lines.begin() + sectionIdx + 1, newline);
    else                       lines.insert(lines.begin(), newline);
    return writeLinesAtomic(iniFile, lines);
}

void Settings::updateMisterIniScandoubler(bool) {
    bool on = (parseMisterIniScandoubler() == "On");
    if (!applySdToIni(activeMisterIniPath(), on ? 0 : 1)) return;
    m_iniSdCurrent = on ? "Off" : "On";
    currentValue = m_iniSdCurrent;
    settingsMap[Configuration::MISTER_INI_SD].value = currentValue;
}

void Settings::updateMisterIniHdr(bool increase) {
    int n = 4;   // current + three hdr modes
    do {   // skip the current value
        m_iniHdrSel = (m_iniHdrSel + (increase ? 1 : -1) + n) % n;
    } while (m_iniHdrSel > 0 && kHdrLabels[m_iniHdrSel - 1] == m_iniHdrCurrent);
    currentValue = (m_iniHdrSel <= 0) ? m_iniHdrCurrent : kHdrLabels[m_iniHdrSel - 1];
    settingsMap[Configuration::MISTER_INI_HDR].value = currentValue;
}

static bool applyHdrToIni(const std::string& iniFile, int hdr) {
    std::ifstream in(iniFile);
    if (!in) return false;
    std::vector<std::string> lines; std::string l;
    while (std::getline(in, l)) lines.push_back(l);
    in.close();
    int activeIdx = -1, commentedIdx = -1, sectionIdx = -1;
    bool inMister = true;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = lines[i];
        size_t s = t.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        t = t.substr(s);
        if (t[0] == '[') { inMister = (lowerCopy(t).rfind("[mister]", 0) == 0); if (inMister) sectionIdx = (int)i; continue; }
        if (!inMister) continue;
        bool isComment = (t[0] == ';' || t[0] == '#');
        std::string body = t;
        if (isComment) { size_t b = body.find_first_not_of(";# \t"); body = (b == std::string::npos) ? "" : body.substr(b); }
        if (lowerCopy(body).rfind("hdr", 0) != 0) continue;
        char after = body.size() > 3 ? body[3] : '\0';   // not hdr_max_nits / hdr_avg_nits
        if (after != '=' && after != ' ' && after != '\t') continue;
        if (!isComment && activeIdx < 0) activeIdx = (int)i;
        else if (isComment && commentedIdx < 0) commentedIdx = (int)i;
    }
    std::string newline = "hdr=" + std::to_string(hdr);
    int replaceIdx = (activeIdx >= 0) ? activeIdx : commentedIdx;
    if (replaceIdx >= 0 && !lines[replaceIdx].empty() && lines[replaceIdx].back() == '\r')
        newline += '\r';
    if      (replaceIdx  >= 0) lines[replaceIdx] = newline;
    else if (sectionIdx  >= 0) lines.insert(lines.begin() + sectionIdx + 1, newline);
    else                       lines.insert(lines.begin(), newline);
    return writeLinesAtomic(iniFile, lines);
}

bool Settings::applyMisterIniHdr() {
    if (m_iniHdrSel <= 0) return false;
    return applyHdrToIni(activeMisterIniPath(), m_iniHdrSel - 1);
}

bool Settings::writeMisterIniVideoMode(int mode) {
    // target only the active ini, each variant carries its own video_mode
    return applyVideoModeToIni(activeMisterIniPath(), mode);
}

// RetroAchievements: toggle a per-core main=<binary> chain-load line in the active ini
std::string Settings::activeMisterIniPath() {
    std::string dir = cfg.get(Configuration::HOME_PATH);
    const char* nm = misterIniName(misterCurrentAlt(), dir.c_str());
    return dir + "/" + ((nm && nm[0]) ? nm : "MiSTer.ini");
}


// case-insensitive [coreName] span [start+1, end), start = -1 if absent
static void findIniSection(const std::vector<std::string>& lines, const std::string& coreName,
                           int& start, int& end) {
    std::string want = lowerCopy("[" + coreName + "]");
    start = -1; end = (int)lines.size();
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = lines[i];
        size_t s = t.find_first_not_of(" \t\r\n");
        if (s == std::string::npos || t[s] != '[') continue;
        if (start >= 0) { end = (int)i; break; }                 // next section closes ours
        if (lowerCopy(t.substr(s)).rfind(want, 0) == 0) start = (int)i;
    }
    if (start < 0) end = -1;
}

// active (uncommented) main=<mainName> present in [coreName]?
bool Settings::misterIniMainOverrideEnabled(const std::string& coreName, const std::string& mainName) {
    std::ifstream f(activeMisterIniPath());
    if (!f) return false;
    std::vector<std::string> lines; std::string l;
    while (std::getline(f, l)) lines.push_back(l);
    int s, e; findIniSection(lines, coreName, s, e);
    if (s < 0) return false;
    for (int i = s + 1; i < e; i++) {
        std::string t = lines[i];
        size_t b = t.find_first_not_of(" \t\r\n");
        if (b == std::string::npos || t[b] == ';' || t[b] == '#') continue;
        std::string body = t.substr(b);
        if (body.rfind("main", 0) != 0) continue;
        size_t eq = body.find('=');
        if (eq == std::string::npos || body.find_first_not_of(" \t", 4) != eq) continue;
        std::string v = body.substr(eq + 1);
        size_t a = v.find_first_not_of(" \t"); v = (a == std::string::npos) ? "" : v.substr(a);
        size_t c = v.find_first_of("; #\r\n"); if (c != std::string::npos) v = v.substr(0, c);
        if (v == mainName) return true;
    }
    return false;
}

// toggle main=<mainName> in [coreName] of the active ini
bool Settings::setMisterIniMainOverride(const std::string& coreName, const std::string& mainName, bool enable) {
    std::string iniFile = activeMisterIniPath();
    std::ifstream in(iniFile);
    std::vector<std::string> lines; std::string l;
    if (in) { while (std::getline(in, l)) lines.push_back(l); in.close(); }
    else if (!enable) return true;                               // no ini + disable, nothing to do

    int s, e; findIniSection(lines, coreName, s, e);
    const std::string mainLine = "main=" + mainName;

    // classify: 0 none, 1 active main=<mainName>, 2 commented main=<mainName>, 3 active main=<other>
    auto classify = [&](const std::string& t) -> int {
        size_t b = t.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return 0;
        bool commented = (t[b] == ';' || t[b] == '#');
        std::string body = t.substr(b);
        if (commented) { size_t p = body.find_first_not_of(";# \t"); body = (p == std::string::npos) ? "" : body.substr(p); }
        if (body.rfind("main", 0) != 0) return 0;
        size_t eq = body.find('=');
        if (eq == std::string::npos || body.find_first_not_of(" \t", 4) != eq) return 0;
        std::string v = body.substr(eq + 1);
        size_t a = v.find_first_not_of(" \t"); v = (a == std::string::npos) ? "" : v.substr(a);
        size_t c = v.find_first_of("; #\r\n"); if (c != std::string::npos) v = v.substr(0, c);
        if (v != mainName) return commented ? 0 : 3;
        return commented ? 2 : 1;
    };

    if (enable) {
        if (s < 0) {                                             // no section, append one
            if (!lines.empty() && !lines.back().empty()) lines.push_back("");
            lines.push_back("[" + coreName + "]");
            lines.push_back(mainLine);
        } else {
            int activeIdx = -1, commentedIdx = -1, otherIdx = -1;
            for (int i = s + 1; i < e; i++) {
                int k = classify(lines[i]);
                if      (k == 1 && activeIdx    < 0) activeIdx    = i;
                else if (k == 2 && commentedIdx < 0) commentedIdx = i;
                else if (k == 3 && otherIdx     < 0) otherIdx     = i;
            }
            if      (activeIdx    >= 0) ;                        // already enabled
            else if (otherIdx     >= 0) lines[otherIdx]     = mainLine;   // replace other main=
            else if (commentedIdx >= 0) lines[commentedIdx] = mainLine;   // uncomment
            else                        lines.insert(lines.begin() + s + 1, mainLine);
        }
    } else {
        if (s < 0) return true;
        for (int i = s + 1; i < e; i++)
            if (classify(lines[i]) == 1) lines[i] = ";" + mainLine;      // comment out
    }

    return writeLinesAtomic(iniFile, lines);
}

// informational: is an installer-style [RA_*] wildcard block present in the active ini?
bool Settings::misterIniHasRaWildcard() {
    std::ifstream f(activeMisterIniPath());
    if (!f) return false;
    std::string l; bool inRa = false;
    while (std::getline(f, l)) {
        size_t b = l.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        if (l[b] == '[') { inRa = (l.compare(b, 6, "[RA_*]") == 0); continue; }
        if (!inRa || l[b] == ';' || l[b] == '#') continue;
        if (l.compare(b, 5, "main=") == 0) return true;
    }
    return false;
}

void Settings::initializeSettings() {

    bgColors.clear();
    bgColors.push_back(BLACK);
    bgColors.push_back(GRAY);
    bgColors.push_back(GREEN);
    bgColors.push_back(BLUE);
    bgColors.push_back(YELLOW);
    bgColors.push_back(RED);
    {   // 7th slot = the custom color
        std::string cust = cfg.get(Configuration::CUSTOM_BG_COLOR);
        bgColors.push_back((!cust.empty() && cust[0] == '#') ? cust : "#808080");
    }

    selColors.clear();
    selColors.push_back(WHITE);
    selColors.push_back(GRAY);
    selColors.push_back(GREEN);
    selColors.push_back(BLUE);
    selColors.push_back(YELLOW);
    selColors.push_back(RED);

    // Flix renders as List until built
    gameStyles.clear();
    gameStyles.push_back("List View");
    gameStyles.push_back("Grid View");
    gameStyles.push_back("Big Grid View");
    gameStyles.push_back("NX View");
    gameStyles.push_back("Big NX View");
    gameStyles.push_back("SS4");

    buttonStyles.clear();
    buttonStyles.push_back("ABXY");
    buttonStyles.push_back("ABXY Color");
    buttonStyles.push_back("PlayStation");
    buttonStyles.push_back("PlayStation Color");
    buttonStyles.push_back("PlayStation Gray");

    clockFormats.clear();
    clockFormats.push_back("24-Hour");
    clockFormats.push_back("12-Hour");

    // BT reconnect policy consumed by bt_boot.sh
    btReconnectModes.clear();
    btReconnectModes.push_back("Balanced");    // boot/drop windows + slow steady poll
    btReconnectModes.push_back("Aggressive");  // page missing pads every cycle
    btReconnectModes.push_back("Passive");     // boot/drop windows only
    btReconnectModes.push_back("Off");         // never page, pad-initiated only

    btRepairModes.clear();
    btRepairModes.push_back("Auto-Accept");    // pads may re-pair anytime
    btRepairModes.push_back("Menu Only");      // pairing accepted only in the BT menu

    // CRT-only font look: Sharp / Smooth / Legible
    crtFonts.clear();
    crtFonts.push_back("Sharp");
    crtFonts.push_back("Smooth");
    crtFonts.push_back("Legible");

    MisterInis.clear();
    std::string path = cfg.get(Configuration::HOME_PATH);
    for(int i = 0 ; i < 4; i++)
        MisterInis.push_back(misterIniLabel(i,path.c_str() ) );

    for (const auto& key : defaultKeys) {
        // every key is always shown, config.ini only supplies the value
        std::string value = cfg.get(key);
        // seed History Size so updateInt's stoi doesn't throw on a missing key
        if (value.empty() && key == Configuration::HISTORY_SIZE) value = "20";
        if (key == Configuration::BOOT_SPEED) {                  // map legacy Fast/Moderate/Slow values
            if (value.empty() || value == "Moderate") value = "Safe";
            else if (value == "Fast") value = "Unsafe";
            else if (value == "Slow") value = "Safest";
        }
        if (value.empty() && key == Configuration::BGM_VOLUME) value = "70%";
        if (value.empty() && key == Configuration::BGM_MODE)   value = "Static";
        if (key == Configuration::BGM_MENU || key == Configuration::SFX_MENU) value = ">";
        // absent = on iff a file is set
        if (value.empty() && key == Configuration::BGM_ENABLED)
            value = cfg.get(Configuration::BGM_FILE).empty() ? "false" : "true";
        if (value.empty() && key == Configuration::SEL_COLOR)    value = WHITE;
        if (value.empty() && key == Configuration::SS4_ANIM)     value = "true";
        if (value.empty() && key == Configuration::USE_MIRROR)   value = "false";
        if (value.empty() && key == Configuration::BOOT_MAIN_MENU) value = "false";
        if (value.empty() && key == Configuration::AB_SWAP)      value = "false";
        if (value.empty() && key == Configuration::XY_SWAP)      value = "false";
        if (value.empty() && key == Configuration::PROMPT_SWAP)  value = "false";
        if (value.empty() && key == Configuration::BOOT_TO_GAMES) value = "false";
        if (value.empty() && key == Configuration::CENTER_LIST)   value = "false";
        if (value.empty() && key == Configuration::BG_ART)       value = "true";
        if (value.empty() && key == Configuration::PERF_MODE)    value = "Off";
        if (value.empty() && key == Configuration::CRT_OVERSCAN) value = "0%";
        if (value.empty() && key == Configuration::CRT_FONT)      value = "Sharp";
        if (value.empty() && key == Configuration::CLOCK_FORMAT)  value = "24-Hour";
        if (value.empty() && key == Configuration::BT_RECONNECT)  value = "Balanced";
        if (value.empty() && key == Configuration::BT_REPAIR)     value = "Auto-Accept";
        if (key == Configuration::MISTER_INI_RES) {                                     // computed, not stored
            value = parseMisterIniVideoMode();
            // show which ini this row reads/writes for alt-ini setups
            std::string ini = activeMisterIniPath();
            size_t sl = ini.find_last_of('/');
            std::string base = (sl == std::string::npos) ? ini : ini.substr(sl + 1);
            if (base != "MiSTer.ini") value += " @ " + base;
            m_iniVmCurrent = value;   // the "keep current" label
            m_iniVmSel = 0;
        }
        if (key == Configuration::MISTER_INI_HDR) {                                     // computed, not stored
            value = parseMisterIniHdr();
            m_iniHdrCurrent = value;
            m_iniHdrSel = 0;
        }
        if (key == Configuration::MISTER_INI_SD) {                                      // computed, not stored
            value = parseMisterIniScandoubler();
            m_iniSdCurrent = value;
        }
        settingsMap[key] = {key, value, true};
        if (!value.empty())
            notifySettingsChange(key, value);
    }

    // gate on the CONSOLEMODE_CRT env, not crt::active() which is false before crt::init() runs
    if (settingsMap.count(Configuration::CRT_OVERSCAN)) {
        const char* crtEnv = getenv("CONSOLEMODE_CRT");
        settingsMap[Configuration::CRT_OVERSCAN].enabled =
            (crtEnv && std::string(crtEnv) == "1") || getenv("CONSOLEMODE_SAFE_AREA");
        int pct = 0; try { pct = std::stoi(settingsMap[Configuration::CRT_OVERSCAN].value); } catch (...) {}
        crt::setOverscan(pct);
    }

    // 31kHz menu modes only
    if (settingsMap.count(Configuration::MISTER_INI_SD))
        settingsMap[Configuration::MISTER_INI_SD].enabled =
            (crt::videoMode() >= crt::MODE_480P && crt::videoMode() <= crt::MODE_480P_SQ);

    // CRT-only, gate on the CRT env like CRT Overscan
    if (settingsMap.count(Configuration::CRT_FONT)) {
        const char* crtEnv = getenv("CONSOLEMODE_CRT");
        settingsMap[Configuration::CRT_FONT].enabled =
            (crtEnv && std::string(crtEnv) == "1") || getenv("CONSOLEMODE_SAFE_AREA");
    }

    enabledKeys = getEnabledKeys();

    currentIndex = 0;
    if(!enabledKeys.empty()) {
        currentKey = enabledKeys[currentIndex];
    }

    int index = misterCurrentAlt();
    if (index < 0 || index >= (int)MisterInis.size())
        index = 0;

    iniIndex = index;
}

std::vector<std::string> Settings::getEnabledKeys() {
    std::vector<std::string> enabledKeys;
    for (const auto& key : defaultKeys) {
        if (settingsMap[key].enabled) {
            enabledKeys.push_back(key);
        }
    }

    return enabledKeys;
}

void Settings::updateInt(bool increase, std::string setting,
                         int min, int max, int inc) {
    // blank/garbage value would make stoi throw, so fall back to min
    int intValue;
    try { intValue = std::stoi(settingsMap[setting].value); }
    catch (...) { intValue = min; }
    if (increase) {
        if (intValue + inc <= max) {
            intValue += inc; 
        }

    } else if (!increase && intValue - inc >= min) {
        intValue -= inc;
    }

    currentValue = std::to_string(intValue);
    settingsMap[setting].value = currentValue;
}

void Settings::updateString(std::string key, std::string value)
{
    settingsMap[key].value = value;
}

void Settings::updateListSetting(const std::set<std::string>& values, bool increase) {
    auto it = values.find(currentValue);
    
    if (!increase) {
        if (it == values.begin()) {
            it = std::prev(values.end());
        } else {
            --it;
        }
    } else {
        ++it;
        if (it == values.end()) {
            it = values.begin();
        }
    }
    currentValue = *it;
}


void Settings::updateListVector(const std::vector<std::string>& values, bool increase, bool isSystemConfig)
{
    int idx = 0;
    for (int i = 0; i < values.size();i++)
    {
        if (currentValue == values[i]){
            idx = i;
            break;
        }
    }
    if (!increase) {
        idx--;
        if (idx<0)
            idx = values.size() -1;
    }
    else{
        idx++;
        if (idx >= values.size())
            idx = 0;
    }
    currentValue = values[idx];
    if(isSystemConfig)
        iniIndex = idx;
}


void AppSettings::updateConfigIni(bool increase) {
    updateListVector(MisterInis, increase);

    settingsMap[Configuration::LOADCONFIG].value = currentValue;
}

void AppSettings::updateBgColor(bool increase)
{
    std::string cust = cfg.get(Configuration::CUSTOM_BG_COLOR);
    bgColors[6] = (!cust.empty() && cust[0] == '#') ? cust : "#808080";
    updateListVector(bgColors, increase, false);
    settingsMap[Configuration::BG_COLOR].value = currentValue;
}

void AppSettings::updateSelColor(bool increase)
{
    updateListVector(selColors, increase, false);
    settingsMap[Configuration::SEL_COLOR].value = currentValue;
}

void AppSettings::updateGameStyle(bool increase)
{
    updateListVector(gameStyles, increase, false);
    settingsMap[Configuration::GAME_STYLE].value = currentValue;
}

void AppSettings::updateButtonStyle(bool increase)
{
    updateListVector(buttonStyles, increase, false);
    settingsMap[Configuration::BUTTON_STYLE].value = currentValue;
}

void AppSettings::updateClockFormat(bool increase)
{
    updateListVector(clockFormats, increase, false);
    settingsMap[Configuration::CLOCK_FORMAT].value = currentValue;
}

void AppSettings::updateBtReconnect(bool increase)
{
    updateListVector(btReconnectModes, increase, false);
    settingsMap[Configuration::BT_RECONNECT].value = currentValue;
}

void AppSettings::updateBtRepair(bool increase)
{
    updateListVector(btRepairModes, increase, false);
    settingsMap[Configuration::BT_REPAIR].value = currentValue;
}

void AppSettings::updateTime()
{
    if (system("/media/fat/scripts/rtc.sh &") == -1)
        std::cerr << "Error executing script rtc.sh (system() failed)" << std::endl;
}

void AppSettings::updateUSBMode(bool increase) {
}


void AppSettings::updateOverclock(bool increase) {
}

void Settings::updateBoolSetting() {
    bool value = settingsMap[currentKey].value == "true";
    value = !value;
    currentValue = value ? "true" : "false";
    settingsMap[currentKey].value = currentValue;
}


void AppSettings::restartApplication() {
}

void AppSettings::quitApplication() {
    notifySettingsChange(Configuration::QUIT, "QUIT");
}

void RomSettings::updateRomOverclock(bool increase) {
}

void RomSettings::updateAutoStart(bool increase) {
    updateBoolSetting();

    settingsMap[Configuration::ROM_AUTOSTART].value = currentValue;
}

void RomSettings::updateCoreSelection(bool increase) {
    updateListSetting(cores, increase);

    settingsMap[Configuration::CORE_SELECTION].value = currentValue;
}

void RomSettings::updateCoreOverride(bool increase) {
    updateListSetting(cores, increase);

    settingsMap[Configuration::CORE_OVERRIDE].value = currentValue;
}

std::string Settings::getCurrentKey() {
    return currentKey;
};

std::string Settings::getCurrentValue() {
    return settingsMap[currentKey].value;
};


int AppSettings::getConfigIndex(){
    return iniIndex;
}


void AppSettings::reloadSettingRows() {
    settingRows.clear();

    for (const auto& key : enabledKeys) {

        if (key.find("APPLICATION.") == 0) {

            size_t pos = key.find_last_of(".");
            
            if (pos != std::string::npos) {
                try {
                    settingRows.push_back({languages.getOr(key.substr(pos + 1), defaultSettingLabel(key.substr(pos + 1))), 
                                            settingsMap[key].value
                                            });
                } catch (boost::property_tree::ptree_bad_path e) {
                    throw ItemNotFoundException("reloadSettingRows Language translation not found for " 
                    + key + " in " + languages.getLang());
                }
            } else {
                throw ItemNotFoundException("Setting key format unknown: " 
                    + key);
            }
        }
    }
}

// settings observers
void Settings::attach(ISettingsObserver *observer) {
    observers.push_back(observer);
}

void Settings::detach(ISettingsObserver *observer) {
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
}

void Settings::notifySettingsChange(const std::string &key, const std::string &value) {
    for (ISettingsObserver *observer : observers) {
        observer->settingsChanged(key, value);
    }
}

void RomSettings::languageChanged() {
}

std::string AppSettings::getName() {
    return "AppSettings::" + std::to_string((unsigned long long)(void**)this);
}

std::string RomSettings::getName() {
    return "RomSettings::" + std::to_string((unsigned long long)(void**)this);
}




std::string DevSettings::getName() {
    return "DevSettings:" + std::to_string((unsigned long long)(void**)this);
}

int DevSettings::getConfigIndex(){
    return iniIndex;
}

void DevSettings::reloadSettingRows() {
    settingRows.clear();

    for (const auto& key : enabledKeys) {

        if (key.find("DEVTOOLS.") == 0) {

            size_t pos = key.find_last_of(".");
            
            if (pos != std::string::npos) {
                try {
                    settingRows.push_back({languages.getOr(key.substr(pos + 1), defaultSettingLabel(key.substr(pos + 1))), 
                                            settingsMap[key].value
                                            });
                } catch (boost::property_tree::ptree_bad_path e) {
                    throw ItemNotFoundException("DevSettings reloadSettingRows Language translation not found for " 
                    + key + " in " + languages.getLang());
                }
            } else {
                throw ItemNotFoundException("Setting key format unknown: " 
                    + key);
            }
        }
    }
}


DevSettings::DevSettings(Configuration& cfg, Languages& languages, 
                               int minValue, int maxValue, int delta)
    : Settings(cfg, languages, minValue, maxValue, delta) {
    defaultKeys = {
        Configuration::SHOW_FPS,
        Configuration::SCREEN_REFRESH,
        Configuration::SHOW_HOMEBREW,
        Configuration::SHOW_RESUME,
        Configuration::HISTORY_SIZE,
        Configuration::TITLE_FILTER,
        Configuration::LOCK_FLIX_TITLE,
        Configuration::CENTER_LIST,
        Configuration::SS4_ANIM,
        Configuration::USE_MIRROR,
        Configuration::BOOT_SPEED,
        Configuration::PERF_MODE,
        Configuration::CD_TESTER,
        Configuration::AUTOBOOT,
        Configuration::BOOT_MAIN_MENU,
        Configuration::LOADSCRIPT
    };
    #ifdef HAS_PS1
        defaultKeys.push_back(Configuration::DISC_OPTIONS);
    #endif

    titleFilters.clear();
    titleFilters.push_back("Ext.");
    titleFilters.push_back("Region");
    titleFilters.push_back("Ext. + Region");

    bootSpeeds.clear();
    bootSpeeds.push_back("Unsafe");
    bootSpeeds.push_back("Safe");
    bootSpeeds.push_back("Safest");

    perfModes.clear();
    perfModes.push_back("Off");
    perfModes.push_back("Text Only");
}

void DevSettings::updateBootSpeed(bool increase) {
    updateListVector(bootSpeeds, increase, false);
    settingsMap[Configuration::BOOT_SPEED].value = currentValue;
}

void DevSettings::updatePerfMode(bool increase) {
    updateListVector(perfModes, increase, false);
    settingsMap[Configuration::PERF_MODE].value = currentValue;
}

void DevSettings::updateTitleFiltering(bool increase) {
    updateListVector(titleFilters, increase, false);
    settingsMap[Configuration::TITLE_FILTER].value = currentValue;
}

int DevSettings::getDevToolSize(){
    std::vector<SettingRow> settingRows;

    for (const auto& key : enabledKeys) {

        if (key.find("DEVTOOLS.") == 0) {
        
            size_t pos = key.find_last_of(".");
            
            if (pos != std::string::npos) {
                try {
                    settingRows.push_back({languages.getOr(key.substr(pos + 1), defaultSettingLabel(key.substr(pos + 1))), 
                                            settingsMap[key].value
                                            });
                } catch (boost::property_tree::ptree_bad_path e) {
                    throw ItemNotFoundException("getDevSettings Language translation not found for " 
                    + key + " in " + languages.getLang());
                    
                }
            } else {
                throw ItemNotFoundException("Setting key format unknown: " 
                    + key);
            }
        }
    }
    return settingRows.size();
}


std::vector<Settings::SettingRow> DevSettings::getDevSettings() {
    std::vector<SettingRow> settingRows;

    for (const auto& key : enabledKeys) {

        if (key.find("DEVTOOLS.") == 0) {
        
            size_t pos = key.find_last_of(".");
            
            if (pos != std::string::npos) {
                try {
                    settingRows.push_back({languages.getOr(key.substr(pos + 1), defaultSettingLabel(key.substr(pos + 1))), 
                                            settingsMap[key].value
                                            });
                } catch (boost::property_tree::ptree_bad_path e) {
                    throw ItemNotFoundException("getDevSettings Language translation not found for " 
                    + key + " in " + languages.getLang());
                    
                }
            } else {
                throw ItemNotFoundException("Setting key format unknown: " 
                    + key);
            }
        }
    }
    return settingRows;
}

void AppSettings::updateOverscan(bool increase) {
    // 0..5% per edge, applied live and persisted
    int pct = 0; try { pct = std::stoi(settingsMap[Configuration::CRT_OVERSCAN].value); } catch (...) {}
    if (increase)      { if (pct < 5) pct += 1; }
    else               { if (pct > 0)  pct -= 1; }
    currentValue = std::to_string(pct) + "%";
    settingsMap[Configuration::CRT_OVERSCAN].value = currentValue;
    crt::setOverscan(pct);                                  // live on device, no-op off-CRT
    cfg.set(Configuration::CRT_OVERSCAN, currentValue);
    cfg.saveConfigIni();
}

void AppSettings::updateCrtFont(bool increase) {
    // applied live via notifySettingsChange and persisted
    updateListVector(crtFonts, increase, false);
    settingsMap[Configuration::CRT_FONT].value = currentValue;
    cfg.set(Configuration::CRT_FONT, currentValue);
    cfg.saveConfigIni();
}

void AppSettings::updateResolution(bool increase) {
    // "Default" (index 0) adopts native at boot, applied on Enter
    static const std::vector<std::string> kResList =
        {"Default", "320x240", "640x480", "960x540", "1024x768", "1280x720",
         "1920x1080", "2048x1536", "2560x1440"};
        // 2560x1440 = MiSTer pixel-repetition preset (vmode 14)

    int idx = 0;
    const std::string& cur = settingsMap[Configuration::SCREEN_RES].value;
    for (size_t i = 0; i < kResList.size(); i++)
        if (kResList[i] == cur) { idx = (int)i; break; }

    idx += increase ? 1 : -1;
    if (idx < 0) idx = (int)kResList.size() - 1;
    if (idx >= (int)kResList.size()) idx = 0;

    currentValue = kResList[idx];
    settingsMap[Configuration::SCREEN_RES].value = currentValue;
}

void DevSettings::updateLanguage(bool increase) {
    // stubbed: language switching disabled until the strings are translated
    (void)increase;
    currentValue = settingsMap[currentKey].value;
}

void DevSettings::updateShowFPS() {
    updateBoolSetting();
}

#ifdef HAS_PS1
void DevSettings::updatePSFast() {
    updateBoolSetting();
}
#endif
void DevSettings::updateAutoboot(){
    updateBoolSetting();
}
