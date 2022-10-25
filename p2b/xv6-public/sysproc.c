#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// new system calls

int
sys_settickets(void)
{
  int pty;
  if (argint(0, &pty) < 0) {
    return -1;
  }
  if (pty != 0 && pty != 1) {
    return -1;
  }
  return settickets(pty);
}

int
sys_getpinfo(void)
{
  struct pstat *pc;

  if(argptr(0, (void*)&pc, sizeof(struct pstat*)) < 0) {
    return -1;
  }

  return getpinfo(pc);
}

int 
sys_mprotect(void)
{
  int addr, len;
  if (argint(0, &addr) < 0) {
    return -1;
  }
  if (argint(1, &len) < 0) {
    return -1;
  }
  if (len <= 0) {
    return -1;
  }
  if (addr % 4096 != 0) {
    return -1;
  }
  if (addr + len * PGSIZE > myproc()->sz) {
    return -1;
  }

  return mprotect((void*)addr, len);
}

int 
sys_munprotect(void)
{
  int addr, len;
  if (argint(0, &addr) < 0) {
    return -1;
  }
  if (argint(1, &len) < 0) {
    return -1;
  }
  if (len <= 0) {
    return -1;
  }
  if (addr % 4096 != 0) {
    return -1;
  }
  if (addr + len * PGSIZE > myproc()->sz) {
    return -1;
  }
  return munprotect((void*)addr, len);
}
