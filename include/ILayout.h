// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// The getIntValue/getColor accessor interface derives from simplermenu_plus
// (rg35xx-cfw), MPL-2.0.
#pragma once
#include <string>
#include <SDL/SDL.h>

class ILayout {
public:
    virtual ~ILayout() = default;
    virtual int getIntValue(const std::string& key) const = 0;
    virtual SDL_Color getColor(const std::string& key) const = 0;
    virtual int marginX() const = 0;
    virtual int marginY() const = 0;
    virtual void setResolution(int screenWidth, int screenHeight) = 0;
};
