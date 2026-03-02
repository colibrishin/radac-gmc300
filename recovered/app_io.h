// C++ I/O API for recovered app: debug stream and data.bin.
// Replaces C stdio (get_debug_file, fopen_data_bin / FILE*).

#ifndef GMC_RECOVERED_APP_IO_H
#define GMC_RECOVERED_APP_IO_H

#include <fstream>
#include <iostream>

/** Debug log stream (e.g. stderr). Always returns a valid reference. */
std::ostream& get_debug_stream();

/** Debug stream if enabled, otherwise a no-op stream that discards output. */
std::ostream& get_debug_stream_if(bool enabled);

/** Open data.bin for reading (resume / line count). */
std::ifstream open_data_bin_for_resume();

/** Open data.bin for appending (one line per sample). */
std::ofstream open_data_bin_append();

#endif
