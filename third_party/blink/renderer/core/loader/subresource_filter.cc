// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/subresource_filter.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_names.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

String GetErrorStringForDisallowedLoad(const KURL& url) {
  StringBuilder builder;
  builder.Append("Chrome blocked resource ");
  builder.Append(url.GetString());
  builder.Append(
      " on this site because this site tends to show ads that interrupt, "
      "distract, mislead, or prevent user control. Learn more at "
      "https://www.chromestatus.com/feature/5738264052891648");
  return builder.ToString();
}

}  // namespace

namespace {

constexpr unsigned kMaxCosmeticSelectorsPerDocument = 2000;
constexpr unsigned kMaxCosmeticElementsToInspect = 20000;
constexpr unsigned kMaxCosmeticDebugSelectorMatches = 200;
constexpr char kCosmeticFilteringStyleId[] =
    "subresource-filter-cosmetic-style";
constexpr char kCosmeticFilteringDebugPanelId[] =
    "subresource-filter-hidden-elements-debug-panel";

bool IsSafeCosmeticSelector(std::string_view selector) {
  if (selector.empty() || selector.size() > 4096) {
    return false;
  }
  for (char c : selector) {
    // The selector is embedded before a generated declaration block. Reject
    // characters that could terminate that selector context or create markup.
    if (c == '{' || c == '}' || c == '<' || c == '>' || c == '\0') {
      return false;
    }
  }
  return true;
}

void AppendSelector(std::string_view selector,
                    std::vector<std::string>& selectors,
                    bool& cap_hit) {
  if (!IsSafeCosmeticSelector(selector)) {
    return;
  }
  if (selectors.size() >= kMaxCosmeticSelectorsPerDocument) {
    cap_hit = true;
    return;
  }
  if (std::ranges::find(selectors, selector) == selectors.end()) {
    selectors.emplace_back(selector);
  }
}

String BuildCosmeticStyleSheet(const std::vector<std::string>& selectors) {
  StringBuilder builder;
  for (const std::string& selector : selectors) {
    builder.Append(String::FromUtf8(selector));
    builder.Append("{display:none!important;}\n");
  }
  return builder.ToString();
}

String BuildDebugPanelText(unsigned selector_count,
                           unsigned hidden_element_count,
                           bool cap_hit) {
  StringBuilder builder;
  builder.Append("Shields cosmetic filtering\\A ");
  builder.AppendNumber(selector_count);
  builder.Append(" selectors applied\\A ");
  builder.AppendNumber(hidden_element_count);
  builder.Append(" blocked/hidden elements matched");
  if (cap_hit) {
    builder.Append("\\A selector cap reached");
  }
  return builder.ToString();
}

void InjectCosmeticStyle(Document& document,
                         const AtomicString& style_id,
                         const String& css_text) {
  Element* root =
      document.head() ? document.head() : document.documentElement();
  if (!root) {
    return;
  }

  if (Element* existing = document.getElementById(style_id)) {
    existing->remove(IGNORE_EXCEPTION);
  }

  auto* style_element = MakeGarbageCollected<HTMLStyleElement>(document);
  style_element->setAttribute(html_names::kIdAttr, style_id);
  style_element->SetInnerHTMLWithoutTrustedTypes(css_text);
  root->AppendChild(style_element);
}

void InjectCosmeticDebugPanel(Document& document,
                              unsigned selector_count,
                              unsigned hidden_element_count,
                              bool cap_hit) {
  Element* root =
      document.body() ? document.body() : document.documentElement();
  if (!root) {
    return;
  }

  if (Element* existing = document.getElementById(
          AtomicString(String::FromUtf8(kCosmeticFilteringDebugPanelId)))) {
    existing->remove(IGNORE_EXCEPTION);
  }

  StringBuilder css;
  css.Append("#");
  css.Append(kCosmeticFilteringDebugPanelId);
  css.Append(
      "{position:fixed!important;right:8px!important;bottom:8px!important;"
      "z-index:2147483647!important;display:block!important;background:#111!"
      "important;"
      "color:#fff!important;border:1px solid #555!important;"
      "border-radius:4px!important;padding:8px!important;font:12px "
      "sans-serif!important;white-space:pre!important;opacity:.92!important;}"
      "#");
  css.Append(kCosmeticFilteringDebugPanelId);
  css.Append("::before{content:\"");
  css.Append(
      BuildDebugPanelText(selector_count, hidden_element_count, cap_hit));
  css.Append("\";}");

  InjectCosmeticStyle(document, AtomicString("subresource-filter-debug-style"),
                      css.ToString());

  auto* panel = MakeGarbageCollected<HTMLDivElement>(document);
  panel->setAttribute(html_names::kIdAttr,
                      AtomicString(kCosmeticFilteringDebugPanelId));
  panel->setAttribute(html_names::kTitleAttr,
                      AtomicString("Blocked/hidden elements debug panel"));
  root->AppendChild(panel);
}

}  // namespace

