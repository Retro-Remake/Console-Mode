// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once

#include <string>
#include <map>
#include <vector>
#include "Languages.h"
#include "IObservers.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include "utils.h"

#define MiSTer "MiSTer"

class Configuration;

class Settings : public ISettingsSubject {
public: 
    struct Setting {
        std::string key;
        std::string value;
        bool enabled;
    };

    struct SettingRow {
        std::string title;
        std::string value;
    };

    virtual void navigateUp();
    virtual void navigateDown();
    virtual void navigateLeft() = 0;
    virtual void navigateRight() = 0;
    virtual void navigateEnter() = 0;

private:
    std::vector<ISettingsObserver *> observers;
    
protected:
    int currentIndex;

    std::vector<std::string> bgColors;
    std::vector<std::string> selColors;
    std::vector<std::string> gameStyles;
    std::vector<std::string> buttonStyles;
    std::vector<std::string> clockFormats;
    std::vector<std::string> btReconnectModes;
    std::vector<std::string> btRepairModes;
    std::vector<std::string> crtFonts;     // CRT-only

    Configuration& cfg;
    Languages& languages;
    std::vector<SettingRow> settingRows;

    std::vector<std::string> enabledKeys;

    std::map<std::string, Setting> settingsMap;

    int minValue;
    int maxValue;
    int delta;

    void updateInt(bool increase, std::string setting, 
                   int min, int max, int inc);
    

    std::set<std::string> cores;

    std::vector<std::string> getEnabledKeys();

    // "MiSTer INI Resolution" row: reads/edits the active ini's video_mode, applied on reboot
    std::string parseMisterIniVideoMode();
    void        updateMisterIniVideoMode(bool increase);
    bool        writeMisterIniVideoMode(int mode);
    std::string misterIniVmLabel(int sel);
public:
    bool        applyMisterIniVideoMode();

    // RetroAchievements: toggle a per-core "main=<binary>" chain-load override in the active ini
    std::string activeMisterIniPath();
    bool        misterIniMainOverrideEnabled(const std::string& coreName, const std::string& mainName);
    bool        setMisterIniMainOverride(const std::string& coreName, const std::string& mainName, bool enable);
    bool        misterIniHasRaWildcard();
protected:
    int         m_iniVmSel = 0;         // 0 = keep current, 1..15 = video_mode 0..14
    std::string m_iniVmCurrent;
    void notifySettingsChange(const std::string &key, const std::string &value) override;

public:

    std::vector<std::string> MisterInis;
    int iniIndex = 0;

    std::string currentKey;
    std::string currentValue;
    
public:

    void updateString(std::string key, std::string value);

    Settings(Configuration& cfg, Languages& languages,
             int minValue, int maxValue, int delta);

    std::vector<std::string> defaultKeys;

    void initializeSettings();

    void updateListSetting(const std::set<std::string>& values, bool increase);
    void updateBoolSetting();

    void updateListVector(const std::vector<std::string>& values, bool increase, bool isSystemConfig = true);

    std::string getCurrentKey();
    std::string getCurrentValue();
    int settingCount() const { return (int)enabledKeys.size(); }

    // display-only value for action rows, never persisted
    void setTransientValue(const std::string& key, const std::string& v) {
        if (settingsMap.count(key)) settingsMap[key].value = v;
    }

    // restrict the visible rows to a group's enabled keys and reset selection to the top
    void setVisibleGroup(const std::vector<std::string>& groupKeys) {
        enabledKeys.clear();
        for (const auto& k : groupKeys)
            if (settingsMap.count(k) && settingsMap[k].enabled) enabledKeys.push_back(k);
        currentIndex = 0;
        currentKey   = enabledKeys.empty() ? std::string() : enabledKeys[0];
        currentValue = settingsMap.count(currentKey) ? settingsMap[currentKey].value : std::string();
    }


    void attach(ISettingsObserver *observer) override;
    void detach(ISettingsObserver *observer) override;

    virtual std::string getName() = 0;
};

class AppSettings : public Settings {
public:

    AppSettings(Configuration& cfg, Languages& languages, 
                   int minValue, int maxValue, int delta);

    std::vector<Settings::SettingRow> getAppSettings();

    int getConfigIndex();
    
    void reloadSettingRows();

    void navigateUp() { Settings::navigateUp(); };
    void navigateDown() { Settings::navigateDown();};
    void navigateLeft() override {
        
        if (settingsMap[currentKey].enabled) {
            if (currentKey == Configuration::BG_COLOR){
                updateBgColor(false);
            } else if (currentKey == Configuration::SEL_COLOR){
                updateSelColor(false);
            } else if (currentKey == Configuration::BG_ART){
                updateBoolSetting();
            } else if (currentKey == Configuration::AB_SWAP){
                updateBoolSetting();
            } else if (currentKey == Configuration::GAME_STYLE) {
                updateGameStyle(false);
            } else if (currentKey == Configuration::BUTTON_STYLE) {
                updateButtonStyle(false);
            } else if (currentKey == Configuration::CLOCK_FORMAT) {
                updateClockFormat(false);
            } else if (currentKey == Configuration::BT_RECONNECT) {
                updateBtReconnect(false);
            } else if (currentKey == Configuration::BT_REPAIR) {
                updateBtRepair(false);
            } else if (currentKey == Configuration::SCREEN_RES) {
                updateResolution(false);
            } else if (currentKey == Configuration::MISTER_INI_RES) {
                updateMisterIniVideoMode(false);
            } else if (currentKey == Configuration::CRT_OVERSCAN) {
                updateOverscan(false);
            } else if (currentKey == Configuration::CRT_FONT) {
                updateCrtFont(false);
            }
        }

        notifySettingsChange(currentKey, currentValue);
    }

