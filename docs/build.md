# Build Environment (GMC300 Recovered Code)

This document describes how to **build the recovered GMC300 application** (or a reimplementation). There is **no original project file**; the build is provided by CMake at the repo root. The recovered code uses **C++20**.

**Build modes:**
1. **Standalone (default):** Uses `stubs_standalone.cpp` for BOINC and wrapper symbols; no BOINC library. For testing and development.
2. **BOINC-linked:** Set `-DGMC_USE_BOINC=ON`; uses `boinc_wrapper.cpp` and links libboinc + libboincapi. See section 2 (Option B) and sections 3, 5, 6.

---

## 1. Goals

- **Compile** recovered (or reimplemented) C++ that implements the GMC300.exe behavior (config, COM, GMC-300 protocol, trickle-up).
- **Standalone:** One executable that runs with stubs (no BOINC). **BOINC-linked:** Link against the BOINC client library (libboinc + libboincapi) on your platform.
- **Target:** C++20 (required); CMake 3.16+; compilers: **GCC, Clang, or MSVC** (any platform).
- **Output:** An executable that reads gmc.xml, talks to the GMC-300 (serial), and (when BOINC-linked) sends trickle-up messages under the wrapper.

---

## 2. Build with CMake

### Option A: Standalone (default, for testing)

Uses stubs for all BOINC and wrapper symbols; no BOINC library required.

From the **repo root**:

1. **Submodules:** CMake requires all third-party deps as submodules. From the **repo root**, run **once**:
   ```bash
   git submodule update --init --recursive
   ```
   This initializes **every** submodule path (all of `third_party/tinyxml2`, `third_party/asio`, `third_party/system`, `third_party/config`, `third_party/assert`, `third_party/throw_exception`, `third_party/align`, `third_party/winapi`, `third_party/predef`, `third_party/boinc`, and any nested submodules). One command applies to all paths. Optional: set `-DTINYXML2_DIR=/path/to/tinyxml2` if TinyXML2 is elsewhere.

2. **Configure:**
   ```bash
   cmake -B build -S .
   ```
   If TinyXML2 is elsewhere: `cmake -B build -S . -DTINYXML2_DIR=/path/to/tinyxml2`

3. **Build:**
   ```bash
   cmake --build build
   ```

The executable is `build/gmc_recovered` (or `build\gmc_recovered.exe` on Windows). Data.bin and gmc.xml are read from the current working directory.

### Option B: BOINC-linked build

Uses `boinc_wrapper.cpp` and links **libboinc** and **libboincapi**; stubs are not used.

1. **Build BOINC** from `third_party/boinc/` (or your BOINC tree). Produce **libboinc** and **libboincapi** (see “How to get the BOINC library” below).

2. **Configure with BOINC:**
   ```bash
   cmake -B build -S . -DGMC_USE_BOINC=ON
   ```
   **`BOINC_DIR`** defaults to **`third_party/boinc`** when not set. CMake then:
   - Sets **BOINC_INCLUDE_DIR** = `BOINC_DIR/api` + `BOINC_DIR/lib`
   - Tries to find **BOINC_LIB** in this order: `BOINC_DIR/lib/Release/libboinc.lib`, `lib/Debug/libboinc.lib`, `win_build/Build/x64/Release/libboinc.lib`, `win_build/Build/x64/Debug/libboinc.lib`, `win_build/.../boinc.lib`, or `BOINC_DIR/lib/libboinc.a`
   - Tries to find **BOINC_API_LIB** (libboincapi.lib or libboincapi.a) next to libboinc or in `BOINC_DIR/lib`

   If BOINC is elsewhere:
   ```bash
   cmake -B build -S . -DGMC_USE_BOINC=ON -DBOINC_DIR=/path/to/boinc
   ```
   Or set libs explicitly:
   ```bash
   cmake -B build -S . -DGMC_USE_BOINC=ON -DBOINC_INCLUDE_DIR="C:/boinc/api;C:/boinc/lib" -DBOINC_LIB=C:/boinc/lib/Release/libboinc.lib
   ```

3. **Build:**
   ```bash
   cmake --build build
   ```
   On **Windows with MSVC**, CMake sets static CRT (`/MT`) and `_ITERATOR_DEBUG_LEVEL=0` to match typical prebuilt BOINC libs; use `--config Release` for Release. On **Linux/macOS**, BOINC and this app use the system compiler and runtime

**Note:** The BOINC client library may not export every symbol the recovered code expects (e.g. `get_debug_file`, `fopen_data_bin`, `init_time_dwords`). Those are often provided by the **wrapper** that launches the app. If the link fails with unresolved externals, you must either link an additional wrapper library that defines them or implement a small “wrapper glue” object. See **docs/extern-symbols.md** for the full list.

