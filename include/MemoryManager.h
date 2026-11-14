#pragma once

#include <vector>
#include<iostream>

using namespace std;

struct MemoryBlock {
    int startAddress;
    int size;
    int processId;  // -1 means free
};

class MemoryManager {
private:
    int totalMemory;
    vector<MemoryBlock> memory;

public:
    MemoryManager(int totalMemory);

    bool firstFit(int processId, int requiredSize);
    bool bestFit(int processId, int requiredSize);
    bool worstFit(int processId, int requiredSize);
    void displayMemory() const;
    bool deallocate(int processId);
    int getTotalFreeMemory() const;
    int getFreeBlockCount() const;
    int getLargestFreeBlock() const;
};