// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/shields/shields_url_loader_throttle.h"

#include <string_view>

#include "chrome/browser/privacy/shields/shields_service.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"

namespace privacy::shields {

ShieldsURLLoaderThrottle::ShieldsURLLoaderThrottle(ShieldsService* service)
    : service_(service) {}

ShieldsURLLoaderThrottle::~ShieldsURLLoaderThrottle() = default;

void ShieldsURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!service_) {
    return;
  }

  ShieldsService::EvaluationResult result = service_->EvaluateRequest(*request);
  if (result.blocked) {
    delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT,
                               "Blocked by Shields content blocking");
    return;
  }
  if (result.redirect_url) {
    request->url = *result.redirect_url;
  }
}

const char* ShieldsURLLoaderThrottle::NameForLoggingWillStartRequest() {
  return "ShieldsURLLoaderThrottle";
}

}  // namespace privacy::shields
