// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/shields/shields_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/privacy/shields/shields_prefs.h"
#include "chrome/browser/privacy/shields/shields_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace privacy::shields {

// static
ShieldsServiceFactory* ShieldsServiceFactory::GetInstance() {
  static base::NoDestructor<ShieldsServiceFactory> instance;
  return instance.get();
}

// static
ShieldsService* ShieldsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ShieldsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ShieldsServiceFactory::ShieldsServiceFactory()
    : ProfileKeyedServiceFactory("ShieldsService",
                                 ProfileSelections::BuildForRegularProfile()) {}

ShieldsServiceFactory::~ShieldsServiceFactory() = default;

std::unique_ptr<KeyedService>
ShieldsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ShieldsService>(profile->GetPrefs());
}

void ShieldsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  prefs::RegisterProfilePrefs(registry);
}

}  // namespace privacy::shields
