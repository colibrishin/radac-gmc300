// Standalone entry point: call recovered main_app() then exit.
// With GMC_USE_BOINC: init BOINC, get init_data from BOINC API into g_init_data_buf + g_init_data_len, then main_app().
// See docs/init_data-culprit.md: runtime/num_samples only applied when init_data is set (g_init_data_len > 0).

#include "init_data.h"
#include <cstring>
#include <iostream>

void main_app();

#if defined(GMC_USE_BOINC) && GMC_USE_BOINC
#include "boinc_api.h"
#endif

std::array<char, k_init_data_buf_size> g_init_data_buf{};
std::size_t g_init_data_len = 0u;

#if defined(GMC_USE_BOINC) && GMC_USE_BOINC
static void set_init_data_from_boinc() {
  if (boinc_parse_init_data_file() != 0) {
    std::cerr << "Init data: parse failed (init_data file missing or invalid)\n";
    return;
  }
  APP_INIT_DATA aid;
  if (boinc_get_init_data(aid) != 0 || !aid.project_preferences) {
    std::cerr << "Init data: not available (get_init_data failed or no project_preferences)\n";
    return;
  }
  /* BOINC stores full "<project_preferences>...</project_preferences>"; use as-is so <runtime> is a direct child of root. */
  std::size_t len = std::strlen(aid.project_preferences);
  if (len >= g_init_data_buf.size()) {
    std::cerr << "Init data: project_preferences too large (" << len << " bytes)\n";
    return;
  }
  std::memcpy(g_init_data_buf.data(), aid.project_preferences, len + 1);
  g_init_data_len = len;
  std::cerr << "Init data: " << g_init_data_len << " bytes (project_preferences)\n";
}
#endif

int main() {
#if defined(GMC_USE_BOINC) && GMC_USE_BOINC
  int ret = boinc_init();
  if (ret) {
    return ret;
  }
  set_init_data_from_boinc();  /* BOINC getter → g_init_data_buf + g_init_data_len for main_app() */
#endif
  main_app();
  return 0;
}
