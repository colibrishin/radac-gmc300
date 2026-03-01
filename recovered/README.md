# Recovered GMC300 Source

This directory contains **recovered or reimplemented C++** from GMC300.exe.

**Disclaimer:** The maintainer of this repository is not the author of the original GMC300.exe or the Radioactive@home application. Do not use or modify this code to harm the Radioactive@home project (e.g. do not submit fake or malicious data to the project servers).

## Status

- **Critical-path functions** (main_app, config, com_port, detector, serial_port) with section comments and named parameters.
- **Human-readable pass:** Parameters and locals renamed; section comments and constants; main_app struct members named (num_samples, com_handle, fraction_done, etc.).
- **Files:** main_app.cpp, config.cpp (read_project_preferences, get_com_port_number), com_port.cpp (open_com_port, init_com_after_open), detector.cpp (read_detector_sample). Trickle and data.bin are inline in main_app.
- **Config / XML:** **TinyXML2** in config.cpp; `config.h` declares the API. XML layout matches BOINC slot files (see **docs/boinc-xml-reference.md**). **Init_data (BOINC build):** main.cpp copies `aid.project_preferences` **as-is** into `g_init_data_buf` (BOINC already provides full `<project_preferences>...</project_preferences>`; do not wrap again or `<runtime>` is not found). See **docs/init_data-culprit.md**.
- **Platform-agnostic serial:** `serial_port.h` + `serial_port.cpp` (Windows and POSIX). HEARTBEAT0 disabled in init (GETVER often returns 2 bytes).
- **BOINC progress:** fraction_done = total_samples_done/num_samples after each sample; boinc_fraction_done(1.0) before exit. Startup line logs `num_samples=N (init_data=yes|no)`. main.cpp (BOINC) logs init_data parse/availability to stderr. **Standalone (debug) build:** no init_data (g_init_data_len=0); num_samples = default 300.
- **C++20** style; app_io.h for debug/data streams.

## Naming and layout (suggested)

- `main.cpp` — entry, boinc_init, main_app.
- `config.cpp` / `config.h` — read_project_preferences, get_com_port_number, gmc.xml loading.
- `com_port.cpp` / `com_port.h` — open_com_port, init_com_after_open.
- `detector.cpp` / `detector.h` — read_detector_sample (GETCPM).
- `trickle.cpp` / `trickle.h` — build rad_report_xml payload, boinc_send_trickle_up.
## Build

See **docs/build.md** for full instructions. For a **full list of extern symbols** (BOINC API, globals, file/time, command strings) and what to implement or stub, see **docs/extern-symbols.md**.

- **CMake (recommended):** From repo root, run `git submodule update --init --recursive` (TinyXML2 is a submodule in `third_party/tinyxml2/`), then `cmake -B build -S .` and `cmake --build build`. Optionally set `-DTINYXML2_DIR=/path/to/tinyxml2` if using a different TinyXML2 location. **Standalone (default):** uses `stubs_standalone.cpp`; no BOINC. **BOINC-linked:** use `-DGMC_USE_BOINC=ON -DBOINC_DIR=/path/to/built/boinc` (see docs/build.md).
- **Serial port:** Compile `serial_port.cpp` with your app. The file selects Windows (CreateFile/ReadFile/WriteFile) or POSIX (open/read/write/termios) via `_WIN32`; works with MSVC, GCC, and Clang.
- **TinyXML2:** Provided as a git submodule at `third_party/tinyxml2/` ([TinyXML2](https://github.com/leethomason/tinyxml2)). Initialize with `git submodule update --init --recursive`.

## References

- **Data flow:** docs/phase5-data-flow.md
- **Protocol:** docs/phase3-protocol.md
- **Config/XML:** docs/boinc-xml-reference.md
