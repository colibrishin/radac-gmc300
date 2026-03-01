# Phase 4: Server Communication (GMC300.exe)

This document describes how GMC300.exe sends detector data to the Radioactive@home project: trickle-up only (no direct HTTP), the XML payload, data.bin, and how URLs and authentication are handled by the BOINC client.

---

## 1. Scope

- **Mechanism:** The app does **not** perform direct HTTP or HTTPS requests. It builds a payload and uses the BOINC API **trickle-up** to send data; the BOINC client forwards trickles to the project server.
- **Contents:** Trickle variety and payload format, data.bin (path, when written, format), absence of URLs and in-payload auth in the app, function roles, and known gaps to confirm in the binary.

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
| **Payload** | XML string. Exact tag names and structure are to be confirmed in the binary (see Known gaps). |

### 3.2 Inferred payload fields

From the data flow (detector protocol and config), the XML payload is expected to include:

| Field (inferred) | Likely role |
|------------------|-------------|
| **CPM** | Counts per minute from GETCPM (Phase 3). |
| **Timestamp / time** | When the sample was taken (`<timestamp>` in trickle; format in recovered main_app and docs/boinc-xml-reference.md). |
| **Sample index or count** | Which sample in the run (from runtime and 240‑second intervals; see Phase 2). |

Other elements (e.g. version, device id) may exist; **exact XML tag names and attribute order are to be confirmed in the binary** (discovery had no Ghidra MCP; manual or Ghidra analysis is needed).

### 3.3 How the BOINC API writes the trickle

- **boinc_send_trickle_up(char* variety, char* text)** is implemented in the BOINC library.
- It writes to a BOINC-managed trickle-up file (e.g. the file named by `TRICKLE_UP_FILENAME`): first a line with `<variety>...</variety>`, then the raw payload text.
- The BOINC client later reads this file and sends the message to the project scheduler; the app does not handle the actual network send.

---

## 4. data.bin

| Aspect | Finding |
|--------|--------|
| **Logical name** | **data.bin**. The app uses this name; the physical path is typically resolved via **boinc_resolve_filename("data.bin", buf, len)** when using the BOINC API for file resolution. |
| **When written** | In the same high-level flow as the trickle: after reading the detector and building the payload. The exact order relative to **boinc_send_trickle_up** (before or after) is not confirmed. |
| **Format / layout** | **Resolved.** Text, one line per sample: time_diff_ms (1000 first line), counter (accumulated CPM), timestamp Y-M-D H:M:S, 0, sample_type "r"/"n", 0. Implemented in main_app.cpp; see docs/boinc-xml-reference.md. |
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

The following were inferred from codebase and BOINC reference; **exact details are to be filled when Ghidra (or manual analysis) is used:**

- **Exact trickle XML tags** — Tag names, attribute order, and full set of elements (e.g. `<report>`, `<cpm>`, `<timestamp>`, `<sample>`) for **rad_report_xml**.
- **data.bin format** — Resolved in recovered code; see docs/boinc-xml-reference.md and main_app.cpp (line format: time_diff_ms, counter, timestamp, 0, sample_type, 0).
- **Builder/writer function names** — Resolved: build and write are inline in main_app.cpp.

---

*Phase 4 deliverable: server communication for GMC300.exe (trickle-up, data.bin, URLs and auth, function roles, and gaps).*
