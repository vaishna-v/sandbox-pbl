
#include <iostream>
#include <limits>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "ManagerProtocol.h"

const char* FIFO_PATH = "/tmp/banker_command_fifo";

bool sendCommand(int fd, const UserCommand& command) {
    ssize_t result = write(fd, &command, sizeof(command));

    if (result != sizeof(command)) {
        std::cerr << "Failed to send command to manager.\n";
        return false;
    }

    return true;
}

int main() {

    int fd = open(FIFO_PATH, O_WRONLY);

    if (fd == -1) {
        perror("Cannot connect to manager");
        return 1;
    }

    UserCommand command{};

    std::cout << "\n========== RESOURCE CONFIGURATION ==========\n";

    std::cout << "Total memory (MB): ";
    std::cin >> command.totalMemoryMB;

    std::cout << "Total file descriptors: ";
    std::cin >> command.totalFileDescriptors;

    std::cout << "Number of workers: ";
    std::cin >> command.workerCount;

    if (std::cin.fail() ||
        command.totalMemoryMB <= 0 ||
        command.totalFileDescriptors <= 0 ||
        command.workerCount <= 0) {

        std::cout << "Invalid configuration.\n";
        close(fd);
        return 1;
    }

    command.type = CommandType::INITIALIZE;

    if (!sendCommand(fd, command)) {
        close(fd);
        return 1;
    }

    std::cout << "\n========== WORKER REGISTRATION ==========\n";

    for (int i = 1; i <= command.workerCount; i++) {

        UserCommand registration{};

        std::cout << "\nWorker " << i << "\n";

        std::cout << "Maximum memory claim (MB): ";
        std::cin >> registration.memoryMB;

        std::cout << "Maximum FD claim: ";
        std::cin >> registration.fileDescriptors;

        if (std::cin.fail() ||
            registration.memoryMB < 0 ||
            registration.fileDescriptors < 0 ||
            registration.memoryMB > command.totalMemoryMB ||
            registration.fileDescriptors > command.totalFileDescriptors) {

            std::cout << "Invalid maximum claim.\n";
            close(fd);
            return 1;
        }

        registration.type = CommandType::REGISTER_WORKER;
        registration.workerId = i;

        if (!sendCommand(fd, registration)) {
            close(fd);
            return 1;
        }

        std::cout << "Registration sent for Worker " << i << ".\n";
    }

    while (true) {

        std::cout << "\n========== RESOURCE REQUEST INTERFACE ==========\n";
        std::cout << "1. Request resources\n";
        std::cout << "2. Release resources\n";
        std::cout << "3. Cancel pending request\n";
        std::cout << "4. View resource status\n";
        std::cout << "5. Exit\n";

        int choice;
        std::cout << "Enter choice: ";
        std::cin >> choice;

        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            std::cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        if (choice == 5) {
            command = {};
            command.type = CommandType::SHUTDOWN;
            sendCommand(fd, command);
            break;
        }

        if (choice < 1 || choice > 5) {
            std::cout << "Invalid choice.\n";
            continue;
        }

        command = {};

        if (choice == 1) {
            std::cout << "Worker ID: ";
            std::cin >> command.workerId;
            command.type = CommandType::REQUEST;

            std::cout << "Memory requested (MB): ";
            std::cin >> command.memoryMB;

            std::cout << "FDs requested: ";
            std::cin >> command.fileDescriptors;

        } else if (choice == 2) {
            std::cout << "Worker ID: ";
            std::cin >> command.workerId;
            command.type = CommandType::RELEASE;

            std::cout << "Memory to release (MB): ";
            std::cin >> command.memoryMB;

            std::cout << "FDs to release: ";
            std::cin >> command.fileDescriptors;

        } else if (choice == 3) {
            std::cout << "Worker ID: ";
            std::cin >> command.workerId;
            command.type = CommandType::CANCEL;

        } else if (choice == 4) {

            command.type = CommandType::STATUS;
        }

        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            std::cout << "Invalid input. Command cancelled.\n";
            continue;
        }

        if ((choice == 1 || choice == 2) &&
            (command.memoryMB < 0 || command.fileDescriptors < 0)) {

            std::cout << "Resource amounts cannot be negative.\n";
            continue;
        }

        if (!sendCommand(fd, command)) {
            break;
        }

        std::cout << "Command sent to manager.\n";
    }

    close(fd);

    std::cout << "User interface closed.\n";

    return 0;
}