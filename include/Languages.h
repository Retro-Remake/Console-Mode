// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <memory>

class Languages {
private:

    std::string languagesFilepath;

    boost::property_tree::ptree mainPt;

    std::set<std::string> languages;

    std::string lang;

public:

    static const std::string APP_SETTINGS;

    static const std::string SYSTEM_SETTINGS;

    static const std::string ROM_SETTINGS;

    Languages(const std::string& languagesFilepath);

    std::string getLang() const;
    void setLang(const std::string& newLang);

    std::string get(const std::string& id) const;

    // translation, or fallback if the key is absent
    std::string getOr(const std::string& id, const std::string& fallback) const;

    std::set<std::string> getLanguages() const;
};
