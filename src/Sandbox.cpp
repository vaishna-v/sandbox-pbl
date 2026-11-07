#include "../include/Sandbox.h"
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <sstream>
#include <sys/resource.h> 
#include <algorithm>

Sandbox::Sandbox(std::string name) : sandboxName(std::move(name)), childPid(-1) {
    cgroupPath = "/sys/fs/cgroup/" + sandboxName;
}

Sandbox::~Sandbox() {
    Cleanup();
}

bool Sandbox::SetupCgroup() {
    std::error_code ec;
    std::filesystem::create_directories(cgroupPath, ec);
    if (ec) return false;
    return true;
}

void Sandbox::SetEnvironmentVariables(size_t memLimitBytes) {
    setenv("SANDBOX_MEM_LIMIT", std::to_string(memLimitBytes).c_str(), 1);
    setenv("SANDBOX_NAME", sandboxName.c_str(), 1);
}

pid_t Sandbox::Launch(const std::string& programPath, const std::vector<std::string>& args, size_t memLimitBytes) {
    if (!SetupCgroup()) return -1;
    
    pid_t pid = fork();
    if (pid == 0) {
        SetEnvironmentVariables(memLimitBytes);
        
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(programPath.c_str()));
        for (const auto& arg : args) c_args.push_back(const_cast<char*>(arg.c_str()));
        c_args.push_back(nullptr);

        execvp(programPath.c_str(), c_args.data());
        exit(1);
    } else if (pid > 0) {
        std::ofstream procsFile(cgroupPath + "/cgroup.procs");
        if (procsFile.is_open()) procsFile << pid;
        childPid = pid;
    }
    return pid;
}

void Sandbox::Cleanup() {
    std::error_code ec;
    if (std::filesystem::exists(cgroupPath, ec)) std::filesystem::remove(cgroupPath, ec);
}

bool Sandbox::SetProcessPriority(pid_t pid, const std::string& level) {
    std::string lowerLevel = level;
    std::transform(lowerLevel.begin(), lowerLevel.end(), lowerLevel.begin(), ::tolower);
    
    int nice_value = 0;
    if (lowerLevel == "high") nice_value = -10;
    else if (lowerLevel == "low") nice_value = 10;
    else if (lowerLevel == "normal") nice_value = 0;
    else return false;

    return (setpriority(PRIO_PROCESS, pid, nice_value) == 0);
}

std::vector<SandboxProcess> Sandbox::GetRunningProcesses() const {
    std::vector<SandboxProcess> procs;
    std::ifstream cgroupFile(cgroupPath + "/cgroup.procs");
    std::string pidStr;
    
    while (std::getline(cgroupFile, pidStr)) {
        if (pidStr.empty()) continue;
        pid_t pid = std::stoi(pidStr);

        std::ifstream statFile("/proc/" + pidStr + "/stat");
        if (statFile.is_open()) {
            std::string line;
            if (std::getline(statFile, line)) {
                size_t startName = line.find('(');
                size_t endName = line.rfind(')');
                if (startName == std::string::npos || endName == std::string::npos) continue;
                
                std::string name = line.substr(startName + 1, endName - startName - 1);
                
                if (name.find("xterm") != std::string::npos || 
                    name.find("bash") != std::string::npos || 
                    name.find("run_target") != std::string::npos) {
                    continue;
                }
                
                std::istringstream iss(line.substr(endName + 1));
                char state;
                int os_ppid;
                std::string dummy;
                int os_priority, os_nice;
                long rss;
                
                iss >> state; // Field 3
                if (state == 'Z') continue; 
                
                iss >> os_ppid; // Field 4 is Parent PID

                for(int i = 0; i < 13; ++i) iss >> dummy; // Skip fields 5 to 17
                
                iss >> os_priority >> os_nice; // Fields 18 and 19
                
                for(int i = 0; i < 4; ++i) iss >> dummy; // Skip fields 20 to 23
                iss >> rss; // Field 24
                
                long exact_heap_mem = rss * sysconf(_SC_PAGESIZE);
                std::ifstream usageFile("/tmp/sandbox_mem_" + pidStr + ".txt");
                if (usageFile.is_open()) usageFile >> exact_heap_mem;
                
                // Properly pushes all 5 required fields: pid, ppid, name, memory, priority
                procs.push_back({pid, os_ppid, name, exact_heap_mem, os_nice}); 
            }
        }
    }
    return procs;
}