// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include "ControlMapping.h"
#include "utils.h"

ControlMapping::ControlMapping(Configuration& cfg) : m_cfg(cfg) {

    controls["KEY_A"] = cfg.getInt("CONTROLS.KEY_A");
    controls["KEY_B"] = cfg.getInt("CONTROLS.KEY_B");
    controls["KEY_X"] = cfg.getInt("CONTROLS.KEY_X");
    controls["KEY_Y"] = cfg.getInt("CONTROLS.KEY_Y");
    controls["KEY_L1"] = cfg.getInt("CONTROLS.KEY_L1");
    controls["KEY_L2"] = cfg.getInt("CONTROLS.KEY_L2");
    controls["KEY_R1"] = cfg.getInt("CONTROLS.KEY_R1");
    controls["KEY_R2"] = cfg.getInt("CONTROLS.KEY_R2");
    controls["KEY_UP"] = cfg.getInt("CONTROLS.KEY_UP");
    controls["KEY_DOWN"] = cfg.getInt("CONTROLS.KEY_DOWN");
    controls["KEY_LEFT"] = cfg.getInt("CONTROLS.KEY_LEFT");
    controls["KEY_RIGHT"] = cfg.getInt("CONTROLS.KEY_RIGHT");
    controls["KEY_START"] = cfg.getInt("CONTROLS.KEY_START");
    controls["KEY_SELECT"] = cfg.getInt("CONTROLS.KEY_SELECT");

    controls["BTN_A"] = cfg.getInt("CONTROLS.BTN_A");
    controls["BTN_B"] = cfg.getInt("CONTROLS.BTN_B");
    controls["BTN_X"] = cfg.getInt("CONTROLS.BTN_X");
    controls["BTN_Y"] = cfg.getInt("CONTROLS.BTN_Y");
    controls["BTN_L1"] = cfg.getInt("CONTROLS.BTN_L1");
    controls["BTN_L2"] = cfg.getInt("CONTROLS.BTN_L2");
    controls["BTN_R1"] = cfg.getInt("CONTROLS.BTN_R1");
    controls["BTN_R2"] = cfg.getInt("CONTROLS.BTN_R2");
    controls["BTN_UP"] = cfg.getInt("CONTROLS.BTN_UP");
    controls["BTN_DOWN"] = cfg.getInt("CONTROLS.BTN_DOWN");
    controls["BTN_LEFT"] = cfg.getInt("CONTROLS.BTN_LEFT");
    controls["BTN_RIGHT"] = cfg.getInt("CONTROLS.BTN_RIGHT");
    controls["BTN_START"] = cfg.getInt("CONTROLS.BTN_START");
    controls["BTN_SELECT"] = cfg.getInt("CONTROLS.BTN_SELECT");
}

int ControlMapping::getControl(const std::string& controlName) const {
    auto it = controls.find(controlName);
    if (it != controls.end()) {
        return it->second;
    } else {
        return -1;
    }
}

