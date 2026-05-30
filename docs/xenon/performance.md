# Xenon performance pillar

Xenon's "faster" pillar trades a little binary size and CPU-compatibility for
speed, in the spirit of performance-focused forks like Thorium. Everything is
gated behind GN args in
[`//build/config/xenon/xenon.gni`](../../build/config/xenon/xenon.gni) and
implemented by the `xenon_perf` config in
[`//build/config/xenon/BUILD.gn`](../../build/config/xenon/BUILD.gn), which is
appended to `default_compiler_configs`.

With every knob at its default, `xenon_perf` adds **no flags** — the build is
byte-for-byte upstream. This keeps the change auditable and bisectable.

## Knobs

| GN arg | Default | Effect (clang/gcc) |
| --- | --- | --- |
| `xenon_full_optimization` | `false` | Adds `-O3` (after the standard `-O2`, so `-O3` wins). |
| `xenon_simd_level` | `"sse2"` | x86-64 SIMD baseline: `sse3`→`-msse3`, `sse4`→`-msse4.2`, `avx`→`-mavx`, `avx2`→`-mavx2 -mfma`. |
| `xenon_use_aes_ni` | `false` | Adds `-maes -mpclmul` (AES-NI + CLMUL; helps TLS/GCM). |

Example `args.gn` for a modern-desktop build:

```gn
is_xenon = true
xenon_full_optimization = true
xenon_simd_level = "avx2"
xenon_use_aes_ni = true
```

## Scope and safety

* **Target toolchain only.** The flags apply only when
  `current_toolchain == default_toolchain`, so host build tools (protoc, etc.)
  stay at the portable baseline and still run on older build machines.
* **x86-64 only** for the SIMD/AES flags (gated on `current_cpu`); arm64 is
  unaffected for now.
* **Non-Windows only** for now. The clang/gcc driver syntax is handled;
  clang-cl (`/clang:` / `/arch:`) support is a deliberate follow-up.

## Distribution caution

Raising `xenon_simd_level` raises the **minimum CPU** required to run the
browser. `avx2`, for example, will `SIGILL` on CPUs without AVX2. Distributors
should either:

* ship a conservative baseline (e.g. `sse4`) for general downloads, and/or
* publish separate optimized builds (e.g. an `avx2` build) clearly labeled with
  their CPU requirement.

## How to verify (needs a real build)

A Chromium build can't run in a typical CI container (100GB+ checkout,
hours-long build). To verify:

1. `gn gen out/Default` with the args above, then
   `gn args out/Default --list | grep xenon` to confirm the values.
2. Inspect the emitted flags:
   `ninja -C out/Default -t commands <some_target.o> | grep -E '\-O3|\-mavx2|\-maes'`.
3. Confirm host tools do **not** get the SIMD flags (check a `host`-toolchain
   object's command line).
4. Run the resulting browser on target hardware; benchmark (e.g. Speedometer)
   against an `is_xenon=false` build to quantify the gain.
