# Xenon privacy / safety pillar

This document tracks Xenon's "safer" pillar: reducing dependence on remote
(Google) web services and hardening privacy defaults, **without making users
less safe**.

Everything here is gated behind GN args declared in
[`//build/config/xenon/xenon.gni`](../../build/config/xenon/xenon.gni) and
surfaced to C++ via the buildflag header
[`//build/config/xenon:buildflags`](../../build/config/xenon/BUILD.gn):

* `XENON_DISABLE_GOOGLE_SERVICES` ← `xenon_disable_google_services`
* `XENON_PRIVACY_HARDENING` ← `xenon_privacy_hardening`

Consume them as:

```c++
#include "build/config/xenon/xenon_buildflags.h"

#if BUILDFLAG(XENON_PRIVACY_HARDENING)
// privacy-hardened default
#endif
```

Targets that include the header must depend on
`"//build/config/xenon:buildflags"`.

## Guardrail: do not reduce safety

Several "de-Google" forks disable Safe Browsing outright. Xenon must **not** do
that blindly — Safe Browsing prevents real phishing/malware harm. The plan:

* Keep Safe Browsing available; prefer the privacy-preserving (hash-prefix,
  no-account) mode over fully disabling it.
* Where a Google connection is removed, prefer a local or opt-in replacement
  rather than simply deleting protection.
* Every change here keeps `XENON_PRIVACY_HARDENING=false` (and
  `XENON_DISABLE_GOOGLE_SERVICES=false`) behaving exactly like upstream, so the
  hardening is auditable and reversible.

## Status

| Change | Flag | Status | Location |
| --- | --- | --- | --- |
| Default "resolve navigation errors via web service" (link-doctor) OFF | `XENON_PRIVACY_HARDENING` | implemented (needs build verification) | `chrome/browser/net/profile_network_context_service.cc` (`kAlternateErrorPagesEnabled`) |

### Worked example (this PR)

`kAlternateErrorPagesEnabled` controls the "Use a web service to help resolve
navigation errors" setting, which sends the failed URL to Google's link-doctor
service. Its default is flipped to `false` when `XENON_PRIVACY_HARDENING` is on;
the user-facing toggle still exists, so users can opt back in.

## Backlog (next, grounded in the ungoogled-chromium patch series)

These are concrete call-site groups to gate. Each should be a small, separately
reviewable change behind one of the two flags, ideally consolidating a related
group. (See the upstream
[ungoogled-chromium `patches/series`](https://github.com/ungoogled-software/ungoogled-chromium/blob/master/patches/series)
for the canonical inventory.)

Under `XENON_DISABLE_GOOGLE_SERVICES`:

* Disable update/RLZ pings and the network-time tracker phone-home.
* Disable domain reliability uploads and the WebRTC log uploader.
* Disable GCM / push registration to Google by default.
* Remove implicit Google host connections: profile-avatar downloading,
  `fonts.googleapis.com` references, MEI preload, field-trial/variations
  phone-home where not required for security updates.
* Replace the hard-wired Google default search/host-detection with a neutral
  default.

Under `XENON_PRIVACY_HARDENING`:

* Default third-party cookies to blocked.
* Reduce passive fingerprinting surface (trim/limit high-entropy client hints).
* Tighten referrer policy defaults.
* Disable search-as-you-type suggestions by default (sends keystrokes to the
  search provider) — opt-in retained.

## How to verify

A real Chromium build is required (a checkout is 100GB+ and a build takes hours
on dedicated hardware), so these defaults cannot be exercised in a typical CI
container. To verify a change:

1. Build with the flag off, confirm upstream behavior.
2. Build with `xenon_privacy_hardening=true` (or
   `xenon_disable_google_services=true`) in `args.gn`, confirm the new default
   and that the corresponding `chrome://settings` toggle still works.
3. Where applicable, confirm no outbound request is made to the affected Google
   endpoint (e.g. via `chrome://net-export` or a proxy).
