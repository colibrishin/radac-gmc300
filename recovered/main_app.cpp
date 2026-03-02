// Recovered: main_app. See docs/phase5-data-flow.md.

#include "app_io.h"
#include "config.h"
#include "constants.h"
#include "init_data.h"
#include "safe_c.h"
#include "serial_port.h"
#include "time_compat.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
extern std::uint32_t init_time_dwords[3];
extern std::uint32_t data_bin_open_arg1;
extern std::uint32_t data_bin_open_arg2;
extern std::uint32_t data_bin_open_arg3;
extern double ms_to_sec_divisor;
extern double uSv_h_divisor;
extern std::uint16_t trickle_buf_pad;
extern void boinc_checkpoint(void);
extern void boinc_fraction_done(double);
extern void boinc_finish_and_exit(int);
extern int boinc_send_trickle_up(const char*, const char*);
extern int boinc_time_to_checkpoint(void);
extern void boinc_begin_critical_section(void);
extern void boinc_end_critical_section(void);
extern int gmc_boinc_should_exit(void);
extern void gmc_boinc_wait_if_suspended(void);
extern int parse_int_cstr(char*);
extern void sleep_one_second(unsigned int, int);
extern __time64_t get_time64(__time64_t*);
}
void open_com_port(const struct gmc_com_settings*, void**);
int init_com_after_open(void**, int);
void read_detector_sample(void*, int, int*);
void debug_dump_com_handle(void**);

/** Fill struct tm from __time64_t for data.bin field 3 (timestamp). UTC. */
static void time64_to_tm(__time64_t t64, std::tm* out) {
  if (out == nullptr) return;
#if defined(_WIN32)
  __time64_t t = t64;
  _gmtime64_s(out, &t);
#else
  time_t t = static_cast<time_t>(t64);
  gmtime_r(&t, out);
#endif
}

/** Returns data.bin sample_type: "f" first line fresh, "r" resume/long-gap, "n" normal. */
static const char* get_sample_type(int total_samples_done, int data_bin_line_count,
                                   unsigned int field1_ms, unsigned int prev_timer_ms) {
  if (total_samples_done == 1)
    return (data_bin_line_count > 0 ? "r" : "f");
  if (field1_ms > prev_timer_ms && (field1_ms - prev_timer_ms) > gmc::LONG_GAP_MS)
    return "r";
  return "n";
}

/** Application state for the main sampling loop. */
struct main_app_state {
  char line_buf[gmc::DATA_BIN_LINE_BUF];
  char resume_token_storage[gmc::RESUME_TOKENS_EXPECTED * gmc::RESUME_TOKEN_SIZE];
  char* strtok_ptr;
  int token_count;
  int config_node;
  int config_handle;
  /** Buffered trickle payload: multiple <sample>...</sample> sent in one trickle (like sample program). */
  std::string trickle_msg_buff;
  int total_samples_done;
  int data_bin_line_count;
  bool com_open_ok{false};
  /** Number of samples currently in trickle_msg_buff. */
  int trickle_pending{0};
  bool trickle_sent{false};
  __time64_t last_trickle_send_time{0};
  __time64_t time_start{0};
  double fraction_done{0};
  void* com_handle{nullptr};
  bool data_bin_resumed{false};
  unsigned int elapsed_ticks{0};
  __time64_t time_now{0};
  unsigned int time_diff_ms{0};
  unsigned int sample_interval_sec{0};
  bool debug_enabled{false};
  int num_samples{0};
  int retry_count_max{0};
  int retry_delay_sec{0};
  int read_error_threshold{0};
  double cpm_per_sec{0};
  int cpm_value{0};
  __time64_t last_sample_time{0};
  int read_error_count{0};
  __time64_t time_prev_sample{0};
  double elapsed_sec{0};
  double uSv_h{0};
  unsigned int elapsed_ticks_prev{0};
  unsigned short trickle_buf[512];
  /** Time-weighted counter: sum of (CPM * interval_minutes) per sample. */
  unsigned int counter{0};
  /** Previous line's timer (ms); used for long-gap sample_type "r". */
  unsigned int prev_timer_ms{0};
};