ControlMap ControlMapping::convertCommand(const SDL_Event& event) {
    // with A/B swap on, A backs out and B confirms
    const bool swapAB = m_cfg.getBool(Configuration::AB_SWAP);
    const ControlMap A_CMD = swapAB ? CMD_BACK : CMD_ENTER;
    const ControlMap B_CMD = swapAB ? CMD_ENTER : CMD_BACK;
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == getControl("KEY_A")) return A_CMD;
        if (event.key.keysym.sym == getControl("KEY_B")) return B_CMD;
        if (event.key.keysym.sym == getControl("KEY_UP")) return CMD_UP;
        if (event.key.keysym.sym == getControl("KEY_DOWN")) return CMD_DOWN;
        if (event.key.keysym.sym == getControl("KEY_LEFT")) return CMD_LEFT;
        if (event.key.keysym.sym == getControl("KEY_RIGHT")) return CMD_RIGHT;
        if (event.key.keysym.sym == getControl("KEY_X")) return CMD_X;
        if (event.key.keysym.sym == getControl("KEY_Y")) return CMD_Y;
        if (event.key.keysym.sym == getControl("KEY_SELECT")) return CMD_OPTIONS;
        
        // keyboard fallbacks (macOS only, device uses evdev)
        if (event.key.keysym.sym == 27) return B_CMD;           // Escape / virtual-pad B
        if (event.key.keysym.sym == 13) return A_CMD;           // Return / virtual-pad A
        if (event.key.keysym.sym == 32) return CMD_X;           // Space  = X (search)
        if (event.key.keysym.sym == 273) return CMD_UP;         // arrow keys (SDL 1.2 syms)
        if (event.key.keysym.sym == 274) return CMD_DOWN;
        if (event.key.keysym.sym == 275) return CMD_RIGHT;
        if (event.key.keysym.sym == 276) return CMD_LEFT;
        if (event.key.keysym.sym == 9)  return CMD_SYS_SETTINGS; // Tab = Start (settings)
        
    }
    if (event.type == SDL_JOYBUTTONDOWN) {
        if (event.jbutton.button == getControl("BTN_A")) return A_CMD;
        if (event.jbutton.button == getControl("BTN_B")) return B_CMD;
        if (event.jbutton.button == getControl("BTN_UP"))
        {
            logMessage(INFO,"SDL_JOYBUTTONDOWN 1","UP");
            return CMD_UP;
        }
        if (event.jbutton.button == getControl("BTN_DOWN")) return CMD_DOWN;
        if (event.jbutton.button == getControl("BTN_LEFT")) return CMD_LEFT;
        if (event.jbutton.button == getControl("BTN_RIGHT")) return CMD_RIGHT;
        if (event.jbutton.button == getControl("BTN_X")) return CMD_X;
        if (event.jbutton.button == getControl("BTN_Y")) return CMD_Y;
        if (event.jbutton.button == getControl("BTN_SELECT")) return CMD_OPTIONS;
        if (event.jbutton.button == getControl("BTN_START")) return CMD_SYS_SETTINGS;
    } else if (event.type == SDL_JOYAXISMOTION) {
        int axis = event.jaxis.axis;
        int value = event.jaxis.value;
        char buf[64];
        sprintf(buf,"%d,%d",axis,value);

        if (abs(value) < 30000 )
            return CMD_NONE;

        if (axis == 1) {    // y axis inverted here: negative = down
            if (value < 0) {
                logMessage(INFO,"SDL_JOYAXISMOTION CMD_DOWN",buf);
                return CMD_DOWN;
            } else {
                logMessage(INFO,"SDL_JOYAXISMOTION CMD_UP",buf);
                return CMD_UP;
            }
        } else if (axis == 0) { // x axis inverted here: negative = right
            if (value < 0) {
                logMessage(INFO,"SDL_JOYAXISMOTION CMD_RIGHT",buf);
                return CMD_RIGHT;
            } else {
                logMessage(INFO,"SDL_JOYAXISMOTION CMD_LEFT",buf);
                return CMD_LEFT;
            }
        }
    } else if (event.type == SDL_JOYHATMOTION) {
        if (event.jhat.value == SDL_HAT_UP)
        {
            logMessage(INFO,"SDL_JOYHATMOTION","UP");
            return CMD_UP;
        }
            
        if (event.jhat.value == SDL_HAT_DOWN) {
            logMessage(INFO,"SDL_JOYHATMOTION","DOWN");
            return CMD_DOWN;
        }
        if (event.jhat.value == SDL_HAT_RIGHT) 
        {
            logMessage(INFO,"SDL_JOYHATMOTION","RIGHT");
            return CMD_RIGHT;
        }
            
        if (event.jhat.value == SDL_HAT_LEFT) 
        {
            logMessage(INFO,"SDL_JOYHATMOTION","LEFT");
            return CMD_LEFT;
        }            
    }

    return CMD_NONE; 
}
