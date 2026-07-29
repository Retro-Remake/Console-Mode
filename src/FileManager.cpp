// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include "FileManager.h"
#include "Configuration.h"
#include <vector>
#include <string>
#include <set>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {
bool pathIsInTree(const std::filesystem::path& path, const std::filesystem::path& root)
{
    auto pathIt = path.begin();
    auto rootIt = root.begin();

    for (; rootIt != root.end(); ++rootIt, ++pathIt) {
        if (pathIt == path.end() || *pathIt != *rootIt)
            return false;
    }

    return true;
}

bool isHiddenName(const std::string& filename)
{
    return !filename.empty() && filename[0] == '.';
}

bool shouldSkipGameDirectory(const std::string& dirname)
{
    return isHiddenName(dirname) || dirname == "bios" || dirname == "media";
}

bool pathMatchesRomExts(const std::filesystem::path& path, const std::vector<std::string>& romExts)
{
    std::string filename = path.filename().string();
    if (isHiddenName(filename) || filename == "sbi.zip")
        return false;

    std::string ext = lowerCopy(path.extension().string());
    for (const auto& romExt : romExts) {
        if (ext == lowerCopy(romExt))
            return true;
    }

    return false;
}

bool directoryContainsRomRecursive(const std::filesystem::path& dir, const std::vector<std::string>& romExts)
{
    std::error_code ec;
    auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator it(dir, options, ec);
    std::filesystem::recursive_directory_iterator end;

    while (!ec && it != end) {
        const auto& entry = *it;
        std::string filename = entry.path().filename().string();

        if (entry.is_directory(ec)) {
            if (ec) {
                ec.clear();
            } else if (shouldSkipGameDirectory(filename)) {
                it.disable_recursion_pending();
            }
        } else if (entry.is_regular_file(ec)) {
            if (ec) {
                ec.clear();
            } else if (pathMatchesRomExts(entry.path(), romExts)) {
                return true;
            }
        } else if (ec) {
            ec.clear();
        }

        it.increment(ec);
        if (ec)
            ec.clear();
    }

    return false;
}
}

std::vector<std::string> FileManager::getFiles(const std::string& folder) {
    std::vector<std::string> files;
    std::set<std::string> excludedExtensions = 
        cfg.getList("GLOBAL.excludedExtensions");

    try {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();

                if (isHiddenName(filename)) {
                    continue;
                }

                std::string ext = entry.path().extension().string();
                if (excludedExtensions.find(ext) == excludedExtensions.end()) {
                    files.push_back(entry.path().filename().string());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing directory " << folder << ": " << e.what() << std::endl;
    }
    
    std::sort(files.begin(), files.end(), romNameLess);
    return files;
}

std::vector<FileItem> FileManager::getFileItemsByExts(const std::string& folder, const std::vector<std::string> romExts,
                                                      const std::function<void(int)>& onProgress){

    std::vector<FileItem> fileItems;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            std::string filename = entry.path().filename().string();

            if (entry.is_regular_file()) {
                if (pathMatchesRomExts(entry.path(), romExts)) {
                    FileItem fi;
                    fi.fileName = filename;
                    fi.isFolder = false;
                    fileItems.push_back(fi);
                }
            }else if (entry.is_directory() && !shouldSkipGameDirectory(filename)) {
                if (directoryContainsRomRecursive(entry.path(), romExts)) {
                    FileItem fi;
                    fi.fileName = filename;
                    fi.isFolder = true;
                    fileItems.push_back(fi);
                }
            }
            if (onProgress) onProgress((int)fileItems.size());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing directory " << folder << ": " << e.what() << std::endl;
    }
    
    std::sort(fileItems.begin(), fileItems.end(), 
              [](const FileItem& a, const FileItem& b) {
                  if (a.isFolder != b.isFolder)
                      return a.isFolder;
                  return romNameLess(a.fileName, b.fileName);
              });
    return fileItems;
}


std::vector<std::string> FileManager::getFiles(const std::string& folder, const std::string& ext) {
    std::vector<std::string> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();

                if (isHiddenName(filename)) {
                    continue;
                }

                std::string extTmp = entry.path().extension().string();
                if (extTmp== ext) {
                    files.push_back(entry.path().stem().string());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing directory " << folder << ": " << e.what() << std::endl;
    }
    
    std::sort(files.begin(), files.end(), romNameLess);
    return files;
}

