// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_NATIVE_SHIELDS_CUSTOM_FILTER_LIST_SUBSCRIPTION_MANAGER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_NATIVE_SHIELDS_CUSTOM_FILTER_LIST_SUBSCRIPTION_MANAGER_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace extensions::declarative_net_request::native_shields {

// Manages user supplied Shields filter-list subscriptions without ever sending
// local browsing state to the subscription providers. Callers download list
// bytes independently and pass the body into CompileDownloadedList(); this
// manager persists only subscription metadata and compiled indexed rulesets.
class CustomFilterListSubscriptionManager {
 public:
  struct Budget {
    size_t max_rules_per_list = 30'000;
    size_t max_rules_global = 150'000;
    size_t max_indexed_bytes_per_list = 8 * 1024 * 1024;
    size_t max_indexed_bytes_global = 32 * 1024 * 1024;
    size_t max_download_bytes_per_list = 16 * 1024 * 1024;
  };

  enum class ListError {
    kNone,
    kInvalidUrl,
    kTooLarge,
    kInvalid,
    kChecksumMismatch,
    kBudgetExceeded,
    kPartiallyUnsupported,
    kStale,
    kIoError,
  };

  struct SubscriptionMetadata {
    std::string id;
    GURL url;
    bool enabled = true;
    base::TimeDelta update_interval = base::Days(7);
    base::Time last_attempt;
    base::Time last_success;
    std::string expected_sha256_hex;
    std::string last_sha256_hex;
    int indexed_checksum = 0;
    size_t rule_count = 0;
    size_t regex_rule_count = 0;
    size_t indexed_size_bytes = 0;
    ListError last_error = ListError::kNone;
    std::string last_error_message;
    base::FilePath indexed_ruleset_path;
  };

  struct CompileResult {
    bool success = false;
    bool rolled_back_to_last_known_good = false;
    ListError error = ListError::kNone;
    std::string message;
    std::vector<std::string> warnings;
    SubscriptionMetadata metadata;
  };

  using CompileCallback = base::OnceCallback<void(CompileResult)>;

  CustomFilterListSubscriptionManager(
      base::FilePath storage_dir,
      Budget budget,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  CustomFilterListSubscriptionManager(
      const CustomFilterListSubscriptionManager&) = delete;
  CustomFilterListSubscriptionManager& operator=(
      const CustomFilterListSubscriptionManager&) = delete;
  ~CustomFilterListSubscriptionManager();

  bool AddOrUpdateSubscription(std::string id,
                               GURL url,
                               base::TimeDelta update_interval,
                               std::string expected_sha256_hex);
  bool RemoveSubscription(const std::string& id);
  bool SetEnabled(const std::string& id, bool enabled);

  std::optional<SubscriptionMetadata> GetSubscription(
      const std::string& id) const;
  std::vector<SubscriptionMetadata> GetSubscriptions() const;
  std::vector<SubscriptionMetadata> GetEnabledSubscriptions() const;

  // Returns subscriptions that should be refreshed by the caller. This method
  // only examines metadata; it never contacts providers and never includes
  // browsing history or matched URLs in provider-visible data.
  std::vector<SubscriptionMetadata> GetSubscriptionsDueForUpdate(
      base::Time now) const;

  // Marks enabled subscriptions as stale once they are more than one full
  // update interval overdue, allowing UI to surface a clear stale-list error.
  bool RefreshStaleSubscriptionErrors(base::Time now);

  // Compiles `list_contents` using the DNR filter-list converter path on the
  // configured background sequence. The temporary downloaded filter-list and
  // intermediate JSON rules are deleted after compilation; only metadata and
  // the compiled indexed ruleset are kept.
  void CompileDownloadedList(std::string id,
                             std::string list_contents,
                             base::Time fetch_time,
                             CompileCallback callback);

  bool LoadMetadata();
  bool SaveMetadata() const;

  static const char* ListErrorToString(ListError error);

 private:
  static CompileResult CompileDownloadedListOnBackground(
      base::FilePath storage_dir,
      Budget budget,
      std::vector<SubscriptionMetadata> all_metadata,
      SubscriptionMetadata metadata,
      std::string list_contents,
      base::Time fetch_time);

  void OnCompileDone(std::string id,
                     CompileCallback callback,
                     CompileResult result);

  base::FilePath storage_dir_;
  Budget budget_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::map<std::string, SubscriptionMetadata> subscriptions_;
  base::WeakPtrFactory<CustomFilterListSubscriptionManager> weak_factory_{this};
};

}  // namespace extensions::declarative_net_request::native_shields

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_NATIVE_SHIELDS_CUSTOM_FILTER_LIST_SUBSCRIPTION_MANAGER_H_
