#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include "ManagerProtocol.h"

void displaySnapshot(const MonitorEvent& event) {
    std::cout << "\n========== RESOURCE SNAPSHOT ==========\n";

    std::cout << "Available:\n";
    std::cout << "  Memory: " << event.available[0] << " MB\n";
    std::cout << "  File Descriptors: " << event.available[1] << "\n";

    std::cout << "\nProcess Resource Table:\n";

    if (event.processCount == 0) {
        std::cout << "No registered workers.\n";
    }

    for (int i = 0; i < event.processCount; i++) {
        std::cout << "\nWorker ID: " << event.workerIds[i]
          << " | PID: " << event.pids[i] << "\n";

        std::cout << "  Maximum:    "
                  << event.maximum[i][0] << " MB, "
                  << event.maximum[i][1] << " FDs\n";

        std::cout << "  Allocation: "
                  << event.allocation[i][0] << " MB, "
                  << event.allocation[i][1] << " FDs\n";

        std::cout << "  Need:       "
                  << event.need[i][0] << " MB, "
                  << event.need[i][1] << " FDs\n";
    }

    std::cout << "\nSafe State: "
              << (event.safeState ? "YES" : "NO") << "\n";

    std::cout << "Safe Sequence: ";

    if (event.safeSequenceCount == 0) {
        std::cout << "None";
    } else {
        for (int i = 0; i < event.safeSequenceCount; i++) {
            int pid = event.safeSequence[i];
            int workerId = -1;

            for (int j = 0; j < event.processCount; j++) {
                if (event.pids[j] == pid) {
                    workerId = event.workerIds[j];
                    break;
                }
            }

            std::cout << "W" << workerId << " (PID " << pid << ")";

            if (i + 1 < event.safeSequenceCount)
                std::cout << " -> ";
        }
    }

    std::cout << "\n=======================================\n";
}

int main() {
    const char* fifoPath = "/tmp/banker_monitor_fifo";

    std::cout << "========== RESOURCE MANAGER MONITOR ==========\n";
    std::cout << "Waiting for manager...\n\n";
    std::cout.flush();

    int fd = open(fifoPath, O_RDONLY);

    if (fd == -1) {
        perror("Failed to open monitor FIFO");
        return 1;
    }

    while (true) {
        MonitorEvent event{};

        char* buffer = reinterpret_cast<char*>(&event);
        size_t received = 0;

        while (received < sizeof(event)) {
            ssize_t bytes = read(
                fd,
                buffer + received,
                sizeof(event) - received
            );

            if (bytes < 0 && errno == EINTR)
                continue;

            if (bytes <= 0) {
                if (bytes == 0) {
                    close(fd);
                    return 0;
                }

                perror("Monitor read failed");
                close(fd);
                return 1;
            }

            received += bytes;
        }

        if (event.type == EventType::SHUTDOWN)
            break;

        std::cout << "\n[Manager] ";

        if (event.workerId > 0) {
            std::cout << "[Worker " << event.workerId
                      << " | PID " << event.pid << "] ";
        }

        std::cout << event.message << '\n';

        displaySnapshot(event);

        std::cout.flush();
    }

    close(fd);

    std::cout << "Monitor shutting down.\n";

    return 0;
}