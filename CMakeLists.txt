cmake_minimum_required(VERSION 3.16)
project(gmc_recovered LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Standalone (stubs) vs BOINC-linked build. Default: standalone for testing.
option(GMC_USE_BOINC "Link against BOINC library instead of using stubs" OFF)

# Asio: use git submodule (third_party/asio) for serial port
set(ASIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio")
if(NOT EXISTS "${ASIO_DIR}/include/boost/asio.hpp")
  message(FATAL_ERROR "Asio not found. Init submodule: git submodule update --init third_party/asio")
endif()
# Boost.System: use git submodule (third_party/system) - header-only
set(SYSTEM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/system")
if(NOT EXISTS "${SYSTEM_DIR}/include/boost/system/error_code.hpp")
  message(FATAL_ERROR "Boost.System not found. Init submodule: git submodule update --init third_party/system")
endif()
# Boost.WinAPI + Predef: only required on Windows (Boost.System uses them for Windows error handling)
if(WIN32)
  set(WINAPI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/winapi")
  if(NOT EXISTS "${WINAPI_DIR}/include/boost/winapi/error_handling.hpp")
    message(FATAL_ERROR "Boost.WinAPI not found (Windows build). Init submodule: git submodule update --init third_party/winapi")
  endif()
  set(PREDEF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/predef")
  if(NOT EXISTS "${PREDEF_DIR}/include/boost/predef/version_number.h")
    message(FATAL_ERROR "Boost.Predef not found (Windows build). Init submodule: git submodule update --init third_party/predef")
  endif()
endif()
# Boost.Config: use git submodule (third_party/config) for boost/config.hpp
set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/config")
if(NOT EXISTS "${CONFIG_DIR}/include/boost/config.hpp")
  message(FATAL_ERROR "Boost.Config not found. Init submodule: git submodule update --init third_party/config")
endif()
# Boost.Assert: use git submodule (third_party/assert) for boost/assert.hpp
set(ASSERT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/assert")
if(NOT EXISTS "${ASSERT_DIR}/include/boost/assert.hpp")
  message(FATAL_ERROR "Boost.Assert not found. Init submodule: git submodule update --init third_party/assert")
endif()
# Boost.ThrowException: use git submodule (third_party/throw_exception) for boost/throw_exception.hpp
set(THROW_EXCEPTION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/throw_exception")
if(NOT EXISTS "${THROW_EXCEPTION_DIR}/include/boost/throw_exception.hpp")
  message(FATAL_ERROR "Boost.ThrowException not found. Init submodule: git submodule update --init third_party/throw_exception")
endif()
# Boost.Align: use git submodule (third_party/align) for boost/align/
set(ALIGN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/align")
if(NOT EXISTS "${ALIGN_DIR}/include/boost/align/aligned_alloc.hpp")
  message(FATAL_ERROR "Boost.Align not found. Init submodule: git submodule update --init third_party/align")
endif()
# TinyXML2: submodule at third_party/tinyxml2, or set TINYXML2_DIR
if(NOT DEFINED TINYXML2_DIR)
  set(TINYXML2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyxml2")
endif()
if(NOT EXISTS "${TINYXML2_DIR}/tinyxml2.cpp")
  message(FATAL_ERROR "TinyXML2 not found. Run: git submodule update --init --recursive (or set TINYXML2_DIR).")
endif()

set(RECOVERED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recovered")

# Base sources (always built)
set(GMC_SOURCES
  "${RECOVERED_DIR}/main.cpp"
  "${RECOVERED_DIR}/main_app.cpp"
  "${RECOVERED_DIR}/config.cpp"
  "${RECOVERED_DIR}/com_port.cpp"
  "${RECOVERED_DIR}/detector.cpp"
  "${RECOVERED_DIR}/serial_port.cpp"
  "${RECOVERED_DIR}/commands.cpp"
  "${TINYXML2_DIR}/tinyxml2.cpp"
)

# Standalone: add stubs. BOINC: add boinc_wrapper (globals + file/time) and link libboincapi + libboinc (boinc_*).
# Note: boinc_api.cpp (boinc_finish, boinc_fraction_done, etc.) is in libboincapi; libboinc has the rest of the client lib.
if(GMC_USE_BOINC)
  message(STATUS "GMC: BOINC-linked build (boinc_wrapper + libboincapi + libboinc)")
  list(APPEND GMC_SOURCES "${RECOVERED_DIR}/boinc_wrapper.cpp")
  if(NOT DEFINED BOINC_DIR)
    set(BOINC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boinc")
  elseif(NOT EXISTS "${BOINC_DIR}/api")
    set(BOINC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boinc" CACHE PATH "BOINC source directory" FORCE)
  endif()
  if(NOT DEFINED BOINC_INCLUDE_DIR AND NOT DEFINED BOINC_LIB)
    if(NOT EXISTS "${BOINC_DIR}/api")
      message(FATAL_ERROR "GMC_USE_BOINC=ON: BOINC not found at ${BOINC_DIR}. Init submodule: git submodule update --init third_party/boinc. Then build BOINC and set BOINC_DIR if needed.")
    endif()
  endif()
  if(DEFINED BOINC_DIR)
    set(BOINC_INCLUDE_DIR "${BOINC_DIR}/api" "${BOINC_DIR}/lib")
    if(NOT DEFINED BOINC_LIB)
      if(EXISTS "${BOINC_DIR}/lib/Release/libboinc.lib")
        set(BOINC_LIB "${BOINC_DIR}/lib/Release/libboinc.lib")
      elseif(EXISTS "${BOINC_DIR}/lib/Debug/libboinc.lib")
        set(BOINC_LIB "${BOINC_DIR}/lib/Debug/libboinc.lib")
      elseif(EXISTS "${BOINC_DIR}/win_build/Build/x64/Release/libboinc.lib")
        set(BOINC_LIB "${BOINC_DIR}/win_build/Build/x64/Release/libboinc.lib")
      elseif(EXISTS "${BOINC_DIR}/win_build/Build/x64/Debug/libboinc.lib")
        set(BOINC_LIB "${BOINC_DIR}/win_build/Build/x64/Debug/libboinc.lib")
      elseif(EXISTS "${BOINC_DIR}/win_build/Build/x64/Release/boinc.lib")
        set(BOINC_LIB "${BOINC_DIR}/win_build/Build/x64/Release/boinc.lib")
      elseif(EXISTS "${BOINC_DIR}/win_build/Build/x64/Debug/boinc.lib")
        set(BOINC_LIB "${BOINC_DIR}/win_build/Build/x64/Debug/boinc.lib")
      elseif(EXISTS "${BOINC_DIR}/lib/libboinc.a")
        set(BOINC_LIB "${BOINC_DIR}/lib/libboinc.a")
      else()
        message(FATAL_ERROR "BOINC library not found. Build BOINC first: open third_party/boinc/win_build/boinc.sln (Windows) or build third_party/boinc/lib with CMake (Unix). Then set BOINC_DIR if not using third_party/boinc, or set BOINC_LIB to the path of libboinc.lib / libboinc.a.")
      endif()
    endif()
    # libboincapi contains boinc_api.cpp (boinc_finish, boinc_fraction_done, etc.); same dir as libboinc on Windows.
    if(NOT DEFINED BOINC_API_LIB AND BOINC_LIB)
      get_filename_component(BOINC_LIB_DIR "${BOINC_LIB}" DIRECTORY)
      get_filename_component(BOINC_LIB_EXT "${BOINC_LIB}" EXT)
      if(BOINC_LIB_EXT STREQUAL ".lib" AND EXISTS "${BOINC_LIB_DIR}/libboincapi.lib")
        set(BOINC_API_LIB "${BOINC_LIB_DIR}/libboincapi.lib")
      elseif(BOINC_LIB_EXT STREQUAL ".a" AND EXISTS "${BOINC_DIR}/lib/libboincapi.a")
        set(BOINC_API_LIB "${BOINC_DIR}/lib/libboincapi.a")
      endif()
    endif()
  endif()
  if(GMC_USE_BOINC AND NOT EXISTS "${BOINC_LIB}")
    message(FATAL_ERROR "BOINC_LIB does not exist: ${BOINC_LIB}. Build BOINC or set BOINC_LIB to a valid path.")
  endif()
  if(DEFINED BOINC_API_LIB AND NOT EXISTS "${BOINC_API_LIB}")
    message(FATAL_ERROR "BOINC_API_LIB does not exist: ${BOINC_API_LIB}. Build the libboincapi project in boinc/win_build.")
  endif()
  message(STATUS "BOINC_LIB=${BOINC_LIB}")
  if(DEFINED BOINC_API_LIB)
    message(STATUS "BOINC_API_LIB=${BOINC_API_LIB}")
  endif()
else()
  message(STATUS "GMC: Standalone build (stubs)")
  list(APPEND GMC_SOURCES "${RECOVERED_DIR}/stubs_standalone.cpp")
endif()

add_executable(gmc_recovered ${GMC_SOURCES})

target_include_directories(gmc_recovered PRIVATE
  "${RECOVERED_DIR}"
  "${TINYXML2_DIR}"
  "${CONFIG_DIR}/include"
  "${ASSERT_DIR}/include"
  "${THROW_EXCEPTION_DIR}/include"
  "${ALIGN_DIR}/include"
  "${ASIO_DIR}/include"
  "${SYSTEM_DIR}/include"
)
if(WIN32)
  target_include_directories(gmc_recovered PRIVATE "${PREDEF_DIR}/include" "${WINAPI_DIR}/include")
endif()
# Linux/macOS: Asio io_context needs pthread
if(UNIX AND NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(gmc_recovered PRIVATE Threads::Threads)
endif()
# Windows: target Windows 7+ so Asio/WinAPI headers are satisfied; link Winsock (WSAStartup/WSACleanup)
if(MSVC OR (WIN32 AND (MINGW OR CYGWIN)))
  target_compile_definitions(gmc_recovered PRIVATE _WIN32_WINNT=0x0601)
  target_link_libraries(gmc_recovered PRIVATE ws2_32)
endif()

if(GMC_USE_BOINC)
  target_include_directories(gmc_recovered PRIVATE ${BOINC_INCLUDE_DIR})
  target_compile_definitions(gmc_recovered PRIVATE GMC_USE_BOINC=1)
  # Link order: libboincapi first (it depends on libboinc); libboinc second so the linker resolves api's undefined refs.
  if(DEFINED BOINC_API_LIB)
    target_link_libraries(gmc_recovered PRIVATE ${BOINC_API_LIB} ${BOINC_LIB})
  else()
    target_link_libraries(gmc_recovered PRIVATE ${BOINC_LIB})
  endif()
  # Match prebuilt BOINC libs (built as Release with /MT and _ITERATOR_DEBUG_LEVEL=0).
  if(MSVC)
    set_target_properties(gmc_recovered PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_definitions(gmc_recovered PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    message(STATUS "GMC: BOINC build: using static CRT (/MT) and _ITERATOR_DEBUG_LEVEL=0 to match prebuilt BOINC libs. Prefer --config Release.")
  endif()
endif()
