#ifndef TASKINFO_H
#define TASKINFO_H

#include "types.h"
#include "proc.h"   // for MAX_SYSCALL_NUM

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_ZOMBIE,
} TaskStatus;

struct TaskInfo {
    TaskStatus status;
    unsigned int syscall_times[MAX_SYSCALL_NUM];
    int time;
};

#endif // TASKINFO_H