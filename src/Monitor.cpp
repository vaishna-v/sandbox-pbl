#include "../include/Monitor.h"
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

Monitor::Monitor(pid_t pid, DatabaseAdapter* db) : targetPid(pid), dbAdapter(db) {
    ptrace(PTRACE_ATTACH, targetPid, nullptr, nullptr);
    waitpid(targetPid, nullptr, 0);
    ptrace(PTRACE_SETOPTIONS, targetPid, 0, PTRACE_O_TRACESYSGOOD);
}

ProcessStats Monitor::GetCurrentStats() const {
    ProcessStats stats = {0, 0.0, '?'};
    std::ifstream statFile("/proc/" + std::to_string(targetPid) + "/stat");
    
    if (statFile.is_open()) {
        std::string line;
        if (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string dummy;
            char state;
            long rss;
            
            for(int i = 0; i < 2; ++i) iss >> dummy;
            iss >> state;
            for(int i = 0; i < 20; ++i) iss >> dummy;
            iss >> rss;
            
            stats.state = state;
            stats.memoryUsageBytes = rss * sysconf(_SC_PAGESIZE);
        }
    }
    return stats;
}

void Monitor::TraceSyscallStep() {
    ptrace(PTRACE_SYSCALL, targetPid, nullptr, nullptr);
    int status;
    waitpid(targetPid, &status, 0);
    
    if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, targetPid, nullptr, &regs);
        syscallCounts[regs.orig_rax]++;
    }
}

void Monitor::PrintSyscallSummary() const {
    for (const auto& [syscall, count] : syscallCounts) {
        if (dbAdapter) {
            dbAdapter->LogSyscall(targetPid, syscall, count);
        }
    }
}

int Monitor::WaitForTermination() {
    int status;
    long signal_to_deliver = 0; 

    while (true) {
        // Resume the program and inject any pending signals
        ptrace(PTRACE_SYSCALL, targetPid, nullptr, (void*)signal_to_deliver);
        waitpid(targetPid, &status, 0);
        
        signal_to_deliver = 0; 

        if (WIFEXITED(status)) {
            PrintSyscallSummary(); // Safely flush to DB on normal exit
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            PrintSyscallSummary(); // Safely flush to DB if killed (e.g. clicking 'X')
            return WTERMSIG(status);
        }
        
        if (WIFSTOPPED(status)) {
            int stopsig = WSTOPSIG(status);
            
            if (stopsig == (SIGTRAP | 0x80)) {
                // It's a system call! Safely tally it without blocking for DB I/O
                struct user_regs_struct regs;
                ptrace(PTRACE_GETREGS, targetPid, nullptr, &regs);
                syscallCounts[regs.orig_rax]++; 
            } else if (stopsig == SIGTRAP) {
                // Ignore normal SIGTRAP
            } else {
                // Queue real OS signals to be delivered on the next loop
                signal_to_deliver = stopsig;
            }
        }
    }
}