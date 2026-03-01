// Safe C string/io helpers. Use reentrant or bounds-checked variants where available.

#ifndef GMC_RECOVERED_SAFE_C_H
#define GMC_RECOVERED_SAFE_C_H

#include <cstring>

/** Reentrant strtok: strtok_s (MSVC), strtok_r (POSIX), else strtok. Call with *ctx = nullptr first. */
inline char* safe_strtok(char* str, const char* delim, char** ctx) {
#if defined(_MSC_VER)
  return ::strtok_s(str, delim, ctx);
#elif defined(_POSIX_C_SOURCE) || defined(__APPLE__) || defined(__linux__) || defined(__unix__)
  return ::strtok_r(str, delim, ctx);
#else
  (void)ctx;
  return std::strtok(str, delim);
#endif
}

#endif
