// Config implementation using TinyXML2. Replaces original XML/config library (FUN_*).
// XML layout reference: docs/boinc-xml-reference.md (e.g. F:\BOINCdata\slots\1\gmc.xml, init_data.xml).
// See docs/phase2-config.md. Requires TinyXML2 (tinyxml2.h / tinyxml2.cpp).

#include "app_io.h"
#include "config.h"
#include "constants.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include "../third_party/tinyxml2/tinyxml2.h"
#if defined(_MSC_VER)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

extern "C" {
extern void boinc_finish_and_exit(int status);
}

namespace {

struct DocOwner {
  std::unique_ptr<tinyxml2::XMLDocument> doc;
};

struct ConfigHandleImpl {
  std::shared_ptr<DocOwner> doc_owner{};
  tinyxml2::XMLElement* element{};
};

using HandleTable = std::vector<std::shared_ptr<ConfigHandleImpl>>;

HandleTable& get_table() {
  static ConfigHandleImpl g_empty{nullptr, nullptr};
  static std::shared_ptr<ConfigHandleImpl> g_empty_ptr(std::shared_ptr<void>(), &g_empty);
  static HandleTable table;
  if (table.empty())
    table.push_back(g_empty_ptr);
  return table;
}

inline ConfigHandleImpl* ptr_from_handle(int* slot) {
  if (slot == nullptr) return nullptr;
  HandleTable& t = get_table();
  const int idx = *slot;
  if (idx < 0 || static_cast<size_t>(idx) >= t.size() || !t[idx])
    return t[0].get();
  return t[idx].get();
}

inline ConfigHandleImpl* empty_handle() {
  return get_table()[0].get();
}

inline void store_handle(int* slot, std::shared_ptr<ConfigHandleImpl> p) {
  if (slot == nullptr) return;
  HandleTable& t = get_table();
  const int old_idx = *slot;
  if (old_idx > 0 && static_cast<size_t>(old_idx) < t.size())
    t[old_idx].reset();
  t.push_back(std::move(p));
  *slot = static_cast<int>(t.size() - 1);
}

void release_slot(int* slot) {
  if (slot == nullptr) return;
  HandleTable& t = get_table();
  const int idx = *slot;
  *slot = 0;
  if (idx > 0 && static_cast<size_t>(idx) < t.size())
    t[idx].reset();
}

bool element_name_equals(const char* a, const char* b) {
  if (!a || !b) return a == b;
  return strcasecmp(a, b) == 0;
}

tinyxml2::XMLElement* find_child_element(tinyxml2::XMLElement* parent, const char* key) {
  if (!parent || !key) return nullptr;
  for (tinyxml2::XMLElement* child = parent->FirstChildElement(); child; child = child->NextSiblingElement()) {
    if (element_name_equals(child->Name(), key))
      return child;
  }
  return nullptr;
}

std::shared_ptr<ConfigHandleImpl> make_handle(std::shared_ptr<DocOwner> owner, tinyxml2::XMLElement* element) {
  auto h = std::make_shared<ConfigHandleImpl>();
  h->doc_owner = std::move(owner);
  h->element = element;
  return h;
}

} // namespace

static int empty_handle_slot = 0;
int* empty_config_ref = &empty_handle_slot;

void ref_assign_inc(void* config_out, int* config_handle) {
  if (config_out == nullptr) return;
  int* dest_slot = static_cast<int*>(config_out);
  HandleTable& t = get_table();
  const int src_idx = (config_handle != nullptr && *config_handle >= 0 && static_cast<size_t>(*config_handle) < t.size() && t[*config_handle])
      ? *config_handle
      : 0;
  const int old_idx = (dest_slot != nullptr && *dest_slot > 0 && static_cast<size_t>(*dest_slot) < t.size()) ? *dest_slot : -1;
  if (old_idx > 0)
    t[old_idx].reset();
  t.push_back(t[src_idx]);
  *dest_slot = static_cast<int>(t.size() - 1);
}

void release_config_value(int* config_handle) {
  release_slot(config_handle);
}

int config_string_empty(int* config_handle) {
  if (config_handle == nullptr) return 1;
  ConfigHandleImpl* h = ptr_from_handle(config_handle);
  if (!h || h == empty_handle() || !h->element) return 1;
  const char* text = h->element->GetText();
  return (text == nullptr || *text == '\0') ? 1 : 0;
}

