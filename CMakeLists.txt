cmake_minimum_required(VERSION 3.21)
project(bearpaw-sane VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(SANE sane-backends)   # optional: the backend builds if present

# ---------- core (transport + session + decoder) ----------
add_library(bearpawcore STATIC
    core/bearpaw.h
    core/bearpaw.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/session_ops.o
)
target_link_libraries(bearpawcore PUBLIC PkgConfig::LIBUSB)
target_include_directories(bearpawcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)

# the recorded session is embedded into the library as an object (ld -b binary):
# the backend becomes a single self-contained .so with no external files
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/session_ops.o
    COMMAND ld -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/session_ops.o session.ops
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/session.ops
    VERBATIM)

# ---------- console scanner (SANE-free testing) ----------
add_executable(bptestcli testcli/main.cpp)
target_link_libraries(bptestcli PRIVATE bearpawcore)

# ---------- SANE backend ----------
if(SANE_FOUND)
    add_library(sane-bearpaw2448 MODULE backend/bearpaw2448.cpp)
    set_target_properties(sane-bearpaw2448 PROPERTIES
        PREFIX "libsane-"
        OUTPUT_NAME "bearpaw2448"
        SUFFIX ".so.1")
    target_link_libraries(sane-bearpaw2448 PRIVATE bearpawcore)
    target_include_directories(sane-bearpaw2448 PRIVATE ${SANE_INCLUDE_DIRS})
    install(TARGETS sane-bearpaw2448 LIBRARY DESTINATION lib)
    message(STATUS "SANE backend: enabled")
else()
    message(STATUS "SANE headers not found - building without the backend (bptestcli only)")
endif()
