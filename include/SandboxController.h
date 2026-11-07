#pragma once
#include "Sandbox.h"
#include "Monitor.h"
#include <string>
#include <vector>
#include <memory>

class SandboxController {
private:
    Sandbox sandbox;
    std::unique_ptr<Monitor> monitor;

public:
    SandboxController() = default;

    // Fixed signature: Removed cpuQuota and priority to match Sandbox::Launch
    bool ExecuteAndMonitor(const std::string& programPath, const std::vector<std::string>& args, size_t memLimitBytes);
};