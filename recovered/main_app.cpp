// Recovered: main_app. See docs/phase5-data-flow.md.

#include "app_io.h"
#include "config.h"
#include "constants.h"
#include "init_data.h"
#include "safe_c.h"
#include "serial_port.h"
#include "time_compat.h"
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

/** Fill struct tm from __time64_t for data.bin field 3 (timestamp). UTC to match original program. */
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

struct main_app_state {
  char line_buf[2048];
  char token_buf[256];
  char resume_token_storage[1536];
  char* strtok_ptr;
  int token_count;
  int config_node;
  int config_handle;
  char trickle_scratch[784];
  int total_samples_done;
  int data_bin_line_count;
  char fraction_fmt[32];
  char com_open_ok;
  int trickle_result;
  int trickle_pending;
  int trickle_sent;
  __time64_t last_trickle_send_time;  /* for time-based send (local_510 in original) */
  __time64_t time_start;
  double fraction_done;
  unsigned int time_hi;
  unsigned int time_med;
  unsigned int time_lo;
  void* com_handle;
  char data_bin_resumed;
  unsigned int elapsed_ticks;
  char trickle_xml_buf[28];
  __time64_t time_now;
  int com_open_failed_count;
  unsigned int time_diff_ms;
  __time64_t time_prev;
  unsigned int sample_interval_sec;
  char use_short_intervals;
  char debug_enabled;
  int num_samples;
  int sample_interval_sec_copy;
  int retry_count_max;
  int retry_delay_sec;
  int read_error_threshold;
  double cpm_per_sec;
  int cpm_value;
  __time64_t last_sample_time;
  int read_error_count;
  __time64_t time_prev_sample;
  double elapsed_sec;
  double uSv_h;
  unsigned int elapsed_ticks_prev;
  unsigned short trickle_buf[512];
  /** Accumulated counter for data.bin second field (Ghidra local_4b8). Incremented by elapsed_ticks each sample. */
  unsigned int counter;
};

