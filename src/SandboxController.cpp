#include "../include/SandboxController.h"
#include <iostream>

bool SandboxController::ExecuteAndMonitor(const std::string& programPath, const std::vector<std::string>& args, size_t memLimitBytes, int cpuQuota, int priority) {
    pid_t pid = sandbox.Launch(programPath, args, memLimitBytes, cpuQuota, priority);
    if (pid <= 0) return false;

    monitor = std::make_unique<Monitor>(pid);

    ProcessStats initialStats = monitor->GetCurrentStats();
    std::cout << "Process launched in Sandbox. PID: " << pid << "\n"
              << "State: " << initialStats.state << "\n"
              << "Initial Mem: " << initialStats.memoryUsageBytes << " bytes\n";

    int exitCode = monitor->WaitForTermination();
    
    std::cout << "\nProcess terminated. Exit code: " << exitCode << "\n";
    std::cout << "--- Syscall Summary ---\n";
    monitor->PrintSyscallSummary();

    sandbox.Cleanup();
    return true;
}