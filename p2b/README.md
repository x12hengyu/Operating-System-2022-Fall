## Project 2b: Scheduling and Virtual Memory in xv6

Xizheng Yu, Oct 24, 2022

Codes below are my solutions to this project. Including implementation to priority-based scheduler (1 or 0 ticket), Null-pointer Dereference, and Read-only Code.

## Prerequsites

### pstat.h
```
#ifndef _PSTAT_H_
#define _PSTAT_H_

#include "param.h"

struct pstat {
  int inuse[NPROC];   // whether this slot of the process table is in use (1 or 0)
  int tickets[NPROC]; // the number of tickets this process has
  int pid[NPROC];     // the PID of each process 
  int ticks[NPROC];   // the number of ticks each process has accumulated 
};

#endif // _PSTAT_H_
```

### user.h
```
int settickets(int); // new system call scheduler
int getpinfo(struct pstat*); // new system call for scheduler
int mprotect(void*, int); // new system call for vm
int munprotect(void*, int); // new system call for vm
```

### defs.h
```
int             settickets(int);
int             getpinfo(struct pstat*);

int             mprotect(void *, int);
int             munprotect(void *, int);
```

### usys.S
```
SYSCALL(settickets)
SYSCALL(getpinfo)
SYSCALL(mprotect)
SYSCALL(munprotect)
```

### syscall.c
```
extern int sys_settickets(void);
extern int sys_getpinfo(void);
extern int sys_mprotect(void);
extern int sys_munprotect(void);

[SYS_settickets]  sys_settickets,
[SYS_getpinfo]  sys_getpinfo,
[SYS_mprotect]  sys_mprotect,
[SYS_munprotect]  sys_munprotect,
```

### syscall.h
```
#define SYS_settickets  22
#define SYS_getpinfo    23
#define SYS_mprotect    24
#define SYS_munprotect  25
```

## Function Implementation

### proc.h
```
// in proc struct
int ticket;  // Process ticket, 1 or 0, 1 for high priority
int ticks;   // Process ticks
```

### sysproc.c
```
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
```

### proc.c
`p->ticket = 1;` in `allocproc(void)`<br>
`np->ticket = curproc->ticket;` in `fork(void)`
```
void
scheduler(void)
{
  struct proc *p = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    // find if at least one high
    int high = 0;
    for(struct proc *cur_p = ptable.proc; cur_p < &ptable.proc[NPROC]; cur_p++) {
      if(cur_p->state == RUNNABLE && cur_p->ticket == 1) {
        high = 1;
        break;
      }
    }

    if(high) {
      // run high
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        // find first high priority
        if(p->state == RUNNABLE && p->ticket == 1) {
          // Switch to chosen process.  It is the process's job
          // to release ptable.lock and then reacquire it
          // before jumping back to us.
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
          p->ticks++;
          swtch(&(c->scheduler), p->context);
          switchkvm();

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
        }
      }

    } else {
      // run all
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p->state != RUNNABLE)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->ticks++;
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }

    release(&ptable.lock);
  }
}

int
settickets(int number)
{
  acquire(&ptable.lock);
  myproc()->ticket = number;
  release(&ptable.lock);
  return 0;
}

int 
getpinfo(struct pstat *pc)
{
  if (pc == 0) {
    return -1;
  }

  struct proc *p;
  int i = 0;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++, i++)
  {
    pc->pid[i] = p->pid;
    pc->tickets[i] = p->ticket;
    pc->ticks[i] = p->ticks;
    pc->inuse[i] = p->state == UNUSED ? 0 : 1;
  }

  release(&ptable.lock);
  return 0;
}
```

### vm.c
```
int 
mprotect(void *addr, int len)
{
  pte_t *pte;
  int i = (int)addr;
  int max = (int)addr + len * PGSIZE;
  while (i < max) {
    pte = walkpgdir(myproc()->pgdir, (void *) i, 0);
    *pte &= ~PTE_W; // make write bit to 0
    i += PGSIZE;
  }
  lcr3(V2P(myproc()->pgdir));
  return 0;
}

int
munprotect(void *addr, int len)
{
  pte_t *pte;
  int i = (int)addr;
  int max = (int)addr + len * PGSIZE;
  while (i < max) {
    pte = walkpgdir(myproc()->pgdir, (void *) i, 0);
    *pte |= PTE_W; // make write bit to 0
    i += PGSIZE;
  }
  lcr3(V2P(myproc()->pgdir));
  return 0;
}
```

### Null-pointer Dereference
`sz = 4096` in `exec.c`<br>
`i = 4096` in `copyuvm(pde_t *pgdir, uint sz)` from `vm.c`<br>
`$(LD) $(LDFLAGS) -N -e main -Ttext 0x1000 -o $@ $^` on line 149 in `Makefile`