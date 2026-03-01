// Standalone (no BOINC) stubs for recovered app. Defines all extern symbols
// required by main_app.cpp and config.cpp. See docs/extern-symbols.md.

#include "app_io.h"
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

// -----------------------------------------------------------------------------
// Globals (sensible defaults for standalone)
// -----------------------------------------------------------------------------
extern "C" {
std::uint32_t init_time_dwords[3] = {0, 0, 0};
std::uint32_t data_bin_open_arg1 = 0;
std::uint32_t data_bin_open_arg2 = 0;
std::uint32_t data_bin_open_arg3 = 0;
double boinc_fraction_done_val = 0.0;
double ms_to_sec_divisor = 1000.0;
double uSv_h_divisor = 151.0;
std::uint16_t trickle_buf_pad = 0;
}

// -----------------------------------------------------------------------------
// BOINC API stubs
// -----------------------------------------------------------------------------
extern "C" void boinc_finish_and_exit(int status) {
  std::exit(status);
}

extern "C" void boinc_checkpoint(void) {
  /* no-op */
}

extern "C" void boinc_begin_critical_section(void) {
  /* no-op */
}

extern "C" void boinc_end_critical_section(void) {
  /* no-op */
}

extern "C" void boinc_fraction_done(double) {
  /* no-op */
}

extern "C" int boinc_send_trickle_up(const char*, const char*) {
  (void)0;
  return 0;
}

extern "C" int boinc_time_to_checkpoint(void) {
  return 0;
}

extern "C" int gmc_boinc_should_exit(void) {
  return 0;  /* standalone: never exit due to BOINC */
}

extern "C" void gmc_boinc_wait_if_suspended(void) {
  /* no-op: standalone has no pause */
}

// -----------------------------------------------------------------------------
// C++ I/O (debug stream + data.bin). Standalone: stderr; data.bin in cwd.
// -----------------------------------------------------------------------------
std::ostream* get_debug_stream() {
  return &std::cerr;
}

std::ifstream open_data_bin_for_resume() {
  return std::ifstream("data.bin");
}

std::ofstream open_data_bin_append() {
  return std::ofstream("data.bin", std::ios::app);
}

// -----------------------------------------------------------------------------
// parse_int_cstr (still used by config)
// -----------------------------------------------------------------------------
extern "C" int parse_int_cstr(char* str) {
  return static_cast<int>(std::strtol(str, nullptr, 10));
}

#if defined(_WIN32)
extern "C" __time64_t get_time64(__time64_t* out) {
  return _time64(out);
}
#else
typedef long long __time64_t;
extern "C" __time64_t get_time64(__time64_t* out) {
  time_t t = time(nullptr);
  __time64_t t64 = static_cast<__time64_t>(t);
  if (out)
    *out = t64;
  return t64;
}
#endif

extern "C" void sleep_one_second(unsigned int sec, int) {
#if defined(_WIN32)
  Sleep(sec * 1000);
#else
  sleep(sec);
#endif
}