    void navigateRight() override {

        if (settingsMap[currentKey].enabled) {

            if (currentKey == Configuration::BG_COLOR) {
                updateBgColor(true);
            } else if (currentKey == Configuration::SEL_COLOR) {
                updateSelColor(true);
            } else if (currentKey == Configuration::BG_ART) {
                updateBoolSetting();
            } else if (currentKey == Configuration::GAME_STYLE) {
                updateGameStyle(true);
            } else if (currentKey == Configuration::BUTTON_STYLE) {
                updateButtonStyle(true);
            } else if (currentKey == Configuration::CLOCK_FORMAT) {
                updateClockFormat(true);
            } else if (currentKey == Configuration::BT_RECONNECT) {
                updateBtReconnect(true);
            } else if (currentKey == Configuration::BT_REPAIR) {
                updateBtRepair(true);
            } else if (currentKey == Configuration::SCREEN_RES) {
                updateResolution(true);
            } else if (currentKey == Configuration::MISTER_INI_RES) {
                updateMisterIniVideoMode(true);
            } else if (currentKey == Configuration::CRT_OVERSCAN) {
                updateOverscan(true);
            } else if (currentKey == Configuration::CRT_FONT) {
                updateCrtFont(true);
            }
        }

        notifySettingsChange(currentKey, currentValue);
    }


    void navigateEnter() override {
        if (settingsMap[currentKey].enabled) {
            if (currentKey == Configuration::QUIT)
                quitApplication();
            else if (currentKey == Configuration::UPDATE_TIME)
                updateTime();
            else if (currentKey == Configuration::SCREEN_RES) {
                // restart-applied: persist the pick and re-exec
                cfg.set(Configuration::SCREEN_RES, settingsMap[Configuration::SCREEN_RES].value);
                cfg.saveConfigIni();
                restartConsoleMode();   // does not return on success
            }
        }
    }
    
    void updateTime();
    
    
    void updateConfigIni(bool increase);

    void updateUSBMode(bool increase);
    
    void updateOverclock(bool increase);
    
    void restartApplication();
    void quitApplication();
    
    void updateBgColor(bool increase);
    void updateSelColor(bool increase);
    void updateGameStyle(bool increase);
    void updateButtonStyle(bool increase);
    void updateClockFormat(bool increase);
    void updateBtReconnect(bool increase);
    void updateBtRepair(bool increase);
    void updateResolution(bool increase);
    void updateOverscan(bool increase);
    void updateCrtFont(bool increase);

    std::string getName() override;
};


class DevSettings : public Settings {
public:

    DevSettings(Configuration& cfg, Languages& languages, 
                   int minValue, int maxValue, int delta);

    std::vector<Settings::SettingRow> getDevSettings();

    int getDevToolSize();

    int getConfigIndex();
    
    void reloadSettingRows();

    void navigateUp() { Settings::navigateUp(); };
    void navigateDown() { Settings::navigateDown();};
    void navigateLeft() override {
        if (settingsMap[currentKey].enabled) {            
            if (currentKey == Configuration::SCREEN_REFRESH) {
                updateInt(false, currentKey, minValue + delta, maxValue, delta);
            } else if (currentKey == Configuration::HISTORY_SIZE) {
                updateInt(false, currentKey, 5, 100, 5);
            } else if (currentKey == Configuration::SHOW_FPS) {
                updateShowFPS();

            } else if (currentKey == Configuration::LANGUAGE) {
                updateLanguage(false);
#ifdef HAS_PS1
            } else if (currentKey == Configuration::PS1_FAST) {
                updatePSFast();
#endif
            }else if (currentKey == Configuration::SHOW_HOMEBREW) {
                updateBoolSetting();
            }else if (currentKey == Configuration::AUTOBOOT) {
                updateAutoboot();
            }else if (currentKey == Configuration::BOOT_MAIN_MENU) {
                updateBoolSetting();
            }else if (currentKey == Configuration::TITLE_FILTER) {
                updateTitleFiltering(false);
            }else if (currentKey == Configuration::BOOT_SPEED) {
                updateBootSpeed(false);
            }else if (currentKey == Configuration::PERF_MODE) {
                updatePerfMode(false);
            }else if (currentKey == Configuration::LOCK_FLIX_TITLE) {
                updateBoolSetting();
            }else if (currentKey == Configuration::SS4_ANIM) {
                updateBoolSetting();
            }else if (currentKey == Configuration::USE_MIRROR) {
                updateBoolSetting();
            }else if (currentKey == Configuration::SHOW_RESUME) {
                updateBoolSetting();
            }
        }
        notifySettingsChange(currentKey, currentValue);
    }

