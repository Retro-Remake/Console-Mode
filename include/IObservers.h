// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Retro Remake
// Derived from simplermenu_plus (rg35xx-cfw), MPL-2.0.
#pragma once

#include <string>

class ISettingsObserver {
 public:
  virtual ~ISettingsObserver(){};
  virtual void settingsChanged(const std::string &key,
                               const std::string &value) = 0;
  virtual std::string getName() = 0;
};

class ISettingsSubject {
 protected:
  virtual void notifySettingsChange(const std::string &key, const std::string &value) = 0;
 public:
  virtual ~ISettingsSubject(){};
  virtual void attach(ISettingsObserver *observer) = 0;
  virtual void detach(ISettingsObserver *observer) = 0;
  virtual std::string getName() = 0;
};

class ILanguageObserver {
 public:
  virtual ~ILanguageObserver(){};
  virtual void languageChanged() = 0;
  virtual std::string getName() = 0;
};

class ILanguageSubject {
 protected:
  virtual void notifyLanguageChange() = 0;
 public:
  virtual ~ILanguageSubject(){};
  virtual void attach(ILanguageObserver *observer) = 0;
  virtual void detach(ILanguageObserver *observer) = 0;
  virtual std::string getName() = 0;
};