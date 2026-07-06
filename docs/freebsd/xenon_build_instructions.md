# Building Xenon on FreeBSD

This guide is the Xenon-specific FreeBSD build path. It starts from the
Chromium/FreeBSD requirements in [build_instructions.md](./build_instructions.md)
but calls out the parts that are different for **Xenon**: this repository,
Xenon's in-tree FreeBSD work, the remaining vendored patches, and the
`xenon_*` GN args.

[TOC]

*** note
**Status: work in progress — does not fully build on FreeBSD yet.**
Xenon brings FreeBSD support *in-tree* incrementally. As of now:
* **In-tree:** GN OS detection (`is_freebsd`/`is_bsd`), the
  `//build/toolchain/freebsd` clang toolchain, and the migrated `base/` patch
  bucket.
* **Still vendored patches:** `sandbox`, `net`, `media`, `ui`/ozone, `content`,
  `chrome`, etc. live under `build/freebsd/patches/` and must be applied on top
  of the checkout (see [Apply the remaining FreeBSD patches](#apply-the-remaining-freebsd-patches)).
* **Not compile-verified.** The in-tree changes apply cleanly against this
  tree's Chromium version but have not been built on real FreeBSD hardware yet.

Treat this as a developer bring-up guide, not a "download and run" path. If you
just want a working browser today, use the upstream `www/chromium` package/port
(see the generic guide).
***

## 1. Install the FreeBSD build prerequisites

Use the dependency list from the generic FreeBSD guide:
[System requirements](./build_instructions.md#system-requirements) and
[Install dependencies](./build_instructions.md#install-dependencies). In short:

* 64-bit `amd64` or `aarch64`; FreeBSD 13.2+ (14.x/15.x recommended).
* ~8GB RAM / ~35GB disk for a release build (more is better; the final link is
  the peak — configure swap).
* Base-system `clang`/`libc++`. `python` must resolve to Python 3.9+.

Install the `pkg` dependency set listed there before continuing.

## 2. Get the Xenon source

Unlike upstream Chromium on FreeBSD (which builds from the official source
tarball), Xenon's source lives in this Git repository:

```shell
$ git clone https://github.com/gold-is-eepy-now/project_xenon.git xenon
$ cd xenon
```

This tree already contains the in-tree FreeBSD support (GN OS detection +
toolchain + migrated `base/` bucket) and, under `build/freebsd/`, the vendored
snapshot of the remaining `www/chromium` patches plus their provenance
(`MANIFEST`) and a refresh script (`fetch_port_patches.sh`).

*** note
**Version skew.** The vendored patches were snapshotted from a pinned
freebsd-ports commit targeting a slightly older Chromium version than this tree
(see `build/freebsd/MANIFEST`). Some may not apply cleanly and need
hand-rebasing — this is expected while the in-tree migration is in progress.
See `build/freebsd/README.md`.
***

## 3. Apply the remaining FreeBSD patches

The buckets that haven't been migrated in-tree yet are still carried as
`patch-*` files. Apply everything currently in `build/freebsd/patches/` on top
of the checkout. Buckets already migrated in-tree (e.g. `base/`) are **not**
present in that directory, so this loop only applies what's still needed:

```shell
# From the repo root. Patches use the FreeBSD port's -p0 path convention.
$ for p in build/freebsd/patches/patch-*; do
    if git apply --check -p0 "$p" 2>/dev/null; then
      git apply -p0 "$p"
    else
      echo "SKIP (needs rebase): $p"
    fi
  done
```

Patches printed as `SKIP` drifted against this tree's version and must be
rebased by hand before a full build will succeed. Cross-reference the upstream
file with the patch's hunks. (Re-running `sh build/freebsd/fetch_port_patches.sh`
restores the full upstream snapshot if you need a clean baseline.)

## 4. Configure Xenon (`args.gn`)

Start from the FreeBSD baseline in the generic guide
([Setting up the build](./build_instructions.md#setting-up-the-build)), then add
the Xenon args below.

On a native FreeBSD host, do **not** set `target_os`; GN detects FreeBSD and
selects Xenon's `//build/toolchain/freebsd` toolchain automatically. If you are
only generating FreeBSD files from another host for bring-up/debugging, set
`target_os = "freebsd"` explicitly so Xenon selects the FreeBSD target toolchain.

```shell
$ gn gen out/Xenon
```

Edit `out/Xenon/args.gn`:

```gn
# --- FreeBSD baseline (see generic guide for the full list) ---
is_clang = true
use_lld = true
use_sysroot = false
use_custom_libcxx = true
use_udev = false
use_qt5 = true
treat_warnings_as_errors = false
is_debug = false
is_official_build = true
symbol_level = 0

# --- Xenon configuration surface (build/config/xenon/xenon.gni) ---
is_xenon = true                      # build Xenon behavior where implemented

# Pillar 1 — performance knobs
xenon_full_optimization = true       # -O3
xenon_simd_level = "sse4"            # sse2|sse3|sse4|avx|avx2 (match your CPU floor)
xenon_use_aes_ni = true              # -maes -mpclmul

# Pillar 2 — privacy/safety knobs
xenon_disable_google_services = false
xenon_privacy_hardening = false

# Pillar 3 — customization knobs
xenon_enable_customization_ui = false
```

`gn args out/Xenon --list | grep xenon` shows the resolved Xenon args. The
performance/privacy/customization knobs are still being wired in incrementally;
`build/config/xenon/xenon.gni` is the source of truth for current defaults.
Use `is_xenon = false` only when you need to isolate a regression against
upstream-like Chromium behavior.

*** note
**Distribution caution.** Raising `xenon_simd_level` raises the minimum CPU
required to run the resulting binary (e.g. `avx2` will `SIGILL` on older CPUs).
Ship a conservative baseline for general use. See
[performance docs](../xenon/performance.md).
***

API keys behave exactly as in the generic guide — see
[its note on API keys](./build_instructions.md#setting-up-the-build).

## 5. Build and run

```shell
$ ninja -C out/Xenon chrome
$ ./out/Xenon/chrome
```

Out-of-memory during the final link is the most common failure; mitigations
(release build, `symbol_level=0`, lower `-j`, more swap) are in the generic
guide's [Tips](./build_instructions.md#tips-tricks-and-troubleshooting).

## 6. What works / what's WIP

| Area | State |
| --- | --- |
| GN OS detection (`is_freebsd`/`is_bsd`, `is_linux` true for BSD) | in-tree |
| `//build/toolchain/freebsd` clang toolchain | in-tree |
| `base/` subsystem | migrated in-tree (a few patches deferred) |
| `sandbox` (incl. Capsicum), `net`, `media`, `ui`/ozone, `content`, `chrome` | vendored patches (apply step 3) |
| Full compile/link on FreeBSD | **not yet verified** |

Progress and the per-bucket migration status are tracked in
[`build/freebsd/README.md`](../../build/freebsd/README.md) and
[`docs/xenon/freebsd_support_gaps.md`](../xenon/freebsd_support_gaps.md).

## 7. Reporting build breakage

When a step fails, first diff your configuration against the upstream
`www/chromium` port (the source of truth for FreeBSD), then check whether the
relevant bucket has been migrated in-tree or is still a vendored patch. File the
failure with the failing target, the `args.gn`, and the FreeBSD version so it
can be folded into the migration.
