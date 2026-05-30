# FreeBSD support gaps in Chromium (and Xenon's plan)

## Summary

Upstream Chromium officially supports only Windows, macOS, Linux, Android,
iOS/ChromeOS/Fuchsia targets. **FreeBSD is not an officially supported host or
target.** What makes Chromium build and run on FreeBSD today is the community
[`www/chromium`](https://cgit.freebsd.org/ports/tree/www/chromium) port, which
carries a large **out-of-tree patch set** plus FreeBSD-specific GN args and
dependency wiring.

For Xenon, "support FreeBSD" means closing the gap between upstream and that
port — ideally by bringing the support **in-tree** so it is maintained as a
first-class platform rather than as an external patch stack.

## Where the gaps are

The FreeBSD port's `files/patch-*` set (hundreds of patches) is the concrete map
of what upstream is missing. The patches cluster around a few areas:

* **Build system / GN** (`patch-BUILD.gn`, `patch-base_BUILD.gn`, many
  `*_BUILD.gn`): teach the build that FreeBSD is a BSD/POSIX-like target,
  select system vs bundled libraries appropriate for FreeBSD, and drop
  Linux-only assumptions (udev, some allocator shims, etc).
* **`base/`** (allocator/partition_alloc, threading, process, files,
  sysinfo, CPU detection): FreeBSD has different syscalls and `/proc`-less
  semantics, so platform abstractions need FreeBSD branches.
* **Sandbox**: Linux's seccomp-BPF / namespace sandbox does not exist on
  FreeBSD; the port adapts/loosens sandboxing using FreeBSD primitives
  (e.g. Capsicum is the long-term right answer). This is the most
  security-sensitive area.
* **Media / GPU / Ozone**: V4L2, VA-API, DRM/GBM, and Wayland/X11 paths need
  FreeBSD-appropriate wiring (the port depends on `libepoll-shim`,
  `v4l_compat`, `mesa-dri`, `libva`, `wayland`, `libxkbcommon`, etc).
* **net / DNS / TLS**: resolver and certificate-store integration differences.
* **Toolchain**: FreeBSD uses base `clang`/`lld`/`libc++`; the port sets
  `use_sysroot=false`, `is_clang=true`, and bundles libc++/libunwind
  (`use_custom_libcxx=true`, `use_custom_libunwind=true`).

See [the FreeBSD build guide](../freebsd/build_instructions.md) for the GN args
and dependency list distilled from the port.

## Why it's out-of-tree today

* Upstream CI does not test FreeBSD, so patches would bitrot without a
  maintainer/runner.
* Some changes (especially sandbox relaxations) are not acceptable upstream as-is
  because they weaken the security model on the supported platforms unless
  carefully `#if BUILDFLAG(IS_BSD)`-gated.

## Xenon's plan (tracked in the roadmap, Phase X)

1. **Vendor the patch set in-tree.** Import the `www/chromium` FreeBSD patches
   into Xenon, organized by subsystem, so FreeBSD builds from Xenon's own tree.
   Track provenance so we can refresh against the port on each Chromium uprev.
2. **Add FreeBSD to GN OS detection / toolchain** so `is_bsd`/FreeBSD branches
   are part of the normal build config, not a patch.
3. **Stand up a best-effort FreeBSD CI job** so regressions are caught.
4. **Harden the sandbox story** using FreeBSD-native facilities (Capsicum)
   instead of simply disabling sandbox features — this aligns with Xenon's
   "safer" pillar.
5. **Upstream-clean** what we can, shrinking the long-term maintenance delta.

## Honest status

No FreeBSD build has been performed in this repo yet. This document and the
build guide are derived from the upstream port and Chromium build configuration;
the in-tree implementation work above is not started. Treat the FreeBSD build
guide as accurate-to-the-port guidance, not as a claim that Xenon currently
builds on FreeBSD from this tree.