### Submodules (third_party)

CMake expects these under **`third_party/`** (all via `git submodule update --init --recursive`):

| Submodule | Purpose | Required on |
|-----------|---------|-------------|
| **tinyxml2** | XML (gmc.xml, init_data) | All |
| **asio** | Serial port (recovered/serial_port.cpp) | All |
| **system** | Boost.System (error_code for Asio) | All |
| **config** | Boost.Config | All |
| **assert** | Boost.Assert | All |
| **throw_exception** | Boost.ThrowException | All |
| **align** | Boost.Align | All |
| **winapi** | Boost.WinAPI | Windows only |
| **predef** | Boost.Predef | Windows only |
| **boinc** | BOINC client (when GMC_USE_BOINC=ON) | BOINC build only |

On Linux/macOS, **winapi** and **predef** are not required (CMake checks them only when `WIN32`).


### How to get the BOINC library (from this repo's `third_party/boinc/`)

BOINC is built with the **native toolchain** for each platform.

**Linux / macOS (CMake)**

- **`third_party/boinc/lib/CMakeLists.txt`** builds libboinc (needs CMake 3.20+, OpenSSL, libzip). Build from a separate dir, then set **`BOINC_DIR`** to `third_party/boinc` (or the path where you built). CMake will use **`BOINC_DIR/lib/libboinc.a`** and, if present, **`BOINC_DIR/lib/libboincapi.a`**.

**Windows (Visual Studio)**

1. Open **`third_party/boinc/win_build/boinc.sln`** in Visual Studio (2019 or 2022).
2. Restore or install dependencies (vcpkg etc.; see `third_party/boinc/win_build/boinc.props` and BOINC README).
3. Select **Release** (or Debug) and **x64**, then build **libboinc** and **libboincapi**.
4. Outputs: **`third_party/boinc/win_build/Build/x64/Release/libboinc.lib`** and **`libboincapi.lib`** (or Debug).
5. GMC CMake with **`BOINC_DIR=third_party/boinc`** (default) looks for libs in `lib/Release/` or `lib/Debug/` first, then in `win_build/Build/x64/Release/` (or Debug). Copy/symlink into **`third_party/boinc/lib/Release/`** if needed, or leave in `win_build/Build/...` — CMake auto-detects.

**Windows (MinGW)**

- **`third_party/boinc/lib/Makefile.mingw`** builds from `third_party/boinc/lib` with `make -f Makefile.mingw BOINC_SRC=..`. Output is **`libboinc.a`** in that directory; set **`BOINC_LIB`** and **`BOINC_INCLUDE_DIR`** (or **`BOINC_DIR=third_party/boinc`**).

### Building on Linux / macOS

- **Submodules:** Run **`git submodule update --init --recursive`** once from the repo root so **every** submodule path is initialized. On Linux/macOS, CMake does not require **winapi** or **predef** (it only checks them when building on Windows), but initializing all paths is recommended for a consistent repo.
- **Build:** From repo root, `cmake -B build -S .` then `cmake --build build`. CMake will link **pthread** (via `Threads::Threads`) for Asio on Unix.
- **Serial port:** Device names are `/dev/ttyS*` on Linux and `/dev/cu.usbserial*` on macOS (see `serial_port.cpp`). Ensure the user has access to the serial device (e.g. add to `dialout` on Linux).

---

## 3. Prerequisites (BOINC-linked build)

| Dependency | Source | Notes |
|------------|--------|--------|
| **BOINC library** | `third_party/boinc/` in this repo | Build libboinc and libboincapi with the **native toolchain** for your platform (CMake on Linux/macOS, Visual Studio or MinGW on Windows). See “How to get the BOINC library” above. |
| **BOINC API headers** | `third_party/boinc/api/boinc_api.h`, `third_party/boinc/lib/*.h` | APP_INIT_DATA in third_party/boinc/lib/app_ipc.h; common_defs.h, etc. |
| **Platform** | — | **Linux/macOS:** GCC or Clang, pthread (for Asio). **Windows:** MSVC or MinGW; recovered serial uses Asio (no direct Windows SDK in app code). |

---

## 4. Project layout (CMake)

Layout used by the root **CMakeLists.txt**:

