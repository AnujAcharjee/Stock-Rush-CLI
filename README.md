# Stock Rush CLI

**Stock Rush CLI** is a high-performance C++ stock trading simulation engine and interactive terminal user interface (TUI). It features a multi-threaded matching engine built on a thread pool architecture, capable of processing over **180,000 orders/sec** with sub-1.5 microsecond matching latencies.

---

## Features

- **Interactive FTXUI Dashboard**: Side-by-side bid/ask order books, user demat portfolios, ledger transaction history, and real-time statistics.
- **Multi-Threaded Order Matcher**: Asynchronous matching engine powered by a custom worker thread pool.
- **Robust Thread Safety**: Mutex-protected user demat holdings and corrected queue locks preventing priority queue data races.
- **Headless Stress Testing Runner**: Dedicated benchmark executable to test throughput and latency percentiles under extreme loads.
- **HFT-Grade Diagnostics**: Tracks throughput (orders/sec), Trades Generated, Average Latency, Median/P95/P99 latency percentiles, CPU core utilization, and Peak Queue Size.

---

## Prerequisites & Installation

### Requirements:
- A C++23 compatible compiler (e.g., `MSVC 19.30+` on Windows, `g++ 13+` or `clang++ 16+` on Linux/macOS)
- **CMake 3.14+** installed

### Clone the Repository:
```bash
git clone https://github.com/AnujAcharjee/Stock-Rush-CLI.git
cd Stock-Rush-CLI
```

---

## Build Instructions

Compile the application using CMake:

```bash
# 1. Configure the build output (generates build files)
cmake -B build -S .

# 2. Compile the binaries in Release mode
cmake --build build --config Release
```

After successful compilation, two executables will be generated in the build output:
1. `Engine` / `Engine.exe`: The interactive FTXUI trading dashboard.
2. `StressTestRunner` / `StressTestRunner.exe`: The high-performance headless stress test benchmark.

---

## 🖥️ Running the Interactive TUI Dashboard

Start the fullscreen trading terminal:

```bash
# On Windows
./build/Release/Engine.exe

# On Linux / macOS
./build/Release/Engine
```

### Controls & Navigation:
* **Switch Navigation Tabs**: Use the `Left` and `Right` arrow keys while focus is on the top menu.
* **Cycle Control Focus**: Use the `Tab` key to move forward through inputs/buttons, and `Shift+Tab` to cycle backward.
* **Select Stock / Choice Options**: Use the `Up`/`Down` arrow keys and `Enter` to expand and choose from dropdown elements or radioboxes.
* **Input Text**: Focus on input boxes (like Username, Quantity, Price) and type directly.
* **Execute Buttons**: Focus on buttons (like "Submit Order" or "Create User") and press `Enter` or click them.

---

## ⚡ Running the Headless Stress Test Benchmark

Run the dedicated benchmarking tool to stress test the matching engine's limits. Pass the number of concurrent orders to dispatch as an argument (defaults to 1000):

```bash
# On Windows
./build/Release/StressTestRunner.exe 1000

# On Linux / macOS
./build/Release/StressTestRunner 1000
```

### Example Benchmark Output:
```text
=== Running Headless Stress Test with 1000 orders ===
Dispatched 1000 orders in 1 ms.
Processing matches...

Orders Submitted:       1000
Orders Executed:        1000
Trades Generated:       410

Total Runtime:          5.4 ms

Throughput:             186,459 orders/sec
Average Match Latency:  1.4 μs
Median Latency:         1.1 μs
P95 Latency:            2.6 μs
P99 Latency:            5.9 μs

Worker Threads:         4
CPU Utilization:        55%
Peak Queue Size:        148
---------------------------------------------------
=== Stress Test Complete ===
```

### Telemetry Telemetry Breakdown:
- **Orders Submitted / Executed**: Total orders successfully generated and popped through the thread pool worker queues.
- **Trades Generated**: Total matching order crossings triggered.
- **Throughput**: Calculated orders matched and cleared per second.
- **Latency Percentiles (Avg, Median, P95, P99)**: Measured in microseconds (`μs`), showing the tail-latency performance of order matching.
- **CPU Utilization**: Sum of CPU active time across all worker threads relative to total elapsed time.
- **Peak Queue Size**: The maximum height of the priority queue during concurrent multi-threaded order submissions.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

