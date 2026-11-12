#!/usr/bin/env bash
# stockRush runner for Linux and macOS
set -e

# Always run from repository root
cd "$(dirname "$0")"

CMD="${1:-ui}"
COUNT="${2:-1000}"

# Auto-detect CPU cores for fast parallel compilation
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

build_target() {
    local target="$1"
    echo "==> Configuring and building $target with $CORES cores..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target "$target" -j "$CORES"
}

case "$CMD" in
    ui)
        if [ ! -f "build/Engine" ]; then
            build_target "Engine"
        fi
        echo "==> Starting stockRush Interactive Dashboard..."
        exec ./build/Engine
        ;;
    test)
        if [ ! -f "build/StressTestRunner" ]; then
            build_target "StressTestRunner"
        fi
        echo "==> Running stress test with $COUNT orders..."
        exec ./build/StressTestRunner "$COUNT"
        ;;
    build)
        echo "==> Building all targets in Release mode..."
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build --config Release -j "$CORES"
        ;;
    clean)
        rm -rf build
        echo "Cleaned build directory."
        ;;
    *)
        echo "Usage: ./run.sh [ui|test|build|clean] [count]"
        echo "  ./run.sh          -> Launch interactive UI (builds if missing)"
        echo "  ./run.sh test     -> Run stress test with 1000 orders"
        echo "  ./run.sh test 5000-> Run stress test with 5000 orders"
        echo "  ./run.sh build    -> Build Release binaries"
        echo "  ./run.sh clean    -> Delete build directory"
        exit 1
        ;;
esac
