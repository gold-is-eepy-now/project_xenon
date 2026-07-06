# Project Xenon

Project Xenon is a Chromium fork with three goals, in priority order:

1. **Faster** — higher optimization levels and modern CPU targeting.
2. **Safer** — less remote-service dependency and stronger privacy defaults.
3. **More customizable** — user-facing toggles for behavior and appearance.

A fourth, cross-cutting goal is **first-class FreeBSD support**, which upstream
Chromium does not officially provide.

## Start here

* To see the Xenon-specific build knobs, read
  [`//build/config/xenon/xenon.gni`](../../build/config/xenon/xenon.gni).
* To understand what is implemented now and what is still planned, read
  [roadmap.md](roadmap.md).
* To build or debug Xenon on FreeBSD, use
  [../freebsd/xenon_build_instructions.md](../freebsd/xenon_build_instructions.md).
* To understand why FreeBSD support is still being migrated in-tree, read
  [freebsd_support_gaps.md](freebsd_support_gaps.md).

## Design principles

* **Auditable**: prefer one central config surface plus small, gated changes
  over large sprawling diffs. Every behavioral change should be reachable from a
  named `xenon_*` GN arg.
* **Bisectable**: with `is_xenon = false` (and all pillar args at their upstream
  defaults), Xenon should behave like stock Chromium. This makes it possible to
  tell whether a regression came from Xenon or from upstream.
* **Verified**: a GN knob is only "implemented" after it is wired into the
  build and validated against a real build. Declaring a knob in `xenon.gni` is
  only the first step.
