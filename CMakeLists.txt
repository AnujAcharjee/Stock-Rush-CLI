cmake_minimum_required(VERSION 3.14)
project(StockRushCLI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch FTXUI library from GitHub
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Find thread support
find_package(Threads REQUIRED)

# Executable source files
file(GLOB SOURCES "cppsrc/*.cpp")

add_executable(Engine ${SOURCES})

target_compile_features(Engine PRIVATE cxx_std_23)
if (MSVC)
  target_compile_options(Engine PRIVATE /W3)
else()
  target_compile_options(Engine PRIVATE -Wall -Wextra -pthread)
endif()

# Enable performance metrics by default
option(ENABLE_METRICS "Enable performance metrics collection" ON)
if (ENABLE_METRICS)
  target_compile_definitions(Engine PRIVATE ENABLE_METRICS)
endif()

# Link FTXUI libraries and thread library
target_link_libraries(Engine PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
  Threads::Threads
)