// ---- Init: derived inlines (mainline: init_prefs_and_config) ----
inline void init_prefs_from_init_data(main_app_state& state) {
  static constexpr char project_preferences[] = "project_preferences";
  static constexpr char radacdebug[] = "radacdebug";
  static constexpr char runtime[] = "runtime";

  int runtime_minutes = 0;
  if (g_init_data_len > 0u) {
    read_project_preferences(&state.config_handle, static_cast<const void*>(g_init_data_buf.data()), project_preferences, nullptr);
    get_config_value(&state.config_handle, &state.config_node, radacdebug, nullptr);
    if (!config_string_empty(&state.config_node)) {
      char* cstr = get_config_cstr(&state.config_node, 0);
      if (cstr != nullptr)
        state.debug_enabled = (parse_int_cstr(cstr) != 0);
    }
    int runtime_node;
    get_config_value(&state.config_handle, &runtime_node, runtime, nullptr);
    get_config_int(&state.config_node, &runtime_node);
    release_config_value(&runtime_node);
    if (!config_string_empty(&state.config_node)) {
      char* cstr = get_config_cstr(&state.config_node, 0);
      if (cstr != nullptr) {
        int parsed = parse_int_cstr(cstr);
        runtime_minutes = parsed;
        int available_sec = parsed * gmc::SECONDS_PER_MINUTE - gmc::RUNTIME_BUFFER_SEC;
        state.num_samples = (available_sec > 0) ? static_cast<int>(available_sec / gmc::EFFECTIVE_SEC_PER_SAMPLE) : 0;
        if (state.num_samples < 1)
          state.num_samples = 1;
      }
    }
    release_config_value(&state.config_node);
    release_config_value(&state.config_handle);
  } else {
    state.debug_enabled = true;
  }
  std::ostream* os = get_debug_stream();
  if (os) {
    if (g_init_data_len > 0u) {
      if (runtime_minutes > 0)
        *os << "Debug: runtime " << runtime_minutes << " min -> num_samples " << state.num_samples << '\n';
      else
        *os << "Debug: init_data present but runtime missing or zero, using num_samples " << state.num_samples << '\n';
    } else {
      *os << "Debug: no init_data, using default num_samples " << state.num_samples << '\n';
    }
  }
}

inline void apply_debug_overrides(main_app_state& state) {
  if (state.debug_enabled) {
    state.retry_count_max = 30;
    state.read_error_threshold = gmc::READ_ERROR_THRESHOLD;
    state.retry_delay_sec = 10;
  }
}

inline void log_startup(const main_app_state& state) {
  std::ostream* os = get_debug_stream();
  if (os)
    *os << "Radioactive@Home app rev ? for GMC-300 starting... num_samples=" << state.num_samples
        << " (init_data=" << (g_init_data_len > 0u ? "yes" : "no") << ")\n";
}

/** Init: read project preferences (radacdebug, runtime), compute num_samples, load COM settings. */
inline void init_prefs_and_config(main_app_state& state, gmc_com_settings& com_settings) {
  init_prefs_from_init_data(state);
  get_gmc_com_settings(&com_settings);
  apply_debug_overrides(state);
  log_startup(state);
}

// ---- Resume: derived inlines (mainline: resume_data_bin_and_trickle) ----
inline bool load_data_bin_line_count(main_app_state& state) {
  std::ifstream in = open_data_bin_for_resume();
  if (!in.is_open()) return false;
  while (in.getline(state.line_buf, static_cast<std::streamsize>(gmc::DATA_BIN_LINE_BUF)))
    state.data_bin_line_count++;
  return true;
}

/** If almost done, report fraction and exit. Does not return when exiting. */
inline void try_almost_done_exit(main_app_state& state) {
  if (state.num_samples - gmc::ALMOST_DONE_MARGIN >= state.data_bin_line_count) return;
  boinc_checkpoint();
  state.fraction_done = (state.num_samples > 0)
      ? static_cast<double>(state.data_bin_line_count) / state.num_samples
      : 0.99;
  boinc_fraction_done(state.fraction_done);
  if (state.debug_enabled) {
    std::ostream* os = get_debug_stream();
    if (os)
      *os << "Found previously created output file, workunit almost completed - exiting\n";
  }
  boinc_finish_and_exit(0);
}

