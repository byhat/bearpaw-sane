cmake_minimum_required(VERSION 3.21)
project(bearpaw-sane VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(SANE sane-backends)   # опционально: бэкенд собирается, если есть

# ---------- ядро (транспорт + сессия + декодер) ----------
add_library(bearpawcore STATIC
    core/bearpaw.h
    core/bearpaw.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/session_ops.o
)
target_link_libraries(bearpawcore PUBLIC PkgConfig::LIBUSB)
target_include_directories(bearpawcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)

# эталонная сессия вшивается в библиотеку объектником (ld -b binary):
# бэкенд становится одним самодостаточным .so без внешних файлов
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/session_ops.o
    COMMAND ld -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/session_ops.o session.ops
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/session.ops
    VERBATIM)

# ---------- консольный сканер (тесты без SANE) ----------
add_executable(bptestcli testcli/main.cpp)
target_link_libraries(bptestcli PRIVATE bearpawcore)

# ---------- SANE-бэкенд ----------
if(SANE_FOUND)
    add_library(sane-bearpaw2448 MODULE backend/bearpaw2448.cpp)
    set_target_properties(sane-bearpaw2448 PROPERTIES
        PREFIX "libsane-"
        OUTPUT_NAME "bearpaw2448"
        SUFFIX ".so.1")
    target_link_libraries(sane-bearpaw2448 PRIVATE bearpawcore)
    target_include_directories(sane-bearpaw2448 PRIVATE ${SANE_INCLUDE_DIRS})
    install(TARGETS sane-bearpaw2448 LIBRARY DESTINATION lib)
    message(STATUS "SANE-бэкенд: включён")
else()
    message(STATUS "SANE-заголовки не найдены — собираю без бэкенда (только bptestcli)")
endif()
