#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>

#include "WorkerProtocol.h"


bool readFull(int fd, void* buffer, size_t size) {
    char* ptr = static_cast<char*>(buffer);
    size_t received = 0;

    while (received < size) {
        ssize_t bytes = read(
            fd,
            ptr + received,
            size - received
        );

        if (bytes < 0 && errno == EINTR)
            continue;

        if (bytes <= 0)
            return false;

        received += bytes;
    }

    return true;
}


int main(int argc, char* argv[]) {

    if (argc != 3) {
        std::cerr << "Invalid worker arguments.\n";
        return 1;
    }

    int controlFd = std::atoi(argv[1]);
    int eventFd = std::atoi(argv[2]);

    std::cout << "Worker started. PID: "
              << getpid() << std::endl;

    while (true) {

        WorkerCommand command{};

        // Wait for a command from the manager.
        if (!readFull(
            controlFd,
            &command,
            sizeof(command)
        )) {
            break;
        }

        switch (command.type) {

            case WorkerCommandType::START_WORK:

                std::cout
                    << "Worker " << getpid()
                    << " received resource allocation."
                    << std::endl;

                // Remain alive.
                // Resources are released only when
                // the manager receives a user release request.

                break;


            case WorkerCommandType::SHUTDOWN_WORKER:

                std::cout
                    << "Worker " << getpid()
                    << " shutting down."
                    << std::endl;

                close(controlFd);
                close(eventFd);

                return 0;
        }
    }

    close(controlFd);
    close(eventFd);

    return 0;
}