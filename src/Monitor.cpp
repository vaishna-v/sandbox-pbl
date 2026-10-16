#include "../include/Monitor.h"
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

Monitor::Monitor(pid_t pid) : targetPid(pid) {
    ptrace(PTRACE_ATTACH, targetPid, nullptr, nullptr);
    waitpid(targetPid, nullptr, 0);
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
        std::cout << "Syscall " << syscall << " called " << count << " times\n";
    }
}

int Monitor::WaitForTermination() {
    int status;
    while (true) {
        ptrace(PTRACE_SYSCALL, targetPid, nullptr, nullptr);
        waitpid(targetPid, &status, 0);
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            return WTERMSIG(status);
        }
        if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, targetPid, nullptr, &regs);
            syscallCounts[regs.orig_rax]++;
        }
    }
}