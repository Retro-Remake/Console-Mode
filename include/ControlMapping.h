// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once

#include "Configuration.h"
#include <SDL/SDL.h>
#include <unordered_map>
#include <string>

enum ControlMap {
    CMD_UP,
    CMD_DOWN,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_ENTER,
    CMD_BACK,
    CMD_X,        
    CMD_Y,
    CMD_OPTIONS,
    CMD_SYS_SETTINGS,    
    CMD_NONE
};

class ControlMapping {
private:
    std::unordered_map<std::string, int> controls;
    Configuration& m_cfg;   // read live so runtime A/B swap applies

public:
    ControlMapping(Configuration& cfg);

    int getControl(const std::string& controlName) const;

    // fromPad: the event came from a gamepad, only those obey A/B swap
    ControlMap convertCommand(const SDL_Event& event, bool fromPad = false);
};