    void navigateRight() override {
        if (settingsMap[currentKey].enabled) {
            if (currentKey == Configuration::SCREEN_REFRESH) {
                updateInt(true, currentKey, minValue + delta, maxValue, delta);

            } else if (currentKey == Configuration::HISTORY_SIZE) {
                updateInt(true, currentKey, 5, 100, 5);
            } else if (currentKey == Configuration::SHOW_FPS) {
                updateShowFPS();

            } else if (currentKey == Configuration::LANGUAGE) {
                updateLanguage(true);
#ifdef HAS_PS1
            } else if (currentKey == Configuration::PS1_FAST) {
                updatePSFast();
#endif
            }else if (currentKey == Configuration::SHOW_HOMEBREW) {
                updateBoolSetting();
            }else if (currentKey == Configuration::AUTOBOOT) {
                updateAutoboot();
            }else if (currentKey == Configuration::BOOT_MAIN_MENU) {
                updateBoolSetting();
            }else if (currentKey == Configuration::TITLE_FILTER) {
                updateTitleFiltering(true);
            }else if (currentKey == Configuration::BOOT_SPEED) {
                updateBootSpeed(true);
            }else if (currentKey == Configuration::PERF_MODE) {
                updatePerfMode(true);
            }else if (currentKey == Configuration::LOCK_FLIX_TITLE) {
                updateBoolSetting();
            }else if (currentKey == Configuration::SS4_ANIM) {
                updateBoolSetting();
            }else if (currentKey == Configuration::USE_MIRROR) {
                updateBoolSetting();
            }else if (currentKey == Configuration::SHOW_RESUME) {
                updateBoolSetting();
            }
        }
        notifySettingsChange(currentKey, currentValue);
    }


    void navigateEnter() override {
    }

    void updateShowFPS();
#ifdef HAS_PS1
    void updatePSFast();
#endif
    void updateLanguage(bool increase);
    void updateAutoboot();
    void updateTitleFiltering(bool increase);
    void updateBootSpeed(bool increase);
    void updatePerfMode(bool increase);

    std::vector<std::string> titleFilters;
    std::vector<std::string> bootSpeeds;
    std::vector<std::string> perfModes;

    std::string getName() override;
};



class RomSettings : public Settings, public ILanguageObserver {
public:
    RomSettings(Configuration& cfg, Languages& languages, 
                int minValue, int maxValue, int delta);

    std::vector<Settings::SettingRow> getRomSettings();

    void updateRomOverclock(bool increase);
    void updateAutoStart(bool increase);
    void updateCoreSelection(bool increase);
    void updateCoreOverride(bool increase);

    void navigateUp() { Settings::navigateUp(); };
    void navigateDown() { Settings::navigateDown();};
    void navigateEnter() override {
        ;
    };
    void navigateLeft() override {
        if (settingsMap[currentKey].enabled) {
            if (currentKey == Configuration::CORE_OVERRIDE) {
                updateCoreOverride(false);
            } else if (currentKey == Configuration::ROM_OVERCLOCK) {
                updateRomOverclock(false); 
            } else if (currentKey == Configuration::ROM_AUTOSTART) {
                updateAutoStart(false);            
            } else if (currentKey == Configuration::CORE_SELECTION) {
                updateCoreSelection(false);
            }  
        }
        notifySettingsChange(currentKey, currentValue);
    }

    void navigateRight() override {
        if (settingsMap[currentKey].enabled) {
            if (currentKey == Configuration::CORE_OVERRIDE) {
                updateCoreOverride(true);
            } else if (currentKey == Configuration::ROM_OVERCLOCK) {
                updateRomOverclock(true); 
            } else if (currentKey == Configuration::ROM_AUTOSTART) {
                updateAutoStart(true);         
            } else if (currentKey == Configuration::CORE_SELECTION) {
                updateCoreSelection(true);

            }     
        }
        notifySettingsChange(currentKey, currentValue);
    }
    void languageChanged() override;

    std::string getName() override;

public:
    void getCores(std::string sectionName, std::string folderName) {

        std::map<std::string, ConsoleData> consoleDataMap = cfg.parseIniFile(cfg.get(Configuration::HOME_PATH) + "ConsoleMode/themeconfig/section_groups/" + sectionName);

        cores.clear();

        if (consoleDataMap.find(folderName) != consoleDataMap.end()) {
            ConsoleData consoleData = consoleDataMap[folderName];

            if (!consoleData.execs.empty()) {
                for(auto exec: consoleData.execs) {
                    cores.insert(exec.substr(exec.find_last_of("/\\") + 1));
                }
            }
        }

        // TODO: per-rom core/launcher override
        std::string currentCore = *cores.begin();
        settingsMap[Configuration::CORE_OVERRIDE] = {Configuration::CORE_OVERRIDE, currentCore, true};
        notifySettingsChange(Configuration::CORE_OVERRIDE, currentCore);
    }
};


