// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace privacy::shields {

class ShieldsService : public KeyedService {
 public:
  enum class Mode {
    kOff = 0,
    kBalanced = 1,
    kStrict = 2,
  };

  struct CustomFilterList {
    GURL url;
    bool enabled = true;
  };

  struct PageUiState {
    GURL site_url;
    bool allowlisted = false;
    int blocked_count = 0;
    bool show_blocked_count = true;
    bool cosmetic_filtering_enabled = true;
  };

  struct EvaluationResult {
    bool blocked = false;
    std::optional<GURL> redirect_url;
  };

  explicit ShieldsService(PrefService* prefs);
  ShieldsService(const ShieldsService&) = delete;
  ShieldsService& operator=(const ShieldsService&) = delete;
  ~ShieldsService() override;

  static Mode NormalizeMode(int pref_value);

  Mode GetMode() const;
  void SetMode(Mode mode);

  bool IsCosmeticFilteringEnabled() const;
  void SetCosmeticFilteringEnabled(bool enabled);

  bool ShouldShowBlockedCount() const;
  void SetShowBlockedCount(bool enabled);

  std::vector<CustomFilterList> GetCustomFilterLists() const;
  void SetCustomFilterLists(std::vector<CustomFilterList> lists);

  bool IsSiteExcepted(const GURL& url) const;
  void SetSiteException(const GURL& url, bool allowlisted);

  PageUiState GetPageUiState(const GURL& url) const;
  void ToggleAllowlistForSite(const GURL& url);

  // Converts EasyList/EasyPrivacy-compatible filter list text files to DNR JSON
  // through the browser process. The current implementation intentionally keeps
  // download/update orchestration outside renderers and page scripts; callers
  // pass local files obtained by the browser service's updater.
  bool ConvertFilterListsForInstall(
      const std::vector<base::FilePath>& filter_list_inputs,
      const base::FilePath& json_ruleset_output) const;

  // Installs verified indexed DNR rulesets owned by the browser service. The
  // built-in slot is reserved for browser-shipped balanced/strict rules; user
  // subscriptions are assigned stable ids by the browser service updater.
  bool InstallBuiltInRuleset(std::string indexed_ruleset_data);
  bool InstallUserRuleset(
      std::string indexed_ruleset_data,
      extensions::declarative_net_request::RulesetID ruleset_id);
  bool InstallIndexedRuleset(
      std::string indexed_ruleset_data,
      extensions::declarative_net_request::RulesetID ruleset_id);
  void ClearRulesets();

  EvaluationResult EvaluateRequest(const network::ResourceRequest& request);

 private:
  void OnPrefsChanged();
  std::string SiteKeyFromUrl(const GURL& url) const;
  url::Origin GetFirstPartyOrigin(
      const network::ResourceRequest& request) const;

  raw_ptr<PrefService> prefs_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<extensions::declarative_net_request::CompositeMatcher>
      matcher_;
  base::flat_map<std::string, int> blocked_counts_by_site_;
};

}  // namespace privacy::shields

#endif  // CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_SERVICE_H_
