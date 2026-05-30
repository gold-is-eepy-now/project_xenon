# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Project Xenon

**Project Xenon** is a Chromium-based browser project. It tracks upstream
[Chromium](https://www.chromium.org) — an open-source project that aims to build
a safer, faster, and more stable way for all users to experience the web — while
adding its own changes on top.

Because Xenon is a Chromium fork, the upstream Chromium documentation, build
system, and source layout all apply here. This README points you at the parts
you need to get productive quickly.

## Quick links

| I want to… | Go to |
| --- | --- |
| Get and build the code | [docs/get_the_code.md](docs/get_the_code.md) |
| Browse all documentation | [docs/README.md](docs/README.md) |
| Understand the source tree | [Getting Around the Chromium Source](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code) |
| Contribute a change | [docs/contributing.md](docs/contributing.md) |
| Report a bug | https://crbug.com/new |

## Getting the code

Do **not** use a plain `git clone` for a full development checkout — Chromium
relies on `depot_tools` and `gclient` to fetch its many dependencies. Follow the
[instructions on how to get the code](docs/get_the_code.md).

## Building

Each platform has a self-contained build guide:

* [Android](docs/android_build_instructions.md) (build on Linux)
* [Chrome OS](docs/chromeos_build_instructions.md) (build on Linux)
* [FreeBSD](docs/freebsd/build_instructions.md) (community-supported)
* [Fuchsia](docs/fuchsia/build_instructions.md)
* [iOS](docs/ios/build_instructions.md) (build on Mac)
* [Linux](docs/linux/build_instructions.md)
* [Mac](docs/mac_build_instructions.md)
* [Windows](docs/windows_build_instructions.md)

The build process is broadly the same on every platform; each guide covers the
platform-specific quirks (dependencies, toolchain, and GN args).

## Repository layout

Documentation in the source is rooted at [docs/README.md](docs/README.md).

For historical reasons there are some small top-level directories. The current
guidance is that new top-level directories are for products (e.g. Chrome,
Android WebView, Ash). Even when a product has multiple executables, its code
should live in subdirectories of that product.

## Contributing

Contributions are welcome. Start with [docs/contributing.md](docs/contributing.md),
and please follow the project [Code of Conduct](CODE_OF_CONDUCT.md). Changes are
expected to keep the tree building and passing presubmit checks
([PRESUBMIT.py](PRESUBMIT.py)).

## Relationship to upstream Chromium

Project Xenon is derived from Chromium and stays close to upstream. Where this
repository does not document something specific to Xenon, the upstream Chromium
resources are authoritative:

* Project site: https://www.chromium.org
* Source search: https://source.chromium.org/chromium
* Issue tracker: https://crbug.com

## License

Chromium, and therefore Project Xenon, is made available under a BSD-style
license. See [LICENSE](LICENSE) for details. Additional license terms for
specific components are noted in their respective directories.
