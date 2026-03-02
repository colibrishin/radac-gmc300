# Active Context

## Current Work Focus

- **Recovered code** in **recovered/** is refactored and modular: **main_app** is split into four mainline functions (**init_prefs_and_config**, **resume_data_bin_and_trickle**, **open_com_until_ready**, **run_main_loop**) with derived inline helpers. **Trickle** uses buffered multi-sample sends and a **trickle checkpoint file** for resume; **fraction_done** is restored on resume and uses (data_bin_line_count + total_samples_done). **Next:** Optional: **read_detector_sample** return int; tests.

## Recent Changes

- **main_app structure:** Flow is Init → Resume → COM open → Main loop. Each phase implemented as inline functions; mainline functions call derived helpers (e.g. init_prefs_from_init_data, load_data_bin_line_count, try_one_com_open, finish_and_exit_with_trickle, write_data_bin_and_trickle). Section comments and single-purpose helpers make the flow easy to follow.
- **Trickle:** Buffered like sample program (radac release-1.77): multiple `<sample>` in one payload; send when pending > 3 (or 2 debug) and ≥ 20 min (or 10 min debug) since last send; keep last sample in buffer after send. **trickle_checkpoint.dat** written at BOINC checkpoint (last_send_time, pending count, buffer); restored on resume so unsent samples and 20-min interval are preserved. Always log "trickle sent len=… samples=…" (or "trickle send failed ret=…") for validation.
- **fraction_done:** Recovered on resume: report (data_bin_line_count/num_samples) immediately after loading data.bin; when a sample completes use (data_bin_line_count + total_samples_done)/num_samples so progress does not reset to zero after BOINC resume.
- **Stdio:** get_debug_stream() called at most once per function and result reused; conditional call when debug_enabled only where appropriate.
- **Init_data / runtime / data.bin / config:** Unchanged; see progress.md and docs.

## Next Steps

1. **Build:** CMakeLists.txt at repo root; see docs/build.md. Standalone (default) vs BOINC (`-DGMC_USE_BOINC=ON`).
2. **read_detector_sample:** Consider return int (0 success, -1 fail).
3. Optional: tests; data.bin path/slot doc.

## Active Decisions

- **Ghidra MCP:** Available as server **user-ghidra** (when configured).
- **Config:** TinyXML2 only. **Serial:** Platform-agnostic API; Windows and POSIX in serial_port.cpp.
- **Init_data:** Entry point (main.cpp when GMC_USE_BOINC) must set `g_init_data_buf` and `g_init_data_len` from BOINC API; main_app uses buffer as-is for read_project_preferences.
- **Recovery target:** C++20; TinyXML2 + BOINC (or stubs) + one serial implementation.
