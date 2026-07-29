// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include "utils.h"
#include <filesystem>

enum ConsoleModeExitCode {
	EXIT_CONSOLE_QUIT = 0,
	EXIT_CONSOLE_RUN_GAME = 10,   // value is the host contract
	EXIT_CONSOLE_CDROM_RUN = 11,
    EXIT_CONSOLE_KEEP_SHELL = 12,
};

#define MAX_BT_DEVS 4

#include <cctype>
#include <algorithm>

// natural case-insensitive sort, strips a trailing extension first
static inline std::string romSortKey(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0 && name.size() - dot <= 5 &&
        name.find(' ', dot) == std::string::npos)
        return name.substr(0, dot);
    return name;
}
static inline bool romNaturalLess(const std::string& A, const std::string& B) {
    size_t i = 0, j = 0;
    while (i < A.size() && j < B.size()) {
        unsigned char a = (unsigned char)A[i], b = (unsigned char)B[j];
        if (std::isdigit(a) && std::isdigit(b)) {
            size_t ia = i, jb = j;
            while (ia < A.size() && std::isdigit((unsigned char)A[ia])) ++ia;
            while (jb < B.size() && std::isdigit((unsigned char)B[jb])) ++jb;
            size_t sa = i; while (sa + 1 < ia && A[sa] == '0') ++sa;   // strip leading zeros
            size_t sb = j; while (sb + 1 < jb && B[sb] == '0') ++sb;
            size_t la = ia - sa, lb = jb - sb;
            if (la != lb) return la < lb;
            int cmp = A.compare(sa, la, B, sb, lb);
            if (cmp != 0) return cmp < 0;
            i = ia; j = jb;
        } else {
            unsigned char la = (unsigned char)std::tolower(a), lb = (unsigned char)std::tolower(b);
            if (la != lb) return la < lb;
            ++i; ++j;
        }
    }
    return (A.size() - i) < (B.size() - j);   // shorter remainder sorts first
}
static inline bool romNameLess(const std::string& a, const std::string& b) {
    return romNaturalLess(romSortKey(a), romSortKey(b));
}

struct BluetoothDevice {
    std::string mac;
    std::string name;
    int connectStatus;
    int battery;   // 0-100, or -1 if not reported

    bool operator==(const BluetoothDevice& other) const {
        return name == other.name && mac == other.mac;
    }
    void Init(){
        mac = "";
        name = "";
        connectStatus = 0;
        battery = -1;
    }
};

// one WiFi menu row parsed from wifi.sh scan output
struct WifiNetwork {
    std::string ssid;   // raw bytes, for display
    std::string enc;    // wpa_cli escaped form, matches list_networks
    std::string sec;    // Open / WEP / WPA / WPA2 / WPA3
    std::string band;   // 2.4GHz / 5GHz
    int bars = 0;       // 0-4 signal strength
    bool active = false;
    bool saeOnly = false;   // WPA3 with no WPA2-PSK fallback
};

class Rom {
private:
    std::string name;
    std::string path;
    bool isFolder;
    bool isCue;
    std::vector<Rom> roms;
    std::string launcher;
    bool lazy = false;                      // folder marker, children not built yet
    std::vector<std::string> lazyExts;      // exts to expand on first open
    int lazyEnterable = -1;                 // -1 unknown / 0 collapses / 1 browsable
public:
    Rom(const std::string& name, const std::string& path, std::string launcher = "", bool isFolder=false)
        : name(name), path(path),isFolder(isFolder),launcher(launcher) {isCue = false;}

    // defer building children until the folder is first opened
    void setLazy(const std::vector<std::string>& exts) { lazy = true; isFolder = true; lazyExts = exts; }
    bool isLazy() const { return lazy; }
    const std::vector<std::string>& getLazyExts() const { return lazyExts; }

    int  lazyPeek() const { return lazyEnterable; }
    void setLazyPeek(bool enterable) { lazyEnterable = enterable ? 1 : 0; }

