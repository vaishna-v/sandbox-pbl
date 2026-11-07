#include "../include/Sandbox.h"
#include "../include/Monitor.h" 
#include "../DatabaseAdapter.hpp"
#include <ncurses.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <deque>
#include <thread> 
#include <chrono> 
#include <set>
#include <mutex>

std::string PromptUser(const std::string& prompt, int input_line) {
    timeout(-1);
    echo();
    move(input_line, 0);
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

    std::ofstream(sandboxName + "_log.txt", std::ios::trunc).close();

    std::cout << "Enter Memory Limit (bytes): ";
    std::string memInput;
    std::getline(std::cin, memInput);
    try { memLimitBytes = std::stoull(memInput); } 
    catch (...) { memLimitBytes = 10485760; }

    DatabaseAdapter db(sandboxName, true);
    Sandbox sandbox(sandboxName);

    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); timeout(1000);

    int top_h = LINES * 0.3;
    int bot_h = LINES - top_h - 2; 

    WINDOW* top_win = newwin(top_h, COLS, 0, 0);
    WINDOW* bot_win = newwin(bot_h, COLS, top_h, 0);
    scrollok(bot_win, TRUE);

    bool running = true;
    std::string lastMessage = "Sandbox initialized. Logs writing to database.";
    
    std::deque<std::string> live_events;
    std::ifstream logFile(sandboxName + "_log.txt");

    std::vector<pid_t> ui_launched_pids;
    
    // FIX: Thread-safe trackers to prevent multiple monitors from attaching to the same PID
    std::set<pid_t> active_monitors;
    std::mutex monitor_mutex;

    while (running) {
        
        for (auto it = ui_launched_pids.begin(); it != ui_launched_pids.end(); ) {
            int status;
            pid_t res = waitpid(*it, &status, WNOHANG);
            if (res == *it && (WIFEXITED(status) || WIFSIGNALED(status))) {
                it = ui_launched_pids.erase(it);
            } else {
                ++it;
            }
        }

        werase(top_win);
        auto procs = sandbox.GetRunningProcesses();
        size_t totalRamUsed = 0;
        
        for (const auto& p : procs) {
            totalRamUsed += p.memoryUsageBytes;
            db.LogProcessStart(p.pid, p.ppid, p.name);
            db.LogResourceSnapshot(p.pid, 0.0, p.memoryUsageBytes); 
        }

        wattron(top_win, A_BOLD);
        mvwprintw(top_win, 0, 0, "=== SANDBOX MANAGER [%s] ===", sandboxName.c_str());
        wattroff(top_win, A_BOLD);

        mvwprintw(top_win, 1, 0, "RAM Limit: %zu Bytes | RAM Used: %zu Bytes | Active Programs: %zu", 
                  memLimitBytes, totalRamUsed, procs.size());
        
        mvwprintw(top_win, 3, 0, "%-10s %-20s %-15s %-10s", "PID", "NAME", "RAM (Bytes)", "PRIORITY (Nice)");
        mvwhline(top_win, 4, 0, '-', COLS);

        int row = 5;
        for (const auto& p : procs) {
            if (row >= top_h - 1) break;
            mvwprintw(top_win, row++, 0, "%-10d %-20s %-15ld %-10d", p.pid, p.name.c_str(), p.memoryUsageBytes, p.priority);
        }
        wrefresh(top_win);

        werase(bot_win);
        mvwprintw(bot_win, 0, 0, "--- LIVE MEMORY REQUESTS ---");
        
        if (logFile.is_open()) {
            logFile.clear(); 
            std::string line;
            while (std::getline(logFile, line)) {
                live_events.push_back(line);
                if (live_events.size() > (size_t)(bot_h - 2)) live_events.pop_front();
            }
        } else {
            logFile.open(sandboxName + "_log.txt");
        }

        int event_row = 1;
        for (const auto& ev : live_events) mvwprintw(bot_win, event_row++, 0, "%s", ev.c_str());
        wrefresh(bot_win);

        move(LINES - 2, 0); clrtoeol(); 
        printw("[l] Launch Program | [p] Set Priority | [q] Exit"); 
        
        move(LINES - 1, 0); clrtoeol(); 
        printw("Status: %s", lastMessage.c_str());
        refresh();

        int ch = getch();
        if (ch == 'q') {
            running = false;
        } 
        else if (ch == 'p') {
            std::string pidStr = PromptUser("Enter PID: ", LINES - 1);
            if (!pidStr.empty()) {
                try {
                    pid_t target = std::stoi(pidStr);
                    bool isValidPid = false;
                    for (const auto& p : procs) {
                        if (p.pid == target) {
                            isValidPid = true;
                            break;
                        }
                    }
                    if (!isValidPid) {
                        lastMessage = "Error: PID " + pidStr + " is not inside the sandbox.";
                    } else {
                        std::string prioStr = PromptUser("Enter Priority (high/normal/low): ", LINES - 1);
                        if (sandbox.SetProcessPriority(target, prioStr)) {
                            lastMessage = "Priority of PID " + pidStr + " updated to " + prioStr + ".";
                        } else {
                            lastMessage = "Failed to set priority. (Setting 'high' requires sudo).";
                        }
                    }
                } catch (...) {
                    lastMessage = "Invalid PID entered.";
                }
            }
        } 
        else if (ch == 'l') {
            std::string input = PromptUser("Enter program path and args: ", LINES - 1);
            if (!input.empty()) {
                std::istringstream iss(input);
                std::string programPath;
                iss >> programPath;

                if (programPath.find('/') == std::string::npos) {
                    programPath = "./tests/" + programPath;
                }

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
                while (iss >> arg) terminalArgs.push_back(arg);

                pid_t pid = sandbox.Launch("/usr/bin/xterm", terminalArgs, memLimitBytes);
                if (pid > 0) {
                    lastMessage = "Launched " + programPath + " (PID: " + std::to_string(pid) + ").";
                    ui_launched_pids.push_back(pid);
                    db.LogProcessStart(pid, getpid(), programPath);

                    // FIX: Pass active_monitors and mutex by reference to safely track assignments
                    std::thread([&sandbox, &db, &active_monitors, &monitor_mutex]() {
                        pid_t target_pid = -1;
                        for (int i = 0; i < 20; ++i) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            auto current_procs = sandbox.GetRunningProcesses();
                            
                            std::lock_guard<std::mutex> lock(monitor_mutex);
                            for (const auto& p : current_procs) {
                                // Only attach if we aren't already monitoring this PID
                                if (active_monitors.find(p.pid) == active_monitors.end()) {
                                    target_pid = p.pid;
                                    active_monitors.insert(target_pid);
                                    break;
                                }
                            }
                            if (target_pid != -1) break;
                        }
                        
                        if (target_pid != -1) {
                            Monitor mon(target_pid, &db);
                            mon.WaitForTermination();  
                            
                            // Cleanup so the tracking stays accurate
                            std::lock_guard<std::mutex> lock(monitor_mutex);
                            active_monitors.erase(target_pid);
                        }
                    }).detach();
                } else {
                    lastMessage = "Failed to launch " + programPath;
                }
            }
        }
    }

    endwin();
    return 0;
}