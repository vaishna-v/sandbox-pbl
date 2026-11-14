#include "BankersAlgorithm.h"

BankersAlgorithm::BankersAlgorithm(int totalMemoryMB, int totalFileDescriptors) {
    available.push_back(totalMemoryMB);
    available.push_back(totalFileDescriptors);
}


bool BankersAlgorithm::addProcess(
    int pid,
    int maxMemoryMB,
    int maxFileDescriptors
) {
    if (maxMemoryMB < 0 || maxFileDescriptors < 0) {
        return false;
    }

    for (const auto& process : processes) {
        if (process.pid == pid) {
            return false;
        }
    }

    ProcessResources process;

    process.pid = pid;
    process.maximum = {maxMemoryMB, maxFileDescriptors};
    process.allocation = {0, 0};
    process.need = process.maximum;

    processes.push_back(process);

    return true;
}

bool BankersAlgorithm::isSafeState() {
    return getSafeSequence().size() == processes.size();
}

bool BankersAlgorithm::requestResources(
    int pid,
    int memoryMB,
    int fileDescriptors
) {
    if (memoryMB < 0 || fileDescriptors < 0) {
        return false;
    }

    int index = -1;

    // Find the process
    for (int i = 0; i < processes.size(); i++) {
        if (processes[i].pid == pid) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return false;
    }

    std::vector<int> request = {memoryMB, fileDescriptors};

    // Check whether request exceeds declared maximum need
    for (int j = 0; j < 2; j++) {
        if (request[j] > processes[index].need[j]) {
            return false;
        }
    }

    // Check currently available resources
    for (int j = 0; j < 2; j++) {
        if (request[j] > available[j]) {
            return false;
        }
    }

    // Temporarily allocate resources
    for (int j = 0; j < 2; j++) {
        available[j] -= request[j];
        processes[index].allocation[j] += request[j];
        processes[index].need[j] -= request[j];
    }

    // Grant only if the resulting state is safe
    if (isSafeState()) {
        return true;
    }

    // Otherwise, roll back the allocation
    for (int j = 0; j < 2; j++) {
        available[j] += request[j];
        processes[index].allocation[j] -= request[j];
        processes[index].need[j] += request[j];
    }

    return false;
}


bool BankersAlgorithm::finishProcess(int pid) {
    for (int i = 0; i < processes.size(); i++) {
        if (processes[i].pid == pid) {

            // Release all resources held by this process
            for (int j = 0; j < 2; j++) {
                available[j] += processes[i].allocation[j];
            }

            // Remove the process from the process table
            processes.erase(processes.begin() + i);

            return true;
        }
    }

    return false;
}

bool BankersAlgorithm::releaseResources(
    int pid,
    int memoryMB,
    int fileDescriptors
) {
    if (memoryMB < 0 || fileDescriptors < 0) {
        return false;
    }

    int index = -1;

    // Find the process
    for (int i = 0; i < processes.size(); i++) {
        if (processes[i].pid == pid) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return false;
    }

    std::vector<int> release = {memoryMB, fileDescriptors};

    // A process cannot release more than it currently holds
    for (int j = 0; j < 2; j++) {
        if (release[j] > processes[index].allocation[j]) {
            return false;
        }
    }

    // Return resources to the available pool
    for (int j = 0; j < 2; j++) {
        processes[index].allocation[j] -= release[j];
        processes[index].need[j] += release[j];
        available[j] += release[j];
    }

    return true;
}

const std::vector<ProcessResources>& BankersAlgorithm::getProcesses() const {
    return processes;
}

const std::vector<int>& BankersAlgorithm::getAvailable() const {
    return available;
}

std::vector<int> BankersAlgorithm::getSafeSequence() {
    std::vector<int> work = available;
    std::vector<bool> finish(processes.size(), false);
    std::vector<int> sequence;

    while (sequence.size() < processes.size()) {
        bool found = false;

        for (int i = 0; i < processes.size(); i++) {
            if (finish[i]) continue;

            bool canFinish = true;

            for (int j = 0; j < 2; j++) {
                if (processes[i].need[j] > work[j]) {
                    canFinish = false;
                    break;
                }
            }

            if (canFinish) {
                for (int j = 0; j < 2; j++) {
                    work[j] += processes[i].allocation[j];
                }

                finish[i] = true;
                sequence.push_back(processes[i].pid);
                found = true;
            }
        }

        if (!found) {
            return {};
        }
    }

    return sequence;
}