char* get_config_cstr(int* config_handle, int /*index*/) {
  if (config_handle == nullptr) return nullptr;
  ConfigHandleImpl* h = ptr_from_handle(config_handle);
  if (!h || h == empty_handle() || !h->element) return nullptr;
  const char* text = h->element->GetText();
  return text ? const_cast<char*>(text) : nullptr;
}

int* get_config_value(int* config_handle, int* out_node, const char* key, int* /*options*/) {
  ConfigHandleImpl* parent = ptr_from_handle(config_handle);
  if (!parent || !out_node || !key) return nullptr;
  if (parent == empty_handle() || !parent->element) {
    store_handle(out_node, get_table()[0]);
    return out_node;
  }
  tinyxml2::XMLElement* child = find_child_element(parent->element, key);
  if (!child) {
    store_handle(out_node, get_table()[0]);
    return out_node;
  }
  store_handle(out_node, make_handle(parent->doc_owner, child));
  return out_node;
}

void get_config_int(int* config_handle, int* node) {
  if (config_handle == nullptr || node == nullptr) return;
  HandleTable& t = get_table();
  const int src_idx = (*node >= 0 && static_cast<size_t>(*node) < t.size() && t[*node]) ? *node : 0;
  const int old_idx = (*config_handle > 0 && static_cast<size_t>(*config_handle) < t.size()) ? *config_handle : -1;
  if (old_idx > 0)
    t[old_idx].reset();
  t.push_back(t[src_idx]);
  *config_handle = static_cast<int>(t.size() - 1);
}

// Build document from project_preferences inner XML using TinyXML2 (parse then re-serialize).
const char* build_project_preferences_document(const char* inner_xml, char* out_buf, size_t out_buf_size) {
  if (inner_xml == nullptr || out_buf == nullptr || out_buf_size < 3) return nullptr;
  char const prefix[] = "<project_preferences>\n";
  char const suffix[] = "\n</project_preferences>";
  size_t prefix_len = sizeof(prefix) - 1;
  size_t suffix_len = sizeof(suffix) - 1;
  size_t inner_len = std::strlen(inner_xml);
  if (prefix_len + inner_len + suffix_len >= out_buf_size) return nullptr;
  std::vector<char> wrap(prefix_len + inner_len + suffix_len + 1);
  std::memcpy(wrap.data(), prefix, prefix_len);
  std::memcpy(wrap.data() + prefix_len, inner_xml, inner_len + 1);
  std::memcpy(wrap.data() + prefix_len + inner_len, suffix, suffix_len + 1);

  tinyxml2::XMLDocument doc;
  if (doc.Parse(wrap.data()) != tinyxml2::XML_SUCCESS)
    return nullptr;
  tinyxml2::XMLPrinter printer(/*file*/ nullptr, /*compact*/ false);
  doc.Print(&printer);
  const char* result = printer.CStr();
  if (!result) return nullptr;
  size_t len = std::strlen(result);
  if (len >= out_buf_size) return nullptr;
  std::memcpy(out_buf, result, len + 1);
  return out_buf;
}

// init_data XML: root <app_init_data>; section "project_preferences" has <radacdebug>, <runtime>, etc.
void* read_project_preferences(void* config_out, const void* init_data, const char* section_name, int* options) {
  if (config_out == nullptr) return nullptr;
  int* dest_slot = static_cast<int*>(config_out);
  if (options != nullptr) {
    options[0] = 0;
    options[1] = 0;
    options[2] = 0;
  }

  if (init_data == nullptr) {
    if (options) options[0] = 9;
    ref_assign_inc(config_out, empty_config_ref);
    return config_out;
  }

  const char* xml = static_cast<const char*>(init_data);  // read-only; buffer is not modified
  auto doc = std::make_unique<tinyxml2::XMLDocument>();
  if (doc->Parse(xml) != tinyxml2::XML_SUCCESS) {
    if (options) options[0] = 0xb;
    ref_assign_inc(config_out, empty_config_ref);
    return config_out;
  }

  tinyxml2::XMLElement* root = doc->FirstChildElement();
  if (!root) {
    ref_assign_inc(config_out, empty_config_ref);
    return config_out;
  }

  tinyxml2::XMLElement* section = nullptr;
  if (section_name && *section_name != '\0') {
    if (element_name_equals(root->Name(), section_name))
      section = root;
    else
      section = find_child_element(root, section_name);
  } else {
    section = root;
  }

  if (!section) {
    ref_assign_inc(config_out, empty_config_ref);
    return config_out;
  }

  auto owner = std::make_shared<DocOwner>();
  owner->doc = std::move(doc);
  int temp_slot = 0;
  store_handle(&temp_slot, make_handle(std::move(owner), section));
  ref_assign_inc(config_out, &temp_slot);
  release_slot(&temp_slot);
  return config_out;
}

