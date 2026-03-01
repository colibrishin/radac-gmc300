# BOINC API: Where Functions Live in the BOINC Repository

This document maps the symbols used by the recovered app to their definitions in the **boinc/** repository (and notes name/signature differences).

---

## Location of BOINC API (client library)

- **Header:** `boinc/api/boinc_api.h`  
  Declarations are under `extern "C"` (lines 31–128).
- **Implementation:** `boinc/api/boinc_api.cpp`

---

## Symbol mapping: recovered app → BOINC repo

| Recovered symbol | BOINC repo | File:line | Notes |
|------------------|------------|-----------|--------|
| `boinc_finish_and_exit(int)` | **boinc_finish(int)** | boinc_api.cpp:890 | Different name; same behavior (exit with status). |
| `boinc_checkpoint(void)` | **boinc_checkpoint_completed(void)** | boinc_api.cpp:1677 | BOINC has no `boinc_checkpoint()`; app calls this after saving state, so map to `boinc_checkpoint_completed()`. |
| `boinc_fraction_done(double)` | **boinc_fraction_done(double)** | boinc_api.cpp:1741 | Same. |
| `boinc_send_trickle_up(const char*, const char*)` | **boinc_send_trickle_up(char*, char*)** | boinc_api.cpp:1655 | Same; BOINC uses non-const in header. |
| `boinc_time_to_checkpoint(void)` | **boinc_time_to_checkpoint(void)** | boinc_api.cpp:1669 | Same; returns int (0/1). |
| `boinc_begin_critical_section(void)` | **boinc_begin_critical_section(void)** | boinc_api.cpp:1689 | Same. |
| `boinc_end_critical_section(void)` | **boinc_end_critical_section(void)** | boinc_api.cpp:1700 | Same. |

---

## Name/signature differences

1. **boinc_finish_and_exit**  
   The recovered app calls `boinc_finish_and_exit(status)`. The BOINC client API exposes **`boinc_finish(int status)`** (boinc_api.h:87, boinc_api.cpp:890). To link against the BOINC library you must either:
   - Provide a small wrapper that defines `boinc_finish_and_exit(int)` and calls `boinc_finish(status)`, or  
   - Change the recovered code to call `boinc_finish` instead of `boinc_finish_and_exit`.

2. **boinc_checkpoint**  
   The recovered app calls `boinc_checkpoint()`. The BOINC API has **`boinc_checkpoint_completed(void)`** (boinc_api.h:93, boinc_api.cpp:1677), which is called *after* the app has written its checkpoint. There is no `boinc_checkpoint()` in the current API. You must either:
   - Provide a wrapper that defines `boinc_checkpoint(void)` and calls `boinc_checkpoint_completed()`, or  
   - Change the recovered code to call `boinc_checkpoint_completed()` where it currently calls `boinc_checkpoint()`.

---

## Building the BOINC client library (Windows)

The library that exports the above symbols is built from the **lib** project in BOINC (not the full client GUI). Typical locations after building:

- **Visual Studio (win_build):** e.g. `boinc/win_build/Build/x64/Release/libboinc.lib` or `.../Debug/libboinc.lib`
- **CMake (lib):** e.g. `boinc/lib/Release/libboinc.lib` or `boinc/lib/libboinc.a` on Unix

Set `BOINC_DIR` to the BOINC **source** directory (so that `BOINC_DIR/api/boinc_api.h` and the lib path above exist), or set `BOINC_LIB` to the full path of the `.lib` / `.a` file.

---

## Quick reference: function locations in boinc/api/boinc_api.cpp

| Function | Line |
|----------|------|
| boinc_finish | 890 |
| boinc_send_trickle_up | 1655 |
| boinc_time_to_checkpoint | 1669 |
| boinc_checkpoint_completed | 1677 |
| boinc_begin_critical_section | 1689 |
| boinc_end_critical_section | 1700 |
| boinc_fraction_done | 1741 |
