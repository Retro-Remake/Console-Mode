// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once

#include "AppCore.h"
#include "IObservers.h"
#include <vector>
#include <string>

class Application : public AppCore, public ISettingsObserver, public ILanguageSubject {
    std::vector<ILanguageObserver *> langObservers;
public:
    Application();
    ~Application() override = default;
    void run();
    void handleCommand(ControlMap cmd) override;
    void drawCurrentState();
    void launchRom();
    void settingsChanged(const std::string &key, const std::string &value) override;
    void attach(ILanguageObserver *observer) override;
    void detach(ILanguageObserver *observer) override;
    void notifyLanguageChange() override;
    std::string getName() override;
};
