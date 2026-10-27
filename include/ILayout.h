// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// The getIntValue/getColor accessor interface derives from simplermenu_plus
// (rg35xx-cfw), MPL-2.0.
#pragma once
#include <string>
#include <SDL/SDL.h>

struct ThemeProp {
    std::string text;
    float a = 0, b = 0;           // first and second number
    bool num = false, pair = false;
    Uint32 rgba = 0;              // 0xRRGGBBAA
    bool color = false;
};

class ILayout {
public:
    virtual ~ILayout() = default;
    virtual int getIntValue(const std::string& key) const = 0;
    virtual SDL_Color getColor(const std::string& key) const = 0;
    virtual int marginX() const = 0;
    virtual int marginY() const = 0;
    virtual void setResolution(int screenWidth, int screenHeight) = 0;
    // a theme's font file for a role ("ui", "bold"), empty for the built-in face
    virtual std::string fontFile(const std::string&) const { return std::string(); }
    // a theme's 0xRRGGBB for a colour the renderer otherwise takes from settings
    virtual bool colorOverride(const std::string&, Uint32&) const { return false; }
    virtual bool metricSet(const std::string&) const { return false; }
    // any theme property by "view.element.property"
    virtual const ThemeProp* prop(const char*) const { return nullptr; }
};
