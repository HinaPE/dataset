dataset — Portable dataset packer and reader (C++/CLI)

[![CI](https://github.com/OWNER/REPO/actions/workflows/ci.yml/badge.svg)](https://github.com/OWNER/REPO/actions/workflows/ci.yml)

Overview
- dataset builds a compact “hostpack” file from a NeRF Synthetic dataset (e.g., lego), and provides a small C++ API to read frames, camera parameters, and scene metadata.
- A minimal CLI is included to build packs and inspect their contents.

Features
- Fast PNG decode via libspng + zlib
- Optional float output with linearized sRGB (RGBA32F)
- Memory-mapped read API for zero-copy image access
- Self-contained CMake build using FetchContent (no system deps required)

Requirements
- CMake 3.26+
- A C++23-capable toolchain (MSVC 19.3x+, Clang 16+, GCC 12+)
- Ninja or Make (CI uses Ninja; see below)

Third‑party dependencies (auto-fetched)
- simdjson v4.0.6
- zlib 1.3.1
- libspng v0.7.4 (static)
- oneTBB v2022.2.0
- OpenEXR v3.4.0 + Imath v3.2.1

Build
- Windows (MSVC + Ninja)
  - `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build -j`

- Linux/macOS (Clang/GCC + Ninja)
  - `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build -j`

- Options
  - `-DBUILD_CLI=ON|OFF` build CLI (default ON)
  - `-DFETCHCONTENT_UPDATES_DISCONNECTED=ON` to avoid network updates after first fetch

CLI Usage
- Help
  - `build/dataset_cli --help`

- Build a hostpack from NeRF Synthetic “lego”
  - RGBA8: `build/dataset_cli build data/nerf_synthetic/lego auto build/lego_rgba8.hpk --threads 4`
  - RGBA32F: `build/dataset_cli build data/nerf_synthetic/lego auto build/lego_rgba32f.hpk --pf rgba32f --threads 4`

- Inspect
  - `build/dataset_cli info build/lego_rgba8.hpk`
  - `build/dataset_cli list build/lego_rgba8.hpk`

Library API (C++)
- Public header: `include/dataset.h`
- Example
  - `dataset::PackHandle h = dataset::open_hostpack("build/lego_rgba8.hpk");`
  - `size_t n = dataset::frame_count(h);`
  - `auto v = dataset::image_view(h, 0); // width/height/strides`
  - `dataset::close_hostpack(h);`

Notes
- PNG file paths are taken from the JSON’s `frames[*].file_path`. If no extension is present, `.png` is assumed and resolved relative to the dataset root.
- Packs are written with block and row alignment for efficient reading; see CLI flags.

Continuous Integration
- GitHub Actions builds the project on Windows, Linux, and macOS using CMake + Ninja and runs a small smoke test.
- Workflow file: `.github/workflows/ci.yml`

Contributing
- Use minimal, focused changes that keep the current code style.
- Open a pull request with a clear description; CI will build on all three OSes.
- Please file issues for bugs or enhancement requests.

License
- This project is licensed under the Mozilla Public License 2.0. See `LICENSE`.

Acknowledgements
- libspng, zlib, simdjson, oneTBB, OpenEXR, and Imath are used under their respective licenses.