void FileManager::getAllRbfFilePaths(std::vector<std::string>& vecAllRbfFiles, std::vector<Rom>& vecRbfFiles)
{
    vecAllRbfFiles.clear();
    vecRbfFiles.clear();

#ifdef X86
    std::filesystem::path root = "/home/ubuntu/menuconfig/media/fat";
#else
    std::filesystem::path root = "/media/fat";
#endif
    std::filesystem::path consolePath = root / "_Console";
    std::filesystem::path computerPath = root / "_Computer";

    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return;
    }

    auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator it(root, options, ec);
    std::filesystem::recursive_directory_iterator end;

    while (it != end) {
        const auto& entry = *it;

        if (entry.is_regular_file(ec)) {
            std::string filename = entry.path().filename().string();
            if (isHiddenName(filename)) {
                it.increment(ec);
                if (ec) ec.clear();
                continue;
            }

            std::string ext = lowerCopy(entry.path().extension().string());

            if (ext == ".rbf") {
                vecAllRbfFiles.push_back(entry.path().string());

                if (pathIsInTree(entry.path(), consolePath) || pathIsInTree(entry.path(), computerPath)) {
                    std::string name = entry.path().stem().string();
                    Rom rom(name, entry.path());
                    vecRbfFiles.push_back(rom);
                }
            }
        }

        it.increment(ec);
        if (ec) ec.clear();
    }

    std::sort(vecAllRbfFiles.begin(), vecAllRbfFiles.end());
    std::sort(vecRbfFiles.begin(), vecRbfFiles.end(),
        [](const Rom& a, const Rom& b) {
            return a.getTitle() < b.getTitle();
        });
}

uint8_t FileManager::LoadVolume()
{
    std::string path = "";
#ifdef X86
    path ="/home/ubuntu/menuconfig/media/fat/config/Volume.dat";
#else
    path ="/media/fat/config/Volume.dat";
#endif
    
    std::ifstream file(path, std::ios::binary);

    // MiSTer volume byte: 0x10 = mute, low bits = attenuation (0 = loudest).
    // No Volume.dat yet (fresh SD) defaults to loudest, matching bare MiSTer.
    uint8_t vol = 0;
    file.read(reinterpret_cast<char*>(&vol), sizeof(vol));
    if (!file) vol = 0;
    vol &= 0x17;
    file.close();
    return vol;
}

void FileManager::SetVolume(uint8_t vol)
{
    std::string path = "";
#ifdef X86
    path ="/home/ubuntu/menuconfig/media/fat/config/Volume.dat";
#else
    path ="/media/fat/config/Volume.dat";
#endif
    
    std::ofstream file(path, std::ios::binary);
    if (!file) return;

    file.write(reinterpret_cast<const char*>(&vol), sizeof(uint8_t));

    file.close();
}

void FileManager::LoadBlueToothLog(std::vector<BluetoothDevice>& btInfo)
{
    btInfo.clear();
    std::string path = "";
#ifdef X86
    path ="/home/ubuntu/menuconfig/media/fat/ConsoleMode/bluetooth_devices.log";
#else
    path ="/tmp/consolemode/bluetooth_devices.log";
#endif
    
    std::ifstream file(path, std::ios::binary);

    BluetoothDevice device;

    std::string line;
    while (std::getline(file, line)) {
        // strip trailing CR from Windows files
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        
        if (!line.empty()) {

            device.Init();

            size_t nameStart = line.find("NAME=[");
            if (nameStart != std::string::npos) {
                size_t nameEnd = line.find(']', nameStart);
                if (nameEnd != std::string::npos && nameEnd > nameStart) {
                    device.name = line.substr(nameStart + 6, nameEnd - (nameStart + 6));
                }
            }

            size_t macStart = line.find(";MAC=[");
            if (macStart != std::string::npos) {
                size_t macEnd = line.find(']', macStart);
                if (macEnd != std::string::npos && macEnd > macStart) {
                    device.mac = line.substr(macStart + 6, macEnd - (macStart + 6));
                }
            }

            size_t connStart = line.find(";connected=[");
            if (connStart != std::string::npos) {
                size_t connEnd = line.find(']', connStart);
                if (connEnd != std::string::npos && connEnd > connStart) {
                    std::string status = line.substr(connStart + 12, connEnd - (connStart + 12));
                    if (status == "yes")
                        device.connectStatus = 1;
                    else
                        device.connectStatus = 0;
                }
            }

            size_t battStart = line.find(";battery=[");
            if (battStart != std::string::npos) {
                size_t battEnd = line.find(']', battStart);
                if (battEnd != std::string::npos && battEnd > battStart) {
                    std::string b = line.substr(battStart + 10, battEnd - (battStart + 10));
                    if (!b.empty()) device.battery = atoi(b.c_str());
                }
            }

            if (device.name != "" && device.mac != ""){
                btInfo.push_back(device);
            }
            std::string log = device.name + "," + device.mac + "," + std::to_string(device.connectStatus);
            logMessage(INFO,"LoadBlueToothLog",log.c_str());
        }
    }
    
    if (file.bad()) {
        std::cerr << "Warning: error reading file " << path << std::endl;
    }

    file.close();
}

