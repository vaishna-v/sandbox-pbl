#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <string>
#include <cstdio>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include <algorithm>

#include "BankersAlgorithm.h"
#include "ManagerProtocol.h"
#include "WorkerProtocol.h"

const char* COMMAND_FIFO = "/tmp/banker_command_fifo";
const char* MONITOR_FIFO = "/tmp/banker_monitor_fifo";

struct WorkerState {
    int id = 0;
    pid_t pid = -1;

    int controlFd = -1;
    int eventFd = -1;

    int maxMemory = 0;
    int maxFDs = 0;

    int allocatedMemory = 0;
    int allocatedFDs = 0;

    int pendingMemory = 0;
    int pendingFDs = 0;

    bool pending = false;
    bool active = false;
    bool alive = true;
};

std::map<int, WorkerState> workers;

std::unique_ptr<BankersAlgorithm> banker;

int commandFd = -1;
int monitorFd = -1;

int totalMemory = 0;
int totalFDs = 0;

int usedMemory = 0;
int usedFDs = 0;

int expectedWorkers = 0;
int registeredWorkers = 0;

bool shuttingDown = false;


// --------------------------------------------------
// PIPE COMMUNICATION
// --------------------------------------------------

bool writeFull(int fd, const void* buffer, size_t size) {
    const char* ptr = static_cast<const char*>(buffer);
    size_t sent = 0;

    while (sent < size) {
        ssize_t bytes = write(fd, ptr + sent, size - sent);

        if (bytes < 0 && errno == EINTR)
            continue;

        if (bytes <= 0)
            return false;

        sent += bytes;
    }

    return true;
}


bool readFull(int fd, void* buffer, size_t size) {
    char* ptr = static_cast<char*>(buffer);
    size_t received = 0;

    while (received < size) {
        ssize_t bytes = read(fd, ptr + received, size - received);

        if (bytes < 0 && errno == EINTR)
            continue;

        if (bytes <= 0)
            return false;

        received += bytes;
    }

    return true;
}


void setCloseOnExec(int fd) {
    int flags = fcntl(fd, F_GETFD);

    if (flags >= 0)
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}


// --------------------------------------------------
// MONITOR COMMUNICATION
// --------------------------------------------------
void emitEvent(
    EventType type,
    int workerId,
    const std::string& message
) {
    if (monitorFd < 0)
        return;

    MonitorEvent event{};

    event.type = type;
    event.workerId = workerId;

    if (workerId > 0 && workers.count(workerId))
        event.pid = workers[workerId].pid;

    if (banker) {
        const auto& processes = banker->getProcesses();
        const auto& available = banker->getAvailable();

        event.processCount = std::min(
            static_cast<int>(processes.size()),
            MAX_TRACKED_WORKERS
        );

        event.available[0] = available[0];
        event.available[1] = available[1];

        event.availableMemoryMB = available[0];
        event.availableFileDescriptors = available[1];

        for (int i = 0; i < event.processCount; i++) {
            event.pids[i] = processes[i].pid;
            
            // Find the application-level Worker ID for this PID.
            for (const auto& entry : workers) {
                if (entry.second.pid == processes[i].pid) {
                    event.workerIds[i] = entry.first;
                    break;
                }
            }

            for (int j = 0; j < RESOURCE_TYPES; j++) {
                event.maximum[i][j] = processes[i].maximum[j];
                event.allocation[i][j] = processes[i].allocation[j];
                event.need[i][j] = processes[i].need[j];
            }
        }

        std::vector<int> sequence = banker->getSafeSequence();

        event.safeState =
            (sequence.size() == processes.size()) ? 1 : 0;

        event.safeSequenceCount = std::min(
            static_cast<int>(sequence.size()),
            MAX_TRACKED_WORKERS
        );

        for (int i = 0; i < event.safeSequenceCount; i++) {
            event.safeSequence[i] = sequence[i];
        }
    }
    else {
        event.availableMemoryMB = totalMemory - usedMemory;
        event.availableFileDescriptors = totalFDs - usedFDs;
    }

    std::snprintf(
        event.message,
        sizeof(event.message),
        "%s",
        message.c_str()
    );

    if (!writeFull(monitorFd, &event, sizeof(event)))
        perror("Monitor event write failed");
}


