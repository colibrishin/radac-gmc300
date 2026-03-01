# Phase 2: Configuration Protocol (GMC300.exe)

This document describes how GMC300.exe loads and uses configuration: **project_preferences** (from BOINC init data), **gmc.xml** (standalone), and the keys and parsing flow involved.

---

## 1. Scope

- **Config sources:** BOINC `init_data.xml` (project_preferences block) and standalone `gmc.xml`.
- **Keys and types:** radacdebug, runtime (from project_preferences); portnumber (from gmc.xml).
- **Parsing flow:** Which code opens/reads each source and in what order; how the COM port is resolved from gmc.xml.

---

## 2. Config Sources

| Source | When used | Role |
|--------|-----------|------|
| **BOINC init_data.xml** | At application startup (BOINC provides init data). | Contains an `<app_init_data>` block; inside it, a **`<project_preferences>`** XML fragment holds app-specific preferences (e.g. radacdebug, runtime). Parsed by `parse_init_data_file`; the project_preferences block is stored and later read in `main_app` via `read_project_preferences(..., "project_preferences", ...)`. |
| **gmc.xml** | When the app needs the COM port (during main flow). | Standalone file in the working directory. Opened with `fopen("gmc.xml", "rb")`, then parsed with the same config machinery as project_preferences, using root section name **`"gmc"`**. Used for COM port and related settings (e.g. **comsettings** → **portnumber**). |

**Summary:**

- **Init vs COM:** Project preferences (radacdebug, runtime) come from **BOINC init_data** → **project_preferences**. The COM port comes from **gmc.xml** → section **gmc** → **comsettings** → **portnumber**.

---

## 3. gmc.xml Format

- **Root element (section):** **`gmc`**. The loader reads the file and parses it with root section `"gmc"`.
- **Real examples:** See **docs/boinc-xml-reference.md** (e.g. `F:\BOINCdata\slots\1\gmc.xml`, `init_data.xml`).
- **Reading:** **get_gmc_com_settings** (in **recovered/config.cpp**) reads the full **comsettings** block from gmc.xml and fills a `gmc_com_settings` struct: **portnumber** (required, 1–99), **baud** (default 57600), **bits** (5–8, default 8), **parity** (n/e/o → none/even/odd), **stopbits** (1 or 2). See **docs/boinc-xml-reference.md** for the full comsettings layout.
- **Sections and keys:**

| Section / key | Path | Type | Meaning |
|---------------|------|------|---------|
| **gmc** | (root) | section | Root of gmc.xml; contains **comsettings**. |
| **comsettings** | gmc → comsettings | section | Container for COM-related settings. |
| **portnumber** | gmc → comsettings → portnumber | string → int | COM port number. Parsed as integer; valid range **1–99**. Required; missing or invalid value causes the app to exit with an error. |
| **baud** | gmc → comsettings → baud | string → int | Baud rate; default 57600. |
| **bits** | gmc → comsettings → bits | string → int | Data bits 5–8; default 8. |
| **parity** | gmc → comsettings → parity | string | n/e/o (none/even/odd). |
| **stopbits** | gmc → comsettings → stopbits | string → int | 1 or 2. |

**Example (inferred):**

```xml
<gmc>
  <comsettings>
    <portnumber>3</portnumber>
    <baud>57600</baud>
    <bits>8</bits>
    <parity>n</parity>
    <stopbits>1</stopbits>
  </comsettings>
</gmc>
```

**Errors (from `get_gmc_com_settings`):**

- File missing or unreadable: *"gmc_ReadPortNumber(): could not find gmc.xml or the file is corrupted"*
- `<comsettings>` missing or empty: *"gmc_ReadPortNumber(): <comsettings> node empty, file corrupted ?"*
- `<portnumber>` missing or empty: *"gmc_ReadPortNumber(): <portnumber> node empty, file corrupted ?"*
- Port not in 1–99: *"gmc_ReadPortNumber(): Wrong port number"*

---

## 4. project_preferences (init_data) Keys

These keys are read from the **project_preferences** block of BOINC init_data (e.g. from `init_data.xml`) in **main_app** after `read_project_preferences(..., "project_preferences", ...)`.

