#include "MemoryManager.h"

MemoryManager::MemoryManager(int totalMemory) {
    this->totalMemory = totalMemory;

    memory.push_back({0, totalMemory, -1});
}


bool MemoryManager::firstFit(int processId, int requiredSize) {

    if (requiredSize <= 0 || requiredSize > totalMemory) {
        return false;
    }

    for (int i = 0; i < memory.size(); i++) {

        if (memory[i].processId == -1 &&
            memory[i].size >= requiredSize) {

            if (memory[i].size == requiredSize) {
                memory[i].processId = processId;
            }
            else {
                int remainingSize = memory[i].size - requiredSize;

                MemoryBlock newBlock = {
                    memory[i].startAddress + requiredSize,
                    remainingSize,
                    -1
                };

                memory[i].size = requiredSize;
                memory[i].processId = processId;

                memory.insert(memory.begin() + i + 1, newBlock);
            }

            return true;
        }
    }

    return false;
}


void MemoryManager::displayMemory() const {

    cout << "\n========== MEMORY LAYOUT ==========\n";

    for (const auto& block : memory) {

        if (block.processId == -1) {
            cout << "| FREE (" << block.size << " MB) ";
        }
        else {
            cout << "| P" << block.processId
                 << " (" << block.size << " MB) ";
        }
    }

    cout << "|\n==================================\n";
}


bool MemoryManager::deallocate(int processId) {

    bool found = false;

    for (auto& block : memory) {

        if (block.processId == processId) {
            block.processId = -1;
            found = true;
        }
    }

    if (!found) {
        return false;
    }

    // Merge adjacent free blocks
    for (int i = 0; i < memory.size() - 1; ) {

        if (memory[i].processId == -1 &&
            memory[i + 1].processId == -1) {

            memory[i].size += memory[i + 1].size;

            memory.erase(memory.begin() + i + 1);
        }
        else {
            i++;
        }
    }

    return true;
}


bool MemoryManager::bestFit(int processId, int requiredSize) {

    if (requiredSize <= 0 || requiredSize > totalMemory) {
        return false;
    }

    int bestIndex = -1;
    int smallestSize = totalMemory + 1;

    for (int i = 0; i < memory.size(); i++) {

        if (memory[i].processId == -1 &&
            memory[i].size >= requiredSize) {

            if (memory[i].size < smallestSize) {
                smallestSize = memory[i].size;
                bestIndex = i;
            }
        }
    }

    if (bestIndex == -1) {
        return false;
    }

    if (memory[bestIndex].size == requiredSize) {
        memory[bestIndex].processId = processId;
    }
    else {
        int remainingSize = memory[bestIndex].size - requiredSize;

        MemoryBlock newBlock = {
            memory[bestIndex].startAddress + requiredSize,
            remainingSize,
            -1
        };

        memory[bestIndex].size = requiredSize;
        memory[bestIndex].processId = processId;

        memory.insert(memory.begin() + bestIndex + 1, newBlock);
    }

    return true;
}


bool MemoryManager::worstFit(int processId, int requiredSize) {

    if (requiredSize <= 0 || requiredSize > totalMemory) {
        return false;
    }

    int worstIndex = -1;
    int largestSize = -1;

    for (int i = 0; i < memory.size(); i++) {

        if (memory[i].processId == -1 &&
            memory[i].size >= requiredSize) {

            if (memory[i].size > largestSize) {
                largestSize = memory[i].size;
                worstIndex = i;
            }
        }
    }

    if (worstIndex == -1) {
        return false;
    }

    if (memory[worstIndex].size == requiredSize) {
        memory[worstIndex].processId = processId;
    }
    else {
        int remainingSize = memory[worstIndex].size - requiredSize;

        MemoryBlock newBlock = {
            memory[worstIndex].startAddress + requiredSize,
            remainingSize,
            -1
        };

        memory[worstIndex].size = requiredSize;
        memory[worstIndex].processId = processId;

        memory.insert(memory.begin() + worstIndex + 1, newBlock);
    }

    return true;
}


int MemoryManager::getTotalFreeMemory() const {
    int totalFree = 0;

    for (const auto& block : memory) {
        if (block.processId == -1) {
            totalFree += block.size;
        }
    }

    return totalFree;
}


int MemoryManager::getFreeBlockCount() const {
    int count = 0;

    for (const auto& block : memory) {
        if (block.processId == -1) {
            count++;
        }
    }

    return count;
}

int MemoryManager::getLargestFreeBlock() const {
    int largest = 0;

    for (const auto& block : memory) {
        if (block.processId == -1 && block.size > largest) {
            largest = block.size;
        }
    }

    return largest;
}