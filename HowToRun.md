### Prerequisites

- Linux environment
- CMake and GNU Make.
- C++ compiler supporting C++17.
- xterm installed.

**Note:** The Banker's Algorithm module uses `xterm` to open separate windows for resource monitoring and user interaction.

## 1. Run the Sandbox Module

Open a terminal in the project directory and execute:

```bash
mkdir -p build
cd build
cmake ..
make
sudo ./sandbox_exec
```

## 2. Run the Banker's Algorithm Module

Open a separate terminal and execute:

```bash
cd build
cmake --build .
./banker_manager
```

The Banker's Algorithm interface will open automatically.

**Note:** The Sandbox and Banker's Algorithm modules are independent. Run them separately.