// --------------------------------------------------
// TERMINAL WINDOWS
// --------------------------------------------------

bool createFifo(const char* path) {
    unlink(path);

    if (mkfifo(path, 0666) == -1) {
        perror("mkfifo failed");
        return false;
    }

    return true;
}


void launchTerminal(const char* executable) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Terminal fork failed");
        return;
    }

    if (pid == 0) {
        execlp(
            "xterm",
            "xterm",
            "-e",
            executable,
            nullptr
        );

        perror("xterm launch failed");
        _exit(1);
    }
}


// --------------------------------------------------
// WORKER CREATION
// --------------------------------------------------

bool launchWorker(WorkerState& worker) {
    int controlPipe[2];
    int eventPipe[2];

    if (pipe(controlPipe) == -1 ||
        pipe(eventPipe) == -1) {

        perror("Worker pipe creation failed");
        return false;
    }

    for (int fd : {
        controlPipe[0], controlPipe[1],
        eventPipe[0], eventPipe[1]
    }) {
        setCloseOnExec(fd);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Worker fork failed");

        close(controlPipe[0]);
        close(controlPipe[1]);
        close(eventPipe[0]);
        close(eventPipe[1]);

        return false;
    }

    if (pid == 0) {
        close(controlPipe[1]);
        close(eventPipe[0]);

        close(commandFd);
        close(monitorFd);

        for (auto& entry : workers) {
            close(entry.second.controlFd);
            close(entry.second.eventFd);
        }

        fcntl(controlPipe[0], F_SETFD, 0);
        fcntl(eventPipe[1], F_SETFD, 0);

        std::string control = std::to_string(controlPipe[0]);
        std::string event = std::to_string(eventPipe[1]);

        execl(
            "./worker",
            "worker",
            control.c_str(),
            event.c_str(),
            nullptr
        );

        perror("Worker exec failed");
        _exit(1);
    }

    close(controlPipe[0]);
    close(eventPipe[1]);

    worker.pid = pid;
    worker.controlFd = controlPipe[1];
    worker.eventFd = eventPipe[0];

    return true;
}


// --------------------------------------------------
// WORKER TASK MANAGEMENT
// --------------------------------------------------

bool startAllocatedTask(
    WorkerState& worker,
    int memory,
    int fds
) {
    WorkerCommand command{};

    command.type = WorkerCommandType::START_WORK;

    if (!writeFull(
        worker.controlFd,
        &command,
        sizeof(command)
    )) {
        banker->releaseResources(worker.pid, memory, fds);

        emitEvent(
            EventType::ERROR,
            worker.id,
            "Could not send work instruction to worker."
        );

        return false;
    }

    worker.allocatedMemory += memory;
    worker.allocatedFDs += fds;

    worker.active = true;

    usedMemory += memory;
    usedFDs += fds;

    emitEvent(
        EventType::APPROVED,
        worker.id,
        "Resource request APPROVED. Resources allocated to worker."
    );

    return true;
}


// --------------------------------------------------
// RESOURCE REQUEST
// --------------------------------------------------

