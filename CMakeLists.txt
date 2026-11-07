cmake_minimum_required(VERSION 3.10)
project(SandboxEnforcer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Curses REQUIRED)
include_directories(include ${CURSES_INCLUDE_DIR})

add_library(sandbox_alloc SHARED allocator/sandbox_alloc.cpp)
# Added pqxx and pq so the injected allocator can talk to the database
target_link_libraries(sandbox_alloc dl pqxx pq)

# Test Programs
add_executable(interactive_test programs/interactive_test.cpp)
add_executable(leak_test programs/leak_test.cpp)

# Route test executables to the /tests folder automatically
set_target_properties(interactive_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
set_target_properties(leak_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(sandbox_exec src/Sandbox.cpp src/main.cpp src/Monitor.cpp)

# Added pqxx and pq for the main orchestrator
target_link_libraries(sandbox_exec ${CURSES_LIBRARIES} pqxx pq)


add_executable(spawner programs/spawner.cpp)
set_target_properties(spawner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")