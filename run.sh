#!/usr/bin/env bash
set -e

# Change into repository root directory
cd "$(dirname "$0")"

CMD="${1:-ui}"
COUNT="${2:-1000}"

# Auto-detect available cores for fast parallel compilation
CORES=$(nproc 2>/dev/null || echo 4)

build_if_needed() {
    local target="$1"
    if [ ! -f "build/$target" ]; then
        echo "==> Binary build/$target not found. Building with $CORES cores..."
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build --config Release -j "$CORES"
    fi
}

case "$CMD" in
    ui)
        build_if_needed "Engine"
        exec ./build/Engine
        ;;
    test)
        build_if_needed "StressTestRunner"
        exec ./build/StressTestRunner "$COUNT"
        ;;
    build)
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build --config Release -j "$CORES"
        ;;
    clean)
        rm -rf build
        echo "Cleaned build directory."
        ;;
    *)
        echo "Usage: ./run.sh [ui|test|build|clean] [count]"
        echo "  ./run.sh          -> Run interactive UI (auto-builds if needed)"
        echo "  ./run.sh test     -> Run stress test with 1000 orders"
        echo "  ./run.sh test 5000-> Run stress test with 5000 orders"
        echo "  ./run.sh build    -> Compile release binaries"
        echo "  ./run.sh clean    -> Delete build directory"
        exit 1
        ;;
esac
