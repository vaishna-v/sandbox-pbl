#pragma once
#include <sys/types.h>
#include <map>
#include "../DatabaseAdapter.hpp"

struct ProcessStats {
    size_t memoryUsageBytes;
    double cpuUsagePercent;
    char state;
};

class Monitor {
private:
    pid_t targetPid;
    std::map<int, int> syscallCounts;
    DatabaseAdapter* dbAdapter; // EXTENSION: Added database pointer

public:
    // EXTENSION: Added database adapter to constructor
    Monitor(pid_t pid, DatabaseAdapter* db = nullptr);
    
    ProcessStats GetCurrentStats() const;
    void TraceSyscallStep();
    void PrintSyscallSummary() const;
    int WaitForTermination();
};