inline void parse_last_line_tokens(main_app_state& state) {
  char* strtok_context = nullptr;
  state.strtok_ptr = safe_strtok(state.line_buf, ",\n", &strtok_context);
  state.token_count = 0;
  while (state.strtok_ptr != nullptr && state.token_count < gmc::RESUME_TOKENS_EXPECTED) {
    state.token_count++;
    char* dest = state.resume_token_storage + (state.token_count - 1) * gmc::RESUME_TOKEN_SIZE;
    std::snprintf(dest, gmc::RESUME_TOKEN_SIZE, "%s", state.strtok_ptr);
    state.strtok_ptr = safe_strtok(nullptr, ",\n", &strtok_context);
  }
  if (state.token_count >= 1)
    state.prev_timer_ms = static_cast<unsigned int>(parse_int_cstr(state.resume_token_storage));
}

inline void report_resumed_fraction_done(main_app_state& state) {
  if (state.data_bin_line_count > 0 && state.num_samples > 0) {
    state.fraction_done = static_cast<double>(state.data_bin_line_count) / state.num_samples;
    boinc_fraction_done(state.fraction_done);
  }
}

inline void restore_trickle_checkpoint(main_app_state& state) {
  std::ifstream tc("trickle_checkpoint.dat");
  if (!tc) return;
  std::string line1, line2;
  if (std::getline(tc, line1) && std::getline(tc, line2)) {
    __time64_t t = static_cast<__time64_t>(std::strtoll(line1.c_str(), nullptr, 10));
    int p = static_cast<int>(std::strtol(line2.c_str(), nullptr, 10));
    state.trickle_msg_buff.assign((std::istreambuf_iterator<char>(tc)), std::istreambuf_iterator<char>());
    if (t >= 0) state.last_trickle_send_time = t;
    if (p >= 0 && p <= 1000) state.trickle_pending = p;
    else if (!state.trickle_msg_buff.empty()) state.trickle_pending = 1;
  }
  tc.close();
  std::remove("trickle_checkpoint.dat");
}

/** Resume: load data.bin line count and last line; report fraction_done; restore trickle checkpoint. May call boinc_finish_and_exit(0) if almost done. */
inline void resume_data_bin_and_trickle(main_app_state& state) {
  if (!load_data_bin_line_count(state)) return;
  try_almost_done_exit(state);
  state.data_bin_resumed = true;
  parse_last_line_tokens(state);
  report_resumed_fraction_done(state);
  restore_trickle_checkpoint(state);
}

// ---- COM open: derived inlines (mainline: open_com_until_ready) ----
inline void sleep_retry_delay(main_app_state& state) {
  for (int d = 0; d < state.retry_delay_sec; ++d) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    sleep_one_second(1, 0);
  }
}

/** One attempt: open port and init (GETVER). Returns true on success. */
inline bool try_one_com_open(main_app_state& state, const gmc_com_settings& com_settings) {
  open_com_port(&com_settings, &state.com_handle);
  if (state.com_handle == nullptr) {
    state.com_open_ok = false;
    return false;
  }
  if (state.debug_enabled) {
    std::ostream* os = get_debug_stream();
    if (os)
      *os << "COM" << com_settings.port_number << " opened for read/write\n";
  }
  if (init_com_after_open(&state.com_handle, static_cast<int>(state.debug_enabled)) != 0) {
    serial_close(state.com_handle);
    state.com_handle = nullptr;
    return false;
  }
  state.com_open_ok = true;
  state.last_sample_time = get_time64(nullptr);
  state.elapsed_ticks = 0;
  state.counter = 0;
  return true;
}

/** COM open: retry until port opens and init (GETVER) succeeds. Returns true if open, false otherwise (caller should exit). */
inline bool open_com_until_ready(main_app_state& state, const gmc_com_settings& com_settings) {
  int retries_left = state.retry_count_max;
  while (retries_left > 0) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    if (try_one_com_open(state, com_settings))
      return true;
    sleep_retry_delay(state);
    retries_left--;
  }
  return false;
}