void handleRequest(const UserCommand& command) {
    int id = command.workerId;

    if (!workers.count(id) || !workers[id].alive) {
        emitEvent(EventType::ERROR, id, "Worker not found.");
        return;
    }

    WorkerState& worker = workers[id];

    if (worker.pending) {
        emitEvent(
            EventType::ERROR,
            id,
            "Worker already has a pending request."
        );
        return;
    }

    if (command.memoryMB < 0 ||
        command.fileDescriptors < 0 ||
        (command.memoryMB == 0 && command.fileDescriptors == 0)) {

        emitEvent(
            EventType::ERROR,
            id,
            "Invalid resource request."
        );
        return;
    }

    // Check the remaining maximum claim.
    int remainingMemory =
        worker.maxMemory - worker.allocatedMemory;

    int remainingFDs =
        worker.maxFDs - worker.allocatedFDs;

    if (command.memoryMB > remainingMemory ||
        command.fileDescriptors > remainingFDs) {

        emitEvent(
            EventType::REJECTED,
            id,
            "Request exceeds the worker's remaining maximum claim."
        );
        return;
    }

    emitEvent(
        EventType::REQUEST_RECEIVED,
        id,
        "Received resource request."
    );

    bool approved = banker->requestResources(
        worker.pid,
        command.memoryMB,
        command.fileDescriptors
    );

    if (approved) {
        startAllocatedTask(
            worker,
            command.memoryMB,
            command.fileDescriptors
        );
    }
    else {
        worker.pending = true;

        worker.pendingMemory = command.memoryMB;
        worker.pendingFDs = command.fileDescriptors;

        emitEvent(
            EventType::WAITING,
            id,
            "Request cannot be granted safely now. Worker placed in pending queue."
        );
    }
}


// --------------------------------------------------
// RESOURCE RELEASE
// --------------------------------------------------

void handleRelease(const UserCommand& command) {
    int id = command.workerId;

    if (!workers.count(id) || !workers[id].alive) {
        emitEvent(EventType::ERROR, id, "Worker not found.");
        return;
    }

    WorkerState& worker = workers[id];

    if (command.memoryMB < 0 ||
        command.fileDescriptors < 0 ||
        (command.memoryMB == 0 && command.fileDescriptors == 0)) {

        emitEvent(
            EventType::ERROR,
            id,
            "Invalid release amounts."
        );
        return;
    }

    if (!banker->releaseResources(
        worker.pid,
        command.memoryMB,
        command.fileDescriptors
    )) {
        emitEvent(
            EventType::ERROR,
            id,
            "Release failed. Worker does not hold those resources."
        );
        return;
    }

    worker.allocatedMemory -= command.memoryMB;
    worker.allocatedFDs -= command.fileDescriptors;

    usedMemory -= command.memoryMB;
    usedFDs -= command.fileDescriptors;

    // Worker becomes available for another request
    // only when all its allocated resources are released.
    if (worker.allocatedMemory == 0 &&
        worker.allocatedFDs == 0) {

        worker.active = false;
    }

    emitEvent(
        EventType::RELEASED,
        id,
        "Resources released successfully."
    );

    // Pending requests will be retried by the main loop.
}


// --------------------------------------------------
// CANCEL PENDING REQUEST
// --------------------------------------------------

void handleCancel(const UserCommand& command) {
    int id = command.workerId;

    if (!workers.count(id) || !workers[id].pending) {
        emitEvent(
            EventType::ERROR,
            id,
            "No pending request exists for this worker."
        );
        return;
    }

    WorkerState& worker = workers[id];

    worker.pending = false;
    worker.pendingMemory = 0;
    worker.pendingFDs = 0;

    emitEvent(
        EventType::INFO,
        id,
        "Pending request cancelled."
    );
}


// --------------------------------------------------
// RETRY PENDING REQUESTS
// --------------------------------------------------

void retryPendingRequests() {
    for (auto& entry : workers) {
        WorkerState& worker = entry.second;

        if (!worker.alive || !worker.pending)
            continue;

        bool approved = banker->requestResources(
            worker.pid,
            worker.pendingMemory,
            worker.pendingFDs
        );

        if (!approved)
            continue;

        int memory = worker.pendingMemory;
        int fds = worker.pendingFDs;

        worker.pending = false;
        worker.pendingMemory = 0;
        worker.pendingFDs = 0;

        startAllocatedTask(
            worker,
            memory,
            fds
        );
    }
}


// --------------------------------------------------
// WORKER EVENTS
// --------------------------------------------------

void handleWorkerEvent(int id) {
    if (!workers.count(id))
        return;

    WorkerState& worker = workers[id];

    WorkerEvent event{};

    if (!readFull(worker.eventFd, &event, sizeof(event))) {
        emitEvent(
            EventType::ERROR,
            id,
            "Lost communication with worker."
        );

        return;
    }

    if (event.type == WorkerEventType::WORK_COMPLETED) {

        // IMPORTANT:
        // Do not release resources here.
        // Resources remain allocated until the user
        // explicitly releases them through the UI.

        emitEvent(
            EventType::INFO,
            id,
            "Worker reported task completion. Resources remain allocated."
        );
    }
    else if (event.type == WorkerEventType::WORK_ERROR) {
        emitEvent(
            EventType::ERROR,
            id,
            "Worker reported an execution error."
        );
    }
}


