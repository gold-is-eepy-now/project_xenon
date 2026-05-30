// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace privacy::shields {

class ShieldsService;

class ShieldsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ShieldsServiceFactory* GetInstance();
  static ShieldsService* GetForProfile(Profile* profile);

  ShieldsServiceFactory(const ShieldsServiceFactory&) = delete;
  ShieldsServiceFactory& operator=(const ShieldsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ShieldsServiceFactory>;

  ShieldsServiceFactory();
  ~ShieldsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace privacy::shields

#endif  // CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_SERVICE_FACTORY_H_
