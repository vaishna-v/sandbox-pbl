#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ncurses.h>
#include "MemoryManager.h"

using namespace std;

// Read integer input without accepting invalid characters
bool PromptInt(const string& prompt, int& value) {
    while (true) {
        char buffer[256];

        timeout(-1);
        echo();
        curs_set(1);

        move(LINES - 2, 0);
        clrtoeol();
        printw("%s", prompt.c_str());

        int result = getnstr(buffer, 255);

        noecho();
        curs_set(0);

        if (result == ERR)
            return false;

        string input(buffer);
        istringstream stream(input);
        char extra;

        if ((stream >> value) && !(stream >> extra))
            return true;

        move(LINES - 1, 0);
        clrtoeol();
        printw("Invalid input! Enter a valid integer.");
        refresh();
    }
}

// Capture the existing memory layout output
vector<string> getMemoryLayout(const MemoryManager& manager) {
    ostringstream output;
    streambuf* oldBuffer = cout.rdbuf(output.rdbuf());

    manager.displayMemory();

    cout.rdbuf(oldBuffer);

    vector<string> lines;
    istringstream input(output.str());
    string line;

    while (getline(input, line)) {
        // Skip the heading and separator lines
        if (line.find("MEMORY LAYOUT") != string::npos ||
            line.find("====") != string::npos) {
            continue;
        }

        if (!line.empty())
            lines.push_back(line);
    }

    return lines;
}

// Display dashboard header
void displayHeader(const MemoryManager& manager, int totalMemory) {
    int freeMemory = manager.getTotalFreeMemory();
    int usedMemory = totalMemory - freeMemory;

    attron(A_BOLD);
    mvprintw(0, 0, "=== MEMORY MANAGER [OS SIMULATION] ===");
    attroff(A_BOLD);

    mvprintw(1, 0,
        "RAM Limit: %d MB | RAM Used: %d MB | RAM Free: %d MB",
        totalMemory, usedMemory, freeMemory);

    mvhline(2, 0, '-', COLS);
}

// Display memory layout
void displayLayout(const MemoryManager& manager) {
    attron(A_BOLD);
    mvprintw(4, 0, "--- MEMORY LAYOUT ---");
    attroff(A_BOLD);

    vector<string> lines = getMemoryLayout(manager);

    int row = 5;

    for (const string& line : lines) {
        if (row >= LINES - 9)
            break;

        mvprintw(row++, 0, "%s", line.c_str());
    }
}

// Display statistics
void displayStats(const MemoryManager& manager, int row) {
    attron(A_BOLD);
    mvprintw(row, 0, "--- MEMORY STATISTICS ---");
    attroff(A_BOLD);

    mvprintw(row + 1, 0, "Total Free Memory: %d MB",
        manager.getTotalFreeMemory());

    mvprintw(row + 2, 0, "Free Blocks: %d",
        manager.getFreeBlockCount());

    mvprintw(row + 3, 0, "Largest Free Block: %d MB",
        manager.getLargestFreeBlock());
}

int main() {
    int totalMemory;

    cout << "Enter total RAM size (MB): ";

    if (!(cin >> totalMemory) || totalMemory <= 0) {
        cout << "Invalid RAM size!\n";
        return 1;
    }

    MemoryManager manager(totalMemory);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    string status = "Memory manager initialized successfully.";

    bool running = true;

    while (running) {
        erase();

        displayHeader(manager, totalMemory);

        displayLayout(manager);

        displayStats(manager, LINES - 8);

        mvhline(LINES - 4, 0, '-', COLS);

        attron(A_BOLD);
        mvprintw(LINES - 3, 0,
            "[a] Allocate Memory | [d] Deallocate Memory | [q] Exit");
        attroff(A_BOLD);

        mvprintw(LINES - 1, 0, "Status: %s", status.c_str());

        refresh();

        int ch = getch();

        if (ch == 'q') {
            running = false;
        }

        else if (ch == 'a') {
            int algorithm, processId, requiredSize;

            if (!PromptInt("Algorithm (1.First Fit 2.Best Fit 3.Worst Fit): ",
                           algorithm)) {
                status = "Input cancelled.";
                continue;
            }

            if (algorithm < 1 || algorithm > 3) {
                status = "Invalid algorithm selected.";
                continue;
            }

            if (!PromptInt("Enter Process ID: ", processId))
                continue;

            if (processId <= 0) {
                status = "Process ID must be positive.";
                continue;
            }

            if (!PromptInt("Enter required memory (MB): ", requiredSize))
                continue;

            if (requiredSize <= 0) {
                status = "Memory requirement must be positive.";
                continue;
            }

            bool allocated = false;
            string algorithmName;

            switch (algorithm) {
                case 1:
                    allocated = manager.firstFit(processId, requiredSize);
                    algorithmName = "First Fit";
                    break;

                case 2:
                    allocated = manager.bestFit(processId, requiredSize);
                    algorithmName = "Best Fit";
                    break;

                case 3:
                    allocated = manager.worstFit(processId, requiredSize);
                    algorithmName = "Worst Fit";
                    break;
            }

            if (allocated) {
                status = "P" + to_string(processId) +
                    " allocated " + to_string(requiredSize) +
                    " MB using " + algorithmName + ".";
            }
            else {
                status = "Allocation failed: No suitable free block.";
            }
        }

        else if (ch == 'd') {
            int processId;

            if (!PromptInt("Enter Process ID to deallocate: ", processId))
                continue;

            if (processId <= 0) {
                status = "Process ID must be positive.";
                continue;
            }

            if (manager.deallocate(processId)) {
                status = "Process P" + to_string(processId) +
                    " deallocated successfully.";
            }
            else {
                status = "Process not found.";
            }
        }
    }

    endwin();

    cout << "Memory Manager exited.\n";

    return 0;
}