```
gmc-reverse/
  CMakeLists.txt             # Root build; options GMC_USE_BOINC, TINYXML2_DIR, BOINC_DIR
  third_party/               # Git submodules (init with git submodule update --init --recursive)
    tinyxml2/                # TinyXML2 (config XML)
    asio/                    # Asio (serial port)
    system/                  # Boost.System
    config/                  # Boost.Config
    assert/                 # Boost.Assert
    throw_exception/         # Boost.ThrowException
    align/                   # Boost.Align
    winapi/                  # Boost.WinAPI (Windows only)
    predef/                  # Boost.Predef (Windows only)
    boinc/                   # BOINC (when GMC_USE_BOINC=ON)
  recovered/                 # App source (all built into gmc_recovered)
    main.cpp                 # Entry; BOINC init + main_app
    main_app.cpp             # Config, COM, loop, data.bin, trickle (inline)
    config.cpp, config.h     # read_project_preferences, get_gmc_com_settings
    com_port.cpp             # open_com_port, init_com_after_open
    detector.cpp             # read_detector_sample
    serial_port.cpp          # Platform serial (Asio)
    commands.cpp             # debug_dump_com_handle, parse_int_cstr
    stubs_standalone.cpp     # Standalone build only
    boinc_wrapper.cpp        # BOINC build only
    (+ headers: constants.h, init_data.h, app_io.h, safe_c.h, serial_port.h, time_compat.h)
  docs/                      # build.md, boinc-xml-reference.md, etc.
```

Output: **`build/gmc_recovered`** (or **`build\gmc_recovered.exe`** on Windows).

---

## 5. Build Steps (Outline) — BOINC-linked

1. **Build BOINC lib**  
   From repo root: use BOINC’s Windows build (e.g. `third_party/boinc/win_build`, or CMake for `third_party/boinc/lib`). Produce libboinc.lib (or libboincapi.lib if the app uses only the API layer). Document the exact target (Debug/Release, static/dynamic).

2. **Create app project**  
   - Add recovered (or stub) source under `recovered/`.  
   - Include path: `third_party/boinc/api`, `third_party/boinc/lib`, and any BOINC dependency includes.  
   - Define any required macros (e.g. `_WIN32`, `HAVE_*` as in BOINC).

3. **Link**  
   - Link libboinc.lib (and libboincapi if used).  
   - Link CRT (default for the compiler).  
   - Link ATL if needed (or stub ATL usage).

4. **Fix compile/link errors**  
   - Missing symbols → add BOINC or CRT libs.  
   - Type mismatches → align with BOINC headers and recovered source types.  
   - Unresolved externals (e.g. BOINC internal) → use BOINC’s export list or rebuild BOINC with the same API surface.

---

## 6. BOINC API Surface (GMC300 usage)

From Phase 2–5 and BOINC reference, the app is expected to use:

- boinc_init / boinc_init_options  
- boinc_get_init_data_p / boinc_parse_init_data_file  
- boinc_resolve_filename  
- boinc_send_trickle_up  
- boinc_fraction_done, boinc_time_to_checkpoint, boinc_checkpoint_completed  
- boinc_begin_critical_section, boinc_end_critical_section  
- boinc_finish  

Ensure the linked BOINC library exports these (C linkage).

---

## 7. CI/CD (GitHub Actions)

From the repo root, **`.github/workflows/`** provide:

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| **CI** | Pull requests to `main` | Build standalone and BOINC on Ubuntu, macOS, and Windows (matrix). All use **Release** build type. |
| **CodeQL** | Push/PR to `main`, weekly | C++ code scanning (security/quality). |
| **Release** | Push of tag `v*` or `*-*` (e.g. `20250228-abc1234`), or `workflow_dispatch` with tag input | Build BOINC-linked binaries on Ubuntu, macOS, and Windows; create GitHub Release with artifacts. |
| **Tag and Release** | Push to `main`, or `workflow_dispatch` | Create tag `{date}-{SHA}`, push it, then call Release workflow (reusable) so a release is built and published. |

- **Release build type** is set explicitly in CI and Release workflows (`-DCMAKE_BUILD_TYPE=Release`). CMake does not default it.
- **Windows:** MinGW builds use `mingw32-make` for BOINC; standalone and BOINC both link **ws2_32** (Winsock) for serial/Asio.
- **BOINC link order:** libboincapi must be linked before libboinc (CMakeLists.txt uses `${BOINC_API_LIB} ${BOINC_LIB}`).

## 8. References

- **Critical path:** docs/phase5-data-flow.md  
- **BOINC API:** third_party/boinc/api/boinc_api.h  
- **BOINC build:** third_party/boinc/win_build (Windows; e.g. libboinc.vcxproj); third_party/boinc/lib (Linux/macOS; CMakeLists.txt).

---

*Update this document as the build is set up and when compiler, libs, or project layout change.*
