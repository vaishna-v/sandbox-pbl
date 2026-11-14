cmake_minimum_required(VERSION 3.10)
project(SandboxEnforcer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Curses REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

include_directories(include ${CURSES_INCLUDE_DIR})

add_library(sandbox_alloc SHARED allocator/sandbox_alloc.cpp)
target_link_libraries(sandbox_alloc PRIVATE dl PkgConfig::PQXX)

# Test Programs
add_executable(interactive_test programs/interactive_test.cpp)
add_executable(leak_test programs/leak_test.cpp)

# Route test executables to the /tests folder automatically
set_target_properties(interactive_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
set_target_properties(leak_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# Main Orchestrator (defined once)
add_executable(sandbox_exec src/Sandbox.cpp src/main.cpp src/Monitor.cpp)
target_link_libraries(sandbox_exec PRIVATE ${CURSES_LIBRARIES} PkgConfig::PQXX)

# Spawner
add_executable(spawner programs/spawner.cpp)
set_target_properties(spawner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# Banker's Algorithm Test
add_executable(bankers_test
    src/algorithms/bankers/main.cpp
    src/algorithms/bankers/BankersAlgorithm.cpp
)

# Banker's Algorithm Manager
add_executable(banker_manager
    src/algorithms/bankers/manager.cpp
    src/algorithms/bankers/BankersAlgorithm.cpp
)

# Worker Process
add_executable(banker_worker
    src/algorithms/bankers/worker.cpp
)

# Keep manager and worker executables in the build directory
set_target_properties(banker_worker PROPERTIES
    OUTPUT_NAME worker
)

add_dependencies(banker_manager banker_worker)

add_executable(user_interface
    src/algorithms/bankers/user_interface.cpp
)

add_executable(resource_monitor
    src/algorithms/bankers/resource_monitor.cpp
)

add_executable(memory_manager
    src/algorithms/memory/main.cpp
    src/algorithms/memory/MemoryManager.cpp
)

target_link_libraries(memory_manager PRIVATE ${CURSES_LIBRARIES})

add_executable(os_project src/project_menu.cpp)