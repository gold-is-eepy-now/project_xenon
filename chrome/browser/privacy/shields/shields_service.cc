// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/shields/shields_service.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/privacy/shields/shields_prefs.h"
#include "extensions/browser/api/declarative_net_request/filter_list_converter/converter.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/common/api/declarative_net_request.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace privacy::shields {
namespace {

namespace dnr = extensions::declarative_net_request;
namespace dnr_api = extensions::api::declarative_net_request;

constexpr char kSyntheticExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr size_t kRulesetRuleCountLimit = 300000;
constexpr char kFilterListUrlKey[] = "url";
constexpr char kFilterListEnabledKey[] = "enabled";

int ModeToPrefValue(ShieldsService::Mode mode) {
  switch (mode) {
    case ShieldsService::Mode::kOff:
      return prefs::kModeOff;
    case ShieldsService::Mode::kBalanced:
      return prefs::kModeBalanced;
    case ShieldsService::Mode::kStrict:
      return prefs::kModeStrict;
  }
}

dnr_api::RequestMethod MethodForRequest(
    const network::ResourceRequest& request) {
  if (base::EqualsCaseInsensitiveASCII(request.method, "GET")) {
    return dnr_api::RequestMethod::kGet;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "POST")) {
    return dnr_api::RequestMethod::kPost;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "PUT")) {
    return dnr_api::RequestMethod::kPut;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "DELETE")) {
    return dnr_api::RequestMethod::kDelete;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "HEAD")) {
    return dnr_api::RequestMethod::kHead;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "OPTIONS")) {
    return dnr_api::RequestMethod::kOptions;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "PATCH")) {
    return dnr_api::RequestMethod::kPatch;
  }
  if (base::EqualsCaseInsensitiveASCII(request.method, "CONNECT")) {
    return dnr_api::RequestMethod::kConnect;
  }
  return dnr_api::RequestMethod::kOther;
}

dnr_api::ResourceType ResourceTypeForRequest(
    const network::ResourceRequest& request) {
  using RequestDestination = network::mojom::RequestDestination;
  switch (request.destination) {
    case RequestDestination::kDocument:
      return request.is_outermost_main_frame ? dnr_api::ResourceType::kMainFrame
                                             : dnr_api::ResourceType::kSubFrame;
    case RequestDestination::kStyle:
      return dnr_api::ResourceType::kStylesheet;
    case RequestDestination::kScript:
      return dnr_api::ResourceType::kScript;
    case RequestDestination::kImage:
      return dnr_api::ResourceType::kImage;
    case RequestDestination::kFont:
      return dnr_api::ResourceType::kFont;
    case RequestDestination::kObject:
    case RequestDestination::kEmbed:
      return dnr_api::ResourceType::kObject;
    case RequestDestination::kAudio:
    case RequestDestination::kVideo:
      return dnr_api::ResourceType::kMedia;
    case RequestDestination::kReport:
      return dnr_api::ResourceType::kCspReport;
    default:
      if (request.is_fetch_like_api) {
        return dnr_api::ResourceType::kXmlhttprequest;
      }
      return dnr_api::ResourceType::kOther;
  }
}

}  // namespace

ShieldsService::ShieldsService(PrefService* prefs) : prefs_(prefs) {
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      prefs::kGlobalMode, base::BindRepeating(&ShieldsService::OnPrefsChanged,
                                              base::Unretained(this)));
}

ShieldsService::~ShieldsService() = default;

// static
ShieldsService::Mode ShieldsService::NormalizeMode(int pref_value) {
  switch (pref_value) {
    case prefs::kModeOff:
      return Mode::kOff;
    case prefs::kModeStrict:
      return Mode::kStrict;
    case prefs::kModeBalanced:
    default:
      return Mode::kBalanced;
  }
}

ShieldsService::Mode ShieldsService::GetMode() const {
  return NormalizeMode(prefs_->GetInteger(prefs::kGlobalMode));
}

