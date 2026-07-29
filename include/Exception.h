// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once
#include <stdexcept>
#include <string>

class SimplerMenuException : public std::runtime_error {
public:
    SimplerMenuException(const std::string& what_arg)
        : std::runtime_error(what_arg) { }
};

class ItemNotFoundException : public SimplerMenuException {
public:
    ItemNotFoundException(const std::string& what_arg)
        : SimplerMenuException(what_arg) { }
};

class StateNotFoundException : public SimplerMenuException {
public:
    StateNotFoundException(const std::string& what_arg)
        : SimplerMenuException(what_arg) { }
};