// ---- Main loop: derived inlines (mainline: run_main_loop) ----
/** Send pending trickle, close COM, report 100%, exit. Does not return. */
inline void finish_and_exit_with_trickle(main_app_state& state) {
  std::ostream* os = get_debug_stream();
  if (state.trickle_pending > 0 && !state.trickle_msg_buff.empty()) {
    int ret = boinc_send_trickle_up("rad_report_xml", const_cast<char*>(state.trickle_msg_buff.c_str()));
    if (ret == 0) {
      if (os)
        *os << "trickle sent (on exit) len=" << state.trickle_msg_buff.size() << " samples=" << state.trickle_pending << "\n";
      state.trickle_msg_buff.clear();
      state.trickle_pending = 0;
    } else if (os)
      *os << "trickle send failed (on exit) ret=" << ret << "\n";
  }
  if (state.debug_enabled && os) {
    if (state.total_samples_done == 0)
      *os << "WARNING: No readings from GM tube !\n";
    *os << "Done - calling boinc_finish()\n";
  }
  if (state.com_handle != nullptr)
    serial_close(state.com_handle);
  boinc_fraction_done(1.0);
  boinc_finish_and_exit(0);
}

/** If COM not open, try to open and init; on success set state. */
inline void try_com_reopen_in_loop(main_app_state& state, const gmc_com_settings& com_settings) {
  if (state.com_open_ok) return;
  open_com_port(&com_settings, &state.com_handle);
  if (state.com_handle == nullptr) {
    for (int w = 0; w < static_cast<int>(state.sample_interval_sec); ++w)
      sleep_one_second(1, 0);
    return;
  }
  if (init_com_after_open(&state.com_handle, static_cast<int>(state.debug_enabled)) == 0) {
    state.com_open_ok = true;
    state.data_bin_resumed = true;
    state.read_error_count = 0;
    state.elapsed_ticks = 0;
    state.counter = 0;
    state.time_start = get_time64(nullptr);
    state.last_trickle_send_time = get_time64(nullptr);
    state.last_sample_time = get_time64(nullptr);
  } else {
    serial_close(state.com_handle);
    state.com_handle = nullptr;
  }
}

