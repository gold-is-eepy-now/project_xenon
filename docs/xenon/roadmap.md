# Project Xenon roadmap

This document tracks the plan to evolve Xenon from a near-stock Chromium fork
into a faster, safer, and more customizable browser, with first-class FreeBSD
support. It is intentionally incremental: each item is a small, separately
reviewable change that is verified against a real build before being marked
done.

## Why incremental

A full Chromium checkout is 100GB+ and a from-scratch build takes hours on
dedicated hardware. Large, speculative diffs to core build files cannot be
trusted until they compile and run. So Xenon's strategy is:

1. Establish a **central config surface**
   ([`//build/config/xenon/xenon.gni`](../../build/config/xenon/xenon.gni)) so
   every fork change is discoverable and gated behind a named `xenon_*` arg.
2. Implement one knob at a time, wiring it into the build and validating it.
3. Keep `is_xenon = false` behaving like upstream so regressions are
   bisectable.

## Status legend

* `declared` — the GN arg exists in `xenon.gni` but is not wired into the build.
* `wip` — partially wired.
* `done` — wired and verified against a real build.

## Phase 0 — Foundations

| Item | Status | Notes |
| --- | --- | --- |
| Central config surface `xenon.gni` | declared | All `xenon_*` args live here. |
| Roadmap + FreeBSD gap analysis | done | This doc + `freebsd_support_gaps.md`. |
| New project README/branding text | done | See repo `README.md`. |

## Phase 1 — Branding / identity (distance from Chromium)

Goal: make Xenon recognizably its own product.

* Product strings & `xenon_product_name` plumbing into
  `chrome/app/theme` and `chrome/common` branding.
* Replace `product_logo_*` assets and `BRANDING` files with Xenon equivalents.
* User-Agent / `version_info` product name (carefully — UA changes affect
  compatibility; prefer keeping the engine token, change only the product
  token).

Prior art: this is exactly what every Chromium fork does first; the work is
mechanical but touches generated resources, so it must be built to verify.

## Phase 2 — Performance (faster)

Knobs: `xenon_full_optimization`, `xenon_simd_level`, `xenon_use_aes_ni`.

* Wire `xenon_full_optimization` to use `-O3` for official builds in
  `//build/config/compiler`.
* Wire `xenon_simd_level` to the SSE3/SSE4/AVX/AVX2 `-m*` flags, mirroring the
  refactor used by performance-focused forks (e.g. Thorium's
  `build/config/compiler_opt.gni`), and document which baseline each published
  build targets (older CPUs need a lower baseline).
* Optional/advanced: ThinLTO tuning, PGO, and link-time optimizations such as
  BOLT. These have large build-time/RAM costs and must be measured.
* Add a reproducible benchmark plan (Speedometer, Motionmark, JetStream) so
  speed claims are backed by numbers, not vibes.

## Phase 3 — Safety / privacy (safer)

Knobs: `xenon_disable_google_services`, `xenon_privacy_hardening`.

Concrete call-site groups to gate (grounded in the well-known
ungoogled-chromium patch series):

* Disable update/RLZ pings, domain reliability, GCM, network-time tracker,
  field-trial and crash phone-home.
* Remove implicit connections to Google hosts (safe browsing reporting,
  `fonts.googleapis.com`, web store URLs, profile-avatar downloads).
* Privacy defaults: third-party cookies off by default, trim passive
  fingerprinting surface, default search engine that is not hard-wired to
  Google.

Care: disabling Safe Browsing has real security trade-offs. Xenon should keep a
*local* protection story (or an opt-in privacy-preserving provider) rather than
simply removing protection. This pillar must not make users *less* safe.

## Phase 4 — Customization (more customizable)

Knob: `xenon_enable_customization_ui`.

* A consolidated `chrome://settings` section for Xenon toggles that map to the
  privacy/perf knobs that make sense at runtime.
* Theming hooks beyond stock theming.
* Optional `xenon://flags`-style surfacing of the most useful experiments.

## Phase X (cross-cutting) — First-class FreeBSD support

See [freebsd_support_gaps.md](freebsd_support_gaps.md). Plan:

1. Vendor the FreeBSD `www/chromium` patch set into Xenon's tree so FreeBSD is
   supported in-tree rather than via an external port.
2. Add FreeBSD to the GN OS detection / toolchain configs.
3. Stand up a FreeBSD build recipe in CI (best-effort runner) so FreeBSD does
   not silently regress.
4. Long-term: upstream-clean the patches so the delta to maintain shrinks.

## Definition of done for any pillar item

* Behavior is gated behind a `xenon_*` arg.
* `is_xenon = false` reproduces upstream behavior.
* It builds, and where measurable, there is a before/after number.
