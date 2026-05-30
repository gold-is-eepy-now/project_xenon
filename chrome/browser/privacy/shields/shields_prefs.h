// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_PREFS_H_
#define CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace privacy::shields::prefs {

inline constexpr char kGlobalMode[] = "privacy.shields.global_mode";
inline constexpr char kSiteExceptions[] = "privacy.shields.site_exceptions";
inline constexpr char kCustomFilterLists[] =
    "privacy.shields.custom_filter_lists";
inline constexpr char kCosmeticFilteringEnabled[] =
    "privacy.shields.cosmetic_filtering_enabled";
inline constexpr char kShowBlockedCount[] =
    "privacy.shields.show_blocked_count";

// Stored values for kGlobalMode.
inline constexpr int kModeOff = 0;
inline constexpr int kModeBalanced = 1;
inline constexpr int kModeStrict = 2;

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy::shields::prefs

#endif  // CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_PREFS_H_
