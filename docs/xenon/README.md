# Project Xenon

Project Xenon is a Chromium fork with three goals, in priority order:

1. **Faster** — higher optimization levels and modern CPU targeting.
2. **Safer** — less remote-service dependency and stronger privacy defaults.
3. **More customizable** — user-facing toggles for behavior and appearance.

A fourth, cross-cutting goal is **first-class FreeBSD support**, which upstream
Chromium does not officially provide.

## Where things live

* All Xenon build knobs are declared in
  [`//build/config/xenon/xenon.gni`](../../build/config/xenon/xenon.gni). This is
  the single place to discover and audit what Xenon changes relative to
  upstream.
* The phased plan and current status are in [roadmap.md](roadmap.md).
* The analysis of what FreeBSD support requires (and why it is currently
  out-of-tree) is in [freebsd_support_gaps.md](freebsd_support_gaps.md).
* How to build Xenon on FreeBSD (in-tree support + remaining patches + the
  `xenon_*` args) is in
  [../freebsd/xenon_build_instructions.md](../freebsd/xenon_build_instructions.md).

## Design principles

* **Auditable**: prefer one central config surface plus small, gated changes
  over large sprawling diffs. Every behavioral change should be reachable from a
  named `xenon_*` GN arg.
* **Bisectable**: with `is_xenon = false` (and all pillar args at their upstream
  defaults), Xenon should behave like stock Chromium. This makes it possible to
  tell whether a regression came from Xenon or from upstream.
* **Verified**: a GN knob is only considered "implemented" once it is wired into
  the build *and* validated against a real build. Declaring a knob is not the
  same as implementing it — see the status table in the roadmap.
