// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "utils.h"
namespace pt = boost::property_tree;

struct CachedMenuItem {
    std::string section;
    std::string folder;
    std::string rom;
    std::string path;
    std::string core;
    bool isFolder;
};

class MenuCache {
private:
    std::unordered_map<std::string, std::vector<CachedMenuItem>> inMemoryCache;

public:
    MenuCache() = default;

    void saveToCache(const std::string& filePath, const std::vector<CachedMenuItem>& data);

    std::vector<CachedMenuItem> loadFromCache(const std::string& filePath);

    bool cacheExists(const std::string& filePath);
};
