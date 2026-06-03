// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/native_shields/custom_filter_list_subscription_manager.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/filter_list_converter/converter.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/install_warning.h"

namespace extensions::declarative_net_request::native_shields {
namespace {

constexpr char kMetadataFileName[] = "subscriptions.json";
constexpr char kRulesetsDirectoryName[] = "rulesets";
constexpr char kNativeShieldsExtensionId[] = "native_shields";
constexpr int kNativeRulesetId = 1;

std::string SafeFileNameForId(const std::string& id) {
  std::string safe;
  safe.reserve(id.size());
  for (char c : id) {
    if (base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-' ||
        c == '_') {
      safe.push_back(c);
    } else {
      safe.push_back('_');
    }
  }
  return safe;
}

base::FilePath MetadataPath(const base::FilePath& storage_dir) {
  return storage_dir.AppendASCII(kMetadataFileName);
}

base::FilePath RulesetPathForId(const base::FilePath& storage_dir,
                                const std::string& id) {
  return storage_dir.AppendASCII(kRulesetsDirectoryName)
      .AppendASCII(SafeFileNameForId(id) + ".indexed");
}

base::FilePath LastKnownGoodPathForId(const base::FilePath& storage_dir,
                                      const std::string& id) {
  return storage_dir.AppendASCII(kRulesetsDirectoryName)
      .AppendASCII(SafeFileNameForId(id) + ".indexed.lkg");
}

std::string Sha256Hex(std::string_view contents) {
  return base::HexEncodeLower(crypto::SHA256HashString(contents));
}

bool IsSha256Hex(std::string_view value) {
  return value.size() == crypto::kSHA256Length * 2 &&
         std::ranges::all_of(value, [](char c) { return base::IsHexDigit(c); });
}

bool GetFileSize(const base::FilePath& path, size_t* size) {
  base::File::Info info;
  if (!base::GetFileInfo(path, &info) || info.is_directory || info.size < 0) {
    return false;
  }
  *size = static_cast<size_t>(info.size);
  return true;
}

base::Value::Dict MetadataToDict(
    const CustomFilterListSubscriptionManager::SubscriptionMetadata& metadata) {
  base::Value::Dict dict;
  dict.Set("id", metadata.id);
  dict.Set("url", metadata.url.spec());
  dict.Set("enabled", metadata.enabled);
  dict.Set("update_interval_seconds",
           static_cast<double>(metadata.update_interval.InSeconds()));
  dict.Set("last_attempt",
           metadata.last_attempt.InMillisecondsFSinceUnixEpoch());
  dict.Set("last_success",
           metadata.last_success.InMillisecondsFSinceUnixEpoch());
  dict.Set("expected_sha256_hex", metadata.expected_sha256_hex);
  dict.Set("last_sha256_hex", metadata.last_sha256_hex);
  dict.Set("indexed_checksum", metadata.indexed_checksum);
  dict.Set("rule_count", static_cast<double>(metadata.rule_count));
  dict.Set("regex_rule_count", static_cast<double>(metadata.regex_rule_count));
  dict.Set("indexed_size_bytes",
           static_cast<double>(metadata.indexed_size_bytes));
  dict.Set("last_error", static_cast<int>(metadata.last_error));
  dict.Set("last_error_message", metadata.last_error_message);
  dict.Set("indexed_ruleset_path",
           metadata.indexed_ruleset_path.AsUTF8Unsafe());
  return dict;
}

std::optional<CustomFilterListSubscriptionManager::SubscriptionMetadata>
MetadataFromDict(const base::Value::Dict& dict,
                 const base::FilePath& storage_dir) {
  const std::string* id = dict.FindString("id");
  const std::string* url = dict.FindString("url");
  if (!id || id->empty() || !url) {
    return std::nullopt;
  }

  CustomFilterListSubscriptionManager::SubscriptionMetadata metadata;
  metadata.id = *id;
  metadata.url = GURL(*url);
  if (!metadata.url.is_valid()) {
    return std::nullopt;
  }

  metadata.enabled = dict.FindBool("enabled").value_or(true);
  metadata.update_interval =
      base::Seconds(dict.FindDouble("update_interval_seconds")
                        .value_or(base::Days(7).InSecondsF()));
  metadata.last_attempt = base::Time::FromMillisecondsSinceUnixEpoch(
      dict.FindDouble("last_attempt").value_or(0));
  metadata.last_success = base::Time::FromMillisecondsSinceUnixEpoch(
      dict.FindDouble("last_success").value_or(0));
  if (const std::string* expected = dict.FindString("expected_sha256_hex")) {
    metadata.expected_sha256_hex = *expected;
  }
  if (const std::string* last = dict.FindString("last_sha256_hex")) {
    metadata.last_sha256_hex = *last;
  }
  metadata.indexed_checksum = dict.FindInt("indexed_checksum").value_or(0);
  metadata.rule_count =
      static_cast<size_t>(dict.FindDouble("rule_count").value_or(0));
  metadata.regex_rule_count =
      static_cast<size_t>(dict.FindDouble("regex_rule_count").value_or(0));
  metadata.indexed_size_bytes =
      static_cast<size_t>(dict.FindDouble("indexed_size_bytes").value_or(0));
  metadata.last_error =
      static_cast<CustomFilterListSubscriptionManager::ListError>(
          dict.FindInt("last_error").value_or(0));
  if (const std::string* error = dict.FindString("last_error_message")) {
    metadata.last_error_message = *error;
  }
  metadata.indexed_ruleset_path = RulesetPathForId(storage_dir, metadata.id);
  return metadata;
}

bool ReplaceFileKeepingLastKnownGood(const base::FilePath& source,
                                     const base::FilePath& destination,
                                     const base::FilePath& last_known_good) {
  if (!base::CreateDirectory(destination.DirName())) {
    return false;
  }
  if (base::PathExists(destination)) {
    base::DeleteFile(last_known_good);
    if (!base::Move(destination, last_known_good)) {
      return false;
    }
  }
  if (base::Move(source, destination)) {
    return true;
  }
  if (base::PathExists(last_known_good)) {
    base::Move(last_known_good, destination);
  }
  return false;
}

CustomFilterListSubscriptionManager::CompileResult ErrorResult(
    CustomFilterListSubscriptionManager::SubscriptionMetadata metadata,
    CustomFilterListSubscriptionManager::ListError error,
    std::string message,
    bool rolled_back) {
  CustomFilterListSubscriptionManager::CompileResult result;
  result.error = error;
  result.message = std::move(message);
  result.rolled_back_to_last_known_good = rolled_back;
  result.metadata = std::move(metadata);
  result.metadata.last_error = error;
  result.metadata.last_error_message = result.message;
  return result;
}

}  // namespace

CustomFilterListSubscriptionManager::CustomFilterListSubscriptionManager(
    base::FilePath storage_dir,
    Budget budget,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : storage_dir_(std::move(storage_dir)),
      budget_(budget),
      background_task_runner_(std::move(background_task_runner)) {
  DCHECK(background_task_runner_);
}

CustomFilterListSubscriptionManager::~CustomFilterListSubscriptionManager() =
    default;

bool CustomFilterListSubscriptionManager::AddOrUpdateSubscription(
    std::string id,
    GURL url,
    base::TimeDelta update_interval,
    std::string expected_sha256_hex) {
  if (id.empty() || !url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      update_interval <= base::TimeDelta() ||
      (!expected_sha256_hex.empty() && !IsSha256Hex(expected_sha256_hex))) {
    return false;
  }

  auto& metadata = subscriptions_[id];
  if (metadata.id.empty()) {
    metadata.id = id;
    metadata.enabled = true;
    metadata.indexed_ruleset_path = RulesetPathForId(storage_dir_, id);
  }
  metadata.url = std::move(url);
  metadata.update_interval = update_interval;
  metadata.expected_sha256_hex = base::ToLowerASCII(expected_sha256_hex);
  return SaveMetadata();
}

bool CustomFilterListSubscriptionManager::RemoveSubscription(
    const std::string& id) {
  if (!subscriptions_.erase(id)) {
    return false;
  }
  base::DeleteFile(RulesetPathForId(storage_dir_, id));
  base::DeleteFile(LastKnownGoodPathForId(storage_dir_, id));
  return SaveMetadata();
}

bool CustomFilterListSubscriptionManager::SetEnabled(const std::string& id,
                                                     bool enabled) {
  auto it = subscriptions_.find(id);
  if (it == subscriptions_.end()) {
    return false;
  }
  it->second.enabled = enabled;
  return SaveMetadata();
}

std::optional<CustomFilterListSubscriptionManager::SubscriptionMetadata>
CustomFilterListSubscriptionManager::GetSubscription(
    const std::string& id) const {
  auto it = subscriptions_.find(id);
  if (it == subscriptions_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<CustomFilterListSubscriptionManager::SubscriptionMetadata>
CustomFilterListSubscriptionManager::GetSubscriptions() const {
  std::vector<SubscriptionMetadata> subscriptions;
  for (const auto& [id, metadata] : subscriptions_) {
    subscriptions.push_back(metadata);
  }
  return subscriptions;
}

std::vector<CustomFilterListSubscriptionManager::SubscriptionMetadata>
CustomFilterListSubscriptionManager::GetEnabledSubscriptions() const {
  std::vector<SubscriptionMetadata> subscriptions;
  for (const auto& [id, metadata] : subscriptions_) {
    if (metadata.enabled) {
      subscriptions.push_back(metadata);
    }
  }
  return subscriptions;
}

std::vector<CustomFilterListSubscriptionManager::SubscriptionMetadata>
CustomFilterListSubscriptionManager::GetSubscriptionsDueForUpdate(
    base::Time now) const {
  std::vector<SubscriptionMetadata> due;
  for (const auto& [id, metadata] : subscriptions_) {
    if (!metadata.enabled) {
      continue;
    }
    if (metadata.last_success.is_null() ||
        now - metadata.last_success >= metadata.update_interval) {
      due.push_back(metadata);
    }
  }
  return due;
}

bool CustomFilterListSubscriptionManager::RefreshStaleSubscriptionErrors(
    base::Time now) {
  bool changed = false;
  for (auto& [id, metadata] : subscriptions_) {
    if (!metadata.enabled || metadata.last_success.is_null() ||
        now - metadata.last_success < metadata.update_interval * 2) {
      continue;
    }
    metadata.last_error = ListError::kStale;
    metadata.last_error_message =
        "The filter list is stale because it has not updated successfully "
        "within two configured update intervals.";
    changed = true;
  }
  return !changed || SaveMetadata();
}

void CustomFilterListSubscriptionManager::CompileDownloadedList(
    std::string id,
    std::string list_contents,
    base::Time fetch_time,
    CompileCallback callback) {
  auto it = subscriptions_.find(id);
  if (it == subscriptions_.end()) {
    SubscriptionMetadata metadata;
    metadata.id = id;
    std::move(callback).Run(
        ErrorResult(std::move(metadata), ListError::kInvalid,
                    "Unknown custom filter-list subscription.", false));
    return;
  }

  it->second.last_attempt = fetch_time;
  SaveMetadata();

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CustomFilterListSubscriptionManager::
                         CompileDownloadedListOnBackground,
                     storage_dir_, budget_, GetSubscriptions(), it->second,
                     std::move(list_contents), fetch_time),
      base::BindOnce(&CustomFilterListSubscriptionManager::OnCompileDone,
                     weak_factory_.GetWeakPtr(), id, std::move(callback)));
}

bool CustomFilterListSubscriptionManager::LoadMetadata() {
  subscriptions_.clear();
  const base::FilePath path = MetadataPath(storage_dir_);
  if (!base::PathExists(path)) {
    return true;
  }
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    return false;
  }
  std::optional<base::Value> value = base::JSONReader::Read(contents);
  if (!value || !value->is_list()) {
    return false;
  }
  for (const auto& entry : value->GetList()) {
    if (!entry.is_dict()) {
      continue;
    }
    auto metadata = MetadataFromDict(entry.GetDict(), storage_dir_);
    if (metadata) {
      subscriptions_[metadata->id] = std::move(*metadata);
    }
  }
  return true;
}

bool CustomFilterListSubscriptionManager::SaveMetadata() const {
  if (!base::CreateDirectory(storage_dir_)) {
    return false;
  }
  base::Value::List list;
  for (const auto& [id, metadata] : subscriptions_) {
    list.Append(MetadataToDict(metadata));
  }
  std::string contents;
  if (!base::JSONWriter::WriteWithOptions(
          list, base::JSONWriter::OPTIONS_PRETTY_PRINT, &contents)) {
    return false;
  }
  return base::WriteFile(MetadataPath(storage_dir_), contents);
}

const char* CustomFilterListSubscriptionManager::ListErrorToString(
    ListError error) {
  switch (error) {
    case ListError::kNone:
      return "none";
    case ListError::kInvalidUrl:
      return "invalid_url";
    case ListError::kTooLarge:
      return "too_large";
    case ListError::kInvalid:
      return "invalid";
    case ListError::kChecksumMismatch:
      return "checksum_mismatch";
    case ListError::kBudgetExceeded:
      return "budget_exceeded";
    case ListError::kPartiallyUnsupported:
      return "partially_unsupported";
    case ListError::kStale:
      return "stale";
    case ListError::kIoError:
      return "io_error";
  }
  NOTREACHED();
}

CustomFilterListSubscriptionManager::CompileResult
CustomFilterListSubscriptionManager::CompileDownloadedListOnBackground(
    base::FilePath storage_dir,
    Budget budget,
    std::vector<SubscriptionMetadata> all_metadata,
    SubscriptionMetadata metadata,
    std::string list_contents,
    base::Time fetch_time) {
  metadata.last_attempt = fetch_time;
  const bool has_lkg =
      base::PathExists(RulesetPathForId(storage_dir, metadata.id));

  if (list_contents.size() > budget.max_download_bytes_per_list) {
    return ErrorResult(
        std::move(metadata), ListError::kTooLarge,
        "The filter list download is larger than the per-list byte budget.",
        has_lkg);
  }

  const std::string sha256_hex = Sha256Hex(list_contents);
  if (!metadata.expected_sha256_hex.empty() &&
      sha256_hex != metadata.expected_sha256_hex) {
    return ErrorResult(
        std::move(metadata), ListError::kChecksumMismatch,
        "The filter list checksum does not match the subscription metadata.",
        has_lkg);
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return ErrorResult(
        std::move(metadata), ListError::kIoError,
        "Unable to create a temporary directory for filter-list compilation.",
        has_lkg);
  }

  const base::FilePath filter_list_path =
      temp_dir.GetPath().AppendASCII("list.txt");
  const base::FilePath json_rules_path =
      temp_dir.GetPath().AppendASCII("rules.json");
  if (!base::WriteFile(filter_list_path, list_contents)) {
    return ErrorResult(
        std::move(metadata), ListError::kIoError,
        "Unable to stage the downloaded filter list for compilation.", has_lkg);
  }

  if (!filter_list_converter::ConvertRuleset(
          {filter_list_path}, json_rules_path,
          filter_list_converter::kJSONRuleset, false)) {
    return ErrorResult(std::move(metadata), ListError::kInvalid,
                       "The filter list could not be converted into DNR rules.",
                       has_lkg);
  }

  std::unique_ptr<FileBackedRulesetSource> source =
      FileBackedRulesetSource::CreateTemporarySource(
          RulesetID(kNativeRulesetId), budget.max_rules_per_list,
          ExtensionId(kNativeShieldsExtensionId));
  if (!source) {
    return ErrorResult(std::move(metadata), ListError::kIoError,
                       "Unable to create a temporary DNR ruleset source.",
                       has_lkg);
  }

  if (!base::CopyFile(json_rules_path, source->json_path())) {
    return ErrorResult(std::move(metadata), ListError::kIoError,
                       "Unable to stage converted DNR rules for indexing.",
                       has_lkg);
  }

  IndexAndPersistJSONRulesetResult index_result =
      source->IndexAndPersistJSONRulesetUnsafe(
          RulesetSource::kRaiseWarningOnInvalidRules |
          RulesetSource::kRaiseWarningOnLargeRegexRules);

  if (index_result.status == IndexAndPersistJSONRulesetResult::Status::kError) {
    return ErrorResult(
        std::move(metadata), ListError::kInvalid,
        "The converted DNR rules are invalid: " + index_result.error, has_lkg);
  }
  if (index_result.status ==
      IndexAndPersistJSONRulesetResult::Status::kIgnore) {
    return ErrorResult(std::move(metadata), ListError::kTooLarge,
                       "The filter list exceeds the per-list DNR rule budget.",
                       has_lkg);
  }

  size_t indexed_size = 0;
  if (!GetFileSize(source->indexed_path(), &indexed_size)) {
    return ErrorResult(std::move(metadata), ListError::kIoError,
                       "Unable to read the compiled DNR ruleset size.",
                       has_lkg);
  }
  if (indexed_size > budget.max_indexed_bytes_per_list ||
      index_result.rules_count > budget.max_rules_per_list) {
    return ErrorResult(std::move(metadata), ListError::kTooLarge,
                       "The compiled filter list exceeds the per-list memory "
                       "or rule-count budget.",
                       has_lkg);
  }

  size_t global_rules = index_result.rules_count;
  size_t global_bytes = indexed_size;
  for (const auto& other : all_metadata) {
    if (other.id == metadata.id || !other.enabled) {
      continue;
    }
    global_rules += other.rule_count;
    global_bytes += other.indexed_size_bytes;
  }
  if (global_rules > budget.max_rules_global ||
      global_bytes > budget.max_indexed_bytes_global) {
    return ErrorResult(std::move(metadata), ListError::kBudgetExceeded,
                       "Enabling this filter list would exceed the global "
                       "Shields ruleset budget.",
                       has_lkg);
  }

  const base::FilePath final_path = RulesetPathForId(storage_dir, metadata.id);
  const base::FilePath lkg_path =
      LastKnownGoodPathForId(storage_dir, metadata.id);
  if (!ReplaceFileKeepingLastKnownGood(source->indexed_path(), final_path,
                                       lkg_path)) {
    return ErrorResult(std::move(metadata), ListError::kIoError,
                       "Unable to replace the previous compiled ruleset; the "
                       "last-known-good ruleset was kept.",
                       has_lkg);
  }

  metadata.last_success = fetch_time;
  metadata.last_sha256_hex = sha256_hex;
  metadata.indexed_checksum = index_result.ruleset_checksum;
  metadata.rule_count = index_result.rules_count;
  metadata.regex_rule_count = index_result.regex_rules_count;
  metadata.indexed_size_bytes = indexed_size;
  metadata.indexed_ruleset_path = final_path;
  metadata.last_error = ListError::kNone;
  metadata.last_error_message.clear();

  CompileResult result;
  result.success = true;
  result.metadata = std::move(metadata);
  for (const auto& warning : index_result.warnings) {
    result.warnings.push_back(warning.message);
  }
  if (!result.warnings.empty()) {
    result.error = ListError::kPartiallyUnsupported;
    result.message =
        "Some filter-list rules were ignored because they are unsupported by "
        "native Shields DNR matching.";
  }
  return result;
}

void CustomFilterListSubscriptionManager::OnCompileDone(
    std::string id,
    CompileCallback callback,
    CompileResult result) {
  auto it = subscriptions_.find(id);
  if (it != subscriptions_.end()) {
    it->second = result.metadata;
    SaveMetadata();
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace extensions::declarative_net_request::native_shields
