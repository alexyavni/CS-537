#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "sysfunc.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return proc->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (proc->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_mprotect(void)
{
  void *addr = 0;
  int len;

  if (argptr(0, (void *)(&addr), sizeof(void *)) < 0)
    return -1;

  if (argint(1, &len) < 0)
    return -1;

  if (((uint)addr % PGSIZE) != 0)
  {
    cprintf("not page aligned \n");
    return -1;
  }

  if (len <= 0)
  {
    cprintf("len <= 0 \n");
    return -1;
  }

  if ((uint)addr + len * PGSIZE > proc->sz)
  {
    cprintf("out of bounds \n");
    return -1;
  }

  return mprotect(addr, len);
}

int sys_munprotect(void)
{
  void *addr = 0;
  int len;

  if (argptr(0, (void *)(&addr), sizeof(void *)) < 0)
    return -1;

  if (argint(1, &len) < 0)
    return -1;

  if (((uint)addr % PGSIZE) != 0)
  {
    cprintf("not page aligned \n");
    return -1;
  }

  if (len <= 0)
  {
    cprintf("len <= 0 \n");
    return -1;
  }

  if ((uint)addr + len * PGSIZE > proc->sz)
  {
    cprintf("out of bounds \n");
    return -1;
  }

  return munprotect(addr, len);
}

int sys_dump_allocated(void)
{
  int *frames = 0;
  int numframes = 0;

  if (argptr(0, (void *)(&frames), sizeof(void *)) < 0)
    return -1;

  if (argint(1, &numframes) < 0)
    return -1;
  return dump_allocated(frames, numframes);
}