#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

int main() {
    std::cout << "--- INTENTIONAL LEAK TEST ---" << std::endl;
    
    std::cout << "Allocating 4096 bytes..." << std::endl;
    
    // 1. Allocate the memory
    void* leaked_ptr = malloc(4096); 
    
    // 2. Printing the exact address forces the C++ compiler to execute the malloc
    std::cout << "Successfully allocated at: " << leaked_ptr << std::endl;

    // 3. Write actual data to the heap so the OS provisions physical RAM
    if (leaked_ptr != nullptr) {
        memset(leaked_ptr, 0xAA, 4096);
    }
    
    // We intentionally DO NOT call free(leaked_ptr);
    
    std::cout << "Sleeping for 3 seconds so the DB logs it..." << std::endl;
    sleep(3);
    
    std::cout << "Exiting! The OS will reclaim the RAM, but our Sandbox will flag the leak." << std::endl;
    return 0;
}