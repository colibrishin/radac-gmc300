#ifndef RECOVERED_INIT_DATA_H
#define RECOVERED_INIT_DATA_H

#include <array>
#include <cstddef>

constexpr std::size_t k_init_data_buf_size = 32768u;

extern std::array<char, k_init_data_buf_size> g_init_data_buf;
extern std::size_t g_init_data_len;  // 0 = not filled

#endif