SubresourceFilter::SubresourceFilter(
    ExecutionContext* execution_context,
    std::unique_ptr<WebDocumentSubresourceFilter> subresource_filter)
    : execution_context_(execution_context),
      subresource_filter_(std::move(subresource_filter)) {
  DCHECK(subresource_filter_);
}

SubresourceFilter::~SubresourceFilter() = default;

bool SubresourceFilter::AllowLoad(
    const KURL& resource_url,
    network::mojom::RequestDestination request_destination,
    ReportingDisposition reporting_disposition) {
  // TODO(csharrison): Implement a caching layer here which is a HashMap of
  // Pair<url string, context> -> LoadPolicy.
  subresource_filter::ScopedRule rule;
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicy(resource_url, request_destination,
                                         /*out_rule=*/&rule);

  if (reporting_disposition == ReportingDisposition::kReport) {
    ReportLoad(resource_url, load_policy);
  }

  last_resource_check_result_ = std::make_pair(
      std::make_pair(resource_url, request_destination),
      ResourceCheckResult{.load_policy = load_policy, .rule = std::move(rule)});

  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

void SubresourceFilter::ReportLoadAsync(
    const KURL& resource_url,
    WebDocumentSubresourceFilter::LoadPolicy load_policy) {
  // Post a task to notify this load to avoid unduly blocking the worker
  // thread. Note that this unconditionally calls reportLoad unlike allowLoad,
  // because there aren't developer-invisible connections (like speculative
  // preloads) happening here.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context_->GetTaskRunner(TaskType::kNetworking);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  task_runner->PostTask(
      FROM_HERE, BindOnce(&SubresourceFilter::ReportLoad, WrapPersistent(this),
                          resource_url, load_policy));
}

bool SubresourceFilter::AllowWebSocketConnection(const KURL& url) {
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicyForWebSocketConnect(url);

  ReportLoadAsync(url, load_policy);
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::AllowWebTransportConnection(const KURL& url) {
  WebDocumentSubresourceFilter::LoadPolicy load_policy =
      subresource_filter_->GetLoadPolicyForWebTransportConnect(url);

  ReportLoadAsync(url, load_policy);
  return load_policy != WebDocumentSubresourceFilter::kDisallow;
}

bool SubresourceFilter::IsAdResource(
    const KURL& resource_url,
    network::mojom::RequestDestination request_destination,
    subresource_filter::ScopedRule* out_rule) {
  WebDocumentSubresourceFilter::LoadPolicy load_policy;
  if (last_resource_check_result_.first ==
      std::make_pair(resource_url, request_destination)) {
    load_policy = last_resource_check_result_.second.load_policy;
    if (out_rule) {
      *out_rule = last_resource_check_result_.second.rule;
    }
  } else {
    load_policy = subresource_filter_->GetLoadPolicy(
        resource_url, request_destination, out_rule);
  }

  return load_policy != WebDocumentSubresourceFilter::kAllow;
}

void SubresourceFilter::ReportLoad(
    const KURL& resource_url,
    WebDocumentSubresourceFilter::LoadPolicy load_policy) {
  switch (load_policy) {
    case WebDocumentSubresourceFilter::kAllow:
      break;
    case WebDocumentSubresourceFilter::kDisallow:
      subresource_filter_->ReportDisallowedLoad();

      // Display console message for actually blocked resource. For a
      // resource with |load_policy| as kWouldDisallow, we will be logging a
      // document wide console message, so no need to log it here.
      // TODO: Consider logging this as a kIntervention for showing
      // warning in Lighthouse.
      if (subresource_filter_->ShouldLogToConsole()) {
        execution_context_->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::ConsoleMessageSource::kOther,
                mojom::ConsoleMessageLevel::kError,
                GetErrorStringForDisallowedLoad(resource_url)));
      }
      [[fallthrough]];
    case WebDocumentSubresourceFilter::kWouldDisallow:
      // TODO(csharrison): Consider posting a task to the main thread from
      // worker thread, or adding support for DidObserveLoadingBehavior to
      // ExecutionContext.
      if (auto* window = DynamicTo<LocalDOMWindow>(execution_context_.Get())) {
        if (auto* frame = window->GetFrame()) {
          frame->Loader().GetDocumentLoader()->DidObserveLoadingBehavior(
              kLoadingBehaviorSubresourceFilterMatch);
        }
      }
      break;
  }
}

