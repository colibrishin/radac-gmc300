# BOINC XML structure reference

Reference layouts for XML files used by the recovered app, based on real BOINC slot data (e.g. `F:\BOINCdata\slots\1\` or equivalent).

---

## gmc.xml (slot directory)

Path: `BOINCdata/slots/<N>/gmc.xml` (or project copy). Root element **`gmc`**; COM port is under **comsettings** → **portnumber**.

**Example (from F:\\BOINCdata\\slots\\1\\gmc.xml):**

```xml
<gmc>
  <comsettings>
    <comment>Edit port number below</comment>
    <portnumber>4</portnumber>
    <baud>115200</baud>
    <bits>8</bits>
    <parity>n</parity>
    <stopbits>1</stopbits>
  </comsettings>
  <options>
  </options>
</gmc>
```

**Used by:** `get_gmc_com_settings()` (or `get_com_port_number()` / `get_com_baud_rate()`) — loads `gmc.xml` once, reads all of **comsettings**: **portnumber** (required, 1–99), **baud** (default 57600), **bits** (5–8, default 8), **parity** (n/e/o → none/even/odd, default n), **stopbits** (1 or 2, default 1). All are applied when opening the serial port.

---

## init_data.xml (slot directory)

Path: `BOINCdata/slots/<N>/init_data.xml`. Root element **`app_init_data`**. App preferences are under **`project_preferences`** (child elements: tag name = key, text = value).

**Relevant fragment (from F:\\BOINCdata\\slots\\1\\init_data.xml):**

```xml
<app_init_data>
  <major_version>8</major_version>
  ...
  <project_preferences>
    <buzzer>0</buzzer>
    <backlight>0</backlight>
    <radacdebug>0</radacdebug>
    <experiments>0</experiments>
    <runtime>2880</runtime>
    <lcd_avg_opts>1</lcd_avg_opts>
  </project_preferences>
  ...
</app_init_data>
```

**Used by:** When built with BOINC, the entry point (main.cpp) copies `aid.project_preferences` from the BOINC API **as-is** into the app buffer (BOINC stores the full `<project_preferences>...</project_preferences>` element; do not wrap again). `read_project_preferences(..., "project_preferences", ...)` then parses that buffer; the app looks up **radacdebug** and **runtime** under the root (case-insensitive). Values are text (e.g. `"0"`, `"2880"`); runtime is in minutes.

**Mapping from init_data.xml (e.g. `F:\BOINCdata\slots\0\init_data.xml`) to code:**

| XML path | Your file value | Where used | Effect |
|----------|-----------------|------------|--------|
| `<app_init_data>` | (root) | BOINC passes full file content as init_data to the app. |
| `<project_preferences>` | section | `main_app.cpp`: `read_project_preferences(..., "project_preferences", ...)` | Parsed as config; children read by key. |
| `<radacdebug>` | `0` | `main_app.cpp` lines 130–137: `get_config_value(..., "radacdebug", ...)`, `parse_int_cstr` | `L.debug_enabled = 0` → debug off. Non-zero → debug on, longer retries, "Debug: ..." messages. |
| `<runtime>` | `2880` | `main_app.cpp` lines 139–147: `get_config_value(..., "runtime", ...)`, `parse_int_cstr` | Runtime in **minutes**. **SAMPLE_INTERVAL_SEC = 30.** `num_samples = (2880 * 60) / SAMPLE_INTERVAL_SEC` → 5760 if interval=30, 720 if interval=240. Run ends after that many samples. |
| `<buzzer>`, `<backlight>`, `<experiments>`, `<lcd_avg_opts>` | (present in file) | **Not read** by recovered app; only **radacdebug** and **runtime** are used from `project_preferences`. |

So for your task: **runtime=2880** (48 h) and **radacdebug=0**. With `SAMPLE_INTERVAL_SEC = 30`, `num_samples = 5760` → progress = total_samples_done / 5760; boinc_fraction_done(1.0) before exit. **Project preferences structure (recovered app):** root = `<project_preferences>`; only **radacdebug** and **runtime** are read (as child elements, text parsed as int); other tags (buzzer, backlight, etc.) are ignored.

---

## boinc_task_state.xml (slot directory)

Path: `BOINCdata/slots/<N>/boinc_task_state.xml`. Task state (fraction_done, checkpoint times, etc.). Not parsed by the recovered config layer; included here for context.

**Example fragment:**

```xml
<active_task>
  <project_master_url>http://radioactiveathome.org/boinc/</project_master_url>
  <result_name>sample_18152418_0</result_name>
  <checkpoint_cpu_time>2.500000</checkpoint_cpu_time>
  <checkpoint_elapsed_time>13263.595274</checkpoint_elapsed_time>
  <fraction_done>0.079167</fraction_done>
  ...
</active_task>
```

---

## rad_report_xml (trickle-up payload)

The original program calls **boinc_send_trickle_up("rad_report_xml", xml)** with an XML string. One payload contains **one** `<sample>...</sample>` per send; samples are queued and sent when both a pending count and a time interval are met (see docs/original-program-behaviour.md).

**Single-sample template:**

```xml
<sample><timer>%u</timer>
<counter>%u</counter>
<timestamp>%u-%u-%u %u:%u:%u</timestamp>
<sensor_revision_int>%i</sensor_revision_int>
<sample_type>%s</sample_type>
<vid_pid_int>%u</vid_pid_int>
</sample>
```

| Element | Value / meaning |
|--------|------------------|
| **timer** | Cumulative ms from run start. |
| **counter** | Time-weighted counter (CPM × interval in minutes). |
| **timestamp** | `YYYY-MM-DD HH:MM:SS` (local time). |
| **sensor_revision_int** | `0`. |
| **sample_type** | `f` (first), `r` (resume or long gap), or `n` (normal). |
| **vid_pid_int** | Unsigned int (e.g. 0 or device id). |

**Send rules:** See docs/original-program-behaviour.md and docs/phase4-server.md §3.3.

---

## Summary

| File / payload   | Root element     | Section / path used by app        | Keys read                    |
|------------------|------------------|-----------------------------------|------------------------------|
| **gmc.xml**      | `<gmc>`          | gmc → comsettings                | portnumber, baud, bits, parity, stopbits |
| **init_data**   | `<app_init_data>`| project_preferences              | radacdebug, runtime (minutes)|
| **rad_report_xml** (trickle) | `<sample>` (one or more) | boinc_send_trickle_up payload | timer, counter, timestamp, sensor_revision_int, sample_type, vid_pid_int |

See **phase2-config.md** for the full config protocol and **recovered/config.cpp** for the TinyXML2 implementation.
