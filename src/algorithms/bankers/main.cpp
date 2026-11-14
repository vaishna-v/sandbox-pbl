
#include <iostream>
#include "BankersAlgorithm.h"

int main() {

    BankersAlgorithm banker(10, 5);

    int pid = 1001;

    std::cout << "Registering Worker 1...\n";

    if (!banker.addProcess(pid, 7, 4)) {
        std::cout << "Registration failed.\n";
        return 1;
    }

    std::cout << "Worker registered successfully.\n";

    std::cout << "\n1. Requesting [3 MB, 2 FDs]\n";

    if (banker.requestResources(pid, 3, 2))
        std::cout << "Allocation approved.\n";
    else
        std::cout << "Allocation rejected.\n";

    std::cout << "\n2. Releasing [2 MB, 1 FD]\n";

    if (banker.releaseResources(pid, 2, 1))
        std::cout << "Resources released successfully.\n";
    else
        std::cout << "Resource release failed.\n";

    std::cout << "\n3. Requesting [2 MB, 1 FD] again\n";

    if (banker.requestResources(pid, 2, 1))
        std::cout << "New allocation approved.\n";
    else
        std::cout << "New allocation rejected.\n";

    std::cout << "\n4. Finishing Worker 1\n";

    if (banker.finishProcess(pid))
        std::cout << "All remaining resources released.\n";
    else
        std::cout << "Process not found.\n";

    std::cout << "\nTest completed.\n";

    return 0;
}