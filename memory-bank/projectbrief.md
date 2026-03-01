# Project Brief: GMC300 Reverse Engineering

## Core Requirements and Goals

- **Target:** Reverse engineer **GMC300.exe**, the Windows BOINC profile executable that sends GQ GMC-300 Geiger counter (radioactive detector) data to the Radioactive@home server.
- **Scope:** Understand and document protocol, configuration, server communication, and BOINC wrapper behavior sufficient for reimplementation, porting, or debugging.
- **Out of scope:** Modifying or distributing the original binary; only analysis and documentation.

## Success Criteria

- Ghidra project contains imported and analyzed GMC300.exe with key functions/labels documented.
- Written documentation of: GMC-300 device protocol, config file (e.g. gmc.xml) format, server API/payload, and BOINC wrapper I/O.
- Memory Bank and progress tracking updated as analysis proceeds.
- Optionally: recover or reimplement original C++ using the recovery checklist. Recovered code in **recovered/** has been refactored to **C++20**, **TinyXML2** for config, and **platform-agnostic serial** (no Windows types in app code).

## Source of Truth

This document defines project scope. All analysis deliverables (protocol docs, config format, server behavior) are derived from the single binary GMC300.exe and any available config samples (e.g. gmc.xml, F:\BOINCdata\slots\N\). Original binary: C++03, MSVC 9.0. Recovered codebase: C++20, TinyXML2, platform-agnostic serial API.
