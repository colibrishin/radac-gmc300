# Extern Symbols Required by the Recovered Code

The recovered app in **recovered/** declares many symbols as `extern`. To **build and run** (or test with stubs), every one of these must be supplied by either:

1. **Linking with BOINC** (and a small app/wrapper layer), or  
2. **Implementing stubs** (for standalone builds or tests).

Below is the full list, grouped by role and where they are used.

---

## 1. BOINC API (from BOINC client library)

| Symbol | Declared in | Purpose |
|--------|-------------|--------|
| `boinc_finish_and_exit(int)` | main_app.cpp, config.cpp | Exit app and tell BOINC the exit code. |
| `boinc_checkpoint(void)` | main_app.cpp | Request checkpoint (save state for restart). |
| `boinc_fraction_done(double)` | main_app.cpp | Report progress 0.0–1.0. |
| `boinc_send_trickle_up(const char*, const char*)` | main_app.cpp | Send trickle-up message (e.g. XML) to server. |
| `boinc_time_to_checkpoint(void)` | main_app.cpp | Returns whether it’s time to checkpoint. |
| `boinc_begin_critical_section(void)` | main_app.cpp | Enter critical section (no suspend). |
| `boinc_end_critical_section(void)` | main_app.cpp | Leave critical section. |

**To proceed:** Link with the BOINC client library (e.g. from `boinc/` in this repo). These are standard BOINC API functions; see `boinc/api/boinc_api.h` (or wrapper reference).

---

## 2. BOINC / wrapper-provided globals (read by recovered code)

| Symbol | Type | Declared in | Purpose |
|--------|------|-------------|--------|
| `init_time_dwords[3]` | `std::uint32_t[3]` | main_app.cpp | Initial time components (hi/med/lo) from BOINC/wrapper. |
| `data_bin_open_arg1` | `std::uint32_t` | main_app.cpp | Argument for first `fopen_data_bin` (e.g. mode/flags). |
| `data_bin_open_arg2` | `std::uint32_t` | main_app.cpp | Argument for second `fopen_data_bin` (no detector path). |
| `data_bin_open_arg3` | `std::uint32_t` | main_app.cpp | Argument for append `fopen_data_bin` (main loop). |
| `boinc_fraction_done_val` | `double` | main_app.cpp | Default/initial fraction_done value. |
| `ms_to_sec_divisor` | `double` | main_app.cpp | Convert milliseconds to seconds (e.g. 1000.0). |
| `uSv_h_divisor` | `double` | main_app.cpp | Convert CPM/elapsed to µSv/h (formula constant). |
| `trickle_buf_pad` | `std::uint16_t` | main_app.cpp | Padding word for trickle buffer (first element). |

**To proceed:** Either (a) provide these from the BOINC wrapper / init code that calls into the recovered app, or (b) define them in a small “app init” or stub module and set values to match BOINC behavior (see BOINC wrapper reference / `APP_INIT_DATA` if applicable).

---

## 3. File, time, and debug (wrapper or CRT)

| Symbol | Declared in | Purpose |
|--------|-------------|--------|
| `get_debug_file(void)` → `FILE*` | main_app.cpp, config.cpp, com_port.cpp, detector.cpp | BOINC/wrapper debug log file. |
| `parse_int_cstr(char*)` → `int` | main_app.cpp, config.cpp | Parse integer from string (e.g. config values). |
| `fopen_data_bin(const unsigned char*, unsigned int*)` → `FILE*` | main_app.cpp | Open `data.bin` in slot dir; second arg is in/out (e.g. mode from wrapper). |
| `get_time64(__time64_t*)` → `__time64_t` | main_app.cpp | Current time (64-bit); can be wrapper over `_time64` or POSIX `time`. |
| `sleep_one_second(unsigned int, int)` | main_app.cpp, com_port.cpp, detector.cpp | Sleep (e.g. retry delay); args may be seconds + a flag. |

**To proceed:**  
- **With BOINC:** Implement or use wrapper functions that match the BOINC slot directory and IPC (e.g. `boinc_resolve_filename("data.bin")` then `fopen`; BOINC’s time API).  
- **Standalone/stub:** Implement `get_debug_file` (e.g. stderr), `parse_int_cstr` (e.g. `strtol`/`atoi`), `fopen_data_bin` (e.g. `fopen(path, "a+b")` with slot dir), `get_time64` (e.g. `_time64` / `time`), `sleep_one_second` (e.g. `sleep`).

---

## 4. Recovered app’s own API (defined in recovered/)

These are **defined** in the recovered code; they are declared `extern` only in **main_app.cpp** for the single “main” compilation unit that calls them. No extra symbol is needed from BOINC or stubs — just link the recovered object files.

| Symbol | Defined in | Purpose |
|--------|------------|--------|
| `open_com_port(unsigned int, void**)` | com_port.cpp | Open serial port; fills handle in second arg. |
| `init_com_after_open(void**, int)` | com_port.cpp | Send GETVER/HEARTBEAT0 after open. |
| `read_detector_sample(void**, int, int*)` | detector.cpp | Send GETCPM, read 2-byte big-endian CPM into third arg. |
| `debug_dump_com_handle(void**)` | commands.cpp | Debug dump of COM handle. |

---

## 5. Config API (defined in recovered/)

Declared in **config.h**, implemented in **config.cpp**. Other modules that include config.h use these; no external library needed.

| Symbol | Purpose |
|--------|--------|
| `empty_config_ref` | Global handle for “empty” config. |
| `ref_assign_inc`, `release_config_value`, `config_string_empty`, `get_config_cstr`, `get_config_value`, `get_config_int` | Ref-counted config handle API. |
| `read_project_preferences(...)` | Parse init_data XML into config handle. |
| `get_com_port_number(...)` | Read gmc.xml and return COM port number. |

---

## 6. Command strings (GMC-300 protocol)

Used by **com_port.cpp** and **detector.cpp**; defined in **constants.h** as constexpr string_view.

| Symbol | Defined in | Type | Value |
|--------|------------|------|-------|
| `gmc::cmd_getver` | constants.h | `inline constexpr std::string_view` | `"<GETVER>>"` |
| `gmc::cmd_heartbeat0` | constants.h | `inline constexpr std::string_view` | `"<HEARTBEAT0>>"` |
| `gmc::cmd_getcpm` | constants.h | `inline constexpr std::string_view` | `"<GETCPM>>"` |

**Current:** Command strings are defined in **constants.h** as `inline constexpr std::string_view`; lengths `CMD_*_LEN` are derived from them. No definitions in commands.cpp (only `debug_dump_com_handle` remains there).
