# Universal Guide packages for Miyoo64

Local preparation for a future `SunnyBoon/universal-guide-miyoo64` repository. The first package is the existing Paper Mario PAL standard guide: 256 stable IDs, German base text, EN/FR/ES overlays, hints, group assignments, image references and the validated PAL image recipe. Guide text and IDs were copied from G3.12 without editorial changes. `standard/manifest.ini` is packaging metadata; G3.11 treats its standard package specially and enables its existing PAL adapter.

The archive does not include ROMs, original or ROM-derived images, game binaries, saves or the native converter. The separately installed PAL converter remains under `App/UniversalGuide/tools/PaperMarioPAL/`; missing guide images remain optional. The standard package belongs under `App/UniversalGuide/PaperMarioPAL/standard/`.

## Catalog and package

`catalog.json` uses `format_version: 1`. Each entry declares `id`, `name`, `port`, `version`, `package_format`, `url`, byte `size` and `sha256`. `availability: planned` means no release exists yet. The URL is a proposed future public endpoint, not a working or published release. The SHA-256 binds the catalog entry to one archive; it does **not** authenticate the catalog itself. Catalog authenticity and trusted distribution are unresolved.

Create a deterministic archive with Python 3:

```
python3 tools/build_package.py dist/paper-mario-pal-standard-0.1.0.zip
python3 tools/test_distribution.py
```

The builder validates the guide and refuses unexpected files. Archive entry order, timestamp and permissions are fixed. Byte-for-byte reproducibility has been checked with the current Python/zlib environment; compression output across differing zlib versions is not guaranteed. The output path must not already exist.

## Host-only download prototype

`tools/install_host.py` reads the catalog, downloads via HTTPS, checks size and SHA-256, rejects unsafe/unexpected ZIP entries, validates the staged package, checks the image-recipe hash and swaps the standard directory with rollback. Existing regular files in `images/` are carried across; symlinks and unknown non-image files cause a safe abort. It does not write progress, save or filter files. Cooperative locking does not protect against unrelated processes modifying files concurrently; close the game and do not run other installers. Interrupted transactions may leave a clearly named backup for inspection.

**This prototype intentionally installs only beneath an existing directory under the host temporary directory. It is not a Miyoo installer.** For a local test, build the archive and pass a `file:` URL under `/tmp` with `--local-test --test-url`; `tools/test_distribution.py` exercises that path automatically. A private release would require authenticated retrieval on the PC/WSL, for example via GitHub's release-asset API with repository Contents read permission; do not place credentials or temporary tokens on the Miyoo. Public release assets can later be requested without authentication. No private or public release has been created by this preparation.

## Rights and publication

No licence is assigned here to the guide text, local game names or ROM-derived data. Resolve rights and provenance before publishing. The copied upstream licence text in `tools/LICENSE.miyoo64` retains its original limited scope; it is not a blanket licence for guide content or third-party material. The local Paper Mario history includes private game images: do not publish it or reuse it as the new repository's history. This repository has a separate, initially empty history.

## Optional PAL image converter

The optional Paper Mario PAL image converter is distributed separately from the guide. Its exact G3.12 source, Makefile, provenance and target requirements are documented in `tools/converters/paper-mario-pal/README.md`. The converter's version is separate from the guide version; `paper-mario-pal-v1` is a port-specific protocol, while the guide's package format is version 1. The guide remains usable without a converter or images.

Using the existing `pm64hh:decomp` image, build the converter with the Makefile and the Miyoo ARM toolchain, then run:

```
python3 tools/package_converter.py PATH/TO/guide-pal-rom-converter dist/paper-mario-pal-converter-0.1.0-arm32.zip
```

This creates exactly `App/UniversalGuide/tools/PaperMarioPAL/guide-pal-rom-converter` in the ZIP. The packager validates ELF32 ARM EABI5 hard-float and the expected libc/libm dependencies. Before any later installation or update, inspect and back up a pre-existing central converter; do not overwrite it automatically. Both catalog URLs are planned, not existing releases. `tools/install_host.py` currently handles only the guide data package, not converter deployment.