/** Write one data.bin line and append/send trickle for this sample. */
inline void write_data_bin_and_trickle(main_app_state& state) {
  state.elapsed_sec = static_cast<double>(state.time_diff_ms) / ms_to_sec_divisor;
  double rate_sec = (state.elapsed_sec >= gmc::MIN_ELAPSED_SEC_FOR_RATE) ? state.elapsed_sec : gmc::MIN_ELAPSED_SEC_FOR_RATE;
  state.uSv_h = (static_cast<double>(state.elapsed_ticks) / rate_sec) / uSv_h_divisor;
  state.cpm_per_sec = static_cast<double>(state.elapsed_ticks) / rate_sec;
  state.trickle_buf[0] = trickle_buf_pad;

  __time64_t elapsed_sec_from_start = state.time_prev_sample - state.time_start;
  unsigned int field1_ms = (elapsed_sec_from_start <= 0) ? 0u : static_cast<unsigned int>(elapsed_sec_from_start) * 1000u;
  const char* sample_type = get_sample_type(state.total_samples_done, state.data_bin_line_count, field1_ms, state.prev_timer_ms);
  std::tm tm{};
  time64_to_tm(state.time_prev_sample, &tm);

  std::ostream* os = get_debug_stream();
  std::ofstream data_out = open_data_bin_append();
  if (!data_out.is_open()) return;
  data_out << field1_ms << ',' << state.counter << ','
           << (tm.tm_year + 1900) << '-' << (tm.tm_mon + 1) << '-' << tm.tm_mday << ' '
           << tm.tm_hour << ':' << tm.tm_min << ':' << tm.tm_sec << ",0," << sample_type << ",0\n";
  if (os)
    *os << field1_ms << ',' << state.counter << ','
        << (tm.tm_year + 1900) << '-' << (tm.tm_mon + 1) << '-' << tm.tm_mday << ' '
        << tm.tm_hour << ':' << tm.tm_min << ':' << tm.tm_sec << ",0," << sample_type << ",0\n";
  if (state.data_bin_resumed)
    state.data_bin_resumed = false;
  state.prev_timer_ms = field1_ms;

  std::ostringstream sample_os;
  sample_os << "<sample><timer>" << field1_ms << "</timer>\n"
    << "<counter>" << state.counter << "</counter>\n"
    << "<timestamp>" << (tm.tm_year + 1900) << '-' << (tm.tm_mon + 1) << '-' << tm.tm_mday
    << ' ' << tm.tm_hour << ':' << tm.tm_min << ':' << tm.tm_sec << "</timestamp>\n"
    << "<sensor_revision_int>0</sensor_revision_int>\n"
    << "<sample_type>" << sample_type << "</sample_type>\n"
    << "<vid_pid_int>0</vid_pid_int>\n</sample>\n";
  std::string sample_str = sample_os.str();
  if (sample_str.empty()) return;
  state.trickle_msg_buff.append(sample_str);
  state.trickle_pending++;
  int min_pending = state.debug_enabled ? gmc::TRICKLE_MIN_PENDING_DEBUG : gmc::TRICKLE_MIN_PENDING;
  int min_interval_min = state.debug_enabled ? gmc::TRICKLE_MIN_INTERVAL_MIN_DEBUG : gmc::TRICKLE_MIN_INTERVAL_MIN;
  int send_now = 0;
  if (state.trickle_pending > min_pending) {
    __time64_t now = get_time64(nullptr);
    __time64_t elapsed_sec = now - state.last_trickle_send_time;
    if (elapsed_sec >= 0 && static_cast<double>(elapsed_sec) / 60.0 >= min_interval_min)
      send_now = 1;
  }
  if (send_now) {
    int ret = boinc_send_trickle_up("rad_report_xml", const_cast<char*>(state.trickle_msg_buff.c_str()));
    if (ret == 0) {
      state.trickle_sent = true;
      if (os)
        *os << "trickle sent len=" << state.trickle_msg_buff.size() << " samples=" << state.trickle_pending << "\n";
      state.trickle_msg_buff.assign(sample_str);
      state.trickle_pending = 1;
      state.last_trickle_send_time = get_time64(nullptr);
    } else if (os)
      *os << "trickle send failed ret=" << ret << "\n";
  }
}

inline void persist_trickle_checkpoint(main_app_state& state) {
  std::ofstream tc("trickle_checkpoint.dat", std::ios::binary);
  if (tc) {
    tc << state.last_trickle_send_time << '\n' << state.trickle_pending << '\n';
    tc.write(state.trickle_msg_buff.data(), static_cast<std::streamsize>(state.trickle_msg_buff.size()));
    tc.close();
  }
  boinc_checkpoint();
}

inline void handle_read_error_and_lost_sensor(main_app_state& state) {
  state.read_error_count++;
  std::ostream* os = state.debug_enabled ? get_debug_stream() : nullptr;
  if (os)
    *os << "Error reading data\n";
  if (state.read_error_count <= gmc::READ_ERROR_THRESHOLD) return;
  if (state.trickle_pending > 0 && !state.trickle_msg_buff.empty()) {
    int ret = boinc_send_trickle_up("rad_report_xml", const_cast<char*>(state.trickle_msg_buff.c_str()));
    if (ret == 0) {
      if (os)
        *os << "trickle sent (lost sensor) len=" << state.trickle_msg_buff.size() << " samples=" << state.trickle_pending << "\n";
      state.trickle_msg_buff.clear();
      state.trickle_pending = 0;
    }
  }
  if (state.com_handle != nullptr)
    serial_close(state.com_handle);
  state.com_open_ok = false;
  state.com_handle = nullptr;
  if (os)
    *os << "Lost sensor, trying to reopen.... \n";
}

