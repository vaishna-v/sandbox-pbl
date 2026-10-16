#include "../include/Sandbox.h"
#include <ncurses.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>

std::string PromptUser(const std::string& prompt) {
    timeout(-1);
    echo();
    move(LINES - 2, 0);
    clrtoeol();
    printw("%s", prompt.c_str());
    char buffer[256];
    getnstr(buffer, 255);
    noecho();
    timeout(1000);
    return std::string(buffer);
}

int main() {
    std::string sandboxName;
    size_t memLimitBytes;

    std::cout << "=== Interactive Resource Sandbox ===\n";
    std::cout << "Enter Sandbox Name: ";
    std::getline(std::cin, sandboxName);

    std::cout << "Enter Memory Limit (bytes): ";
    std::string memInput;
    std::getline(std::cin, memInput);
    try {
        memLimitBytes = std::stoull(memInput);
    } catch (...) {
        memLimitBytes = 10485760;
    }

    Sandbox sandbox(sandboxName);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(1000);

    bool running = true;
    std::string lastMessage = "Sandbox initialized. Logs writing to " + sandboxName + "_log.txt";

    while (running) {
        erase();

        auto procs = sandbox.GetRunningProcesses();
        size_t totalRamUsed = 0;
        for (const auto& p : procs) totalRamUsed += p.memoryUsageBytes;

        attron(A_BOLD);
        mvprintw(0, 0, "=== SANDBOX MANAGER [%s] ===", sandboxName.c_str());
        attroff(A_BOLD);

        mvprintw(1, 0, "RAM Limit: %zu Bytes | RAM Used: %zu Bytes | Active Programs: %zu", 
                 memLimitBytes, totalRamUsed, procs.size());
        
        mvprintw(3, 0, "%-10s %-20s %-15s %-10s", "PID", "NAME", "RAM (Bytes)", "PRIORITY");
        mvhline(4, 0, '-', 60);

        int row = 5;
        for (const auto& p : procs) {
            mvprintw(row++, 0, "%-10d %-20s %-15ld %-10d", p.pid, p.name.c_str(), p.memoryUsageBytes, p.priority);
        }

        mvhline(LINES - 3, 0, '-', COLS);
        mvprintw(LINES - 2, 0, "[l] Launch Program | [q] Exit");
        mvprintw(LINES - 1, 0, "Status: %s", lastMessage.c_str());

        refresh();

        int ch = getch();
        if (ch == 'q') {
            running = false;
        } else if (ch == 'l') {
            std::string input = PromptUser("Enter program path and args: ");
            if (!input.empty()) {
                std::istringstream iss(input);
                std::string programPath;
                iss >> programPath;

                if (programPath.find('/') == std::string::npos) {
                    programPath = "./" + programPath;
                }

                // Write a clean launch script to disk to avoid all shell parsing/quoting issues
                std::ofstream script("run_target.sh");
                script << "#!/bin/bash\n"
                       << "stty erase '^H'\n"
                       << "export LD_PRELOAD=./libsandbox_alloc.so\n"
                       << "exec \"$@\"\n";
                script.close();
                chmod("run_target.sh", 0755);

                std::vector<std::string> terminalArgs = {
                    "-fa", "Monospace", "-fs", "11",
                    "-bg", "black", "-fg", "white",
                    "-hold", "-e", "./run_target.sh", programPath
                };
                
                std::string arg;
                while (iss >> arg) {
                    terminalArgs.push_back(arg);
                }

                pid_t pid = sandbox.Launch("/usr/bin/xterm", terminalArgs, memLimitBytes);
                if (pid > 0) {
                    lastMessage = "Launched " + programPath + " (PID: " + std::to_string(pid) + ").";
                } else {
                    lastMessage = "Failed to launch " + programPath;
                }
            }
        }
    }

    endwin();
    return 0;
}