#pragma once

enum class WorkerCommandType {
    START_WORK,
    SHUTDOWN_WORKER
};

struct WorkerCommand {
    WorkerCommandType type;
};

enum class WorkerEventType {
    WORK_COMPLETED,
    WORK_ERROR
};

struct WorkerEvent {
    WorkerEventType type;
};