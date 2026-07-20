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
file(GLOB ALL_SOURCES "cppsrc/*.cpp")

# Exclude mains and TUI files from shared core sources
set(CORE_SOURCES ${ALL_SOURCES})
list(REMOVE_ITEM CORE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/cppsrc/main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/cppsrc/stress_test_main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/cppsrc/gui.cpp"
)

# 1. Engine Executable (Interactive TUI Dashboard)
add_executable(Engine cppsrc/main.cpp cppsrc/gui.cpp ${CORE_SOURCES})
target_compile_features(Engine PRIVATE cxx_std_23)
if (MSVC)
  target_compile_options(Engine PRIVATE /W3)
else()
  target_compile_options(Engine PRIVATE -Wall -Wextra -pthread)
endif()

# 2. StressTestRunner Executable (Headless Stats Runner)
add_executable(StressTestRunner cppsrc/stress_test_main.cpp ${CORE_SOURCES})
target_compile_features(StressTestRunner PRIVATE cxx_std_23)
if (MSVC)
  target_compile_options(StressTestRunner PRIVATE /W3)
else()
  target_compile_options(StressTestRunner PRIVATE -Wall -Wextra -pthread)
endif()

# Enable performance metrics by default
option(ENABLE_METRICS "Enable performance metrics collection" ON)
if (ENABLE_METRICS)
  target_compile_definitions(Engine PRIVATE ENABLE_METRICS)
  target_compile_definitions(StressTestRunner PRIVATE ENABLE_METRICS)
endif()

# Link FTXUI and Threads to Engine
target_link_libraries(Engine PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
  Threads::Threads
)

# Link Threads to StressTestRunner (No TUI libraries needed)
target_link_libraries(StressTestRunner PRIVATE
  Threads::Threads
)