// --------------------------------------------------
// COMMAND HANDLER
// --------------------------------------------------

void handleCommand(const UserCommand& command) {
    switch (command.type) {

        case CommandType::INITIALIZE: {
            if (banker) {
                emitEvent(
                    EventType::ERROR,
                    0,
                    "Manager already initialized."
                );
                break;
            }

            if (command.totalMemoryMB <= 0 ||
                command.totalFileDescriptors <= 0 ||
                command.workerCount <= 0 ||
                command.workerCount > MAX_TRACKED_WORKERS) {

                emitEvent(
                    EventType::ERROR,
                    0,
                    "Invalid resource configuration."
                );
                break;
            }

            totalMemory = command.totalMemoryMB;
            totalFDs = command.totalFileDescriptors;

            expectedWorkers = command.workerCount;

            banker = std::make_unique<BankersAlgorithm>(
                totalMemory,
                totalFDs
            );

            emitEvent(
                EventType::INFO,
                0,
                "Resource manager initialized successfully."
            );

            break;
        }
 
        case CommandType::REGISTER_WORKER: {
    if (!banker) {
        emitEvent(
            EventType::ERROR,
            0,
            "Initialize manager first."
        );
        break;
    }

    if (registeredWorkers >= expectedWorkers ||
        registeredWorkers >= MAX_TRACKED_WORKERS ||
        workers.count(command.workerId)) {

        emitEvent(
            EventType::ERROR,
            command.workerId,
            "Invalid or duplicate worker registration."
        );
        break;
    }

    if (command.memoryMB < 0 ||
        command.fileDescriptors < 0 ||
        command.memoryMB > totalMemory ||
        command.fileDescriptors > totalFDs) {

        emitEvent(
            EventType::ERROR,
            command.workerId,
            "Invalid maximum resource claim."
        );

        break;
    }

    WorkerState worker{};

    worker.id = command.workerId;
    worker.pid = -1;

    worker.maxMemory = command.memoryMB;
    worker.maxFDs = command.fileDescriptors;

    if (!launchWorker(worker)) {
        emitEvent(
            EventType::ERROR,
            command.workerId,
            "Worker creation failed."
        );
        break;
    }

    if (!banker->addProcess(
        worker.pid,
        worker.maxMemory,
        worker.maxFDs
    )) {
        kill(worker.pid, SIGTERM);
        waitpid(worker.pid, nullptr, 0);

        close(worker.controlFd);
        close(worker.eventFd);

        emitEvent(
            EventType::ERROR,
            command.workerId,
            "Banker's Algorithm registration failed."
        );
        break;
    }

    workers[worker.id] = worker;
    registeredWorkers++;

    emitEvent(
        EventType::REGISTERED,
        worker.id,
        "Worker registered and launched successfully."
    );

    if (registeredWorkers == expectedWorkers) {
        emitEvent(
            EventType::STATUS_UPDATE,
            0,
            "All workers registered. Initial safe sequence calculated."
        );
    }

    break;
}

        case CommandType::REQUEST:
            if (banker)
                handleRequest(command);
            else
                emitEvent(
                    EventType::ERROR,
                    0,
                    "Manager not initialized."
                );
            break;

        case CommandType::RELEASE:
            if (banker)
                handleRelease(command);
            else
                emitEvent(
                    EventType::ERROR,
                    0,
                    "Manager not initialized."
                );
            break;

        case CommandType::CANCEL:
            handleCancel(command);
            break;

        case CommandType::STATUS: {
            emitEvent(
                EventType::STATUS_UPDATE,
                0,
                "Current resource availability displayed below."
            );

            break;
        }

        case CommandType::SHUTDOWN:
            shuttingDown = true;
            break;
    }
}


