#pragma once

#include <vector>

enum ResourceType {
    MEMORY,
    FILE_DESCRIPTORS
};

struct ProcessResources {
    int pid;

    std::vector<int> maximum;
    std::vector<int> allocation;
    std::vector<int> need;
};

class BankersAlgorithm {
private:
    std::vector<ProcessResources> processes;
    std::vector<int> available;

public:
    BankersAlgorithm(int totalMemoryMB, int totalFileDescriptors);
    bool addProcess(int pid, int maxMemoryMB, int maxFileDescriptors);
    bool isSafeState();
    bool requestResources(int pid, int memoryMB, int fileDescriptors);
    bool finishProcess(int pid);
    bool releaseResources(int pid, int memoryMB, int fileDescriptors);
    const std::vector<ProcessResources>& getProcesses() const;
    const std::vector<int>& getAvailable() const;
    std::vector<int> getSafeSequence();
};

