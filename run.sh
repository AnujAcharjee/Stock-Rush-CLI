#!/usr/bin/env bash
set -e

# Change into repository root directory
cd "$(dirname "$0")"

CMD="${1:-ui}"
COUNT="${2:-1000}"

# Auto-locate CMake (standard Linux /usr/bin/cmake or Windows Visual Studio fallback under Git Bash)
find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return
    fi
    local candidate="/c/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    if [ -f "$candidate" ]; then
        echo "$candidate"
        return
    fi
    candidate="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    if [ -f "$candidate" ]; then
        echo "$candidate"
        return
    fi
    candidate="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    if [ -f "$candidate" ]; then
        echo "$candidate"
        return
    fi
    echo ""
}

CMAKE_CMD="$(find_cmake)"

# Detect binaries (Linux './build/Engine' or Windows './build/Release/Engine.exe')
find_binary() {
    local name="$1"
    if [ -f "build/$name" ]; then
        echo "./build/$name"
    elif [ -f "build/Release/$name.exe" ]; then
        echo "./build/Release/$name.exe"
    elif [ -f "build/$name.exe" ]; then
        echo "./build/$name.exe"
    elif [ -f "$name.exe" ]; then
        echo "./$name.exe"
    else
        echo ""
    fi
}

CORES=$(nproc 2>/dev/null || echo 4)

build_if_needed() {
    local target="$1"
    local bin="$(find_binary "$target")"
    if [ -z "$bin" ]; then
        if [ -z "$CMAKE_CMD" ]; then
            echo "Error: cmake not found. Please install cmake or add it to PATH." >&2
            exit 1
        fi
        echo "==> Binary $target not found. Building with $CORES cores..."
        "$CMAKE_CMD" -B build -DCMAKE_BUILD_TYPE=Release
        "$CMAKE_CMD" --build build --config Release -j "$CORES"
    fi
}

case "$CMD" in
    ui)
        build_if_needed "Engine"
        BIN="$(find_binary "Engine")"
        exec "$BIN"
        ;;
    test)
        build_if_needed "StressTestRunner"
        BIN="$(find_binary "StressTestRunner")"
        exec "$BIN" "$COUNT"
        ;;
    build)
        if [ -z "$CMAKE_CMD" ]; then
            echo "Error: cmake not found in PATH." >&2
            exit 1
        fi
        "$CMAKE_CMD" -B build -DCMAKE_BUILD_TYPE=Release
        "$CMAKE_CMD" --build build --config Release -j "$CORES"
        ;;
    clean)
        rm -rf build
        echo "Cleaned build directory."
        ;;
    *)
        echo "Usage: ./run.sh [ui|test|build|clean] [count]"
        echo "  ./run.sh          -> Run interactive UI"
        echo "  ./run.sh test     -> Run stress test with 1000 orders"
        echo "  ./run.sh test 5000-> Run stress test with 5000 orders"
        echo "  ./run.sh build    -> Compile release binaries"
        echo "  ./run.sh clean    -> Delete build directory"
        exit 1
        ;;
esac
