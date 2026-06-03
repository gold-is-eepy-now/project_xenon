// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/shields/shields_prefs.h"

#include "base/values.h"

namespace privacy::shields::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kGlobalMode, kModeBalanced);
  registry->RegisterListPref(kSiteExceptions);
  registry->RegisterListPref(kCustomFilterLists);
  registry->RegisterBooleanPref(kCosmeticFilteringEnabled, true);
  registry->RegisterBooleanPref(kShowBlockedCount, true);
}

}  // namespace privacy::shields::prefs