| Key | Type | Default | How the app uses it |
|-----|------|---------|----------------------|
| **radacdebug** | string → int (0/1) | treated as 0 if missing/empty | Non-zero enables debug: sets an internal debug flag, enables extra `fprintf` to the debug file (e.g. "Debug: num_samples: %d", "Debug: Using port number: %d", "CPM value: %d", "Debug: trickle sent"). |
| **runtime** | string → int (minutes) | effective default 300 sample intervals if not set | Converted to sample count: `num_samples = (runtime_minutes * 60) / SAMPLE_INTERVAL_SEC` (SAMPLE_INTERVAL_SEC is 30 in **constants.h**). When radacdebug is set, the app prints "Debug: num_samples: %d" with this value. |

**Reading:** For each key, the app uses `get_config_value` → check `config_string_empty`; if not empty, `get_config_cstr` and (for numeric use) `parse_int_cstr`. Handles are released with `release_config_value`.

---

## 5. Parsing Flow

1. **BOINC startup**  
   Init data (e.g. init_data.xml) is parsed by **parse_init_data_file**. When the **project_preferences** tag is seen, that XML block is read and stored for later use.

2. **main_app**  
   - Calls **read_project_preferences** with the init_data and section name **"project_preferences"** to obtain the project-preferences config object.  
   - Reads keys in this order:
     - **radacdebug**: `get_config_value` → if not empty, `get_config_cstr` + `parse_int_cstr` → debug flag.  
     - **runtime**: `get_config_value("runtime", ...)` → if not empty, `get_config_cstr` + `parse_int_cstr` → minutes, then `num_samples = (minutes * 60) / SAMPLE_INTERVAL_SEC`.  
   - Releases the config with **release_config_value**.

3. **COM port (gmc.xml)**  
   - **main_app** calls **get_gmc_com_settings(&com_settings)**, which loads **gmc.xml** and fills the struct with **port_number**, **baud**, **bits**, **parity**, **stopbits**.  
   - **open_com_port** is then called with that struct to open the serial port.  
   - **get_com_port_number** is a helper that calls **get_gmc_com_settings** and returns just the port number (used when only the port is needed).

**Summary:** Project preferences (radacdebug, runtime) are read from the BOINC init_data **project_preferences** block in main_app. The COM path uses **get_gmc_com_settings** to load gmc.xml and fill the full comsettings struct; **open_com_port** uses that struct. **get_com_port_number** is a convenience that returns only the port.

---

## 6. Functions Reference

| Function | Role |
|----------|------|
| **read_project_preferences** | Reads a named section from an XML buffer (init_data or file content). Parameters: buffer, section name (e.g. `"project_preferences"` or `"gmc"`), and options. Returns a config object used with get_config_value / get_config_int / get_config_cstr / release_config_value. |
| **get_config_value** | Looks up a key (e.g. `"radacdebug"`, `"runtime"`, `"comsettings"`, `"portnumber"`) in the current config; returns a handle to the node (or empty). Key comparison is case-insensitive. |
| **get_config_int** | Copies the config value handle (reference-counted); used to hold a node before reading its string value. |
| **get_config_cstr** | Returns the string value of a config node (e.g. by index 0 for a single value). |
| **config_string_empty** | Returns true if the config handle has no value (key missing or empty). |
| **release_config_value** | Releases a config handle. |
| **load config from file** (FUN_00402cb0) | Opens a file path (e.g. `"gmc.xml"`) with `fopen(..., "rb")`, reads the full file, then calls **read_project_preferences**(buffer, root_section, ...). Used for gmc.xml with root section **"gmc"**. |
| **parse_init_data_file** | Parses BOINC init_data XML; finds `<app_init_data>` and tags such as **project_preferences**, **global_preferences**, etc.; stores the project_preferences block for later use. |
| **get_gmc_com_settings** | Loads **gmc.xml**, fills struct with **port_number**, **baud**, **bits**, **parity**, **stopbits**; exits on missing/invalid port. |
| **get_com_port_number** | Calls **get_gmc_com_settings**, returns the port number. |
| **main_app** | Calls read_project_preferences for `"project_preferences"`; reads **radacdebug** and **runtime**; uses **get_gmc_com_settings** for the COM open path (then **open_com_port** with the filled struct). |

---

*Phase 2 deliverable: configuration protocol for GMC300.exe (project_preferences, gmc.xml, keys and parsing).*
