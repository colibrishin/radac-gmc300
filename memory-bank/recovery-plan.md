# Recovery Plan: Original C++ Code

**Goal:** Recover or reimplement the original C++code of GMC300.exe. Target: **C++03** (ISO/IEC 14882:2003). Compiler assumption: **Microsoft Visual C++ 9.0 (Visual Studio 2008)**. See projectbrief.md and techContext.md for scope and toolchain.

---

## Recovery Steps (summary)


| #   | Step                                    | Details                                                                                                                                       |
| --- | --------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | **Complete function coverage**          | **DONE.** All FUN_* renamed (subagent: 296 at offsets 0–295; total ~436+). No FUN_* remain.                                                   |
| 2   | **Struct/class layouts**                | Define APP_INIT_DATA-like struct, BOINC types, STL/ATL layouts, vtables.                                                                       |
| 3   | **Globals named and typed**             | Map DAT_* used in main path to names and types; optionally export a globals header.                                                           |
| 4   | **Config and protocol enumerated**      | Phase 2 (config) and Phase 3 (GMC-300 protocol) docs; list every key and command; string search in binary.                                    |
| 5   | **Critical-path decompilation cleaned** | Manually review and fix types/control flow for main_app, open_com_port, read_detector_sample, init_com_after_open, BOINC init/report/trickle. |
| 6   | **Build environment**                   | Recreate project (no original .vcxproj); link BOINC lib from repo, CRT/ATL; document in docs/build.md.                                        |
| 7   | **Naming and comments**                 | Apply consistent naming and recovery notes in recovered source.                                                                               |


---

## Suggested order of work

1. ~~Complete FUN_* coverage~~ **Done.**
2. Recover structs and globals (so types exist for clean C++).
3. Finish Phase 2 and Phase 3 docs (config and protocol).
4. Clean critical-path decompilation and optionally export clean C++.
5. Set up build (project, BOINC, CRT) and fix compile/link errors.

---

## References

- **BOINC API:** boinc/api/boinc_api.h; wrapper: boinc/samples/wrapper/wrapper.cpp
- **Serial protocol:** open_com_port, read_detector_sample, init_com_after_open (57600 8N1, >, >, >)

