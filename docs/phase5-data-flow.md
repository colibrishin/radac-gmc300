# Phase 5: End-to-End Data Flow (GMC300.exe)

This document describes the **end-to-end data flow** of GMC300.exe from startup to finish: how configuration, the GMC-300 device, and server reporting fit together. It synthesizes Phase 2 (configuration), Phase 3 (device protocol), and Phase 4 (server communication).

---

## 1. Scope

- **Purpose:** Single reference for the full application flow—startup, config, device I/O, sampling loop, trickle-up, checkpoint, and finish.
- **Audience:** Reimplementation, porting, debugging, or recovery of the original C++.
- **Related docs:** Phase 2 (config), Phase 3 (protocol), Phase 4 (server); BOINC wrapper reference.

---

## 2. High-Level Flow

```
BOINC init → Parse config (init_data + gmc.xml) → Open COM → Init device (GETVER only; validate "GMC")
    → Main loop: GETCPM → build payload → trickle-up (rad_report_xml) / data.bin → fraction_done / checkpoint
    → boinc_finish
```

**In words:** After BOINC provides init data, the app reads project preferences and the COM port from config, opens and initializes the GMC-300, then runs a loop that reads CPM, builds an XML payload, sends it via trickle-up (and optionally writes data.bin), reports progress and checkpoint, and finally calls `boinc_finish`.

---

## 3. Flow Diagram (Mermaid)

```mermaid
flowchart TD
    subgraph startup [Startup]
        A["Entry / CRT"] --> B[boinc_init]
        B --> C["Parse init_data.xml"]
        C --> D[main_app]
    end

    subgraph config [Configuration]
        D --> E["read_project_preferences: project_preferences"]
        E --> F["Read radacdebug, runtime"]
        F --> G[get_gmc_com_settings]
        G --> H["Load gmc.xml: comsettings port, baud, bits, parity, stopbits"]
        H --> I["com_settings struct"]
    end

    subgraph device [Device]
        I --> J["open_com_port with com_settings: COM port, baud 8N1 from config"]
        J --> K["init_com_after_open: GETVER, read 14 bytes, validate GMC, no HEARTBEAT0"]
        K --> L[Device ready]
    end

    subgraph loop [Main loop]
        L --> M["read_detector_sample: GETCPM, 2-byte big-endian CPM"]
        M --> N[Build XML payload]
        N --> O["boinc_send_trickle_up: rad_report_xml, xml"]
        N --> P["Write data.bin?"]
        O --> Q["boinc_fraction_done / boinc_time_to_checkpoint"]
        P --> Q
        Q --> R{"More samples?"}
        R -->|Yes| M
        R -->|No| S[boinc_finish]
    end
```

---

## 4. Stage-by-Stage Summary

| Stage | What happens | Phase reference |
|-------|----------------|------------------|
| **1. BOINC init** | Entry → CRT → **boinc_init**; BOINC provides init data (e.g. init_data.xml). | Phase 2 |
| **2. Parse init_data** | **parse_init_data_file** parses init_data; **project_preferences** block is stored for later use. | Phase 2 §2, §5 |
| **3. main_app – config** | **read_project_preferences**(init_data, `"project_preferences"`) → read **radacdebug**, **runtime**. num_samples = (runtime_minutes×60 − RUNTIME_BUFFER_SEC) / EFFECTIVE_SEC_PER_SAMPLE so wall time stays within given runtime (EFFECTIVE_SEC_PER_SAMPLE = SAMPLE_INTERVAL_SEC + GETCPM delays ≈ 41 s; RUNTIME_BUFFER_SEC = 120). Then **get_com_port_number** → load **gmc.xml** (section **gmc**), read **comsettings** → **portnumber** (1–99). | Phase 2 §4, §5, §6 |
| **4. Open COM** | **open_com_port**(port) → `COM%d`, 57600 8N1, timeouts 1 s + 100 ms/byte. | Phase 3 §2, §6 |
| **5. Init device** | **init_com_after_open**: send **GETVER** → read 14 bytes → validate **"GMC"**; HEARTBEAT0 is not sent. | Phase 3 §3, §5, §6 |
| **6. Main loop** | For each sample (up to num_samples): **read_detector_sample** → GETCPM → 2 bytes → CPM = byte0×256 + byte1 (big-endian). Wait **SAMPLE_INTERVAL_SEC** (30 s) between reads. Build XML from CPM (and timing/sample index). **boinc_send_trickle_up**(`"rad_report_xml"`, xml): first sample always sent; then if **TRICKLE_INTERVAL_SEC** is 0 send every sample, else at most every TRICKLE_INTERVAL_SEC seconds. Write **data.bin** (one line per sample); field 5 **sample_type** is `"f"` (first line of fresh run), `"r"` (first line after resume), or `"n"` (normal). **boinc_fraction_done**; if **boinc_time_to_checkpoint**, **boinc_checkpoint_completed**. | Phase 3 §4; Phase 4 §2, §3, §4 |
| **7. Finish** | **boinc_finish**(status). | BOINC API |

