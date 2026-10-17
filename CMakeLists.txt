cmake_minimum_required(VERSION 3.20)
project(nv_research LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "nv_research only supports Windows")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
endif()

add_library(nv_research_observer STATIC src/observer.cpp src/output.cpp)
target_include_directories(nv_research_observer PUBLIC include)
target_compile_definitions(nv_research_observer PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(nv_research_observer PUBLIC advapi32 wtsapi32)

add_executable(nv_research src/main.cpp)
target_link_libraries(nv_research PRIVATE nv_research_observer)

enable_testing()
add_executable(nv_research_tests tests/observer_tests.cpp)
target_link_libraries(nv_research_tests PRIVATE nv_research_observer)
add_test(NAME observer_tests COMMAND nv_research_tests)

add_executable(nvovr_library_smoke tests/library_smoke.cpp tests/library_second_tu.cpp)
target_include_directories(nvovr_library_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} include)
add_test(NAME library_smoke COMMAND nvovr_library_smoke)