void FileManager::getAllHomebrewFolders(std::vector<HomeBrewFolder> &folders){

    // homebrew folder -> MiSTer launcher name, where they differ
    std::map<std::string,std::string> launcherMap;

    launcherMap["MegaDrive"]="Genesis";
    launcherMap["N64"]="Nintendo64";
    launcherMap["PCE"]="TurboGrafx16";
    launcherMap["SMS"]="MasterSystem";
    launcherMap["TGFX16"]="TurboGrafx16";
    launcherMap["TGFX16-CD"]="TurboGrafx16CD";
    launcherMap["GAMEBOY"]="Gameboy";
    launcherMap["GBC"]="GameboyColor";

    std::string path = "";
    #ifdef X86
        // dev build reads the real homebrew dir via CONSOLEMODE_HOME
        if (const char* h = getenv("CONSOLEMODE_HOME"))
            path = std::string(h) + "/Homebrew";
        else
            path ="/home/ubuntu/menuconfig/media/fat/_Homebrew";
    #else
        path ="/media/fat/homebrew";
    #endif

    namespace fs = std::filesystem;

    if (!fs::exists(path)){
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();
                if (folderName.empty() || folderName[0] == '.')
                    continue;   // skip dot-dirs

                HomeBrewFolder folder(folderName);

                std::string launcher = folderName;
                auto it = launcherMap.find(folderName);
                if (it != launcherMap.end()){
                    launcher = it->second;
                }
                
                // recurse nested subfolders, but cue containers stay single leaves
                Rom tree(folderName, entry.path().string(), launcher, true);
                buildHomebrewRomTree(entry.path().string(), launcher, tree);
                for (const auto& rom : tree.getRoms())
                    folder.addRom(rom);

                folders.push_back(folder);
            }
        }
    } catch (const fs::filesystem_error& ex) {
        std::cerr << "filesystem err: " << ex.what() << std::endl;
    }
}

void FileManager::buildHomebrewRomTree(const std::string& dir, const std::string& launcher, Rom& outFolder)
{
    std::error_code ec;

    // a dir directly holding .cue files is one launchable game not a folder
    bool hasCue = false;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec) && !isHiddenName(entry.path().filename().string())
            && lowerCopy(entry.path().extension().string()) == ".cue") {
            hasCue = true;
            break;
        }
    }
    if (hasCue) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec) && !isHiddenName(entry.path().filename().string())
                && lowerCopy(entry.path().extension().string()) == ".cue") {
                Rom rom(entry.path().filename().string(), entry.path().string(), launcher);
                rom.setIsCue(true);
                outFolder.addRom(rom);
            }
        }
        outFolder.setIsCue(true);
        outFolder.SortRoms();
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        std::string filename = entry.path().filename().string();
        if (!filename.empty() && filename[0] == '.')
            continue;

        if (entry.is_regular_file(ec)) {
            Rom rom(filename, entry.path().string(), launcher);
            outFolder.addRom(rom);
        } else if (entry.is_directory(ec)) {
            if (filename == "media")
                continue;
            Rom child(filename, entry.path().string(), launcher, true);
            buildHomebrewRomTree(entry.path().string(), launcher, child);
            if (!child.getRoms().empty())
                outFolder.addRom(child);
        }
    }
    outFolder.SortRoms();
}
