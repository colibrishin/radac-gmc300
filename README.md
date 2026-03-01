# GMC-300 Reverse Engineering

Reverse-engineered documentation and **recovered C++** from **GMC300.exe**, the Windows BOINC application that sends [GQ GMC-300](https://www.gqelectronicsllc.com/comersus/store/comersus_viewItem.asp?idProduct=62) Geiger counter data to the [Radioactive@home](https://radioactiveathome.org/) project.
## Disclaimer

**I am not the author of the original GMC300.exe or the Radioactive@home application.** This repository is for analysis, documentation, and a reimplemented/recovered codebase only. It is not affiliated with GQ Electronics or the Radioactive@home project.

**Please do not use or modify this project to harm the Radioactive@home project.** Use it only for compatible clients, research, porting (e.g. Linux), or debugging. Do not submit fake or malicious data to the project’s servers or undermine the project’s goals.

## What’s in this repo

- **recovered/** — Recovered/reimplemented C++ (C++20, TinyXML2, platform-agnostic serial) that reproduces the behavior of GMC300.exe: config (gmc.xml, init_data), COM port, GMC-300 protocol (GETCPM/GETVER), data.bin output, and BOINC trickle/checkpoint/fraction_done (GETVER validates "GMC"; HEARTBEAT0 not used in init).
- **docs/** — Analysis notes: config format, BOINC XML reference, init_data, data flow, build instructions, extern symbols, etc.
- **memory-bank/** — Project context and progress for ongoing work.
- **third_party/** — Third-party libraries as git submodules (see **Third-party libraries** below).

## Third-party libraries

All of the following are used by the build and are included as git submodules under `third_party/`. Run **`git submodule update --init --recursive`** once from the repo root to initialize every submodule path.

| Library | Purpose | Location | License / source |
|--------|---------|----------|-------------------|
| **[TinyXML2](https://github.com/leethomason/tinyxml2)** | XML parsing for `gmc.xml` and init_data (config). | `third_party/tinyxml2` | zlib license |
| **[Asio](https://github.com/chriskohlhoff/asio)** (Boost.Asio) | Serial port I/O in `recovered/serial_port.cpp`. | `third_party/asio` | BSL-1.0 |
| **Boost.System** | Error codes used by Asio. | `third_party/system` | BSL-1.0 |
| **Boost.Config** | Configuration headers for Boost. | `third_party/config` | BSL-1.0 |
| **Boost.Assert** | Assert macros. | `third_party/assert` | BSL-1.0 |
| **Boost.ThrowException** | Exception-throwing helpers. | `third_party/throw_exception` | BSL-1.0 |
| **Boost.Align** | Aligned allocation. | `third_party/align` | BSL-1.0 |
| **Boost.WinAPI** | Windows API wrappers. *Windows builds only.* | `third_party/winapi` | BSL-1.0 |
| **Boost.Predef** | Compiler/platform predefinitions. *Windows builds only.* | `third_party/predef` | BSL-1.0 |
| **[BOINC](https://boinc.berkeley.edu)** | Client library for BOINC-linked build (optional; when `-DGMC_USE_BOINC=ON`). | `third_party/boinc` | LGPL-3.0+ |

Exact licenses and upstream URLs are in each library’s directory. This project is not affiliated with the authors of these libraries.

## Quick start (build)

1. Clone the repo and initialize **every** submodule path (all third-party libraries; see table above). From the repo root, run once:
   ```bash
   git submodule update --init --recursive
   ```
2. From the repo root:
   ```bash
   cmake -B build -S .
   cmake --build build
   ```
3. **Standalone (default):** The executable `gmc_recovered` uses stubs (no BOINC); COM port and config paths are as in `docs/build.md`.
4. **BOINC build:** Use `-DGMC_USE_BOINC=ON -DBOINC_DIR=/path/to/boinc`. See `docs/build.md` for details.

See **recovered/README.md** for file layout and **docs/build.md** for full build and dependency instructions.

## Documentation

| Document | Description |
|----------|-------------|
| [docs/replace-radac-app.md](docs/replace-radac-app.md) | How to replace the default Radioactive@home app with the custom binary, app_info.xml, and gmc.xml. |
| [docs/boinc-xml-reference.md](docs/boinc-xml-reference.md) | gmc.xml and init_data.xml layout; project_preferences (runtime, radacdebug). |
| [docs/build.md](docs/build.md) | CMake, TinyXML2, BOINC, and standalone vs BOINC build. |
| [docs/extern-symbols.md](docs/extern-symbols.md) | BOINC API and stubs required for linking. |

## License and attribution

**This project’s own code and documentation** (e.g. `recovered/`, `docs/`, `memory-bank/`, root CMake and README) are released under the **MIT License** — see [LICENSE](LICENSE).

- **Recovered code** in `recovered/` is derived by reverse engineering from GMC300.exe; it is not the original source. Original GMC300.exe is the property of its authors (Radioactive@home / BOINC project).
- **Documentation and analysis** in this repository are provided for educational and interoperability purposes only.
- **Third-party code** is under its own licenses (see **Third-party libraries** above and each library’s directory).

Use this repository responsibly and in a way that does not harm the Radioactive@home project or its participants.