void SubresourceFilter::ScheduleCosmeticFiltering(
    Document& document,
    CosmeticFilteringMilestone milestone) {
  if (cosmetic_filtering_scheduled_ &&
      milestone == CosmeticFilteringMilestone::kDomContentLoaded) {
    return;
  }
  cosmetic_filtering_scheduled_ = true;
  document.GetTaskRunner(TaskType::kInternalLoading)
      ->PostTask(FROM_HERE, BindOnce(&SubresourceFilter::ApplyCosmeticFiltering,
                                     WrapWeakPersistent(this),
                                     WrapWeakPersistent(&document), milestone));
}

void SubresourceFilter::ApplyCosmeticFiltering(
    Document* document,
    CosmeticFilteringMilestone milestone) {
  if (!document) {
    return;
  }
  cosmetic_filtering_scheduled_ = false;

  if (subresource_filter_->IsDryRun() ||
      subresource_filter_->IsFilteringDisabledForDocument()) {
    return;
  }

  const uint64_t ruleset_id = subresource_filter_->GetRulesetId();
  if (cosmetic_filtering_applied_ && cosmetic_ruleset_id_ == ruleset_id &&
      milestone == CosmeticFilteringMilestone::kDomContentLoaded) {
    return;
  }
  cosmetic_ruleset_id_ = ruleset_id;

  std::vector<std::string> selectors;
  bool cap_hit = false;
  std::vector<std::string_view> domain_selectors;
  subresource_filter_->GetDomainSelectors(domain_selectors);
  for (std::string_view selector : domain_selectors) {
    AppendSelector(selector, selectors, cap_hit);
  }

  unsigned inspected_elements = 0;
  if (document->documentElement()) {
    for (Element& element : ElementTraversal::InclusiveDescendantsOf(
             *document->documentElement())) {
      if (++inspected_elements > kMaxCosmeticElementsToInspect || cap_hit) {
        break;
      }

      const AtomicString& id = element.GetIdAttribute();
      if (!id.empty()) {
        std::string id_utf8 = id.Utf8();
        uint32_t hash = subresource_filter::GetStyleRuleHash(id_utf8);
        if (subresource_filter_->MaybeHasStyleRule(hash)) {
          std::vector<std::string_view> id_selectors;
          subresource_filter_->GetSelectorsById(id_utf8, hash, id_selectors);
          for (std::string_view selector : id_selectors) {
            AppendSelector(selector, selectors, cap_hit);
          }
        }
      }

      if (element.HasClass()) {
        for (const AtomicString& class_name : element.ClassNames()) {
          if (class_name.empty()) {
            continue;
          }
          std::string class_utf8 = class_name.Utf8();
          uint32_t hash = subresource_filter::GetStyleRuleHash(class_utf8);
          if (!subresource_filter_->MaybeHasStyleRule(hash)) {
            continue;
          }
          std::vector<std::string_view> class_selectors;
          subresource_filter_->GetSelectorsByClass(class_utf8, hash,
                                                   class_selectors);
          for (std::string_view selector : class_selectors) {
            AppendSelector(selector, selectors, cap_hit);
          }
        }
      }
    }
  }

  cosmetic_selector_count_ = selectors.size();
  cosmetic_selector_cap_hit_ = cap_hit;

  unsigned hidden_element_count = 0;
  if (subresource_filter_->ShouldLogToConsole()) {
    for (const std::string& selector : selectors) {
      if (hidden_element_count >= kMaxCosmeticDebugSelectorMatches) {
        break;
      }
      StaticElementList* matches = document->QuerySelectorAll(
          AtomicString(String::FromUtf8(selector)), IGNORE_EXCEPTION);
      if (matches) {
        hidden_element_count += matches->length();
      }
    }
  }
  cosmetic_hidden_element_count_ = hidden_element_count;

  if (!selectors.empty()) {
    InjectCosmeticStyle(*document, AtomicString(kCosmeticFilteringStyleId),
                        BuildCosmeticStyleSheet(selectors));
    cosmetic_filtering_applied_ = true;
  }

  if (subresource_filter_->ShouldLogToConsole()) {
    InjectCosmeticDebugPanel(*document, cosmetic_selector_count_,
                             cosmetic_hidden_element_count_,
                             cosmetic_selector_cap_hit_);
  }
}

void SubresourceFilter::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

}  // namespace blink