inline void wait_sample_interval(main_app_state& state, int sample_index) {
  if (sample_index >= state.num_samples) return;
  for (unsigned int w = 0; w < state.sample_interval_sec && state.sample_interval_sec > 1; ++w) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    sleep_one_second(1, 0);
  }
}

/** One iteration: read CPM; on success write data.bin and trickle, else handle errors; report fraction; wait. */
inline void do_one_sample_iteration(main_app_state& state, int sample_index, const gmc_com_settings& com_settings) {
  boinc_begin_critical_section();
  bool read_succeeded = false;
  for (int attempt = 0; attempt < gmc::READ_ATTEMPTS_PER_SAMPLE && !read_succeeded; ++attempt) {
    if (attempt > 0)
      sleep_one_second(gmc::GETCPM_RETRY_DELAY_SEC, 0);
    read_detector_sample(&state.com_handle, static_cast<int>(state.debug_enabled), &state.cpm_value);
    read_succeeded = true;
    state.read_error_count = 0;
  }
  if (read_succeeded) {
    state.time_prev_sample = get_time64(nullptr);
    __time64_t delta_sec = state.time_prev_sample - state.last_sample_time;
    state.time_diff_ms = (delta_sec <= 0) ? 1000u : static_cast<unsigned int>(delta_sec) * 1000u;
    state.last_sample_time = state.time_prev_sample;
    state.elapsed_ticks = static_cast<unsigned int>(state.cpm_value);
    state.total_samples_done++;
    state.fraction_done = static_cast<double>(state.data_bin_line_count + state.total_samples_done) / state.num_samples;
    {
      double interval_minutes = static_cast<double>(state.time_diff_ms) / static_cast<double>(gmc::MS_PER_MINUTE);
      state.counter += static_cast<unsigned int>(std::round(interval_minutes * static_cast<double>(state.elapsed_ticks)));
    }
    write_data_bin_and_trickle(state);
    state.elapsed_ticks_prev = state.elapsed_ticks;
    if (!state.trickle_sent && boinc_time_to_checkpoint())
      persist_trickle_checkpoint(state);
  } else {
    handle_read_error_and_lost_sensor(state);
  }
  boinc_end_critical_section();
  boinc_fraction_done(state.fraction_done);
  wait_sample_interval(state, sample_index);
}

/** Main loop: read CPM, write data.bin, buffer/send trickle, checkpoint, wait. Does not return (exits via boinc_finish_and_exit). */
inline void run_main_loop(main_app_state& state, const gmc_com_settings& com_settings) {
  state.time_start = get_time64(nullptr);
  state.last_trickle_send_time = get_time64(nullptr);
  int sample_index = state.data_bin_line_count;

  for (;;) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();

    if (state.num_samples <= sample_index)
      finish_and_exit_with_trickle(state);

    state.fraction_done = static_cast<double>(sample_index) / state.num_samples;
    try_com_reopen_in_loop(state, com_settings);
    if (state.com_open_ok)
      do_one_sample_iteration(state, sample_index, com_settings);

    sample_index++;
  }
}

void main_app() {
  main_app_state state{};
  state.sample_interval_sec = static_cast<unsigned int>(gmc::SAMPLE_INTERVAL_SEC);
  state.num_samples = gmc::DEFAULT_NUM_SAMPLES;
  state.retry_count_max = gmc::COM_OPEN_RETRY_COUNT;
  state.retry_delay_sec = 20;
  state.read_error_threshold = 10;
  state.last_sample_time = get_time64(nullptr);
  state.time_prev_sample = get_time64(nullptr);
  state.cpm_value = 0;

  gmc_com_settings com_settings;
  init_prefs_and_config(state, com_settings);
  resume_data_bin_and_trickle(state);
  if (!open_com_until_ready(state, com_settings)) {
    std::ostream* os = get_debug_stream();
    if (state.debug_enabled && os)
      *os << "No detector connected (check gmc.xml: COM" << static_cast<unsigned>(com_settings.port_number) << " = port " << com_settings.port_number << ")\n";
    boinc_finish_and_exit(0);
  }
  run_main_loop(state, com_settings);
}
