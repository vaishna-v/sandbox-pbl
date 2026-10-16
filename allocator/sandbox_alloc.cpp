#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <cstdio>

std::atomic<size_t> total_allocated{0};
size_t mem_limit = 0;
bool limit_initialized = false;
char log_file_name[256] = "allocations.txt";

__thread bool in_alloc = false; 

// A unique signature to prove this sandbox owns the memory block
const size_t SANDBOX_MAGIC = 0xDEADBEEFCAFEBABE;

struct BlockHeader {
    size_t magic;          // Signature
    size_t requested_size; // Exact payload size requested by user
    size_t mapped_size;    // Total system size including header
};

void init_sandbox() {
    if (!limit_initialized) {
        limit_initialized = true; 
        
        const char* env_limit = getenv("SANDBOX_MEM_LIMIT");
        if (env_limit) mem_limit = atoll(env_limit); 
        
        const char* env_name = getenv("SANDBOX_NAME");
        if (env_name) {
            snprintf(log_file_name, sizeof(log_file_name), "%s_log.txt", env_name);
        }
        
        int fd = open(log_file_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd >= 0) {
            char buffer[256];
            int len = snprintf(buffer, sizeof(buffer), "--- Sandbox [%s] Allocator Attached (Limit: %zu bytes) ---\n", 
                               env_name ? env_name : "Unknown", mem_limit);
            write(fd, buffer, len);
            close(fd);
        }
    }
}

void log_action(const char* action, size_t requested, bool allowed) {
    int fd = open(log_file_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        char buffer[256];
        int len = snprintf(buffer, sizeof(buffer), "[PID: %d] %s - Requested: %zu bytes | Status: %s | Total Payload: %zu/%zu\n", 
                           getpid(), action, requested, allowed ? "GRANTED" : "DENIED", total_allocated.load(), mem_limit);
        write(fd, buffer, len);
        close(fd);
    }
}

void update_usage() {
    char usage_file[256];
    snprintf(usage_file, sizeof(usage_file), "/tmp/sandbox_mem_%d.txt", getpid());
    int fd = open(usage_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        char buffer[64];
        int len = snprintf(buffer, sizeof(buffer), "%zu", total_allocated.load());
        write(fd, buffer, len);
        close(fd);
    }
}

void* raw_alloc(size_t size) {
    size_t total_size = size + sizeof(BlockHeader);
    void* ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;
    BlockHeader* header = static_cast<BlockHeader*>(ptr);
    header->magic = SANDBOX_MAGIC;
    header->requested_size = size;
    header->mapped_size = total_size;
    return static_cast<char*>(ptr) + sizeof(BlockHeader);
}

extern "C" void* malloc(size_t size) {
    if (size == 0) return nullptr;
    if (in_alloc) return raw_alloc(size);
    in_alloc = true;
    
    if (!limit_initialized) init_sandbox();
    
    if (mem_limit > 0 && total_allocated + size > mem_limit) {
        log_action("ALLOC", size, false);
        in_alloc = false;
        return nullptr;
    }
    
    size_t total_size = size + sizeof(BlockHeader);
    void* ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        log_action("ALLOC_SYSTEM_FAIL", size, false);
        in_alloc = false;
        return nullptr;
    }
    
    total_allocated += size;
    update_usage(); 
    
    BlockHeader* header = static_cast<BlockHeader*>(ptr);
    header->magic = SANDBOX_MAGIC;
    header->requested_size = size;
    header->mapped_size = total_size;
    
    log_action("ALLOC", size, true);
    in_alloc = false;
    return static_cast<char*>(ptr) + sizeof(BlockHeader);
}

extern "C" void free(void* ptr) {
    if (!ptr) return;
    BlockHeader* header = reinterpret_cast<BlockHeader*>(static_cast<char*>(ptr) - sizeof(BlockHeader));
    
    // Only process blocks that we mathematically prove we created
    if (header->magic == SANDBOX_MAGIC) {
        if (!in_alloc && limit_initialized) {
            total_allocated -= header->requested_size; 
            update_usage(); 
        }
        munmap(header, header->mapped_size);
    }
}

extern "C" void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (in_alloc) {
        void* ptr = raw_alloc(total);
        if (ptr) std::memset(ptr, 0, total);
        return ptr;
    }
    
    void* ptr = malloc(total);
    if (ptr) std::memset(ptr, 0, total);
    return ptr;
}

extern "C" void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return nullptr; }
    
    BlockHeader* header = reinterpret_cast<BlockHeader*>(static_cast<char*>(ptr) - sizeof(BlockHeader));
    if (header->magic == SANDBOX_MAGIC && header->requested_size >= size) return ptr;
    
    void* new_ptr = malloc(size);
    if (new_ptr) {
        size_t copy_size = (header->magic == SANDBOX_MAGIC) ? header->requested_size : size;
        std::memcpy(new_ptr, ptr, copy_size);
        free(ptr);
    }
    return new_ptr;
}