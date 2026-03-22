#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"
#include "vm.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
    debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
    if (fd != STDOUT)
        return -1;

    struct proc *p = curr_proc();
    char str[MAX_STR_LEN];
    int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
    debugf("size = %d", size);

    for (int i = 0; i < size; ++i) {
        console_putchar(str[i]);
    }
    return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
    exit(code);
    __builtin_unreachable();
}

uint64 sys_sched_yield()
{
    yield();
    return 0;
}

uint64 sys_gettimeofday(uint64 va, int _tz)
{
    if (va == 0)
        return (uint64)-1;

    struct proc *p = curr_proc();
    uint64 pa = useraddr(p->pagetable, va);
    if (pa == 0)
        return (uint64)-1;

    TimeVal *val = (TimeVal *)pa;
    uint64 cycle = get_cycle();
    val->sec = cycle / CPU_FREQ;
    val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
    return 0;
}

static TaskStatus map_status(enum procstate s)
{
    switch (s) {
    case UNUSED:   return UnInit;
    case USED:     return Ready;
    case RUNNABLE: return Ready;
    case RUNNING:  return Running;
    case SLEEPING: return Ready;
    case ZOMBIE:   return Exited;
    default:       return UnInit;
    }
}

uint64 sys_task_info(uint64 va)
{
    if (va == 0)
        return (uint64)-1;

    struct proc *p = curr_proc();
    uint64 pa = useraddr(p->pagetable, va);
    if (pa == 0)
        return (uint64)-1;

    struct TaskInfo *ti = (struct TaskInfo *)pa;
    ti->status = map_status(p->state);

    for (int i = 0; i < MAX_SYSCALL_NUM; i++) {
        ti->syscall_times[i] = p->syscall_times[i];
    }

    uint64 elapsed = get_cycle() - p->start_tick;
    ti->time = (int)(elapsed * 1000 / CPU_FREQ);

    infof("pid=%d write_count=%d", p->pid, p->syscall_times[64]);

    return 0;
}

uint64 sys_getpid(void)
{
    return (uint64)curr_proc()->pid;
}

uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
    if (len == 0)
        return 0;
    if (len > 1024 * 1024 * 1024ULL)
        return (uint64)-1;
    if (port & ~0x7)
        return (uint64)-1;
    if ((port & 0x7) == 0)
        return (uint64)-1;
    if (start % PAGE_SIZE != 0)
        return (uint64)-1;

    struct proc *p = curr_proc();
    uint64 npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64 i = 0; i < npages; i++) {
        uint64 va = start + i * PAGE_SIZE;
        if (walkaddr(p->pagetable, va) != 0)
            return (uint64)-1;
    }

    int perm = PTE_U;
    if (port & 1) perm |= PTE_R;
    if (port & 2) perm |= PTE_W;
    if (port & 4) perm |= PTE_X;

    for (uint64 i = 0; i < npages; i++) {
        uint64 va = start + i * PAGE_SIZE;
        void *mem = kalloc();
        if (mem == 0)
            return (uint64)-1;
        memset(mem, 0, PAGE_SIZE);
        if (mappages(p->pagetable, va, PAGE_SIZE, (uint64)mem, perm) != 0) {
            kfree(mem);
            return (uint64)-1;
        }
    }
    return 0;
}

uint64 sys_munmap(uint64 start, uint64 len)
{
    if (len == 0)
        return 0;
    if (start % PAGE_SIZE != 0)
        return (uint64)-1;

    struct proc *p = curr_proc();
    uint64 npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64 i = 0; i < npages; i++) {
        uint64 va = start + i * PAGE_SIZE;
        if (walkaddr(p->pagetable, va) == 0)
            return (uint64)-1;
    }

    uvmunmap(p->pagetable, start, npages, 1);
    return 0;
}

extern char trap_page[];

void syscall()
{
    struct trapframe *trapframe = curr_proc()->trapframe;
    int id = trapframe->a7, ret;
    uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
                       trapframe->a3, trapframe->a4, trapframe->a5 };

    tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]",
           id, args[0], args[1], args[2], args[3], args[4], args[5]);

    struct proc *p = curr_proc();
    if (id >= 0 && id < MAX_SYSCALL_NUM) {
        p->syscall_times[id]++;
    }

    switch (id) {
    case SYS_write:
        ret = sys_write(args[0], args[1], args[2]);
        break;
    case SYS_exit:
        sys_exit(args[0]);
    case SYS_sched_yield:
        ret = sys_sched_yield();
        break;
    case SYS_gettimeofday:
        ret = sys_gettimeofday(args[0], args[1]);
        break;
    case SYS_taskinfo:
        ret = sys_task_info(args[0]);
        break;
    case SYS_getpid:
        ret = sys_getpid();
        break;
    case SYS_mmap:
        ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
        break;
    case SYS_munmap:
        ret = sys_munmap(args[0], args[1]);
        break;
    default:
        ret = -1;
        errorf("unknown syscall %d", id);
    }

    trapframe->a0 = ret;
    tracef("syscall ret %d", ret);
}