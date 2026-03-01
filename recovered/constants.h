// GMC-300 recovered app: named constants (protocol, config, timing).
// See docs/phase3-protocol.md, phase2-config.md.

#ifndef GMC_RECOVERED_CONSTANTS_H
#define GMC_RECOVERED_CONSTANTS_H

#include <cstddef>
#include <string_view>

namespace gmc {

// Serial / GMC-300 protocol. Commands as constexpr string_view; lengths kept for compatibility/assert.
inline constexpr unsigned int SERIAL_BAUD_RATE = 57600;
inline constexpr unsigned long SERIAL_READ_TIMEOUT_MS = 1000;
inline constexpr unsigned long SERIAL_TIMEOUT_PER_BYTE_MS = 100;

inline constexpr std::string_view cmd_getcpm = "<GETCPM>>";
inline constexpr std::string_view cmd_getver = "<GETVER>>";
inline constexpr std::string_view cmd_heartbeat0 = "<HEARTBEAT0>>";

inline constexpr std::size_t CMD_GETCPM_LEN = cmd_getcpm.size();
inline constexpr std::size_t CMD_GETVER_LEN = cmd_getver.size();
inline constexpr std::size_t CMD_HEARTBEAT0_LEN = cmd_heartbeat0.size();
inline constexpr std::size_t GETVER_RESPONSE_LEN = 14;  // e.g. "GMC-300Re 4.2"; some devices return 2 bytes only (HEARTBEAT0 disabled then)
inline constexpr int GETVER_DELAY_AFTER_OPEN_SEC = 2;  // wait after port open before first command (device/USB stabilize)
inline constexpr int GETVER_WAIT_AFTER_SEND_SEC = 2;   // wait after sending <GETVER>> before first read
inline constexpr std::size_t CPM_RESPONSE_LEN = 2;     // 16-bit big-endian CPM (MSB first per GQ-RFC1201)
inline constexpr int GETCPM_DELAY_BEFORE_SEND_SEC = 1;  // pause before sending GETCPM (avoid hammering device)
inline constexpr int GETCPM_WAIT_AFTER_SEND_SEC = 10;    // wait after send before first read
inline constexpr int GETCPM_RETRY_ATTEMPTS = 2;        // retries if first read gets 0 bytes
inline constexpr int GETCPM_RETRY_DELAY_SEC = 2;       // delay between retries

// App / config (init_data.xml: runtime in minutes -> sample count)
// Shorter record/trickle intervals: reduce SAMPLE_INTERVAL_SEC and/or TRICKLE_* below; runtime stays same (num_samples = runtime_min*60/interval).
inline constexpr int SECONDS_PER_MINUTE = 60;
inline constexpr unsigned int SAMPLE_INTERVAL_SEC = 30;  // read interval (sec between samples); reduce for more data.bin lines + trickles (they stay 1:1); runtime unchanged
inline constexpr int DEFAULT_NUM_SAMPLES = 300;
inline constexpr int COM_PORT_MIN = 1;
inline constexpr int COM_PORT_MAX = 99;
inline constexpr unsigned int COM_BAUD_DEFAULT = 57600;
inline constexpr int COM_BITS_DEFAULT = 8;
inline constexpr int COM_PARITY_DEFAULT = 0;   // 0=none, 1=odd, 2=even
inline constexpr int COM_STOPBITS_DEFAULT = 1;

// Retry and timing (detector not found: exit after COM_OPEN_RETRY_COUNT * retry_delay_sec seconds)
inline constexpr int COM_OPEN_RETRY_COUNT = 10;
inline constexpr int COM_OPEN_RETRY_DELAY_SEC = 60;
inline constexpr int READ_ERROR_THRESHOLD = 2;
inline constexpr int READ_ATTEMPTS_PER_SAMPLE = 2;

// Trickle-up: set to 1 so each trickle matches one data.bin line (same time/data) for server validation.
inline constexpr int TRICKLE_ONE_PER_SAMPLE = 1;         // 1 = send every sample (trickle time = record time); 0 = original rate-limited

// When TRICKLE_ONE_PER_SAMPLE == 0: send when pending > threshold and interval elapsed (reverse-engineered)
inline constexpr int TRICKLE_MIN_PENDING = 3;
inline constexpr int TRICKLE_MIN_PENDING_DEBUG = 2;
inline constexpr int TRICKLE_MIN_INTERVAL_SEC = 20;
inline constexpr int TRICKLE_MIN_INTERVAL_SEC_DEBUG = 10;

// data.bin resume
inline constexpr std::size_t DATA_BIN_LINE_BUF = 2048;
inline constexpr int RESUME_TOKENS_EXPECTED = 6;
inline constexpr int ALMOST_DONE_MARGIN = 2;

/** Substring required in GETVER response; use "GMC" so other device versions (e.g. GMC-320) are accepted. */
inline constexpr const char GMC_VERSION_PREFIX[] = "GMC";

} // namespace gmc

#endif
