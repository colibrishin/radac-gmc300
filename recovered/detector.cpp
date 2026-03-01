// Recovered: read_detector_sample (GETCPM). See docs/phase5-data-flow.md.

#include "app_io.h"
#include "constants.h"
#include "serial_port.h"
#include <array>
#include <cstdint>

extern "C" {
extern void sleep_one_second(unsigned int, int);
}

namespace {

[[nodiscard]] serial_handle_t get_handle(void* com_handle) {
  if (com_handle == nullptr) return nullptr;
  return *static_cast<serial_handle_t*>(com_handle);
}

[[nodiscard]] bool send_getcpm(void* com_handle) {
  serial_handle_t h = get_handle(com_handle);
  if (h == nullptr) return false;
  int n = serial_write(h, gmc::cmd_getcpm.data(), gmc::cmd_getcpm.size());
  if (n != static_cast<int>(gmc::cmd_getcpm.size())) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_GetCPM(): failed to send data or no device on the other end\n";
    return false;
  }
  return true;
}

[[nodiscard]] bool read_cpm_response(void* com_handle, int debug_enabled, int* cpm_out) {
  if (cpm_out == nullptr) return false;
  serial_handle_t h = get_handle(com_handle);
  if (h == nullptr) return false;
  std::array<std::uint8_t, gmc::CPM_RESPONSE_LEN> buf{};
  int n = 0;
  for (int attempt = 0; attempt < gmc::GETCPM_RETRY_ATTEMPTS; ++attempt) {
    if (attempt > 0) {
      for (int d = 0; d < gmc::GETCPM_RETRY_DELAY_SEC; ++d)
        sleep_one_second(1, 0);
    } else {
      for (int d = 0; d < gmc::GETCPM_WAIT_AFTER_SEND_SEC; ++d)
        sleep_one_second(1, 0);
    }
    n = serial_read(h, buf.data(), gmc::CPM_RESPONSE_LEN);
    if (n == static_cast<int>(gmc::CPM_RESPONSE_LEN))
      break;
    if (attempt < gmc::GETCPM_RETRY_ATTEMPTS - 1 && n == 0)
      continue;
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_GetCPM(): read failed (got " << (n < 0 ? 0 : n) << " bytes, expected " << gmc::CPM_RESPONSE_LEN << ")\n";
    return false;
  }
  if (debug_enabled) {
    if (std::ostream* os = get_debug_stream())
      *os << "<GETCPM>> returned " << static_cast<int>(buf[0]) << ' ' << static_cast<int>(buf[1]) << '\n';
  }
  // GQ-RFC1201: first byte MSB, second byte LSB (big-endian)
  *cpm_out = static_cast<int>(buf[0]) * gmc::CPM_MSB_MULTIPLIER + static_cast<int>(buf[1]);
  return true;
}

} // namespace

void read_detector_sample(void* com_handle, int debug_enabled, int* cpm_out) {
  if (cpm_out != nullptr) *cpm_out = 0;
  if (com_handle == nullptr) return;
  for (int d = 0; d < gmc::GETCPM_DELAY_BEFORE_SEND_SEC; ++d)
    sleep_one_second(1, 0);  /* avoid hammering device */
  if (!send_getcpm(com_handle)) return;
  if (!read_cpm_response(com_handle, debug_enabled, cpm_out)) return;
}
