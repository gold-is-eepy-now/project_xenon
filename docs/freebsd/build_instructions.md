# Checking out and building Chromium on FreeBSD

There are instructions for other platforms linked from the
[get the code](../get_the_code.md) page.

[TOC]

*** note
**Note:** FreeBSD is a community-supported platform. It is **not** covered by
Chromium's official build infrastructure or continuous integration, so things
occasionally break. The canonical FreeBSD build is the
[`www/chromium`](https://cgit.freebsd.org/ports/tree/www/chromium) port, which
carries the patch set, dependency list, and GN args that actually make Chromium
build and run on FreeBSD. Most of the FreeBSD-specific knowledge in this
document is derived from that port. If you just want a working browser, install
the package or build the port — see
[Use the package or port](#use-the-package-or-port).
***

## System requirements

* A 64-bit machine. The port supports `amd64` and `aarch64`
  (`ONLY_FOR_ARCHS = aarch64 amd64`); 32-bit hosts are not supported.
* RAM: a **release** build needs roughly 8GB of RAM, but more is strongly
  recommended. A **debug** build is far heavier and wants ~32GB. Configure
  generous swap if you are short on RAM — the final link is the peak.
* Disk: about 35GB free for a release build, ~60GB for debug. ZFS with
  `atime=off` works well.
* FreeBSD 13.x or newer; 14.x/15.x are what the port is currently built and
  tested against.
* `clang`/`libc++` from base. `clang` is the only supported compiler.

## How FreeBSD ports and packages work

Understanding the ports/packages model makes the rest of this document clearer.

* **Packages** are pre-compiled binaries installed with
  [`pkg`](https://man.freebsd.org/pkg.8). `pkg install chromium` downloads
  Chromium and all of its dependencies from the FreeBSD package mirrors. This is
  the fastest and most common way to install software.
* **Ports** are the recipes those packages are built from. The
  [Ports Collection](https://docs.freebsd.org/en/books/handbook/ports/) lives
  under `/usr/ports`; each port (e.g. `/usr/ports/www/chromium`) is a directory
  with a `Makefile` plus supporting files that tell the build system how to
  **download, extract, patch, configure, compile, and install** the software.
  Running `make install clean` in a port directory performs all of those steps.
* A port `Makefile` declares metadata and dependencies, for example:
  * `BUILD_DEPENDS` / `LIB_DEPENDS` / `RUN_DEPENDS` — what must be present to
    build, link, and run.
  * `USES` — shared build helpers (the Chromium port uses `ninja`, `gmake`,
    `cargo`, `python`, `qt:5`, `xorg`, and more).
  * `OPTIONS_DEFINE` — user-selectable knobs (the Chromium port exposes
    `CODECS`, `CUPS`, `DEBUG`, `DRIVER`, `KERBEROS`, `LTO`, `PIPEWIRE`,
    `WIDEVINE`, and audio backends `ALSA`/`PULSEAUDIO`/`SNDIO`).
  * A patch set under `files/` (`patch-*`) that adapts upstream Chromium source
    to FreeBSD (e.g. replacing Linux-only syscalls and build assumptions).
* **Official binary packages** are produced by building the ports tree inside
  clean, reproducible [poudriere](https://github.com/freebsd/poudriere) jails,
  then published to the FreeBSD package mirrors. So the package you install with
  `pkg` is just the `www/chromium` port compiled in a pristine jail. You can run
  your own `poudriere` to build customized Chromium packages (e.g. with
  different `OPTIONS`).

Practical takeaway: the port is the source of truth. When a source build breaks,
diff your setup against `www/chromium`'s `Makefile` and `files/patch-*`.

## Use the package or port

The fastest path — install the prebuilt package:

```shell
$ sudo pkg install chromium
```

Or build the port (this applies the FreeBSD patch set and uses the right GN args
automatically):

```shell
# Get the ports tree if you don't have it (git is the modern method).
$ sudo git clone --depth 1 https://git.FreeBSD.org/ports.git /usr/ports

$ cd /usr/ports/www/chromium
$ make config        # optional: choose OPTIONS (codecs, audio backends, ...)
$ sudo make install clean
```

To build a *customized* package set repeatably, use `poudriere`, which compiles
ports in a clean jail exactly the way the official mirrors do.

The rest of this document is for developers who want to build Chromium directly
from source rather than through the port.

## Install dependencies

The port's `BUILD_DEPENDS`/`LIB_DEPENDS` are the authoritative dependency list.
At minimum you need the toolchain and codegen tools:

```shell
$ sudo pkg install \
    bash gmake ninja python3 \
    llvm node npm rust rust-bindgen-cli esbuild gperf flock \
    py-Jinja2 py-ply py-html5lib \
    pkgconf xcb-proto v4l_compat usbids
```

Plus the libraries Chromium links against on FreeBSD (subset shown — see the
port's `LIB_DEPENDS` for the full list):

```shell
$ sudo pkg install \
    nss nspr dbus dbus-glib libevent libffi icu jsoncpp re2 \
    cairo libdrm libexif png webp dav1d openh264 \
    freetype2 harfbuzz harfbuzz-icu fontconfig expat \
    at-spi2-core speech-dispatcher flac opus speex \
    libsecret libgcrypt libpci libepoll-shim \
    wayland libxkbcommon libxshmfence mesa-dri libva \
    qt5-core qt5-widgets gtk3 atk
```

*** note
The FreeBSD port builds with **bundled** copies of several libraries (e.g.
`use_custom_libcxx=true`, `use_custom_libunwind=true`, `use_system_freetype=false`)
while using system copies of others (`use_system_harfbuzz=true`,
`use_system_libffi=true`, `use_system_libjpeg=true`). Don't assume every library
is unbundled — follow the GN args below.
***

Most of Chromium's tooling assumes `python` means Python 3. The port works
around this with a `python3` → `python` alias; make sure `python` resolves to a
3.9+ interpreter in your shell.

## Get the code

You have two realistic options on FreeBSD.

### Option A: build from the official source tarball (what the port does)

The `www/chromium` port does **not** use `depot_tools`/`gclient`. It downloads
the official Chromium source tarball
(`chromium-<version>.tar.xz` from
`https://commondatastorage.googleapis.com/chromium-browser-official/`),
extracts it, applies the FreeBSD patches, and bootstraps `gn` from the tree.
This is the most reliable way to build on FreeBSD because it matches the
supported configuration exactly. If you go this route, reuse the port's
machinery rather than reinventing it.

### Option B: a depot_tools checkout (advanced / not officially supported)

`depot_tools` targets Linux/Mac and downloads prebuilt toolchains that do not
exist for FreeBSD, so this path needs manual care.

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ export PATH="${HOME}/depot_tools:$PATH"
# Use $HOME or an absolute path — never '~' — or gclient runhooks will fail.

# Tell depot_tools not to manage/download toolchains it can't provide on FreeBSD.
$ export DEPOT_TOOLS_UPDATE=0
$ export VPYTHON_BYPASS="manually managed python not supported by chrome operations"

$ mkdir ~/chromium && cd ~/chromium
$ fetch --nohooks chromium
$ cd src
$ gclient runhooks
```

Expect to disable hooks that fetch Linux/Mac-only binaries. When a hook or build
step fails, cross-reference the port's `Makefile` and patch set to see how
FreeBSD handles that piece.

## Setting up the build

Chromium uses [GN](https://gn.googlesource.com/gn/+/main/docs/quick_start.md) to
generate `.ninja` files and [Ninja](https://ninja-build.org/) to build. Create a
build directory:

```shell
$ gn gen out/Default
```

Then edit `out/Default/args.gn`. The following args mirror the FreeBSD
`www/chromium` port and are a known-good baseline. (FreeBSD is detected as a BSD
target automatically — you do **not** set `target_os`.)

```gn
# out/Default/args.gn — derived from FreeBSD www/chromium

is_clang = true
clang_use_chrome_plugins = false
use_lld = true
use_sysroot = false

# Memory/allocator configuration used on FreeBSD.
use_allocator_shim = false
use_partition_alloc = true
use_partition_alloc_as_malloc = false

# Bundled vs system libraries (match the port exactly).
use_custom_libcxx = true
use_custom_libunwind = true
use_system_freetype = false
use_system_harfbuzz = true
use_system_libffi = true
use_system_libjpeg = true

# Platform/UI.
use_aura = true
toolkit_views = true
use_udev = false           # FreeBSD uses libudev-devd / epoll-shim, not udev
use_qt5 = true

# Don't let warnings stop the build on an unsupported platform.
treat_warnings_as_errors = false
fatal_linker_warnings = false

# Misc settings from the port.
enable_backup_ref_ptr_support = false
icu_use_data_file = false
optimize_webui = true

# Release vs debug — pick ONE block.
# Release (≈8GB RAM, ≈35GB disk):
is_debug = false
is_official_build = true
symbol_level = 0
blink_symbol_level = 0
# Debug (≈32GB RAM, ≈60GB disk) — replace the three lines above with:
#   is_debug = true
#   is_component_build = false
#   symbol_level = 1

# Codecs: set proprietary_codecs=true + ffmpeg_branding="Chrome" only if you
# want patented codecs (H.264). Otherwise:
proprietary_codecs = false
ffmpeg_branding = "Chromium"
```

*** note
**API keys.** The FreeBSD port ships a `google_api_key` that is licensed for
**FreeBSD use only**. For your own builds you must obtain your own keys — see
[API keys](https://www.chromium.org/developers/how-tos/api-keys). A build
without keys still works; some Google-account-backed services just won't.
***

*** note
**Out of memory during link?** The final link is the peak. Use a release build
(`is_debug=false`), drop symbols (`symbol_level=0`), optionally use a component
build (`is_component_build=true`, development only), lower Ninja parallelism with
`-j`, and add swap. See [Tips](#tips-tricks-and-troubleshooting).
***

## Build Chromium

Build the `chrome` target:

```shell
$ ninja -C out/Default chrome
```

The port also builds `chromedriver` when its `DRIVER` option is enabled:

```shell
$ ninja -C out/Default chromedriver
```

On a memory-constrained machine, cap parallelism:

```shell
$ ninja -C out/Default chrome -j4
```

## Run Chromium

```shell
$ out/Default/chrome
```

For X11 make sure `DISPLAY` is set and an X server (or `Xvfb`) is available. For
Wayland, build with `use_ozone=true` and run with `--ozone-platform=wayland`.

## Running test targets

As on Linux, find a test's target with `gn refs`, then build and run it:

```shell
$ gn refs out/Default --testonly=true --type=executable --all base/values_unittest.cc
//base:base_unittests

$ ninja -C out/Default base_unittests
$ out/Default/base_unittests --gtest_filter="ValuesTest.*"
```

## Tips, tricks, and troubleshooting

### Linker runs out of memory

The final link of `chrome` needs a lot of RAM. If `lld` aborts with
`std::bad_alloc`, `out of memory`, or a signal 6/11:

* Build release: `is_debug = false` (biggest win).
* Drop symbols: `symbol_level = 0`.
* Component build for development: `is_component_build = true`.
* Lower link parallelism (`-j` with a small number).
* Add swap.

### A hook or build step fails on a missing prebuilt binary

`depot_tools` defaults to fetching Linux/Mac prebuilts that don't exist for
FreeBSD. Export `DEPOT_TOOLS_UPDATE=0` and `VPYTHON_BYPASS`, prefer system
packages, and consider Option A (the official tarball + port machinery) instead.

### Something won't build — diff against the port

The canonical reference is
[`www/chromium`](https://cgit.freebsd.org/ports/tree/www/chromium). Its
`Makefile` (GN args, `OPTIONS`, dependencies) and `files/patch-*` set show
exactly how the FreeBSD maintainers keep Chromium building. When upstream
changes break FreeBSD, the fix usually lands there first.

## Next Steps

If you want to contribute FreeBSD support, coordinate with the maintainers of
the [`www/chromium`](https://cgit.freebsd.org/ports/tree/www/chromium) port
(`chromium@FreeBSD.org`), who track the FreeBSD patch set. General contribution
guidance lives in [contributing.md](../contributing.md).
