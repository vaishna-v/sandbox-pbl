#include <cstdlib>
#include <vector>

int main() {
    std::vector<void*> allocations;
    
    for (int i = 0; i < 5; ++i) {
        void* ptr = std::malloc(1024 * 1024);
        if (ptr) {
            allocations.push_back(ptr);
        }
    }
    
    for (void* ptr : allocations) {
        std::free(ptr);
    }
    
    return 0;
}