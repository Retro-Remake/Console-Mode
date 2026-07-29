// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include "MenuCache.h"
#include "utils.h"
#include "IniSafe.h"
#include <fstream>

// coerce to valid UTF-8 (bad bytes -> '?') so the JSON writer never throws
static std::string sanitizeUtf8(const std::string& in) {
    std::string out; out.reserve(in.size());
    size_t i = 0, n = in.size();
    while (i < n) {
        unsigned char c = (unsigned char)in[i];
        int len = (c < 0x80) ? 1 : ((c >> 5) == 0x6) ? 2 : ((c >> 4) == 0xE) ? 3 : ((c >> 3) == 0x1E) ? 4 : 0;
        if (len == 0 || i + (size_t)len > n) { out += '?'; i++; continue; }
        bool ok = true;
        for (int k = 1; k < len; k++)
            if (((unsigned char)in[i + k] >> 6) != 0x2) { ok = false; break; }
        if (!ok) { out += '?'; i++; continue; }
        out.append(in, i, len);
        i += len;
    }
    return out;
}

void MenuCache::saveToCache(const std::string& filePath, const std::vector<CachedMenuItem>& data) {
    boost::property_tree::ptree root;

    for (const auto& item : data) {

        pt::ptree itemNode;
        itemNode.put("section", sanitizeUtf8(item.section));
        itemNode.put("folder", sanitizeUtf8(item.folder));
        itemNode.put("rom", sanitizeUtf8(item.rom));
        itemNode.put("path", sanitizeUtf8(item.path));
        itemNode.put("core", "default");
        itemNode.put("isFolder", item.isFolder);
        root.push_back(std::make_pair(sanitizeUtf8(item.rom), itemNode));   // key must be valid UTF-8 or read_json rejects the file
    }

    inisafe::writeJsonAtomic(filePath, root);   // atomic so a power cut can't truncate the cache
    inMemoryCache[filePath] = data;
}

std::vector<CachedMenuItem> MenuCache::loadFromCache(const std::string& filePath) {

    auto it = inMemoryCache.find(filePath);
    if (it != inMemoryCache.end()) {
        return it->second;
    }

    pt::ptree root;

    // corrupt cache -> empty so the caller rebuilds
    if (!inisafe::readJson(filePath, root)) return {};

    std::vector<CachedMenuItem> data;
    for (const auto& kv : root) {
        CachedMenuItem item;
        item.section = kv.second.get<std::string>("section", "");
        item.folder = kv.second.get<std::string>("folder", "");
        item.rom = kv.second.get<std::string>("rom", "");
        item.path = remapMediaFat(kv.second.get<std::string>("path", ""));
        item.core = kv.second.get<std::string>("core", "default");

        item.isFolder = kv.second.get<bool>("isFolder", false);

        data.push_back(item);
    }

    inMemoryCache[filePath] = data;

    return data;
}

bool MenuCache::cacheExists(const std::string& filePath) {
    std::ifstream infile(filePath);
    return infile.good();
}
