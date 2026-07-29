// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once
#include <vector>
#include <string>
#include <set>
#include <functional>
#include "Configuration.h"
#include "Menu.h"

struct FileItem{
    std::string fileName;
    bool isFolder;
};

class FileManager {

private:
    Configuration& cfg;

public:

    FileManager(Configuration& cfg): cfg(cfg) {}

    std::vector<std::string> getFiles(const std::string& folder);

    std::vector<std::string> getFiles(const std::string& folder, const std::string& ext);

    std::vector<FileItem> getFileItemsByExts(const std::string& folder, const std::vector<std::string> romExts,
                                             const std::function<void(int)>& onProgress = nullptr);

    void getAllRbfFilePaths(std::vector<std::string>& vecAllRbfFiles, std::vector<Rom>& vecRbfFiles);

    uint8_t LoadVolume();
    void SetVolume(uint8_t vol);

    void LoadBlueToothLog(std::vector<BluetoothDevice>& btInfo);

    void getAllHomebrewFolders(std::vector<HomeBrewFolder> &folders);

    // a dir holding .cue files becomes one cue-game leaf, else recurse to any depth
    void buildHomebrewRomTree(const std::string& dir, const std::string& launcher, Rom& outFolder);
};