// --------------------------------------------------
// SHUTDOWN
// --------------------------------------------------

void shutdownManager() {
    emitEvent(
        EventType::INFO,
        0,
        "Shutting down workers and releasing resources."
    );

    for (auto& entry : workers) {
        WorkerState& worker = entry.second;

        if (!worker.alive)
            continue;

        WorkerCommand command{};

        command.type = WorkerCommandType::SHUTDOWN_WORKER;

        writeFull(
            worker.controlFd,
            &command,
            sizeof(command)
        );

        if (banker)
            banker->finishProcess(worker.pid);

        usedMemory -= worker.allocatedMemory;
        usedFDs -= worker.allocatedFDs;

        close(worker.controlFd);
        close(worker.eventFd);

        waitpid(worker.pid, nullptr, 0);

        worker.alive = false;
    }

    emitEvent(
        EventType::SHUTDOWN,
        0,
        "Resource manager shut down."
    );

    if (commandFd >= 0)
        close(commandFd);

    if (monitorFd >= 0)
        close(monitorFd);

    unlink(COMMAND_FIFO);
    unlink(MONITOR_FIFO);
}


// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main() {
    if (!createFifo(COMMAND_FIFO) ||
        !createFifo(MONITOR_FIFO)) {
        return 1;
    }

    commandFd = open(
        COMMAND_FIFO,
        O_RDWR | O_NONBLOCK
    );

    monitorFd = open(
        MONITOR_FIFO,
        O_RDWR | O_NONBLOCK
    );

    if (commandFd == -1 || monitorFd == -1) {
        perror("FIFO open failed");
        return 1;
    }

    setCloseOnExec(commandFd);
    setCloseOnExec(monitorFd);

    launchTerminal("./resource_monitor");
    launchTerminal("./user_interface");

    emitEvent(
        EventType::INFO,
        0,
        "Manager started. Waiting for user configuration."
    );

    std::vector<char> commandBuffer;

    while (!shuttingDown) {
        std::vector<pollfd> descriptors;
        std::vector<int> workerIds;

        descriptors.push_back({
            commandFd,
            POLLIN,
            0
        });

        for (auto& entry : workers) {
            WorkerState& worker = entry.second;

            if (!worker.alive)
                continue;

            descriptors.push_back({
                worker.eventFd,
                POLLIN,
                0
            });

            workerIds.push_back(worker.id);
        }

        int result = poll(
            descriptors.data(),
            descriptors.size(),
            100
        );

        if (result < 0) {
            if (errno == EINTR)
                continue;

            perror("Manager poll failed");
            break;
        }

        // ------------------------------------------
        // READ USER COMMANDS
        // ------------------------------------------

        if (descriptors[0].revents & POLLIN) {
            char buffer[4096];

            while (true) {
                ssize_t bytes = read(
                    commandFd,
                    buffer,
                    sizeof(buffer)
                );

                if (bytes > 0) {
                    commandBuffer.insert(
                        commandBuffer.end(),
                        buffer,
                        buffer + bytes
                    );
                }
                else {
                    if (bytes < 0 && errno == EINTR)
                        continue;

                    break;
                }
            }

            while (commandBuffer.size() >= sizeof(UserCommand)) {
                UserCommand command{};

                std::memcpy(
                    &command,
                    commandBuffer.data(),
                    sizeof(UserCommand)
                );

                commandBuffer.erase(
                    commandBuffer.begin(),
                    commandBuffer.begin() + sizeof(UserCommand)
                );

                handleCommand(command);

                if (shuttingDown)
                    break;
            }
        }

        // ------------------------------------------
        // READ WORKER EVENTS
        // ------------------------------------------

        for (size_t i = 1; i < descriptors.size(); i++) {
            if (descriptors[i].revents & POLLIN) {
                handleWorkerEvent(workerIds[i - 1]);
            }
        }

        // ------------------------------------------
        // RETRY PENDING REQUESTS
        // ------------------------------------------

        if (!shuttingDown && banker)
            retryPendingRequests();
    }

    shutdownManager();

    return 0;
}