void ShieldsService::SetMode(Mode mode) {
  prefs_->SetInteger(prefs::kGlobalMode, ModeToPrefValue(mode));
}

bool ShieldsService::IsCosmeticFilteringEnabled() const {
  return prefs_->GetBoolean(prefs::kCosmeticFilteringEnabled);
}

void ShieldsService::SetCosmeticFilteringEnabled(bool enabled) {
  prefs_->SetBoolean(prefs::kCosmeticFilteringEnabled, enabled);
}

bool ShieldsService::ShouldShowBlockedCount() const {
  return prefs_->GetBoolean(prefs::kShowBlockedCount);
}

void ShieldsService::SetShowBlockedCount(bool enabled) {
  prefs_->SetBoolean(prefs::kShowBlockedCount, enabled);
}

std::vector<ShieldsService::CustomFilterList>
ShieldsService::GetCustomFilterLists() const {
  std::vector<CustomFilterList> lists;
  for (const base::Value& value : prefs_->GetList(prefs::kCustomFilterLists)) {
    const base::Value::Dict* dict = value.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* url_string = dict->FindString(kFilterListUrlKey);
    if (!url_string) {
      continue;
    }
    GURL url(*url_string);
    if (!url.is_valid()) {
      continue;
    }
    lists.push_back(
        {url, dict->FindBool(kFilterListEnabledKey).value_or(true)});
  }
  return lists;
}

void ShieldsService::SetCustomFilterLists(std::vector<CustomFilterList> lists) {
  base::Value::List pref_lists;
  for (const CustomFilterList& list : lists) {
    if (!list.url.is_valid()) {
      continue;
    }
    pref_lists.Append(base::Value::Dict()
                          .Set(kFilterListUrlKey, list.url.spec())
                          .Set(kFilterListEnabledKey, list.enabled));
  }
  prefs_->SetList(prefs::kCustomFilterLists, std::move(pref_lists));
}

bool ShieldsService::IsSiteExcepted(const GURL& url) const {
  const std::string key = SiteKeyFromUrl(url);
  if (key.empty()) {
    return false;
  }
  return std::ranges::any_of(prefs_->GetList(prefs::kSiteExceptions),
                             [&key](const base::Value& value) {
                               return value.is_string() &&
                                      value.GetString() == key;
                             });
}

void ShieldsService::SetSiteException(const GURL& url, bool allowlisted) {
  const std::string key = SiteKeyFromUrl(url);
  if (key.empty()) {
    return;
  }

  base::Value::List exceptions =
      prefs_->GetList(prefs::kSiteExceptions).Clone();
  auto iter =
      std::ranges::find_if(exceptions, [&key](const base::Value& value) {
        return value.is_string() && value.GetString() == key;
      });
  const bool already_excepted = iter != exceptions.end();

  if (allowlisted && !already_excepted) {
    exceptions.Append(key);
  } else if (!allowlisted && already_excepted) {
    exceptions.erase(iter);
  }
  prefs_->SetList(prefs::kSiteExceptions, std::move(exceptions));
}

ShieldsService::PageUiState ShieldsService::GetPageUiState(
    const GURL& url) const {
  PageUiState state;
  state.site_url = url.GetWithEmptyPath();
  state.allowlisted = IsSiteExcepted(url);
  const auto count_iter = blocked_counts_by_site_.find(SiteKeyFromUrl(url));
  state.blocked_count =
      count_iter == blocked_counts_by_site_.end() ? 0 : count_iter->second;
  state.show_blocked_count = ShouldShowBlockedCount();
  state.cosmetic_filtering_enabled = IsCosmeticFilteringEnabled();
  return state;
}

void ShieldsService::ToggleAllowlistForSite(const GURL& url) {
  SetSiteException(url, !IsSiteExcepted(url));
}

