# BOINC wrapper and API reference

Reference: cloned BOINC source under `boinc/` in this repo.

## Wrapper source (runs non-BOINC apps under BOINC)

| Path | Description |
|------|-------------|
| **`boinc/samples/wrapper/wrapper.cpp`** | Main wrapper: parses `job.xml`, runs tasks (e.g. GMC300.exe), suspend/resume/quit, checkpoint, CPU time, trickle-up. |
| `boinc/samples/wrapper/wrapper_win.h` | Windows resource include (not API). |
| `boinc/samples/wrapper/ReadMe.txt` | Mac build notes. |
| `boinc/win_build/wrapper.vcxproj` | Windows Visual Studio project for the wrapper. |

The wrapper does **not** implement the GMC-300 logic; it runs the **application** (e.g. GMC300.exe) as a task. GMC300.exe is the **app** that uses the BOINC API and talks to the detector.

## BOINC API (used by GMC300.exe and other apps)

| Path | Description |
|------|-------------|
| **`boinc/api/boinc_api.h`** | Declarations for C-linkage API used by apps. |

### Key functions (from `boinc_api.h`) – match names used in Ghidra renames

- `boinc_init()` / `boinc_init_options(BOINC_OPTIONS*)` – init BOINC runtime.
- `boinc_finish(int status)` – exit and report result.
- `boinc_send_trickle_up(char* variety, char* text)` – send trickle message (e.g. "rad_report_xml").
- `boinc_checkpoint_completed()` – mark checkpoint done.
- `boinc_fraction_done(double)` – report progress 0..1.
- `boinc_begin_critical_section()` / `boinc_end_critical_section()` – wrap checkpoint writes.
- `boinc_time_to_checkpoint()` – whether it’s time to checkpoint.
- `boinc_is_standalone()` – running without BOINC client.
- `boinc_get_status(BOINC_STATUS*)` – heartbeat, suspend, quit, etc. (used by wrapper; app may use it too).
- `boinc_resolve_filename()` – resolve logical to physical path.
- `boinc_report_app_status(cpu_time, checkpoint_cpu_time, fraction_done)` – status report (wrapper uses this).

## Wrapper flow (from `wrapper.cpp`)

1. Parse cmdline (`--nthreads`, `--device`, `--trickle`, etc.).
2. Parse **job.xml** (tasks, application path, command_line, stdin/stdout/stderr, checkpoint_filename, fraction_done_filename).
3. `boinc_init_options()` + `boinc_get_init_data()`.
4. Start daemons (if any), then for each task: `task.run()` (CreateProcess/exec), then poll: `task.poll()`, `poll_boinc_messages()`, `boinc_report_app_status()`, `boinc_send_trickle_up()` (if `--trickle`), checkpoint if `task.has_checkpointed()`.
5. `boinc_finish(0)`.

## GMC300.exe vs wrapper

- **Wrapper** = generic BOINC sample that runs **any** app described in job.xml (e.g. GMC300.exe).
- **GMC300.exe** = Radioactive@home app: reads config (e.g. project_preferences), opens COM port, talks to GMC-300, writes data.bin, calls `boinc_send_trickle_up("rad_report_xml", xml)`, `boinc_fraction_done()`, `boinc_checkpoint()`, `boinc_finish()`.

So the **wrapper source** is the reference for how the **client** runs GMC300; the **boinc_api.h** (and lib) is the reference for the **symbols/names** we see in the GMC300 binary.
