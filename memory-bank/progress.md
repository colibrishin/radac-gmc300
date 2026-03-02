# Progress

## What Works

- Ghidra project and Phases 1–5 analysis complete (config, protocol, server, data flow, step5 critical-path cleaned).
- **Recovered C++** in **recovered/**:
  - **main_app.cpp**, **config.cpp**, **com_port.cpp**, **detector.cpp** with clear names, constants (constants.h), and control flow. **main_app** is structured as four mainline inline functions (init_prefs_and_config, resume_data_bin_and_trickle, open_com_until_ready, run_main_loop) calling derived inline helpers; get_debug_stream() used at most once per function for stdio optimization.
  - **Config:** TinyXML2; **config.h** API; gmc.xml (comsettings/portnumber) and init_data (project_preferences: radacdebug, runtime). **Init_data:** When GMC_USE_BOINC, main.cpp copies `aid.project_preferences` as-is into g_init_data_buf (no extra wrap so `<runtime>` is direct child of root); see docs/init_data-culprit.md.
  - **Serial:** **serial_port.h** + **serial_port.cpp** (Windows and POSIX). HEARTBEAT0 disabled in init (GETVER often 2 bytes). COM closed on init fail.
  - **data.bin:** One line per CPM sample: time_diff_ms (1000 first line), counter (accumulated CPM), local Y-M-D H:M:S, 0, sample_type **"f"/"r"/"n"** (f = first line fresh run, r = first after resume, n = normal), 0. Resume parses 1st token as time_diff_ms.
  - **Trickle:** Buffered like sample program (radac): multiple `<sample>` per send; send when pending > 3 (2 in debug) and ≥ 20 min (10 in debug) since last send; send on exit and on lost sensor. **trickle_checkpoint.dat** persisted at BOINC checkpoint and restored on resume (preserves unsent buffer and last_send_time). Always log "trickle sent len=… samples=…" or "trickle send failed ret=…". See docs/phase4-server.md §7 Trickle validation.
  - **Timing:** CPM read interval **SAMPLE_INTERVAL_SEC** (30 s). num_samples = (runtime_min×60 − RUNTIME_BUFFER_SEC) / **EFFECTIVE_SEC_PER_SAMPLE** (~41 s per sample). rate uses min 60s; sleep_one_second(1,0) for interval wait.
  - **BOINC progress:** fraction_done = (data_bin_line_count + total_samples_done)/num_samples when a sample completes; recovered on resume (report data_bin_line_count/num_samples right after loading data.bin); boinc_fraction_done(1.0) before boinc_finish_and_exit(0).
  - **Diagnostics:** Startup line shows `num_samples=N (init_data=yes|no)`. When init_data present, debug lines for runtime→num_samples or "runtime missing/zero". main.cpp (BOINC) logs init_data parse/availability/size to stderr. Standalone build: no init_data (g_init_data_len=0).
  - **C++20** style; app_io.h for debug/data streams.
- **Docs:** phase2–5, boinc-xml-reference.md, init_data-culprit.md, recovered/README.md, build.md. All FUN_* renamed (~436+).

## What's Left to Build / Do

1. **Build:** **Done for standalone and BOINC.** CMakeLists.txt at repo root; standalone (default) uses stubs_standalone.cpp; BOINC use `-DGMC_USE_BOINC=ON -DBOINC_DIR=...`. Windows links ws2_32 (Winsock); BOINC link order libboincapi then libboinc. See docs/build.md.
2. **CI/CD:** GitHub Actions: CI (PR builds, standalone + BOINC on Ubuntu/macOS/Windows), CodeQL, Release (on tag or workflow_call), Tag-and-Release (push to main → tag then release). All use Release build type.
3. **read_detector_sample:** void; main_app infers success/fail. Prefer return int (0 / -1).
4. **Optional:** Tests; document data.bin/gmc.xml paths (slot = cwd under BOINC).

## Current Status

- **Analysis:** Complete. Recovery steps 1–5 done.
- **Recovered code:** Refactored, TinyXML2, platform-agnostic serial, C++20. main_app split into four mainline + derived inline functions; trickle buffered with checkpoint file for resume; fraction_done recovered on resume and uses (data_bin_line_count + total_samples_done). Init_data and runtime from project_preferences; data.bin sample_type "f"/"r"/"n"; trickle send rules and validation logging in place.
- **CI/CD:** CI on PR (all OS, Release); CodeQL; Release on tag or workflow_call; Tag-and-Release on push to main. Docs: phase5 (§8 timing/trickle), build.md (§7 CI/CD).
- **Trickle / data.bin:** Reference behaviour documented in docs/original-program-behaviour.md (trickle XML and send rules, data.bin format, sample_type, resume). phase4-server.md and boinc-xml-reference.md point to it; raw RE details kept out of public docs.

## Resolved / optional

- **Known issues and TBDs from earlier phases are resolved:** Build (standalone and BOINC), config, data.bin, trickle XML, fraction_done, and init_data/runtime are implemented and documented. Externs are supplied by stubs or BOINC wrapper (docs/extern-symbols.md).
- **Optional cleanup:** `read_detector_sample` is void; main_app infers success/fail. Could change to return int (0 / -1) for clarity.
