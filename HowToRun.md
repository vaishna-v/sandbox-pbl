### Prerequisites

- Linux environment
- CMake and GNU Make
- C++ compiler supporting C++17
- `xterm` installed

**Note:** The Banker's Algorithm module uses `xterm` to open separate windows for resource monitoring and user interaction.

## 1. Build the Project

Open a terminal in the project directory and execute:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

## 2. Run the Complete Project

From the `build` directory, execute:

```bash
./os_project
```

The main menu provides the following options:

1. Sandbox Manager
2. Banker's Algorithm
3. Memory Management
0. Exit

Select a module by entering its corresponding number. Once a module finishes execution, the main menu will reappear.

## 3. Run Individual Modules

The modules can also be executed independently.

### Sandbox Module

```bash
cd build
sudo ./sandbox_exec
```

### Banker's Algorithm Module

```bash
cd build
./banker_manager
```

The Banker's Algorithm interface will open automatically.

### Memory Management Module

```bash
cd build
./memory_manager
```

**Note:** The Sandbox and Banker's Algorithm modules are independent and can be run separately.