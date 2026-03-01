// Platform-agnostic serial API for GMC-300 detector.
// Implementation: serial_port.cpp (single file; Windows via _WIN32, POSIX otherwise; MSVC, GCC, Clang).

#ifndef GMC_RECOVERED_SERIAL_PORT_H
#define GMC_RECOVERED_SERIAL_PORT_H

#include <cstddef>

typedef void* serial_handle_t;

// data_bits 5-8, parity 0=none 1=odd 2=even, stop_bits 1 or 2
[[nodiscard]] serial_handle_t serial_open(unsigned int port_number, unsigned int baud_rate,
                                          int data_bits, int parity, int stop_bits);
void serial_close(serial_handle_t h) noexcept;
[[nodiscard]] int serial_write(serial_handle_t h, const void* buf, std::size_t len);
[[nodiscard]] int serial_read(serial_handle_t h, void* buf, std::size_t len);

#endif