void main_app() {
  main_app_state L{};
  L.sample_interval_sec = gmc::SAMPLE_INTERVAL_SEC;
  L.use_short_intervals = 1;
  L.sample_interval_sec_copy = static_cast<int>(gmc::SAMPLE_INTERVAL_SEC);
  L.num_samples = gmc::DEFAULT_NUM_SAMPLES;
  L.retry_count_max = gmc::COM_OPEN_RETRY_COUNT;
  L.retry_delay_sec = 20;
  L.read_error_threshold = 10;
  L.time_hi = init_time_dwords[0];
  L.time_med = init_time_dwords[1];
  L.time_lo = init_time_dwords[2];
  L.last_sample_time = get_time64(nullptr);
  L.time_prev_sample = get_time64(nullptr);
  L.cpm_value = 0;

  static constexpr char project_preferences[] = "project_preferences";
  static constexpr char radacdebug[] = "radacdebug";
  static constexpr char runtime[] = "runtime";

  if (g_init_data_len > 0u) {
    int runtime_minutes = 0;
    read_project_preferences(&L.config_handle, static_cast<const void*>(g_init_data_buf.data()), project_preferences, nullptr);
    get_config_value(&L.config_handle, &L.config_node, radacdebug, nullptr);
    if (!config_string_empty(&L.config_node)) {
      char* cstr = get_config_cstr(&L.config_node, 0);
      if (cstr != nullptr) {
        int parsed = parse_int_cstr(cstr);
        L.debug_enabled = static_cast<char>(parsed != 0);
      }
    }
    int runtime_node;
    get_config_value(&L.config_handle, &runtime_node, runtime, nullptr);
    get_config_int(&L.config_node, &runtime_node);
    release_config_value(&runtime_node);
    if (!config_string_empty(&L.config_node)) {
      char* cstr = get_config_cstr(&L.config_node, 0);
      if (cstr != nullptr) {
        int parsed = parse_int_cstr(cstr);
        runtime_minutes = parsed;
        int available_sec = parsed * gmc::SECONDS_PER_MINUTE - gmc::RUNTIME_BUFFER_SEC;
        L.num_samples = (available_sec > 0) ? static_cast<int>(available_sec / gmc::EFFECTIVE_SEC_PER_SAMPLE) : 0;
        if (L.num_samples < 1)
          L.num_samples = 1;
      }
    }
    release_config_value(&L.config_node);
    release_config_value(&L.config_handle);
    if (std::ostream* os = get_debug_stream()) {
      if (runtime_minutes > 0)
        *os << "Debug: runtime " << runtime_minutes << " min -> num_samples " << L.num_samples << '\n';
      else
        *os << "Debug: init_data present but runtime missing or zero, using num_samples " << L.num_samples << '\n';
    }
  } else {
    L.debug_enabled = 1;  // no init_data: default to debug on
    if (std::ostream* os = get_debug_stream())
      *os << "Debug: no init_data, using default num_samples " << L.num_samples << '\n';
  }
  struct gmc_com_settings com_settings;
  get_gmc_com_settings(&com_settings);
  if (L.debug_enabled != 0) {
    L.retry_count_max = 30;
    L.read_error_threshold = gmc::READ_ERROR_THRESHOLD;
    L.retry_delay_sec = 10;
  }
  if (std::ostream* os = get_debug_stream())
    *os << "Radioactive@Home app rev ? for GMC-300 starting... num_samples=" << L.num_samples
        << " (init_data=" << (g_init_data_len > 0u ? "yes" : "no") << ")\n";

  {
    std::ifstream in = open_data_bin_for_resume();
    if (in.is_open()) {
      while (in.getline(L.line_buf, static_cast<std::streamsize>(gmc::DATA_BIN_LINE_BUF)))
        L.data_bin_line_count++;

      if (L.num_samples - gmc::ALMOST_DONE_MARGIN < L.data_bin_line_count) {
        boinc_checkpoint();
        boinc_fraction_done(boinc_fraction_done_val);
        if (std::ostream* os = get_debug_stream(); L.debug_enabled != 0 && os != nullptr)
          *os << "Found previously created output file, workunit almost completed - exiting\n";
        boinc_finish_and_exit(0);
      }
      L.data_bin_resumed = 1;
      char* strtok_context = nullptr;
      L.strtok_ptr = safe_strtok(L.line_buf, ",\n", &strtok_context);
      L.token_count = 0;
      while (L.strtok_ptr != nullptr && L.token_count < gmc::RESUME_TOKENS_EXPECTED) {
        L.token_count++;
        char* dest = reinterpret_cast<char*>(&L.token_buf) + L.token_count * 256;
        std::snprintf(dest, 256, "%s", L.strtok_ptr);
        L.strtok_ptr = safe_strtok(nullptr, ",\n", &strtok_context);
      }
      if (L.token_count == gmc::RESUME_TOKENS_EXPECTED)
        L.time_diff_ms = static_cast<unsigned int>(parse_int_cstr(L.resume_token_storage));
    }
  }

  int retries_left = L.retry_count_max;
  while (retries_left > 0) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    open_com_port(&com_settings, &L.com_handle);
    if (L.com_handle == nullptr) {
      L.com_open_failed_count = 1;
      L.com_open_ok = '\0';
      for (int d = 0; d < L.retry_delay_sec; ++d) {
        if (gmc_boinc_should_exit())
          boinc_finish_and_exit(0);
        gmc_boinc_wait_if_suspended();
        sleep_one_second(1, 0);
      }
      retries_left--;
      continue;
    }
    if (L.debug_enabled != 0) {
      if (std::ostream* os = get_debug_stream())
        *os << "COM" << com_settings.port_number << " opened for read/write\n";
    }
    L.com_open_failed_count = 0;
    if (init_com_after_open(&L.com_handle, static_cast<int>(L.debug_enabled)) == 0) {
      L.com_open_ok = 1;
      L.last_sample_time = get_time64(nullptr);
      L.elapsed_ticks = 0;
      L.counter = 0;
      break;
    }
    serial_close(L.com_handle);
    L.com_handle = nullptr;
    for (int d = 0; d < L.retry_delay_sec; ++d) {
      if (gmc_boinc_should_exit())
        boinc_finish_and_exit(0);
      gmc_boinc_wait_if_suspended();
      sleep_one_second(1, 0);
    }
    retries_left--;
  }

  if (L.com_open_ok == '\0') {
    if (L.debug_enabled != 0) {
      if (std::ostream* os = get_debug_stream())
        *os << "No detector connected (check gmc.xml: COM" << static_cast<unsigned>(com_settings.port_number) << " = port " << com_settings.port_number << ")\n";
    }
    boinc_finish_and_exit(0);
  }

  L.time_start = get_time64(nullptr);
  L.last_trickle_send_time = get_time64(nullptr);  /* original local_510 before loop */
  int sample_index = L.data_bin_line_count;

  for (;;) {
    if (gmc_boinc_should_exit())
      boinc_finish_and_exit(0);
    gmc_boinc_wait_if_suspended();
    if (L.num_samples <= sample_index) {
      if (L.trickle_pending > 0 && L.trickle_sent == 0) {
        if (boinc_send_trickle_up("rad_report_xml", L.trickle_scratch) == 0)
          L.trickle_pending = 0;
      }
      if (std::ostream* os = get_debug_stream(); L.debug_enabled != 0 && os != nullptr) {
        if (L.total_samples_done == 0)
          *os << "WARNING: No readings from GM tube !\n";
        *os << "Done - calling boinc_finish()\n";
      }
      if (L.com_handle != nullptr)
        serial_close(L.com_handle);
      boinc_fraction_done(1.0);  /* report 100% before exit */
      boinc_finish_and_exit(0);
    }
    /* Progress so far when no sample completed this iteration (COM closed, read error, etc.) */
    L.fraction_done = static_cast<double>(sample_index) / L.num_samples;

    if (L.com_open_ok == '\0') {
      open_com_port(&com_settings, &L.com_handle);
      if (L.com_handle == nullptr) {
        for (int w = 0; w < static_cast<int>(L.sample_interval_sec); ++w)
          sleep_one_second(1, 0);
      } else {
        L.com_open_failed_count = 0;
        if (init_com_after_open(&L.com_handle, static_cast<int>(L.debug_enabled)) == 0) {
          L.com_open_ok = 1;
          L.data_bin_resumed = 1;
          L.read_error_count = 0;
          L.elapsed_ticks = 0;
          L.counter = 0;
          L.time_start = get_time64(nullptr);
          L.last_trickle_send_time = get_time64(nullptr);
          L.last_sample_time = get_time64(nullptr);
        } else {
          /* Init failed (e.g. GETVER): close so next iteration does a clean reopen */
          serial_close(L.com_handle);
          L.com_handle = nullptr;
        }
      }
    } else {
      boinc_begin_critical_section();
      int read_ok = 1;
      for (int attempt = 0; attempt < gmc::READ_ATTEMPTS_PER_SAMPLE; ++attempt) {
        read_detector_sample(&L.com_handle, static_cast<int>(L.debug_enabled), &L.cpm_value);
        read_ok = 0;
        L.read_error_count = 0;
        break;
      }
      if (read_ok != 0)
        sleep_one_second(1, 0);
      if (read_ok == 0) {
        L.time_prev_sample = get_time64(nullptr);
        /* Interval since last sample (for rate calc). Field 1 written to data.bin is cumulative ms from time_start (see below). */
        {
          __time64_t delta_sec = L.time_prev_sample - L.last_sample_time;
          L.time_diff_ms = (delta_sec <= 0) ? 1000u : static_cast<unsigned int>(delta_sec) * 1000u;
        }
        L.last_sample_time = L.time_prev_sample;
        L.elapsed_ticks = static_cast<unsigned int>(L.cpm_value);  /* CPM from this sample for uSv_h/cpm_per_sec */
        L.total_samples_done++;  /* count successful samples so "No readings" is accurate */
        L.fraction_done = static_cast<double>(L.total_samples_done) / L.num_samples;  /* report progress after each completed sample */
        /* data.bin: accumulated counter (Ghidra local_4b8); used as second field in each line. */
        L.counter += L.elapsed_ticks;
        std::ofstream data_out = open_data_bin_append();
        if (data_out.is_open()) {
          L.elapsed_sec = static_cast<double>(L.time_diff_ms) / ms_to_sec_divisor;
          /* Use at least 60s for rate to avoid inflated CPM on first sample or short gaps */
          double rate_sec = (L.elapsed_sec >= 60.0) ? L.elapsed_sec : 60.0;
          L.uSv_h = (static_cast<double>(L.elapsed_ticks) / rate_sec) / uSv_h_divisor;
          L.cpm_per_sec = static_cast<double>(L.elapsed_ticks) / rate_sec;
          L.trickle_buf[0] = trickle_buf_pad;

          /*
           * data.bin line format (matches original GMC300.exe output). One line per sample, comma-separated:
           *   Field 1: cumulative elapsed ms since run start. First line: 1000 (original uses 1000,1,...).
           *            Later lines: (time_prev_sample - time_start) * 1000 (e.g. 242000, 483000).
           *   Field 2: counter (accumulated sum of CPM per sample; Ghidra local_4b8).
           *   Field 3: timestamp "Y-M-D H:M:S" UTC, no leading zeros. Field 4: 0. Field 5: sample_type "r"/"n". Field 6: 0.
           */
          __time64_t elapsed_sec_from_start = L.time_prev_sample - L.time_start;
          unsigned int cumulative_ms = (elapsed_sec_from_start <= 0) ? 1000u : static_cast<unsigned int>(elapsed_sec_from_start) * 1000u;
          unsigned int field1_ms = (L.total_samples_done == 1) ? 1000u : cumulative_ms;
          const char* sample_type = (L.total_samples_done == 1) ? "r" : (L.data_bin_resumed != 0 ? "r" : "n");
          std::tm tm{};
          time64_to_tm(L.time_prev_sample, &tm);
          data_out << field1_ms << ',' << L.counter << ','
                   << (tm.tm_year + 1900) << '-' << (tm.tm_mon + 1) << '-' << tm.tm_mday << ' '
                   << tm.tm_hour << ':' << tm.tm_min << ':' << tm.tm_sec << ",0," << sample_type << ",0\n";
          if (L.data_bin_resumed != 0)
            L.data_bin_resumed = 0;
          /* Trickle-up: same timer/counter/timestamp as data.bin line so validation can match. */
          int n = std::snprintf(L.trickle_scratch, sizeof(L.trickle_scratch),
            "<sample><timer>%u</timer>\n<counter>%u</counter>\n<timestamp>%u-%u-%u %u:%u:%u</timestamp>\n"
            "<sensor_revision_int>%i</sensor_revision_int>\n<sample_type>%s</sample_type>\n<vid_pid_int>%u</vid_pid_int>\n</sample>\n",
            field1_ms, L.counter,
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            0, sample_type, 0u);
          if (n > 0 && n < static_cast<int>(sizeof(L.trickle_scratch))) {
            int send_now = 0;
            if (gmc::TRICKLE_ONE_PER_SAMPLE != 0) {
              send_now = 1;  /* one trickle per sample, same time as record */
            } else {
              L.trickle_pending++;
              int min_pending = L.debug_enabled ? gmc::TRICKLE_MIN_PENDING_DEBUG : gmc::TRICKLE_MIN_PENDING;
              int min_interval_sec = L.debug_enabled ? gmc::TRICKLE_MIN_INTERVAL_SEC_DEBUG : gmc::TRICKLE_MIN_INTERVAL_SEC;
              if (L.trickle_pending > min_pending) {
                __time64_t now = get_time64(nullptr);
                __time64_t elapsed = now - L.last_trickle_send_time;
                if (elapsed >= min_interval_sec) {
                  send_now = 1;
                  L.trickle_pending = 0;
                  L.last_trickle_send_time = now;
                }
              }
            }
            if (send_now) {
              L.trickle_result = boinc_send_trickle_up("rad_report_xml", L.trickle_scratch);
              L.trickle_sent = 1;
            }
          }
        }
        L.elapsed_ticks_prev = L.elapsed_ticks;
        if (L.trickle_sent == 0 && boinc_time_to_checkpoint())
          boinc_checkpoint();
      } else {
        L.read_error_count++;
        if (std::ostream* os = get_debug_stream(); L.debug_enabled != 0 && os != nullptr)
          *os << "Error reading data\n";
        if (L.read_error_count > gmc::READ_ERROR_THRESHOLD) {
          if (L.com_handle != nullptr)
            serial_close(L.com_handle);
          L.com_open_ok = '\0';
          L.com_handle = nullptr;
          if (std::ostream* os = get_debug_stream(); L.debug_enabled != 0 && os != nullptr)
            *os << "Lost sensor, trying to reopen.... \n";
        }
      }
      boinc_end_critical_section();
      boinc_fraction_done(L.fraction_done);
      if (sample_index < L.num_samples) {
        L.time_now = get_time64(nullptr);
        for (unsigned int w = 0; w < L.sample_interval_sec && L.sample_interval_sec > 1; ++w) {
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
