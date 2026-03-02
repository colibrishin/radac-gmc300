# System Patterns

## System Architecture (Target: GMC300.exe)

High-level flow (confirmed by analysis):

1. **Startup** → CRT → main.
2. **Config** → read gmc.xml (or equivalent) → COM port and options.
3. **Device** → open COM port (57600 8N1), init_com_after_open sends <GETVER>> (HEARTBEAT0 disabled: GETVER returns 2 bytes on some devices, so no valid 14-byte version → init fails before HEARTBEAT0), read_detector_sample sends <GETCPM>>; 2-byte CPM response (GQ-RFC1201).
4. **Server** → build HTTP (or other) request with detector data → send to Radioactive@home.
5. **BOINC** → if wrapper: read work unit input, write result output (trickle-up rad_report_xml and data.bin; formats implemented in main_app).

## Recovery Path

Recovered code lives in **recovered/**: main_app.cpp, config.cpp (TinyXML2), com_port.cpp, detector.cpp, serial_port.h + serial_port.cpp (Windows/POSIX), constants.h, config.h. No FUN_*; config and serial are reimplemented/abstracted. Build done (standalone and BOINC); optional: return int from read_detector_sample, tests.

## Key Technical Decisions (Analysis Approach)

- **Tool:** Ghidra only for static analysis; no execution in this repo.
- **Order:** Config → device protocol → server → BOINC, to follow data flow.
- **Naming:** Rename functions and add comments in Ghidra for config, serial, and network code paths.

## Component Relationships

- **Config** (config.cpp, TinyXML2): reads gmc.xml (gmc/comsettings/portnumber) and init_data XML (project_preferences: radacdebug, runtime). Init_data buffer is filled by entry point (main.cpp when GMC_USE_BOINC) from BOINC's aid.project_preferences **as-is** (full `<project_preferences>...</project_preferences>`); do not wrap again or `<runtime>` is not a direct child of root. Feeds COM port and app preferences to main_app.
- **Serial** (serial_port.h + win/posix): open/read/write abstracted; com_port.cpp and detector.cpp call this API only.
- **Detector** (detector.cpp): sends GETCPM, reads 2-byte CPM; used by main_app sample loop.
- **main_app:** Structured as four mainline functions: **init_prefs_and_config** (prefs, num_samples, COM settings), **resume_data_bin_and_trickle** (data.bin line count, almost-done exit, parse last line, report fraction_done, restore trickle_checkpoint.dat), **open_com_until_ready** (retry COM open/init), **run_main_loop** (read CPM, write data.bin, buffer/send trickle, checkpoint, wait). Each mainline calls derived inline helpers. fraction_done = (data_bin_line_count + total_samples_done)/num_samples; recovered on resume. Trickle: buffered multi-sample payload; trickle_checkpoint.dat at BOINC checkpoint; send on exit and lost sensor. data.bin line format: time_diff_ms (1000 first line), counter, Y-M-D H:M:S, 0, sample_type "f"/"r"/"n", 0.

## Design Patterns in Use

- Memory Bank for persistent context; .cursor/rules for Cursor-specific workflow (Plan/Act, memory-bank).
