// Compatibility: __time64_t is MSVC CRT; on POSIX use a typedef for the same API.
#ifndef GMC_RECOVERED_TIME_COMPAT_H
#define GMC_RECOVERED_TIME_COMPAT_H

#if !defined(_WIN32)
#include <cstdint>
typedef std::int64_t __time64_t;
#endif

#endif
