#pragma once
#include <string>
#include <vector>
#include <sys/types.h>

struct SandboxProcess {
    pid_t pid;
    std::string name;
    long memoryUsageBytes;
    int priority;
};

class Sandbox {
private:
    std::string sandboxName;
    std::string cgroupPath;
    pid_t childPid;

    bool SetupCgroup();
    void SetEnvironmentVariables(size_t memLimitBytes);

public:
    Sandbox(std::string name);
    ~Sandbox();

    pid_t Launch(const std::string& programPath, const std::vector<std::string>& args, size_t memLimitBytes);
    void Cleanup();
    
    std::vector<SandboxProcess> GetRunningProcesses() const;
};