    void takeChildrenFrom(Rom& src) { roms = std::move(src.roms); isCue = src.isCue; lazy = false; }

    std::string getTitle() const {
        return name;
    }

    std::string getPath() const {
        return path;
    }

    bool operator==(const Rom& other) const {
        return name == other.name && path == other.path;
    }

    bool IsFolder() const {
        return isFolder;
    }

    void addRom(const Rom& rom) {
        roms.push_back(rom);
    }

    std::vector<Rom>& getRoms() {
        return roms;
    }

    const std::vector<Rom>& getRoms() const {
        return roms;
    }

    std::string getlauncher() const {
        return launcher;
    }

    void setIsCue(bool cue) {
        isCue = cue;
    }

    bool IsCue() const {
        return isCue;
    }

    void SortRoms(){
        std::sort(roms.begin(), roms.end(),
            [](const Rom& a, const Rom& b) {
                if (a.isFolder != b.isFolder)
                    return a.isFolder;
                return romNameLess(a.name, b.name);
            }
        );
    }

};

class HomeBrewFolder {
public:
    std::string name;
    std::vector<Rom> roms;

    HomeBrewFolder(const std::string& name) : name(name) {}

    void addRom(const Rom& rom) {
        roms.push_back(rom);
    }

    std::string getTitle() const {
        return name;
    }

    const std::vector<Rom>& getRoms() const {
        return roms;
    }

};

class Folder {
private:
    std::string name;
    std::vector<Rom> roms;
    std::vector<Rom> usbRoms;
    bool isMerger;
public:
    Folder(const std::string& name) : name(name) {isMerger = false;}

    void addRom(const Rom& rom) {
        roms.push_back(rom);
    }

    void addUsbRom(const Rom& rom) {
        usbRoms.push_back(rom);
    }

    void mergerRom(){
        if (isMerger) return;
        if (usbRoms.size()==0)return;

        std::unordered_set<std::string> titles;
        for (const auto& rom : roms) titles.insert(rom.getTitle());
        for (const auto& usbrom : usbRoms) {
            if (titles.insert(usbrom.getTitle()).second) {
                addRom(usbrom);
            }
        }

        isMerger = true;
    }

    std::string getTitle() const {
        return name;
    }

    std::vector<Rom>& getRoms() {
        return roms;
    }

    const std::vector<Rom>& getRoms() const {
        return roms;
    }

};

class Section {
private:
    std::string name;
    std::string groupname;
public:
    std::vector<Folder> folders;

    Section(const std::string& name) : name(name) {
        std::filesystem::path ss(name);
        std::string groupnameTmp(ss.stem().string());
        std::transform(groupnameTmp.begin(), groupnameTmp.end(), groupnameTmp.begin(),
                       [](unsigned char c){ return (char)std::toupper(c); });
        groupname = groupnameTmp;
    }

    void addFolder(const Folder& folder) {
        folders.push_back(folder);
    }

    std::string getTitle() const {
        return name;
    }

    std::string getGroupname() const {
        return groupname;
    }

    Folder* getFolderByName(const std::string& name) {
        for (auto& sys : folders) {
            if (sys.getTitle() == name) {
                return &sys;
            }
        }
        return nullptr;
    }

    std::vector<Folder>& getFolders() {
        return folders;
    }

    const std::vector<Folder>& getFolders() const {
        return folders;
    }

};

class Menu {
public:
    std::vector<Section> sections;

    void addSection(const Section& section) {
        sections.push_back(section);
    }

    Section* getSectionByName(const std::string& name) {
        for (auto& sec : sections) {
            if (sec.getTitle() == name) {
                return &sec;
            }
        }
        return nullptr;
    }

    std::vector<Section>& getSections() {
        return sections;
    }

    const std::vector<Section>& getSections() const {
        return sections;
    }
};
