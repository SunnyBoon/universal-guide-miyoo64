# Paper Mario PAL ROM-image converter

`guide_pal_rom_native.c` is byte-for-byte copied from Paper Mario G3.12 commit `228b138569c4c19f72a4283223987ce0309b2029`. That source produced the central tool tested on Miyoo with build `6FC183A2`. Source SHA-256: `7ad62d47f6a204ddcc5fab03dbc5c6e23c52f07984c49fb5911bfa29cb74bfa1`. The hardware-tested tool's SHA-256 is `9381a62521cfe46a76d4597a8b4e8637de2c67278aaa2ac2887475e4106fcabb`.

From this directory, run `make` inside the existing `pm64hh:decomp` image with its `/opt/miyoomini-toolchain` mounted or present. Override `OUT` to direct the result to a separate build directory. The established flags include `-ffp-contract=off`; no OpenSSL is used. The tool requires ARM hard-float `libc.so.6` and `libm.so.6` and the applicable loader from the target system.

The tool supports `--protocol-paper-mario-pal-v1`, `--check-central-recipe` and `--central <rom-path>`. The latter targets only the existing central Paper Mario standard guide and validates its exact recipe. Existing valid images are retained; optional ROM images remain outside the guide ZIP. It is not a generic converter for other games. Before installing over an existing central tool, compare versions and hashes, preserve the existing file, and verify compatibility; do not overwrite it silently. The ROM is never included in either package.

The original repository's licence text is preserved separately in `tools/LICENSE.miyoo64`. Its stated scope applies; the precise rights and provenance of the converter and guide data require review before publication.
