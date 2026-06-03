// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_UI_CONTROLLER_H_
#define CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_UI_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy/shields/shields_service.h"

class Profile;
class GURL;

namespace privacy::shields {

// Small browser-UI adapter used by toolbar and page-info surfaces to read the
// current site's state and update its allowlist exception without granting page
// script access to ruleset downloads or updates.
class ShieldsUiController {
 public:
  explicit ShieldsUiController(Profile* profile);
  ShieldsUiController(const ShieldsUiController&) = delete;
  ShieldsUiController& operator=(const ShieldsUiController&) = delete;
  ~ShieldsUiController();

  ShieldsService::PageUiState GetPageState(const GURL& url) const;
  void SetCurrentSiteAllowlisted(const GURL& url, bool allowlisted);
  void ToggleCurrentSiteAllowlist(const GURL& url);

 private:
  raw_ptr<ShieldsService> service_ = nullptr;
};

}  // namespace privacy::shields

#endif  // CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_UI_CONTROLLER_H_
