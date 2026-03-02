# Technical Context

## Technologies Used

- **Analysis:** Ghidra (Windows); existing project at `gmc-300.rep` (repository format). Lock file indicates Ghidra has been used on this machine.
- **Target:** Windows PE executable (GMC300.exe). **Language:** C++ (confirmed: STL, ATL, exceptions, vtables). **Standard:** C++03 (ISO/IEC 14882:2003). **Compiler:** Microsoft Visual C++ 9.0 (Visual Studio 2008).
- **Expected APIs:** Serial (CreateFile on COM, ReadFile/WriteFile, SetCommState), file (config), possibly WinInet/WinHTTP or sockets for server.

## Development Setup

- Workspace: `e:\Projects\gmc-reverse`. No build system required for analysis-only work.
- Optional: Python/Jython or PyGhidra for scripts; Wireshark or HTTP proxy to validate reversed server protocol at runtime.

## Technical Constraints

- No source code; analysis is binary-only. Stripped symbols likely.
- Some GMC-300 firmware versions (e.g. v1.14) may produce odd characters; parsing logic may contain workarounds or bugs.
- BOINC wrapper behavior (stdin/stdout vs file) must be inferred from binary.

## Recovery / Recompile

- **Recovered code:** C++20, in **recovered/**. **Config:** TinyXML2. **Serial:** Platform-agnostic API; serial_port.cpp (Windows and POSIX). **main_app** is organized as four mainline inline functions (init_prefs_and_config, resume_data_bin_and_trickle, open_com_until_ready, run_main_loop) plus derived inline helpers; trickle uses buffered payload and **trickle_checkpoint.dat** for resume. **Constants** (constants.h): SAMPLE_INTERVAL_SEC = 30, trickle min pending/interval; runtime from init_data → num_samples. CMake at repo root; BOINC or stubs (see recovered/README.md and docs/build.md).

## Dependencies

- Ghidra installation (user-managed).
- GMC300.exe and optionally BOINC slot XML samples (e.g. F:\BOINCdata\slots\1\gmc.xml, init_data.xml) for reference.
- **Build:** [TinyXML2](https://github.com/leethomason/tinyxml2) (add to project); BOINC source in repo (for API or stubs).
