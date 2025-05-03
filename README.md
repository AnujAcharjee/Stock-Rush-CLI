# CLI-StockTrader

**CLI-StockTrader** is a command-line-based stock trading simulation game where users can place buy and sell orders, manage stock portfolios, and view order books in real time. It simulates the functionality of a stock market using simple commands for creating users, placing orders, and viewing stock-related data.

## Features

- Add users to the system
- Place buy or sell orders for available stocks
- View the available stocks with their prices
- View the order book with buy and sell orders
- View all placed orders, executed orders, and registered users

### Prerequisites & Setup
- Clone the repository:
  ```bash
  git clone https://github.com/AnujAcharjee/CLI-StockTrader.git
  cd CLI-StockTrader
  ```
- Requirements:
  - A C++23 compatible compiler (e.g., `g++ 13+`, `clang++ 16+`)
  - `make` tool installed
- Compile the engine using a C++ compiler:
  ```bash
  make
  ```
- After successful compilation, the executable `engine.exe` will be available and ready to run.
- Run the application via your terminal:
  ```bash
  ./engine.exe 
  ```
- Once executed, the CLI menu will appear for user interaction.
  
## Usage
Once the application is running, the main menu will be displayed:
1) Add User  
2) Add Order  
3) View Stocks  
4) View Order Book  
5) View Users  
6) View Orders  
7) Executed Orders  
8) Exit

## Example Workflow
Here is an example of the flow and output of the application:

1. Starting the application:

```bash
Starting the application...
-->> Placing concurrent system orders
-->> All system orders placed

1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8):
```
2. Adding a new user:

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 1

Please enter a username to create a new user 
> Anuj
Creating user...
User 'Anuj' has been successfully created
```

3. Viewing Available Stocks:

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 3

------------------- Stocks ---------------------
Symbol         Total Quantity    Current Price
------------------------------------------------
GOOGL          10000             1000.00
BABA           10000             777.00
APPLE          10000             888.00
TCS            10000             999.00
------------------------------------------------
```

4. Viewing the Order Book:

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 4

Enter the stock symbol (TCS, APPLE, GOOGL, BABA): TCS

======================= Order Book for Stock: TCS ====================
Qty         Bid             |             Ask         Qty
----------------------------------------------------------------------
2           998.00          |          999.00       10000
1           993.00          |
2           900.00          |
======================================================================
```

5. Viewing Registered Users:

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 5

------------------ Users ------------------
Username       Funds          Demat Holdings
---------------------------------------------
User5          6892.00        BABA:4
User3          10000.00       None
User2          8224.00        APPLE:2
User4          10000.00       None
User1          10000.00       None
---------------------------------------------
```

6. Placing an Order:

i. MARKET ORDER

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 2

----- Place a New Order -----
Enter Username: Anuj
Enter Stock Symbol (TCS, BABA, GOOGL, APPLE): TCS
Enter Quantity: 3
Enter Price per Unit: 888
Is this a Buy Order? (Enter 1 for Buy or 0 for Sell): 1
Enter the Order Type (MARKET or LIMIT): MARKET
Order placed successfully for user 'Anuj' [Buy 3 of TCS @ 888.00 Rs ]
```
ii. LIMIT ORDER

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 2


----- Place a New Order ----- 
Enter Username: Anuj
Enter Stock Symbol (TCS, BABA, GOOGL, APPLE): TCS
Enter Quantity: 4
Enter Price per Unit: 888
Is this a Buy Order? (Enter 1 for Buy or 0 for Sell): 1
Enter the Order Type (MARKET or LIMIT): LIMIT
Enter the Order Expiry Type (DAY, GTC, GTD): GTD
Enter GTD expiry date (DD-MM-YYYY): 04-12-2025
Order placed successfully for user 'Anuj' [Buy 4 of TCS @ 888.00 Rs ]
```

7. Viewing Executed Orders:

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 7

----------------------- All Executed Orders ----------------------------
OrderID        Symbol         ExecutedQty    Timestamp
------------------------------------------------------------------------
2              APPLE          2              Sat May  3 03:37:04 2025
6              APPLE          2              Sat May  3 03:37:04 2025
3              BABA           4              Sat May  3 03:37:04 2025
8              BABA           4              Sat May  3 03:37:04 2025

-------------------------------------------------------------------------
```

8. View Orders Placed

```bash
1) Add User  2) Add Order  3) View Stocks  4) View Order Book 5) View Users  6) View Orders  7) Executed Orders  8) Exit
Select an option (1-8): 6

----- All Orders made -----
OrderId        Symbol    Qty       Price     Trade     Type      Expiry                   ExecutedQty    Username
---------------------------------------------------------------------------------------------------------------------------------------
10             TCS       2         900       BUY       LIMIT     04-05-2025 04:08:01      0              User3
6              APPLE     2         999       BUY       LIMIT     04-05-2025 04:08:01      2              User2
7              GOOGL     3         999       BUY       LIMIT     04-05-2025 04:08:01      0              User4
5              TCS       1         993       BUY       LIMIT     04-05-2025 04:08:01      0              User1
9              TCS       2         998       BUY       LIMIT     04-05-2025 04:08:01      0              User1
8              BABA      4         999       BUY       LIMIT     04-05-2025 04:08:01      4              User5
---------------------------------------------------------------------------------------------------------------------------------------
```

## Key Commands & Options
1) Add User: Creates a new user in the system.

2) Add Order: Allows placing a new buy or sell order for available stocks.

3) View Stocks: Displays the available stocks with their current prices.

4) View Order Book: Shows buy and sell orders for a specific stock.

5) View Users: Lists all registered users along with their funds and demat holdings.

6) View Orders: Displays all orders placed by users, whether executed or pending.

7) Executed Orders: Lists all orders that have been successfully executed.

8) Exit: Closes the application.
