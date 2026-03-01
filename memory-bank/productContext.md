# Product Context

## Why This Project Exists

- The GMC-300 is a USB Geiger counter from GQ Electronics used with the Radioactive@home BOINC project.
- GMC300.exe is the Windows “profile” wrapper that reads the detector and reports data to the server; a Linux version is reportedly unavailable or broken.
- Reverse engineering enables: reimplementation (e.g. Linux/native), protocol documentation, debugging (e.g. firmware odd-character issues), and interoperability. A further goal is to recover or reimplement the original C++ for porting or maintenance.

## How It Should Work (From Public Knowledge)

- Application reads configuration (e.g. gmc.xml) for COM port and possibly other settings.
- Communicates with GMC-300 via USB/serial (virtual COM).
- Parses detector responses (CPM, counts, etc.) and sends them to the Radioactive@home server.
- May act as a BOINC wrapper (stdin/stdout or file-based work unit and result handling).

## User Experience Goals (For This Repo)

- Clear, actionable documentation so a developer can implement a compatible client or fix issues without guessing.
- Ghidra project and notes that persist across sessions and can be resumed quickly.
- Recovered C++ in **recovered/** is human-readable, uses named constants, and does not depend on the original XML library (replaced with TinyXML2) or Windows-specific serial (abstracted behind **serial_port.h** with Win/POSIX implementations).
