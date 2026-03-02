// Config API for project_preferences (init_data XML) and gmc.xml (COM port).
// Implemented with TinyXML2 in config.cpp.

#ifndef GMC_RECOVERED_CONFIG_H
#define GMC_RECOVERED_CONFIG_H

// Opaque config handle (stored as pointer in int* slot).
// Use ref_assign_inc / release_config_value to manage lifetime.

extern int* empty_config_ref;
void ref_assign_inc(void* config_out, int* config_handle);
void release_config_value(int* config_handle);
int config_string_empty(int* config_handle);
char* get_config_cstr(int* config_handle, int index);
int* get_config_value(int* config_handle, int* out_node, const char* key, int* options);
void get_config_int(int* config_handle, int* node);

// Read section (e.g. "project_preferences" or "gmc") from XML buffer or null for empty.
void* read_project_preferences(void* config_out, const void* init_data, const char* section_name, int* options);

// Build full XML document from project_preferences inner content (e.g. APP_INIT_DATA::project_preferences).
// Writes "<project_preferences>\n" + inner_xml + "\n</project_preferences>" into out_buf. Returns out_buf or nullptr if too small.
const char* build_project_preferences_document(const char* inner_xml, char* out_buf, size_t out_buf_size);

// All gmc.xml <comsettings> in one struct (single file read).
struct gmc_com_settings {
  int port_number;       // 0-99, required (0 = first port)
  unsigned int baud;     // default COM_BAUD_DEFAULT
  int bits;              // 5-8, default COM_BITS_DEFAULT
  int parity;            // 0=none, 1=odd, 2=even, default COM_PARITY_DEFAULT
  int stopbits;          // 1 or 2, default COM_STOPBITS_DEFAULT
};

// Load gmc.xml once, fill all comsettings; exits on missing/invalid port.
void get_gmc_com_settings(struct gmc_com_settings* out);

// Convenience: return single field (loads gmc.xml each call).
int get_com_port_number(void);
unsigned int get_com_baud_rate(void);

#endif
