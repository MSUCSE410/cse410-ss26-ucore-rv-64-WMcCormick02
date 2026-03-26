#ifndef MAX_SYSCALL_NUM
#define MAX_SYSCALL_NUM 512
#endif

typedef enum {
    UnInit = 0,
    Ready = 1,
    Running = 2,
    Exited = 3,
} TaskStatus;


typedef struct {
    TaskStatus status;
    unsigned int syscall_times[MAX_SYSCALL_NUM];
    int time;
} TaskInfo;