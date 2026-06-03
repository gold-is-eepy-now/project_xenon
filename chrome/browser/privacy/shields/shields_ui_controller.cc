// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/shields/shields_ui_controller.h"

#include "chrome/browser/privacy/shields/shields_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace privacy::shields {

ShieldsUiController::ShieldsUiController(Profile* profile)
    : service_(ShieldsServiceFactory::GetForProfile(profile)) {}

ShieldsUiController::~ShieldsUiController() = default;

ShieldsService::PageUiState ShieldsUiController::GetPageState(
    const GURL& url) const {
  return service_->GetPageUiState(url);
}

void ShieldsUiController::SetCurrentSiteAllowlisted(const GURL& url,
                                                    bool allowlisted) {
  service_->SetSiteException(url, allowlisted);
}

void ShieldsUiController::ToggleCurrentSiteAllowlist(const GURL& url) {
  service_->ToggleAllowlistForSite(url);
}

}  // namespace privacy::shields
