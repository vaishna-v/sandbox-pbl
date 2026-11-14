#pragma once

const int MAX_TRACKED_WORKERS = 32;
const int RESOURCE_TYPES = 2;

enum class CommandType {
    INITIALIZE,
    REGISTER_WORKER,
    REQUEST,
    RELEASE,
    CANCEL,
    STATUS,
    SHUTDOWN
};

struct UserCommand {
    CommandType type;

    int workerId = 0;

    int totalMemoryMB = 0;
    int totalFileDescriptors = 0;
    int workerCount = 0;

    int memoryMB = 0;
    int fileDescriptors = 0;
};

enum class EventType {
    INFO,
    REGISTERED,
    REQUEST_RECEIVED,
    APPROVED,
    REJECTED,
    WAITING,
    RELEASED,
    FINISHED,
    ERROR,
    STATUS_UPDATE,
    SHUTDOWN
};

struct MonitorEvent {
    EventType type;

    int workerId = 0;
    int pid = 0;

    int memoryMB = 0;
    int fileDescriptors = 0;

    int availableMemoryMB = 0;
    int availableFileDescriptors = 0;

    char message[160] = {};

    // Banker's Algorithm snapshot
    int processCount = 0;

    int pids[MAX_TRACKED_WORKERS] = {};
    int workerIds[MAX_TRACKED_WORKERS] = {};

    int maximum[MAX_TRACKED_WORKERS][RESOURCE_TYPES] = {};
    int allocation[MAX_TRACKED_WORKERS][RESOURCE_TYPES] = {};
    int need[MAX_TRACKED_WORKERS][RESOURCE_TYPES] = {};

    int available[RESOURCE_TYPES] = {};

    int safeSequenceCount = 0;
    int safeSequence[MAX_TRACKED_WORKERS] = {};

    int safeState = 0;
};