bool ShieldsService::ConvertFilterListsForInstall(
    const std::vector<base::FilePath>& filter_list_inputs,
    const base::FilePath& json_ruleset_output) const {
  return extensions::declarative_net_request::filter_list_converter::
      ConvertRuleset(filter_list_inputs, json_ruleset_output,
                     extensions::declarative_net_request::
                         filter_list_converter::kJSONRuleset,
                     /*noisy=*/false);
}

bool ShieldsService::InstallBuiltInRuleset(std::string indexed_ruleset_data) {
  return InstallIndexedRuleset(std::move(indexed_ruleset_data),
                               dnr::kMinValidStaticRulesetID);
}

bool ShieldsService::InstallUserRuleset(std::string indexed_ruleset_data,
                                        dnr::RulesetID ruleset_id) {
  return InstallIndexedRuleset(std::move(indexed_ruleset_data), ruleset_id);
}

bool ShieldsService::InstallIndexedRuleset(std::string indexed_ruleset_data,
                                           dnr::RulesetID ruleset_id) {
  dnr::RulesetSource source(ruleset_id, kRulesetRuleCountLimit,
                            kSyntheticExtensionId, true);
  std::unique_ptr<dnr::RulesetMatcher> matcher;
  if (source.CreateVerifiedMatcher(std::move(indexed_ruleset_data), &matcher) !=
      dnr::LoadRulesetResult::kSuccess) {
    return false;
  }

  if (matcher_) {
    matcher_->AddOrUpdateRuleset(std::move(matcher));
    return true;
  }

  dnr::CompositeMatcher::MatcherList composite_matchers;
  composite_matchers.push_back(std::move(matcher));
  matcher_ = std::make_unique<dnr::CompositeMatcher>(
      std::move(composite_matchers), kSyntheticExtensionId,
      dnr::HostPermissionsAlwaysRequired::kFalse);
  return true;
}

void ShieldsService::ClearRulesets() {
  matcher_.reset();
}

ShieldsService::EvaluationResult ShieldsService::EvaluateRequest(
    const network::ResourceRequest& request) {
  if (GetMode() == Mode::kOff || !matcher_) {
    return {};
  }

  const url::Origin first_party_origin = GetFirstPartyOrigin(request);
  if (!first_party_origin.opaque() &&
      IsSiteExcepted(first_party_origin.GetURL())) {
    return {};
  }
  dnr::RequestParams params(
      request.url, request.request_initiator.value_or(first_party_origin),
      first_party_origin, ResourceTypeForRequest(request),
      MethodForRequest(request), /*tab_id=*/-1,
      /*response_headers=*/nullptr);
  dnr::CompositeMatcher::ActionInfo action_info =
      matcher_->GetAction(params, dnr::RulesetMatchingStage::kOnBeforeRequest,
                          extensions::PermissionsData::PageAccess::kAllowed);
  if (!action_info.action) {
    return {};
  }

  const dnr::RequestAction& action = *action_info.action;
  if (action.IsBlockOrCollapse()) {
    const std::string count_key = first_party_origin.Serialize();
    if (!count_key.empty()) {
      ++blocked_counts_by_site_[count_key];
    }
    EvaluationResult result;
    result.blocked = true;
    return result;
  }
  if (action.IsRedirectOrUpgrade() && action.redirect_url) {
    EvaluationResult result;
    result.redirect_url = action.redirect_url;
    return result;
  }
  return {};
}

void ShieldsService::OnPrefsChanged() {}

std::string ShieldsService::SiteKeyFromUrl(const GURL& url) const {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::string();
  }
  return url::Origin::Create(url).Serialize();
}

url::Origin ShieldsService::GetFirstPartyOrigin(
    const network::ResourceRequest& request) const {
  if (request.trusted_params) {
    const std::optional<url::Origin>& top_frame_origin =
        request.trusted_params->isolation_info.top_frame_origin();
    if (top_frame_origin) {
      return *top_frame_origin;
    }
  }
  if (request.request_initiator) {
    return *request.request_initiator;
  }
  return url::Origin::Create(request.url);
}

}  // namespace privacy::shields
