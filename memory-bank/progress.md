# Progress

## What Works

- Ghidra project and Phases 1–5 analysis complete (config, protocol, server, data flow, step5 critical-path cleaned).
- **Recovered C++** in **recovered/**:
  - **main_app.cpp**, **config.cpp**, **com_port.cpp**, **detector.cpp** with clear names, constants (constants.h), and control flow.
  - **Config:** TinyXML2; **config.h** API; gmc.xml (comsettings/portnumber) and init_data (project_preferences: radacdebug, runtime). **Init_data:** When GMC_USE_BOINC, main.cpp copies `aid.project_preferences` as-is into g_init_data_buf (no extra wrap so `<runtime>` is direct child of root); see docs/init_data-culprit.md.
  - **Serial:** **serial_port.h** + **serial_port.cpp** (Windows and POSIX). HEARTBEAT0 disabled in init (GETVER often 2 bytes). COM closed on init fail.
  - **data.bin:** One line per CPM sample: time_diff_ms (1000 first line), counter (accumulated CPM), local Y-M-D H:M:S, 0, sample_type "r"/"n", 0. Resume parses 1st token as time_diff_ms.
  - **Timing:** time_diff_ms from elapsed time; SAMPLE_INTERVAL_SEC = 30 (constants.h); rate uses min 60s; sleep_one_second(1,0) for interval wait.
  - **BOINC progress:** fraction_done = total_samples_done/num_samples after each completed sample; boinc_fraction_done(1.0) before boinc_finish_and_exit(0).
  - **Diagnostics:** Startup line shows `num_samples=N (init_data=yes|no)`. When init_data present, debug lines for runtime→num_samples or "runtime missing/zero". main.cpp (BOINC) logs init_data parse/availability/size to stderr. Standalone build: no init_data (g_init_data_len=0).
  - **C++20** style; app_io.h for debug/data streams.
- **Docs:** phase2–5, boinc-xml-reference.md, init_data-culprit.md, recovered/README.md, build.md. All FUN_* renamed (~436+).

## What's Left to Build / Do

1. **Build:** **Done for standalone and BOINC.** CMakeLists.txt at repo root; standalone (default) uses stubs_standalone.cpp; BOINC use `-DGMC_USE_BOINC=ON -DBOINC_DIR=...`. See docs/build.md.
2. **read_detector_sample:** void; main_app infers success/fail. Prefer return int (0 / -1).
3. **Optional:** Tests; document data.bin/gmc.xml paths (slot = cwd under BOINC).

## Current Status

- **Analysis:** Complete. Recovery steps 1–5 done.
- **Recovered code:** Refactored, TinyXML2, platform-agnostic serial, C++20. Init_data and fraction_done validated; runtime from project_preferences applied when BOINC build and init_data present.

## Resolved / optional

- **Known issues and TBDs from earlier phases are resolved:** Build (standalone and BOINC), config, data.bin, trickle XML, fraction_done, and init_data/runtime are implemented and documented. Externs are supplied by stubs or BOINC wrapper (docs/extern-symbols.md).
- **Optional cleanup:** `read_detector_sample` is void; main_app infers success/fail. Could change to return int (0 / -1) for clarity.
