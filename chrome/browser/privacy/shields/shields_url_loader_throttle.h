// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_URL_LOADER_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace privacy::shields {

class ShieldsService;

class ShieldsURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit ShieldsURLLoaderThrottle(ShieldsService* service);
  ShieldsURLLoaderThrottle(const ShieldsURLLoaderThrottle&) = delete;
  ShieldsURLLoaderThrottle& operator=(const ShieldsURLLoaderThrottle&) = delete;
  ~ShieldsURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  const char* NameForLoggingWillStartRequest() override;

 private:
  raw_ptr<ShieldsService> service_ = nullptr;
};

}  // namespace privacy::shields

#endif  // CHROME_BROWSER_PRIVACY_SHIELDS_SHIELDS_URL_LOADER_THROTTLE_H_
