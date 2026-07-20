# Stock Rush CLI

**Stock Rush CLI** is a command-line-based stock trading simulation game where users can place buy and sell orders, manage stock portfolios, and view order books in real time. It simulates the functionality of a stock market using simple commands for creating users, placing orders, and viewing stock-related data.

## Features

- Add users to the system
- Place buy or sell orders for available stocks
- View the available stocks with their prices
- View the order book with buy and sell orders
- View all placed orders, executed orders, and registered users
- Performance statistics tracking (orders/sec, matches/sec, thread pool activity, average latency)

### Prerequisites & Setup
- Clone the repository:
  ```bash
  git clone https://github.com/AnujAcharjee/Stock-Rush-CLI.git
  cd Stock-Rush-CLI
  ```
- Requirements:
  - A C++23 compatible compiler (e.g., `MSVC 19.30+`, `g++ 13+`, `clang++ 16+`)
  - **CMake 3.14+** installed
- Compile the engine using CMake:
  ```bash
  # 1. Configure the build output (generates build folders)
  cmake -B build -S .

  # 2. Compile the application
  cmake --build build --config Release
  ```
- After successful compilation, the executable will be available in the build directory.
- Run the application via your terminal:
  ```bash
  # On Windows
  ./build/Release/Engine.exe

  # On Linux / macOS
  ./build/Engine
  ```

### Usage & Navigation
When you start the application, it launches in fullscreen terminal user interface (TUI) mode.

### Controls:
*   **Switch Navigation Tabs**: Use the `Left` and `Right` arrow keys while focus is on the top menu bar.
*   **Cycle Control Focus**: Use the `Tab` key to move forward through forms, dropdowns, and buttons. Use `Shift+Tab` to cycle backward.
*   **Select Options**: Use the `Up` and `Down` arrow keys to change choices in dropdowns or radio lists, and press `Enter` to expand or select.
*   **Input Text**: Focus on input boxes (like Username, Quantity, Price) and type directly.
*   **Execute Buttons**: Focus on buttons (like "Submit Order" or "Trigger Stress Test") and press `Enter` or click them.

---

### Dashboard Tabs Guide

1.  **Dashboard Tab**:
    *   **Stock Directory**: Lists all available stocks, current market prices, and total available share quantities.
    *   **User Portfolios**: Scroll through the list of users (`User1` to `User5` or any custom users) to instantly view their current cash balances and demat holdings.
2.  **Live Order Books Tab**:
    *   Select any stock ticker (TCS, APPLE, BABA, GOOGL) from the dropdown filter to view its live, side-by-side **Bid (Buy)** vs. **Ask (Sell)** order queues.
3.  **Place Order Tab**:
    *   **Form**: Place Limit or Market buy/sell orders. Input a username, select the stock, quantity, and price.
    *   **Register Account**: Register a new trading username with `10,000` Rs start-up funds.
4.  **Ledger & Logs Tab**:
    *   **All Placed Orders**: A running list of all orders submitted to the matching engine, showing their pending/executed quantities.
    *   **All Executed Trades**: Real-time transaction history showing the order ID, stock symbol, matched quantity, and transaction timestamp.
5.  **Diagnostics Tab**:
    *   **Metrics Panel**: Displays live throughput stats:
        *   **Orders Processed/sec**: Processing rate of the execution queues.
        *   **Average Matches/sec**: Trade execution rate.
        *   **Active Worker Threads**: Current active pool threads matching orders.
        *   **Avg Match Latency**: Average time spent running matching logic (in milliseconds).
    *   **Stress Testing**: Click "Trigger 100-Order Stress Test" to launch 100 concurrent threads placing random orders simultaneously. Watch the diagnostic metrics update in real time.
8. View Orders Placed

# TODO: UI GIF to be added

## Key Commands & Options
1) Add User: Creates a new user in the system.

2) Add Order: Allows placing a new buy or sell order for available stocks.

3) View Stocks: Displays the available stocks with their current prices.

4) View Order Book: Shows buy and sell orders for a specific stock.

5) View Users: Lists all registered users along with their funds and demat holdings.

6) View Orders: Displays all orders placed by users, whether executed or pending.

7) Executed Orders: Lists all orders that have been successfully executed.

8) Exit: Closes the application.

9) Performance Metrics: Displays execution statistics including:
   * **Orders Processed/sec**: Rate of processing order requests from the execution queue.
   * **Average Matches/sec**: Rate of matching buy and sell orders.
   * **Active Worker Threads**: Current count of pool threads actively running.
   * **Avg Match Latency**: Average time spent executing order matching logic (in milliseconds).
