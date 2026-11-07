#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <vector>

void create_child(const char* name, bool spawn_child4 = false) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // We are now inside the child process memory space
        // Tell the Linux kernel to rename this specific process
        prctl(PR_SET_NAME, name, 0, 0, 0);
        
        if (spawn_child4) {
            pid_t sub_pid = fork();
            if (sub_pid == 0) {
                // We are now inside the grandchild process memory space
                prctl(PR_SET_NAME, "child4", 0, 0, 0);
                sleep(15); // Keep alive so the Sandbox has time to log it
                exit(0);
            }
            waitpid(sub_pid, nullptr, 0); // child1 waits for child4 to die
        } else {
            sleep(15); // Keep alive so the Sandbox has time to log it
        }
        exit(0);
    }
}

int main() {
    // Rename the root execution program
    prctl(PR_SET_NAME, "spawner", 0, 0, 0);
    std::cout << "Starting Process Spawner Hierarchy..." << std::endl;

    // Spawn the requested hierarchy
    create_child("child1", true);  // child1 will internally fork() child4
    create_child("child2", false);
    create_child("child3", false);

    // Wait for child1, child2, and child3 to finish
    while (wait(nullptr) > 0);
    
    std::cout << "Hierarchy terminated safely." << std::endl;
    return 0;
}