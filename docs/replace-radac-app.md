# Replacing the default Radioactive@home application

This guide describes how to replace the default **radac** (Radioactive@home) application with the custom BOINC-linked binary and configuration from this repository. Use this to run the recovered/custom version of the GMC application with your BOINC client.

**Prerequisites:** BOINC client installed and attached to the [Radioactive@home](https://radioactiveathome.org/) project. You need a BOINC-linked binary (e.g. from this repository’s [Releases](../releases) or built with `-DGMC_USE_BOINC=ON`).

---

## 1. Locate the project directory

The BOINC client stores project data under a **data directory**. The path depends on your OS and installation:

| OS        | Typical data directory |
|-----------|-------------------------|
| **Windows** | `C:\ProgramData\BOINC` or `%ALLUSERSPROFILE%\BOINC` or the path set in the BOINC installer |
| **Linux**   | `~/boinc` or `/var/lib/boinc-client` |
| **macOS**   | `~/Library/Application Support/BOINC Data` |

Inside the data directory, each attached project has a folder named by the project URL. For Radioactive@home the folder is:

```text
{BOINC_DATA_DIR}/projects/radioactiveathome.org_boinc
```

Example (Windows): `C:\ProgramData\BOINC\projects\radioactiveathome.org_boinc`

---

## 2. Stop BOINC or suspend the project (recommended)

To avoid the client overwriting or locking files:

- **Option A:** Exit the BOINC client completely.
- **Option B:** In the BOINC Manager, suspend the Radioactive@home project (or detach and reattach after replacing files).

---

## 3. Clear the project directory

**Remove all existing files** inside `radioactiveathome.org_boinc`. This ensures the client will use your custom binary and XMLs instead of the default application.

- Delete or move aside everything inside that folder (subfolders like `slot_0`, `slot_1`, … and any files at the project level).
- Do **not** delete the folder `radioactiveathome.org_boinc` itself.

---

## 4. Place the binary and XML files

Copy these three items into **`radioactiveathome.org_boinc`**:

| File            | Source / role |
|-----------------|----------------|
| **Executable**  | Your BOINC-linked binary (e.g. `gmc_recovered-windows-x64.exe` from a [release](../../releases), or one you built). |
| **app_info.xml**| From this repo ([`app_info.xml`](../app_info.xml)). Tells BOINC which app and files to run. |
| **gmc.xml**     | From this repo ([`gmc.xml`](../gmc.xml)). **You must edit this** to match your device (see §6). |

**Executable name:** The included `app_info.xml` expects the main program to be named **`gmc300.exe`**. Either:

- Rename your binary to **`gmc300.exe`** when you copy it into the project folder, or  
- Edit `app_info.xml` and change the `<name>` under `<file_info>` and the `<file_name>` in the `<file_ref>` that has `<main_program/>` to your actual executable name (e.g. `gmc_recovered-windows-x64.exe`). Keep the rest of the structure the same.

After copying, the project directory should contain at least:

- Your executable (as `gmc300.exe` or the name you use in `app_info.xml`)
- `app_info.xml`
- `gmc.xml`

---

## 5. Edit gmc.xml for your device

**gmc.xml** is the configuration file for the application. It must match the **communication spec of your Geiger counter** (port, baud rate, data bits, parity, stop bits).

Open **gmc.xml** in a text editor. The relevant section is under `<comsettings>`:

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

| Setting      | Meaning | What to set |
|-------------|---------|-------------|
| **portnumber** | COM port number (1–99). On Windows this is the number in `COM1`, `COM2`, … | The port where your GMC device is connected (e.g. `3` for COM3, `4` for COM4). |
| **baud**       | Baud rate. | **57600** for GMC-300 V3.xx and earlier; **115200** for GMC-300 Plus V4.xx+ and many GMC-320. See [GQ-RFC1201](https://www.gqelectronicsllc.com/download/GQ-RFC1201.txt) and [Phase 3 protocol](phase3-protocol.md). |
| **bits**       | Data bits (5–8). | Usually **8**. |
| **parity**     | Parity: `n` = none, `e` = even, `o` = odd. | Usually **n**. |
| **stopbits**   | Stop bits (1 or 2). | Usually **1**. |

Save the file after editing. The application reads **gmc.xml** from the slot directory when it runs; BOINC copies the file you place here into each task slot.

---

## 6. Restart BOINC and run tasks

- Start the BOINC client again (or resume the Radioactive@home project).
- Let the client fetch work or use existing work; it will use the **app_info.xml** and the executable you placed.
- Tasks will run your custom binary with the **gmc.xml** you configured.

If the app fails to find the device, check:

- **portnumber** matches the actual COM port (Device Manager on Windows; `ls /dev/tty*` or similar on Linux/macOS).
- **baud** (and other settings) match your device’s spec (see [docs/phase3-protocol.md](phase3-protocol.md) and [docs/boinc-xml-reference.md](boinc-xml-reference.md)).

---

## 7. References

| Topic | Document |
|-------|----------|
| Serial protocol (GETVER, GETCPM, baud, format) | [Phase 3: GMC-300 Device Protocol](phase3-protocol.md) |
| gmc.xml and init_data layout | [BOINC XML structure reference](boinc-xml-reference.md) |
| Configuration (comsettings, port, baud) | [Phase 2: Configuration](phase2-config.md) |
| Official GQ protocol | [GQ-RFC1201](https://www.gqelectronicsllc.com/download/GQ-RFC1201.txt) |

---

*This procedure replaces the default radac application with the custom/recovered version. Use it only in line with the project [disclaimer](../README.md#disclaimer) and the Radioactive@home project’s terms.*