// Optional child element text as int; default_value if missing/invalid or out of [min_val, max_val].
static int optional_int(tinyxml2::XMLElement* parent, const char* name, int default_value, int min_val, int max_val) {
  tinyxml2::XMLElement* el = find_child_element(parent, name);
  if (!el) return default_value;
  const char* text = el->GetText();
  if (!text || *text == '\0') return default_value;
  long v = std::strtol(text, nullptr, 10);
  if (v < min_val || v > max_val) return default_value;
  return static_cast<int>(v);
}

// Parity: "n"/"N"=0, "o"/"O"=1, "e"/"E"=2; else default_value.
static int parse_parity(tinyxml2::XMLElement* parent, int default_value) {
  tinyxml2::XMLElement* el = find_child_element(parent, "parity");
  if (!el) return default_value;
  const char* text = el->GetText();
  if (!text || *text == '\0') return default_value;
  char c = static_cast<char>(*text);
  if (c == 'n' || c == 'N') return 0;
  if (c == 'o' || c == 'O') return 1;
  if (c == 'e' || c == 'E') return 2;
  return default_value;
}

// gmc.xml <gmc><comsettings>: portnumber (required), baud, bits, parity, stopbits. One load.
void get_gmc_com_settings(struct gmc_com_settings* out) {
  if (out == nullptr) return;
  out->port_number = 0;
  out->baud = gmc::COM_BAUD_DEFAULT;
  out->bits = gmc::COM_BITS_DEFAULT;
  out->parity = gmc::COM_PARITY_DEFAULT;
  out->stopbits = gmc::COM_STOPBITS_DEFAULT;

  tinyxml2::XMLDocument doc;
  if (doc.LoadFile("gmc.xml") != tinyxml2::XML_SUCCESS) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_ReadPortNumber(): could not find gmc.xml or the file is corrupted\n";
    boinc_finish_and_exit(1);
  }

  tinyxml2::XMLElement* gmc = doc.FirstChildElement("gmc");
  if (!gmc) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_ReadPortNumber(): could not find gmc.xml or the file is corrupted\n";
    boinc_finish_and_exit(1);
  }

  tinyxml2::XMLElement* comsettings = find_child_element(gmc, "comsettings");
  if (!comsettings) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_ReadPortNumber(): <comsettings> node empty, file corrupted ?\n";
    boinc_finish_and_exit(1);
  }

  tinyxml2::XMLElement* portnumber = find_child_element(comsettings, "portnumber");
  if (!portnumber) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_ReadPortNumber(): <portnumber> node empty, file corrupted ?\n";
    boinc_finish_and_exit(1);
  }
  const char* port_str = portnumber->GetText();
  if (!port_str || *port_str == '\0') {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_ReadPortNumber(): <portnumber> node empty, file corrupted ?\n";
    boinc_finish_and_exit(1);
  }
  int port_number = static_cast<int>(std::strtol(port_str, nullptr, 10));
  if (port_number < gmc::COM_PORT_MIN || port_number > gmc::COM_PORT_MAX) {
    if (std::ostream* os = get_debug_stream())
      *os << "gmc_ReadPortNumber(): Wrong port number\n";
    boinc_finish_and_exit(1);
  }
  out->port_number = port_number;

  out->baud = static_cast<unsigned int>(optional_int(comsettings, "baud", static_cast<int>(gmc::COM_BAUD_DEFAULT), 1, 921600));
  out->bits = optional_int(comsettings, "bits", gmc::COM_BITS_DEFAULT, 5, 8);
  out->parity = parse_parity(comsettings, gmc::COM_PARITY_DEFAULT);
  out->stopbits = optional_int(comsettings, "stopbits", gmc::COM_STOPBITS_DEFAULT, 1, 2);
}

int get_com_port_number() {
  struct gmc_com_settings s;
  get_gmc_com_settings(&s);
  return s.port_number;
}

unsigned int get_com_baud_rate() {
  struct gmc_com_settings s;
  get_gmc_com_settings(&s);
  return s.baud;
}
