// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#include "Languages.h"
#include "Exception.h"
#include "IniSafe.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>

const std::string Languages::APP_SETTINGS = std::string("appSettings");
const std::string Languages::SYSTEM_SETTINGS = std::string("systemSettings");
const std::string Languages::ROM_SETTINGS = std::string("romSettings");

Languages::Languages(const std::string& languagesFilepath)
    : languagesFilepath(languagesFilepath) {

    // missing/corrupt ini must not abort boot, fall back to baked defaults
    inisafe::readIni(languagesFilepath, mainPt);

    for (const auto& section : this->mainPt) {
        std::string normalizedLang = boost::algorithm::to_upper_copy(section.first);
        languages.insert(normalizedLang);
    }

    // empty file leaves languages empty and *begin() would be UB
    lang = languages.empty() ? std::string("ENGLISH") : *(languages.begin());
}

std::string Languages::getLang() const {
    return lang;
}

void Languages::setLang(const std::string& newLang) {
    std::string normalizedLang = boost::algorithm::to_upper_copy(newLang);
    if (languages.find(normalizedLang) != languages.end()) {
        lang = normalizedLang;
    } else {
        throw ItemNotFoundException("Language unknown: " + newLang);
    }

}

namespace {
    // lang is upper-normalized but ptree keys keep the file's original case
    const boost::property_tree::ptree* findLangSection(
            const boost::property_tree::ptree& pt, const std::string& lang) {
        for (const auto& section : pt) {
            if (boost::algorithm::to_upper_copy(section.first) == lang) {
                return &section.second;
            }
        }
        return nullptr;
    }
}

std::string Languages::get(const std::string& id) const {
    // missing key falls back to the raw id instead of throwing
    return getOr(id, id);
}

std::string Languages::getOr(const std::string& id, const std::string& fallback) const {
    const auto* section = findLangSection(mainPt, lang);
    if (!section) {
        return fallback;
    }
    return section->get<std::string>(id, fallback);
}

std::set<std::string> Languages::getLanguages() const {
    return languages;
}
