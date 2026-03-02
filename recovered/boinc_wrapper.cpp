// BOINC build: provides app/wrapper symbols not in libboinc (globals, file/time/debug).
// Link this when GMC_USE_BOINC=ON; boinc_* symbols come from the BOINC library.
// See docs/extern-symbols.md, docs/boinc-api-locations.md.

#include "app_io.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "time_compat.h"

#if defined(GMC_USE_BOINC) && GMC_USE_BOINC
#include "boinc_api.h"
#include "util.h"  /* boinc_sleep() */
#endif

extern "C" {

#if defined(GMC_USE_BOINC) && GMC_USE_BOINC
// Recovered app uses these names; BOINC API uses boinc_finish and boinc_checkpoint_completed.
void boinc_finish_and_exit(int status) {
  boinc_finish(status);
}
void boinc_checkpoint(void) {
  (void)boinc_checkpoint_completed();
}
// Return 1 if client requested quit or heartbeat lost (app should exit).
int gmc_boinc_should_exit(void) {
  BOINC_STATUS s;
  boinc_get_status(&s);
  return (s.quit_request || s.no_heartbeat) ? 1 : 0;
}
// If client requested pause (suspend), sleep until resumed. Call periodically so we react to pause.
void gmc_boinc_wait_if_suspended(void) {
  BOINC_STATUS s;
  for (;;) {
    boinc_get_status(&s);
    if (!s.suspended)
      return;
    boinc_sleep(1.0);
  }
}
#endif

// Globals (defaults; a real BOINC wrapper may set these from APP_INIT_DATA).
std::uint32_t init_time_dwords[3] = {0, 0, 0};
std::uint32_t data_bin_open_arg1 = 0;
std::uint32_t data_bin_open_arg2 = 0;
std::uint32_t data_bin_open_arg3 = 0;
double boinc_fraction_done_val = 0.0;
double ms_to_sec_divisor = 1000.0;   /* Time in seconds; counter formula uses interval in minutes (elapsed_sec/60). */
double uSv_h_divisor = 151.0;        /* CPM to µSv/h calibration (GMC-300 / project convention). */
std::uint16_t trickle_buf_pad = 0;

int parse_int_cstr(char* str) {
  return static_cast<int>(std::strtol(str, nullptr, 10));
}

#if defined(_WIN32)
__time64_t get_time64(__time64_t* out) {
  return _time64(out);
}
#else
__time64_t get_time64(__time64_t* out) {
  time_t t = time(nullptr);
  __time64_t t64 = static_cast<__time64_t>(t);
  if (out)
    *out = t64;
  return t64;
}
#endif

void sleep_one_second(unsigned int sec, int) {
#if defined(_WIN32)
  Sleep(sec * 1000);
#else
  sleep(sec);
#endif
}

} // extern "C"

// C++ I/O (app_io.h). BOINC captures stderr as the task log.
std::ostream* get_debug_stream() {
  return &std::cerr;
}

std::ifstream open_data_bin_for_resume() {
  return std::ifstream("data.bin");
}

std::ofstream open_data_bin_append() {
  return std::ofstream("data.bin", std::ios::app);
}
