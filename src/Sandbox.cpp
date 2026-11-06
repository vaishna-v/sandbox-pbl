#include "../include/Sandbox.h"
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <sstream>

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
                
                long exact_heap_mem = 0;
                std::ifstream usageFile("/tmp/sandbox_mem_" + pidStr + ".txt");
                
                if (usageFile.is_open()) {
                    // Read exact usage tracked by sandbox_alloc.so
                    usageFile >> exact_heap_mem;
                } else {
                    // Fallback to OS RSS if file doesn't exist yet
                    std::istringstream iss(line.substr(endName + 1));
                    char state;
                    long rss;
                    std::string dummy;
                    
                    iss >> state;
                    for(int i = 0; i < 20; ++i) iss >> dummy;
                    iss >> rss;
                    exact_heap_mem = rss * sysconf(_SC_PAGESIZE);
                }
                
                procs.push_back({pid, name, exact_heap_mem, 1}); 
            }
        }
    }
    return procs;
}