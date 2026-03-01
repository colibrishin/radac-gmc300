# Phase 4: Server Communication (GMC300.exe)

This document describes how GMC300.exe sends detector data to the Radioactive@home project: trickle-up only (no direct HTTP), the XML payload, data.bin, and how URLs and authentication are handled by the BOINC client.

---

## 1. Scope

- **Mechanism:** The app does **not** perform direct HTTP or HTTPS requests. It builds a payload and uses the BOINC API **trickle-up** to send data; the BOINC client forwards trickles to the project server.
- **Contents:** Trickle variety and payload format, data.bin (path, when written, format), absence of URLs and in-payload auth in the app, and function roles.

---

## 2. Overview

**Trickle-up only.** GMC300.exe never opens a network connection or calls HTTP APIs. The flow is:

1. **Read detector** — Get CPM (and related sample data) from the GMC-300 via the serial protocol (see Phase 3).
2. **Build payload** — Build an XML string (and optionally prepare data for data.bin).
3. **Send trickle** — Call **boinc_send_trickle_up(variety, payload)** with variety **rad_report_xml** and the XML text.
4. **BOINC client** — The BOINC client reads the trickle-up file written by the API, then sends the message to the Radioactive@home project server using its own configuration and identity.

So: **detector → build payload → boinc_send_trickle_up → BOINC client sends to project server.**

---

## 3. Trickle-up

### 3.1 Variety and payload type

| Item | Value |
|------|--------|
| **Variety** | **rad_report_xml** |
| **Payload** | XML string; format matches the original program. See §3.2 and docs/boinc-xml-reference.md; reference behaviour in docs/original-program-behaviour.md. |

### 3.2 Trickle payload (original program behaviour)

The original program passes to **boinc_send_trickle_up("rad_report_xml", xml)**:

- One `<sample>...</sample>` per send. The app may queue samples and send when both conditions in §3.3 are met.
- **Tags:** `<timer>`, `<counter>`, `<timestamp>`, `<sensor_revision_int>`, `<sample_type>`, `<vid_pid_int>`.

| Field | Meaning |
|-------|--------|
| **timer** | Cumulative ms from run start (first sample can be 0 or small). |
| **counter** | Time-weighted counter: sum of (CPM × interval in minutes) per sample. |
| **timestamp** | `YYYY-MM-DD HH:MM:SS` (local time; see docs/original-program-behaviour.md). |
| **sensor_revision_int** | Fixed `0`. |
| **sample_type** | `"f"` (first run), `"r"` (resume or long gap), or `"n"` (normal). |
| **vid_pid_int** | Unsigned int (e.g. 0 or device id). |

Template:  
`<sample><timer>%u</timer>\n<counter>%u</counter>\n<timestamp>%u-%u-%u %u:%u:%u</timestamp>\n<sensor_revision_int>%i</sensor_revision_int>\n<sample_type>%s</sample_type>\n<vid_pid_int>%u</vid_pid_int>\n</sample>\n`

### 3.3 When the original program sends a trickle

- Only when **not** in standalone mode.
- After each sample the payload is queued; a send is attempted when **both**: pending samples ≥ 3 (or 2 in debug), and at least 20 minutes (or 10 in debug) since the last successful send.
- On exit and on sensor loss (after repeated read errors), any pending buffer is sent once.

### 3.4 How the BOINC API writes the trickle

- **boinc_send_trickle_up(char* variety, char* text)** is implemented in the BOINC library.
- It writes to a BOINC-managed trickle-up file (e.g. the file named by `TRICKLE_UP_FILENAME`): first a line with `<variety>...</variety>`, then the raw payload text.
- The BOINC client later reads this file and sends the message to the project scheduler; the app does not handle the actual network send.

---

## 4. data.bin

| Aspect | Finding |
|--------|--------|
| **Logical name** | **data.bin**. The app uses this name; the physical path is typically resolved via **boinc_resolve_filename("data.bin", buf, len)** when using the BOINC API for file resolution. |
| **When written** | In the same high-level flow as the trickle: after reading the detector and building the payload. The exact order relative to **boinc_send_trickle_up** (before or after) is not confirmed. |
| **Format / layout** | Text, one line per sample: timer (ms), counter (time-weighted), timestamp Y-M-D H:M:S, 0, sample_type "f"/"r"/"n", 0. See docs/original-program-behaviour.md and docs/boinc-xml-reference.md; implemented in main_app.cpp. |
| **Relationship to trickle** | Same detector/sample pipeline; one data.bin line and one trickle per sample (same timer/counter/timestamp). |

**Summary:** Path, format, and write order are implemented and documented in recovered code and docs.

---

## 5. URLs and Authentication

### 5.1 No URLs or direct HTTP in the app

- GMC300.exe does **not** open URLs, call WinHTTP/WinInet, or reference project endpoints (e.g. `.php`).
- The **project server URL** (master URL, scheduler, etc.) is part of **BOINC client and project configuration**, not of the app binary.

### 5.2 Authentication

- **Identity** (authenticator, hostid, user_name, team_name, etc.) comes from **init_data** (e.g. init_data.xml), parsed by **parse_init_data_file** (see Phase 2).
- The **BOINC client** uses this identity when sending trickles to the project server, so the server can associate each message with the host and user.
- The app **does not** put authenticator, tokens, or auth headers inside the **rad_report_xml** XML payload; such inclusion would need to be confirmed by inspecting the XML-building code in the binary.

---

## 6. Functions Reference

| Function | Role |
|----------|------|
| **main_app** | Top-level flow: config (project_preferences, gmc.xml), COM open, detector loop, trickle send, and data.bin write. |
| **boinc_send_trickle_up** | BOINC API: called with variety **"rad_report_xml"** and the XML text; writes to the trickle-up file so the client can send it to the project. |
| **read_detector_sample** | Sends GETCPM, reads 2-byte big-endian CPM; result is input to payload building. |
| **Build trickle XML** | **Done.** Inline in main_app.cpp; variety **rad_report_xml**; payload includes &lt;sample&gt;, &lt;timer&gt;, &lt;counter&gt;, &lt;timestamp&gt;, etc. (see docs/boinc-xml-reference.md). |
| **Write data.bin** | **Done.** Inline in main_app.cpp; app_io.h open_data_bin_append(); line format: time_diff_ms, counter, timestamp Y-M-D H:M:S, 0, sample_type, 0. |

Other protocol and config functions (e.g. open_com_port, init_com_after_open, read_project_preferences, parse_init_data_file) are described in Phase 2 and Phase 3.

---

## 7. Known Gaps

- **Trickle XML and timing** — See §3.2–3.3 and docs/original-program-behaviour.md.
- **data.bin format** — See docs/original-program-behaviour.md and main_app.cpp (timer, counter, timestamp, 0, sample_type, 0).

---

*Phase 4 deliverable: server communication for GMC300.exe (trickle-up, data.bin, URLs and auth, function roles, and gaps).*
