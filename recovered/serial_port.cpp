// Serial port implementation using Boost.Asio (serial_port.h API unchanged).

#include "constants.h"
#include "serial_port.h"
#include <boost/asio/serial_port.hpp>
#include <boost/asio/write.hpp>
#include <cstddef>
#include <memory>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#else
#include <termios.h>
#endif

namespace {

struct serial_impl {
  boost::asio::io_context ioc;
  boost::asio::serial_port port;

  explicit serial_impl(const std::string& device)
      : port(ioc, device) {}
};

std::string build_port_name(unsigned int port_number) {
#if defined(_WIN32)
  // 0-based: 0 → COM1, 1 → COM2, ...
  return "\\\\.\\COM" + std::to_string(port_number + 1);
#elif defined(__linux__)
  // 0-based: 0 → ttyUSB0, 1 → ttyUSB1, ...
  return "/dev/ttyUSB" + std::to_string(port_number);
#elif defined(__APPLE__)
  if (port_number == 0)
    return "/dev/cu.usbserial";
  return "/dev/cu.usbserial-" + std::to_string(port_number);
#else
  return "/dev/ttyS" + std::to_string(port_number);
#endif
}

bool set_native_timeouts(serial_impl* impl) {
#if defined(_WIN32)
  HANDLE h = impl->port.native_handle();
  COMMTIMEOUTS timeouts = {};
  timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(gmc::SERIAL_READ_TIMEOUT_MS);
  timeouts.ReadIntervalTimeout = 0;
  timeouts.ReadTotalTimeoutMultiplier = static_cast<DWORD>(gmc::SERIAL_TIMEOUT_PER_BYTE_MS);
  timeouts.WriteTotalTimeoutMultiplier = static_cast<DWORD>(gmc::SERIAL_TIMEOUT_PER_BYTE_MS);
  timeouts.WriteTotalTimeoutConstant = static_cast<DWORD>(gmc::SERIAL_READ_TIMEOUT_MS);
  return SetCommTimeouts(h, &timeouts) != 0;
#else
  int fd = impl->port.native_handle();
  struct termios tio;
  if (tcgetattr(fd, &tio) != 0)
    return false;
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 10;
  return tcsetattr(fd, TCSANOW, &tio) == 0;
#endif
}

} // namespace

serial_handle_t serial_open(unsigned int port_number, unsigned int baud_rate,
                            int data_bits, int parity, int stop_bits) {
  std::string name = build_port_name(port_number);
  std::unique_ptr<serial_impl> impl;
  try {
    impl = std::make_unique<serial_impl>(name);
  } catch (...) {
    return nullptr;
  }

  boost::system::error_code ec;
  impl->port.set_option(boost::asio::serial_port_base::baud_rate(baud_rate), ec);
  if (ec) return nullptr;

  using boost::asio::serial_port_base;
  unsigned int cs = (data_bits >= 5 && data_bits <= 8) ? data_bits : 8;
  impl->port.set_option(serial_port_base::character_size(cs), ec);
  if (ec) return nullptr;

  impl->port.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none), ec);
  if (ec) return nullptr;

  serial_port_base::parity::type p = serial_port_base::parity::none;
  if (parity == 1) p = serial_port_base::parity::odd;
  else if (parity == 2) p = serial_port_base::parity::even;
  impl->port.set_option(serial_port_base::parity(p), ec);
  if (ec) return nullptr;

  serial_port_base::stop_bits::type sb = (stop_bits == 2)
      ? serial_port_base::stop_bits::two
      : serial_port_base::stop_bits::one;
  impl->port.set_option(serial_port_base::stop_bits(sb), ec);
  if (ec) return nullptr;

  if (!set_native_timeouts(impl.get()))
    return nullptr;

  return static_cast<serial_handle_t>(impl.release());
}

void serial_close(serial_handle_t h) noexcept {
  if (h != nullptr)
    delete static_cast<serial_impl*>(h);
}

int serial_write(serial_handle_t h, const void* buf, std::size_t len) {
  if (h == nullptr || buf == nullptr)
    return -1;
  auto* impl = static_cast<serial_impl*>(h);
  boost::system::error_code ec;
  std::size_t n = boost::asio::write(impl->port, boost::asio::buffer(buf, len), ec);
  if (ec)
    return -1;
  return static_cast<int>(n);
}

int serial_read(serial_handle_t h, void* buf, std::size_t len) {
  if (h == nullptr || buf == nullptr)
    return -1;
  auto* impl = static_cast<serial_impl*>(h);
  boost::system::error_code ec;
  std::size_t n = impl->port.read_some(boost::asio::buffer(buf, len), ec);
  if (ec) {
    if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted ||
        ec == boost::asio::error::timed_out)
      return static_cast<int>(n);
    return -1;
  }
  return static_cast<int>(n);
}
