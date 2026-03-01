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

extern "C" {
extern std::uint32_t init_time_dwords[3];
extern std::uint32_t data_bin_open_arg1;
extern std::uint32_t data_bin_open_arg2;
extern std::uint32_t data_bin_open_arg3;
extern double boinc_fraction_done_val;
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
  char trickle_scratch[784];
  int total_samples_done;
  int data_bin_line_count;
  bool com_open_ok{false};
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

  static constexpr char project_preferences[] = "project_preferences";
  static constexpr char radacdebug[] = "radacdebug";
  static constexpr char runtime[] = "runtime";

  if (g_init_data_len > 0u) {
    int runtime_minutes = 0;
    read_project_preferences(&state.config_handle, static_cast<const void*>(g_init_data_buf.data()), project_preferences, nullptr);
    get_config_value(&state.config_handle, &state.config_node, radacdebug, nullptr);
    if (!config_string_empty(&state.config_node)) {
      char* cstr = get_config_cstr(&state.config_node, 0);
      if (cstr != nullptr) {
        int parsed = parse_int_cstr(cstr);
        state.debug_enabled = (parsed != 0);
      }
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
        /* Match task runtime to project runtime: reserve RUNTIME_BUFFER_SEC, use EFFECTIVE_SEC_PER_SAMPLE (~41 s per sample). */
        int available_sec = parsed * gmc::SECONDS_PER_MINUTE - gmc::RUNTIME_BUFFER_SEC;
        state.num_samples = (available_sec > 0) ? static_cast<int>(available_sec / gmc::EFFECTIVE_SEC_PER_SAMPLE) : 0;
        if (state.num_samples < 1)
          state.num_samples = 1;
      }
    }
    release_config_value(&state.config_node);
    release_config_value(&state.config_handle);
    if (std::ostream* os = get_debug_stream()) {
      if (runtime_minutes > 0)
        *os << "Debug: runtime " << runtime_minutes << " min -> num_samples " << state.num_samples << '\n';
      else
        *os << "Debug: init_data present but runtime missing or zero, using num_samples " << state.num_samples << '\n';
    }
  } else {
    state.debug_enabled = true;  // no init_data: default to debug on
    if (std::ostream* os = get_debug_stream())
      *os << "Debug: no init_data, using default num_samples " << state.num_samples << '\n';
  }
  struct gmc_com_settings com_settings;
  get_gmc_com_settings(&com_settings);
  if (state.debug_enabled) {
    state.retry_count_max = 30;
    state.read_error_threshold = gmc::READ_ERROR_THRESHOLD;
    state.retry_delay_sec = 10;
  }
  if (std::ostream* os = get_debug_stream())
    *os << "Radioactive@Home app rev ? for GMC-300 starting... num_samples=" << state.num_samples
        << " (init_data=" << (g_init_data_len > 0u ? "yes" : "no") << ")\n";

  {
    std::ifstream in = open_data_bin_for_resume();
    if (in.is_open()) {
      while (in.getline(state.line_buf, static_cast<std::streamsize>(gmc::DATA_BIN_LINE_BUF)))
        state.data_bin_line_count++;

      if (state.num_samples - gmc::ALMOST_DONE_MARGIN < state.data_bin_line_count) {
        boinc_checkpoint();
        boinc_fraction_done(boinc_fraction_done_val);
        if (std::ostream* os = get_debug_stream(); state.debug_enabled && os != nullptr)
          *os << "Found previously created output file, workunit almost completed - exiting\n";
        boinc_finish_and_exit(0);
      }
      state.data_bin_resumed = true;
      char* strtok_context = nullptr;
      state.strtok_ptr = safe_strtok(state.line_buf, ",\n", &strtok_context);
      state.token_count = 0;
      while (state.strtok_ptr != nullptr && state.token_count < gmc::RESUME_TOKENS_EXPECTED) {
        state.token_count++;
        char* dest = state.resume_token_storage + (state.token_count - 1) * gmc::RESUME_TOKEN_SIZE;
        std::snprintf(dest, gmc::RESUME_TOKEN_SIZE, "%s", state.strtok_ptr);
        state.strtok_ptr = safe_strtok(nullptr, ",\n", &strtok_context);
      }
      /* Resume: only field 1 (timer) is restored; counter is zeroed on COM open. */
      if (state.token_count >= 1)
        state.prev_timer_ms = static_cast<unsigned int>(parse_int_cstr(state.resume_token_storage));
    }
  }

  int retries_left = state.retry_count_max;
  while (retries_left > 0) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    open_com_port(&com_settings, &state.com_handle);
    if (state.com_handle == nullptr) {
      state.com_open_ok = false;
      for (int d = 0; d < state.retry_delay_sec; ++d) {
        if (gmc_boinc_should_exit())
          boinc_finish_and_exit(0);
        gmc_boinc_wait_if_suspended();
        sleep_one_second(1, 0);
      }
      retries_left--;
      continue;
    }
    if (state.debug_enabled) {
      if (std::ostream* os = get_debug_stream())
        *os << "COM" << com_settings.port_number << " opened for read/write\n";
    }
    if (init_com_after_open(&state.com_handle, static_cast<int>(state.debug_enabled)) == 0) {
      state.com_open_ok = true;
      state.last_sample_time = get_time64(nullptr);
      state.elapsed_ticks = 0;
      state.counter = 0;  /* counter zeroed on COM open (no restore on resume) */
      break;
    }
    serial_close(state.com_handle);
    state.com_handle = nullptr;
    for (int d = 0; d < state.retry_delay_sec; ++d) {
      if (gmc_boinc_should_exit())
        boinc_finish_and_exit(0);
      gmc_boinc_wait_if_suspended();
      sleep_one_second(1, 0);
    }
    retries_left--;
  }

  if (!state.com_open_ok) {
    if (state.debug_enabled) {
      if (std::ostream* os = get_debug_stream())
        *os << "No detector connected (check gmc.xml: COM" << static_cast<unsigned>(com_settings.port_number) << " = port " << com_settings.port_number << ")\n";
    }
    boinc_finish_and_exit(0);
  }

  state.time_start = get_time64(nullptr);
  state.last_trickle_send_time = get_time64(nullptr);
  int sample_index = state.data_bin_line_count;

  for (;;) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    if (state.num_samples <= sample_index) {
      if (state.trickle_pending > 0 && !state.trickle_sent) {
        if (boinc_send_trickle_up("rad_report_xml", state.trickle_scratch) == 0) {
          state.trickle_pending = 0;
          if (state.debug_enabled) {
            if (std::ostream* os = get_debug_stream())
              *os << "Debug: trickle sent (on exit)\n";
          }
        }
      }
      if (std::ostream* os = get_debug_stream(); state.debug_enabled && os != nullptr) {
        if (state.total_samples_done == 0)
          *os << "WARNING: No readings from GM tube !\n";
        *os << "Done - calling boinc_finish()\n";
      }
      if (state.com_handle != nullptr)
        serial_close(state.com_handle);
      boinc_fraction_done(1.0);  /* report 100% before exit */
      boinc_finish_and_exit(0);
    }
    /* Progress so far when no sample completed this iteration (COM closed, read error, etc.) */
    state.fraction_done = static_cast<double>(sample_index) / state.num_samples;

    if (!state.com_open_ok) {
      open_com_port(&com_settings, &state.com_handle);
      if (state.com_handle == nullptr) {
        for (int w = 0; w < static_cast<int>(state.sample_interval_sec); ++w)
          sleep_one_second(1, 0);
      } else {
        if (init_com_after_open(&state.com_handle, static_cast<int>(state.debug_enabled)) == 0) {
          state.com_open_ok = true;
          state.data_bin_resumed = true;
          state.read_error_count = 0;
          state.elapsed_ticks = 0;
          state.counter = 0;  /* counter zeroed on COM open */
          state.time_start = get_time64(nullptr);
          state.last_trickle_send_time = get_time64(nullptr);
          state.last_sample_time = get_time64(nullptr);
        } else {
          /* Init failed (e.g. GETVER): close so next iteration does a clean reopen */
          serial_close(state.com_handle);
          state.com_handle = nullptr;
        }
      }
    } else {
      boinc_begin_critical_section();
      bool read_succeeded = false;
      for (int attempt = 0; attempt < gmc::READ_ATTEMPTS_PER_SAMPLE && !read_succeeded; ++attempt) {
        if (attempt > 0)
          sleep_one_second(gmc::GETCPM_RETRY_DELAY_SEC, 0);
        read_detector_sample(&state.com_handle, static_cast<int>(state.debug_enabled), &state.cpm_value);
        read_succeeded = true;  /* assume success; read_detector_sample is void */
        state.read_error_count = 0;
      }
      if (read_succeeded) {
        state.time_prev_sample = get_time64(nullptr);
        /* Interval since last sample (for rate calc). Field 1 written to data.bin is cumulative ms from time_start (see below). */
        {
          __time64_t delta_sec = state.time_prev_sample - state.last_sample_time;
          state.time_diff_ms = (delta_sec <= 0) ? 1000u : static_cast<unsigned int>(delta_sec) * 1000u;
        }
        state.last_sample_time = state.time_prev_sample;
        state.elapsed_ticks = static_cast<unsigned int>(state.cpm_value);  /* CPM from this sample for uSv_h/cpm_per_sec */
        state.total_samples_done++;  /* count successful samples so "No readings" is accurate */
        state.fraction_done = static_cast<double>(state.total_samples_done) / state.num_samples;  /* report progress after each completed sample */
        /* data.bin second field: time-weighted counter. counter += CPM * interval_minutes; interval_minutes = time_diff_ms / MS_PER_MINUTE. */
        {
          double interval_minutes = static_cast<double>(state.time_diff_ms) / static_cast<double>(gmc::MS_PER_MINUTE);
          state.counter += static_cast<unsigned int>(std::round(interval_minutes * static_cast<double>(state.elapsed_ticks)));
        }
        std::ofstream data_out = open_data_bin_append();
        if (data_out.is_open()) {
          state.elapsed_sec = static_cast<double>(state.time_diff_ms) / ms_to_sec_divisor;
          double rate_sec = (state.elapsed_sec >= gmc::MIN_ELAPSED_SEC_FOR_RATE) ? state.elapsed_sec : gmc::MIN_ELAPSED_SEC_FOR_RATE;
          state.uSv_h = (static_cast<double>(state.elapsed_ticks) / rate_sec) / uSv_h_divisor;
          state.cpm_per_sec = static_cast<double>(state.elapsed_ticks) / rate_sec;
          state.trickle_buf[0] = trickle_buf_pad;

          /*
           * data.bin line format. One line per sample, comma-separated:
           *   Field 1: cumulative elapsed ms since run start (can be 0).
           *   Field 2: counter (time-weighted: sum of CPM * (time_diff_ms / MS_PER_MINUTE) per sample).
           *   Field 3: timestamp "Y-M-D H:M:S" UTC. Field 4: 0. Field 5: sample_type "f"/"r"/"n". Field 6: 0.
           */
          __time64_t elapsed_sec_from_start = state.time_prev_sample - state.time_start;
          unsigned int field1_ms = (elapsed_sec_from_start <= 0) ? 0u : static_cast<unsigned int>(elapsed_sec_from_start) * 1000u;
          const char* sample_type = get_sample_type(state.total_samples_done, state.data_bin_line_count, field1_ms, state.prev_timer_ms);
          std::tm tm{};
          time64_to_tm(state.time_prev_sample, &tm);
          data_out << field1_ms << ',' << state.counter << ','
                   << (tm.tm_year + 1900) << '-' << (tm.tm_mon + 1) << '-' << tm.tm_mday << ' '
                   << tm.tm_hour << ':' << tm.tm_min << ':' << tm.tm_sec << ",0," << sample_type << ",0\n";
          if (std::ostream* os = get_debug_stream(); os != nullptr)
            *os << field1_ms << ',' << state.counter << ','
                << (tm.tm_year + 1900) << '-' << (tm.tm_mon + 1) << '-' << tm.tm_mday << ' '
                << tm.tm_hour << ':' << tm.tm_min << ':' << tm.tm_sec << ",0," << sample_type << ",0\n";
          if (state.data_bin_resumed)
            state.data_bin_resumed = false;
          state.prev_timer_ms = field1_ms;  /* used for next line's long-gap check */
          /* Trickle-up: same timer/counter/timestamp as data.bin line so validation can match. */
          int n = std::snprintf(state.trickle_scratch, sizeof(state.trickle_scratch),
            "<sample><timer>%u</timer>\n<counter>%u</counter>\n<timestamp>%u-%u-%u %u:%u:%u</timestamp>\n"
            "<sensor_revision_int>%i</sensor_revision_int>\n<sample_type>%s</sample_type>\n<vid_pid_int>%u</vid_pid_int>\n</sample>\n",
            field1_ms, state.counter,
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            0, sample_type, 0u);
          if (n > 0 && n < static_cast<int>(sizeof(state.trickle_scratch))) {
            int send_now = 0;
            state.trickle_pending++;
            int min_pending = state.debug_enabled ? gmc::TRICKLE_MIN_PENDING_DEBUG : gmc::TRICKLE_MIN_PENDING;
            int min_interval_min = state.debug_enabled ? gmc::TRICKLE_MIN_INTERVAL_MIN_DEBUG : gmc::TRICKLE_MIN_INTERVAL_MIN;
            if (state.trickle_pending > min_pending) {
              __time64_t now = get_time64(nullptr);
              __time64_t elapsed_sec = now - state.last_trickle_send_time;
              if (elapsed_sec >= 0 && static_cast<double>(elapsed_sec) / 60.0 >= min_interval_min) {
                send_now = 1;
                state.trickle_pending = 0;
                state.last_trickle_send_time = now;
              }
            }
            if (send_now) {
              (void)boinc_send_trickle_up("rad_report_xml", state.trickle_scratch);
              state.trickle_sent = true;
              if (state.debug_enabled) {
                if (std::ostream* os = get_debug_stream())
                  *os << "Debug: trickle sent\n";
              }
            }
          }
        }
        state.elapsed_ticks_prev = state.elapsed_ticks;
        if (!state.trickle_sent && boinc_time_to_checkpoint())
          boinc_checkpoint();
      } else {
        state.read_error_count++;
        if (std::ostream* os = get_debug_stream(); state.debug_enabled && os != nullptr)
          *os << "Error reading data\n";
        if (state.read_error_count > gmc::READ_ERROR_THRESHOLD) {
          if (state.com_handle != nullptr)
            serial_close(state.com_handle);
          state.com_open_ok = false;
          state.com_handle = nullptr;
          if (std::ostream* os = get_debug_stream(); state.debug_enabled && os != nullptr)
            *os << "Lost sensor, trying to reopen.... \n";
        }
      }
      boinc_end_critical_section();
      boinc_fraction_done(state.fraction_done);
      if (sample_index < state.num_samples) {
        state.time_now = get_time64(nullptr);
        for (unsigned int w = 0; w < state.sample_interval_sec && state.sample_interval_sec > 1; ++w) {
          if (gmc_boinc_should_exit())
            boinc_finish_and_exit(0);
          gmc_boinc_wait_if_suspended();
          sleep_one_second(1, 0);  /* 1 second per iteration so total wait = sample_interval_sec seconds */
        }
      }
    }
    sample_index++;
  }
}
