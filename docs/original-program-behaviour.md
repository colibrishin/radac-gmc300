# Original program behaviour (GMC300.exe)

This document describes the **reference behaviour** of the original GMC300.exe application. It is used to align the recovered implementation and to interpret data.bin and trickle payloads. No reverse-engineering artefacts (addresses, internal names, or tool output) are included.

---

## 1. Trickle-up (rad_report_xml)

### Variety and payload

- **Variety:** `rad_report_xml`
- **Payload:** One or more `<sample>...</sample>` per send. The **recovered app** buffers multiple samples (aligned with the radac reference) and sends one payload containing several `<sample>` elements when the conditions below are met; after a send it keeps the last sample in the buffer. The program may queue samples and send when both a count and a time condition are met (see below).

### XML structure

- **timer** — Cumulative milliseconds from run start (first sample can be 0 or a small value).
- **counter** — Time-weighted counter: sum over samples of (CPM × interval in minutes). Increment per sample is (elapsed seconds since previous sample) / 60 × CPM, rounded.
- **timestamp** — `YYYY-MM-DD HH:MM:SS` (local time; in slot environments this may be UTC if the host is set to UTC).
- **sensor_revision_int** — Fixed `0`.
- **sample_type** — `f` (first line of a fresh run), `r` (first line after resume, or after a long gap within a run), or `n` (normal).
- **vid_pid_int** — Unsigned integer (e.g. 0 or device identifier).

### When trickles are sent

- Only when **not** in standalone mode.
- After each sample, the payload is queued. A send is attempted when **both**:
  - Pending samples ≥ 3 (or 2 when debug is enabled), and
  - At least 20 minutes (or 10 in debug) have passed since the last successful send.
- Pending data is also sent on normal exit and when the sensor is lost (after repeated read errors, before closing the port and retrying).
- **Trickle checkpoint (recovered app):** At BOINC checkpoint, the app writes **trickle_checkpoint.dat** with last send time, pending count, and the trickle buffer; on resume (when data.bin exists) it restores this state and deletes the file so unsent samples and the send interval are preserved across restarts.

---

## 2. data.bin

### Format (one line per sample)

Comma-separated fields:

1. **Timer (ms)** — Cumulative milliseconds from run start. Can be 0 on the first line when elapsed time is zero.
2. **Counter** — Time-weighted counter (same formula as in the trickle payload).
3. **Timestamp** — `Y-M-D H:M:S` (same convention as trickle).
4. **0** — Fixed.
5. **sample_type** — `f`, `r`, or `n` (same meaning as in trickle).
6. **0** — Fixed.

### sample_type rules

- **f** — First line of a run when there was no existing data.bin (fresh run).
- **r** — First line after a resume (data.bin already existed), or any line after a **long gap** since the previous line (gap &gt; 240×2000 ms, i.e. about 8 minutes).
- **n** — All other lines.

### Resume behaviour

- On startup, if data.bin exists, the program reads the **last line** and parses it.
- Only the **first field** (timer of the last line) is restored; it is used as the “previous timer” for the long-gap rule and for internal timing. The **counter is not restored**; it is reset to 0 when the COM port is opened (fresh or after resume).
- After each written line, the program stores the current line’s timer as the “previous timer” for the next sample.

*Recovered code in main_app.cpp follows this behaviour.*

---

## 3. Config and runtime

- **num_samples** — Derived from project runtime (minutes): `num_samples = (runtime_minutes × 60) / sample_interval_seconds`. The original uses a fixed sample interval (e.g. 240 s) and **no** startup buffer in this formula.
- **radacdebug** — When non-zero, debug mode: shorter trickle interval (10 min), lower pending threshold (2), and extra debug logging.
- **runtime** — Read from project_preferences; interpreted as runtime in minutes.

---

## 4. Relationship to recovered code

The recovered app in `recovered/` follows this behaviour where applicable. **main_app** is structured in four phases: init prefs/config → resume data.bin and trickle checkpoint → open COM until ready → main loop (with inline helpers). **fraction_done** is reported immediately on resume as data_bin_line_count/num_samples and during the loop as (data_bin_line_count + total_samples_done)/num_samples. Some choices differ by design (e.g. sample interval 30 s, runtime formula with buffer and effective seconds per sample so that task wall time matches project runtime). See Phase 4 and Phase 5 docs and `recovered/constants.h` for the current constants and any intentional deviations.

---

*Reference: behaviour of the original GMC300.exe for alignment and validation; no raw reverse-engineering data.*