---

## 5. Data Flow (Config → Device → Server)

| Data | Source | Consumer | Role |
|------|--------|----------|------|
| **project_preferences** | init_data.xml (BOINC) | main_app | radacdebug, runtime → num_samples |
| **portnumber** | gmc.xml (gmc → comsettings → portnumber) | open_com_port | COM port name (COM%d) |
| **CPM** | GMC-300 (GETCPM, 2 bytes big-endian (MSB first)) | Payload builder | Counts per minute in XML (and possibly data.bin) |
| **rad_report_xml payload** | Built in app (XML string) | boinc_send_trickle_up | BOINC client sends to Radioactive@home |
| **Identity (auth)** | init_data (authenticator, hostid, etc.) | BOINC client | Used when client sends trickles; not in app payload |

---

## 6. Critical-Path Function Order (Execution Summary)

Function order along the main path:

1. **Entry / CRT** → **main_app**
2. **boinc_init** (or boinc_init_options)
3. **parse_init_data_file** — init_data.xml → project_preferences stored
4. **read_project_preferences**(init_data, `"project_preferences"`)
5. **get_config_value** / **get_config_cstr** / **parse_int_cstr** — radacdebug, runtime
6. **get_com_port_number** — load gmc.xml, read comsettings/portnumber
7. **open_com_port**(port)
8. **init_com_after_open** — GETVER, validate "GMC" (no HEARTBEAT0)
9. **Loop:** **read_detector_sample** → build XML → **boinc_send_trickle_up**("rad_report_xml", xml) → (data.bin write) → **boinc_fraction_done** / **boinc_checkpoint_completed**
10. **boinc_finish**

Supporting functions (see Phase 2–4): read_project_preferences, get_config_value, get_config_int, get_config_cstr, config_string_empty, release_config_value, load config from file (gmc.xml), parse_init_data_file.

---

## 7. References

- **Phase docs** are the main reference: Phase 2 (config flow and keys), Phase 3 (serial commands and parsing), Phase 4 (trickle-up and data.bin).
- **This document** is the end-to-end summary; §6 gives the function order. Reference behaviour of the original program (trickle, data.bin, sample_type, resume): docs/original-program-behaviour.md.

---

## 8. Timing and trickle (recovered constants)

| Constant | Role |
|----------|------|
| **SAMPLE_INTERVAL_SEC** (30) | Seconds between CPM reads (GETCPM); drives data.bin line rate. |
| **EFFECTIVE_SEC_PER_SAMPLE** (~41) | SAMPLE_INTERVAL_SEC + GETCPM_DELAY_BEFORE_SEND_SEC + GETCPM_WAIT_AFTER_SEND_SEC; used to compute num_samples so wall time fits runtime. |
| **RUNTIME_BUFFER_SEC** (120) | Seconds reserved for startup (COM open, GETVER); subtracted from runtime before dividing by EFFECTIVE_SEC_PER_SAMPLE. |
| **TRICKLE_INTERVAL_SEC** (0 or e.g. 120/240) | 0 = send one trickle per sample; >0 = send at most every N seconds. First sample trickle is always sent. |

---

*Phase 5 deliverable: end-to-end data flow for GMC300.exe (config → device → server), critical-path summary, timing/trickle constants, and reference to Phase 2–4.*
