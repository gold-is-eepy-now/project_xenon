// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/trace_time.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/base/time.h"

#if BUILDFLAG(IS_FREEBSD)
#define CLOCK_BOOTTIME CLOCK_UPTIME
#endif

namespace base::tracing {

int64_t TraceBootTicksNow() {
  // On Windows and Mac, TRACE_TIME_TICKS_NOW() behaves like boottime already.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_BSD)
  struct timespec ts;
  int res = clock_gettime(CLOCK_BOOTTIME, &ts);
  if (res != -1) {
    return static_cast<int64_t>(perfetto::base::FromPosixTimespec(ts).count());
  }
#endif
  return TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds();
}

}  // namespace base::tracing
