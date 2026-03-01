# Active Context

## Current Work Focus

- **Recovered code** in **recovered/** is refactored and self-contained: **TinyXML2** for config, **platform-agnostic serial** (serial_port.h + serial_port.cpp), **C++20**, **data.bin** write with format matching original GMC300.exe. **Init_data** is read from BOINC and used as-is (no double-wrap); **runtime** and **fraction_done** are validated. **Next:** Optional: **read_detector_sample** return int; BOINC-linked build; tests.

## Recent Changes

- **Init_data / runtime:** BOINC stores full `<project_preferences>...</project_preferences>` in `aid.project_preferences` (see BOINC `lib/parse.cpp` `dup_element`). **main.cpp** copies this string as-is into `g_init_data_buf`; do **not** wrap again with `build_project_preferences_document()` or `<runtime>` is not found (nested root). See **docs/init_data-culprit.md**.
- **fraction_done:** Set from `total_samples_done / num_samples` after each completed sample; `boinc_fraction_done(1.0)` called before `boinc_finish_and_exit(0)` so BOINC sees 100% on completion.
- **Runtime debug:** Startup line includes `num_samples=N (init_data=yes|no)`; when init_data present, separate debug lines for runtime→num_samples or "runtime missing/zero". **main.cpp** (BOINC build) logs init_data parse/availability/size to stderr for diagnostics. Debug/standalone build has `g_init_data_len=0` so no init_data messages.
- **data.bin format:** One line per sample: time_diff_ms (1000 first line), counter (accumulated CPM), local Y-M-D H:M:S, 0, sample_type "r"/"n", 0. Resume uses 1st token as time_diff_ms.
- **Timing:** time_diff_ms from (time_prev_sample - last_sample_time); rate uses min 60s; SAMPLE_INTERVAL_SEC = 30 (constants.h). HEARTBEAT0 disabled in init (GETVER often returns 2 bytes).
- **Config / Serial / C++20:** TinyXML2; serial abstracted; constants.h; app_io.h for debug/data file streams.

## Next Steps

1. **Build:** CMakeLists.txt at repo root; see docs/build.md. Standalone (default) vs BOINC (`-DGMC_USE_BOINC=ON`).
2. **read_detector_sample:** Consider return int (0 success, -1 fail).
3. Optional: tests; data.bin path/slot doc.

## Active Decisions

- **Ghidra MCP:** Available as server **user-ghidra** (when configured).
- **Config:** TinyXML2 only. **Serial:** Platform-agnostic API; Windows and POSIX in serial_port.cpp.
- **Init_data:** Entry point (main.cpp when GMC_USE_BOINC) must set `g_init_data_buf` and `g_init_data_len` from BOINC API; main_app uses buffer as-is for read_project_preferences.
- **Recovery target:** C++20; TinyXML2 + BOINC (or stubs) + one serial implementation.
