# FreeBSD support (in-tree)

Upstream Chromium does not officially support FreeBSD; the support that exists
lives in the community [`www/chromium`](https://cgit.freebsd.org/ports/tree/www/chromium)
port as an out-of-tree patch set. Xenon's goal is to bring that support **in-tree**
so FreeBSD is a first-class platform maintained alongside the rest of the code.

This directory is the first step of that effort.

## What's here

* `fetch_port_patches.sh` — reproducibly vendors the FreeBSD port's `patch-*`
  files from a **pinned** freebsd-ports commit into `patches/`.
* `MANIFEST` — provenance (source repo, pinned commit, port/Chromium versions,
  patch count, fetch date).
* `patches/` — the vendored snapshot (1565 files) of the port's patch set.

## GN OS detection (already in effect)

Independent of the vendored patches, `build/config/BUILDCONFIG.gn` now recognizes
FreeBSD:

* `is_freebsd` and `is_bsd` are defined (`current_os == "freebsd"`).
* `is_linux` is **true** for FreeBSD too, so the large body of Linux build logic
  is reused; BSD-specific deltas are gated with `is_freebsd` / `is_bsd`.
* `is_clang`, `host_toolchain`, and the default toolchain selection handle a
  `freebsd` host/target (mirroring the port's BUILDCONFIG patch).

This is the foundation the rest of FreeBSD support builds on. It is a no-op for
non-FreeBSD builds.

> The toolchain target `//build/toolchain/freebsd:clang_<cpu>` referenced by the
> host/target selection does not exist yet — adding it is the next step (see
> below). Non-FreeBSD builds never reference it, so existing platforms are
> unaffected.

## Important: version skew

The vendored patches target **Chromium 148.0.7778.178** (the port's version at
the pinned commit), while this tree is at **150.0.7866.0**. They are a reference
snapshot and will **not** all apply cleanly without a rebase. They are committed
so the in-tree migration can proceed against a known baseline, not as a
claim that Xenon builds on FreeBSD today.

## Refreshing the snapshot

```shell
sh build/freebsd/fetch_port_patches.sh
# then update build/freebsd/MANIFEST with the new commit/version/count
```

To pin a different upstream commit:

```shell
PORT_COMMIT=<sha> sh build/freebsd/fetch_port_patches.sh
```

## Migration plan (tracked in docs/xenon/freebsd_support_gaps.md)

1. **GN OS detection** — done (this change).
2. **Toolchain** — add `//build/toolchain/freebsd` (clang/lld/ar wiring).
3. **Rebase patches to the current Chromium version** and convert them from
   `patch-*` form into real in-tree `#if BUILDFLAG(IS_BSD)` / `is_bsd` changes,
   subsystem by subsystem (base, sandbox, media, ui/ozone, net, ...).
4. **Best-effort FreeBSD CI** so the platform doesn't silently regress.
5. **Harden the sandbox** using FreeBSD-native facilities (Capsicum) rather than
   simply disabling sandbox features.

The patches are organized by the upstream path they modify (encoded in each
filename, e.g. `patch-base_BUILD.gn` → `base/BUILD.gn`), which maps directly to
the subsystem buckets above.
