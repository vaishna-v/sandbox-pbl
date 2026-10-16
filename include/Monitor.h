#pragma once
#include <sys/types.h>
#include <string>
#include <unordered_map>

struct ProcessStats {
    long memoryUsageBytes;
    double cpuUsagePercent;
    char state;
};

class Monitor {
private:
    pid_t targetPid;
    std::unordered_map<long, int> syscallCounts;

public:
    explicit Monitor(pid_t pid);

    ProcessStats GetCurrentStats() const;
    void TraceSyscallStep();
    void PrintSyscallSummary() const;
    int WaitForTermination();
};