#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <unistd.h>

int main() {
    std::vector<void*> blocks;
    std::cout << "Interactive Allocation Test [PID: " << getpid() << "]\n";
    std::cout << "Enter bytes to allocate, 'free' to release last, or 'exit':\n";

    std::string input;
    while (true) {
        std::cout << "> ";
        if (!(std::cin >> input)) break;

        if (input == "exit" || input == "quit") {
            break;
        } else if (input == "free") {
            if (!blocks.empty()) {
                std::free(blocks.back());
                blocks.pop_back();
                std::cout << "Freed last block.\n";
            } else {
                std::cout << "No blocks to free.\n";
            }
        } else {
            try {
                size_t bytes = std::stoull(input);
                void* ptr = std::malloc(bytes);
                if (ptr) {
                    blocks.push_back(ptr);
                    std::cout << "Successfully allocated " << bytes << " bytes.\n";
                } else {
                    std::cout << "Allocation of " << bytes << " bytes FAILED (Denied by Sandbox).\n";
                }
            } catch (...) {
                std::cout << "Invalid input.\n";
            }
        }
    }

    for (void* ptr : blocks) {
        std::free(ptr);
    }
    return 0;
}