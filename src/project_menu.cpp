#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <limits>

using namespace std;

void clearScreen() {
    system("clear");
}

void launchModule(const char* executable, bool useSudo = false) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return;
    }

    if (pid == 0) {
        if (useSudo) {
            execlp("sudo", "sudo", executable, (char*)nullptr);
        } else {
            execl(executable, executable, (char*)nullptr);
        }

        perror("Failed to launch module");
        exit(1);
    }

    int status;
    waitpid(pid, &status, 0);
}

int main() {
    int choice;

    while (true) {
        clearScreen();

        cout << "\n";
        cout << "========================================\n";
        cout << "          OS SIMULATION PROJECT\n";
        cout << "========================================\n\n";

        cout << "  1. Sandbox Manager\n";
        cout << "  2. Banker's Algorithm\n";
        cout << "  3. Memory Management\n\n";

        cout << "  0. Exit\n\n";

        cout << "========================================\n";
        cout << "Enter your choice: ";
        cout.flush();

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nInvalid input!\n";
            continue;
        }

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1:
                launchModule("./sandbox_exec", true);
                break;

            case 2:
                launchModule("./banker_manager");
                break;

            case 3:
                launchModule("./memory_manager");
                break;

            case 0:
                cout << "\nExiting OS Simulation Project...\n";
                return 0;

            default:
                cout << "\nInvalid choice!\n";
                break;
        }
    }

    return 0;
}