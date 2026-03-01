// Recovered: open_com_port, init_com_after_open (GETVER/HEARTBEAT0). See docs/phase5-data-flow.md.

#include "app_io.h"
#include "config.h"
#include "constants.h"
#include "serial_port.h"
#include <array>
#include <cstring>

extern "C" {
extern void sleep_one_second(unsigned int, int);
}

namespace {

[[nodiscard]] serial_handle_t get_handle(void* com_handle) {
  if (com_handle == nullptr) return nullptr;
  return *static_cast<serial_handle_t*>(com_handle);
}

[[nodiscard]] bool send_getver(void* com_handle, char* version_buf, int debug_enabled) {
  if (version_buf == nullptr) return false;
  serial_handle_t h = get_handle(com_handle);
  if (h == nullptr) return false;
  int written = serial_write(h, gmc::cmd_getver.data(), gmc::cmd_getver.size());
  if (written != static_cast<int>(gmc::cmd_getver.size())) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_GetVersion(): failed to send <GETVER>> or no device on the other end\n";
    return false;
  }
  std::memset(version_buf, 0, gmc::GETVER_RESPONSE_LEN + 1);
  for (int d = 0; d < gmc::GETVER_WAIT_AFTER_SEND_SEC; ++d)
    sleep_one_second(1, 0);
  /* Device may send 2 non-ASCII bytes (ack/greeting) then 14 ASCII version; or 14 in one/chunks. */
  std::size_t total = 0;
  const int max_attempts = 15;
  for (int attempt = 0; attempt < max_attempts && total < gmc::GETVER_RESPONSE_LEN; ++attempt) {
    if (attempt > 0)
      sleep_one_second(1, 0);
    int n = serial_read(h, version_buf + total, gmc::GETVER_RESPONSE_LEN - total);
    if (n <= 0)
      break;
    total += static_cast<std::size_t>(n);
    /* If we have exactly 2 bytes and they don't look like start of "GMC", discard and keep reading. */
    if (total == 2 && version_buf[0] != 'G')
      total = 0;
  }
  if (total != gmc::GETVER_RESPONSE_LEN) {
    if (std::ostream* os = get_debug_stream()) {
      *os << "gmc_GetVersion(): read failed (got " << total << " bytes, expected " << gmc::GETVER_RESPONSE_LEN << ")";
      if (total > 0 && total <= gmc::GETVER_RESPONSE_LEN) {
        *os << " response: \"";
        for (std::size_t i = 0; i < total; ++i)
          *os << (version_buf[i] >= 32 && version_buf[i] < 127 ? version_buf[i] : '?');
        *os << "\"";
      }
      if (total == 0)
        *os << " (check gmc.xml baud: 57600 for GMC-300 V3, 115200 for Plus V4)";
      *os << '\n';
    }
    return false;
  }
  version_buf[gmc::GETVER_RESPONSE_LEN] = '\0';
  if (debug_enabled) {
    if (std::ostream* os = get_debug_stream())
      *os << "<GETVER>> returned " << version_buf << '\n';
  }
  return true;
}

/** Send <HEARTBEAT0>> to turn off GMC heartbeat. Per GQ-RFC1201, return is None (no response to read). */
[[nodiscard]] bool send_heartbeat0(void* com_handle) {
  serial_handle_t h = get_handle(com_handle);
  if (h == nullptr) return false;
  int written = serial_write(h, gmc::cmd_heartbeat0.data(), gmc::cmd_heartbeat0.size());
  sleep_one_second(1, 0);
  if (written != static_cast<int>(gmc::cmd_heartbeat0.size())) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_GetVersion(): failed to send <HEARTBEAT0>> or no device on the other end\n";
    return false;
  }
  return true;
}

} // namespace

void open_com_port(const struct gmc_com_settings* s, void** com_handle_out) {
  if (com_handle_out != nullptr && s != nullptr)
    *com_handle_out = serial_open(static_cast<unsigned int>(s->port_number), s->baud,
                                  s->bits, s->parity, s->stopbits);
}

int init_com_after_open(void** com_handle, int debug_enabled) {
  if (com_handle == nullptr) return -1;
  for (int d = 0; d < gmc::GETVER_DELAY_AFTER_OPEN_SEC; ++d)
    sleep_one_second(1, 0);  /* let device/USB stabilize after open */
  std::array<char, gmc::GETVER_RESPONSE_LEN + 1> version_buf{};
  if (!send_getver(com_handle, version_buf.data(), debug_enabled))
    return -1;
  if (std::strstr(version_buf.data(), gmc::GMC_VERSION_PREFIX) == nullptr)
    return -1;  /* version string must contain "GMC" (see docs/phase3-protocol.md) */
  /* HEARTBEAT0 not sent in init; see docs/phase3-protocol.md §5. */
  return 0;
}
