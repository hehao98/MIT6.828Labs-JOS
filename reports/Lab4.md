# Report for Lab 4, Hao He

## Summary

All exercises finished and all questions answered. The output of `make grade` is the following.

```
dumbfork: OK (1.0s) 
Part A score: 5/5

faultread: OK (1.0s) 
faultwrite: OK (1.8s) 
faultdie: OK (1.2s) 
faultregs: OK (1.7s) 
faultalloc: OK (2.1s) 
faultallocbad: OK (2.2s) 
faultnostack: OK (1.8s) 
faultbadhandler: OK (2.1s) 
faultevilhandler: OK (2.1s) 
forktree: OK (1.7s) 
Part B score: 50/50

spin: OK (2.2s) 
stresssched: OK (2.3s) 
sendpage: OK (1.6s) 
pingpong: OK (2.1s) 
primes: OK (2.2s) 
Part C score: 25/25

Score: 80/80
```

For challenge, I choose to implement fixed priority scheduling. I also implemented a a user program that set itself to higher priority and then creates a child process. We can see that after that the child process can only be executed after the parent process has finished.

```
[00000000] new env 00001000
[00001000] new env 00001001
Spinning parent...
Spinning parent...
Spinning parent...
Spinning parent...
Spinning parent...
[00001000] exiting gracefully
[00001000] free env 00001000
Spinning child...
Spinning child...
Spinning child...
Spinning child...
Spinning child...
[00001001] exiting gracefully
[00001001] free env 00001001
```

For more detailed information please see the challenge section of this report.

## Exercise 1

> **Exercise 1.** Implement mmio_map_region in kern/pmap.c. To see how this is used, look at the beginning of lapic_init in kern/lapic.c. You'll have to do the next exercise, too, before the tests for mmio_map_region will run. 

My implementation is the following, by following the instructions in source code comments. I have added an additional assert to make sure that `pa` is aligned to `PGSIZE`

```c
void *mmio_map_region(physaddr_t pa, size_t size)
{
	static uintptr_t base = MMIOBASE;
	assert(pa % PGSIZE == 0);
	size = ROUNDUP(size, PGSIZE);
	if (base + size >= MMIOLIM) {
		panic("mmio_map_region() overflows MMIOLIM!\n");
	}
	boot_map_region(kern_pgdir, base, size, pa, PTE_PCD | PTE_PWT | PTE_W);
	base += size;
	return (void *)base - size;;
}
```

## Exercise 2

> **Exercise 2.** Read boot_aps() and mp_main() in kern/init.c, and the assembly code in kern/mpentry.S. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of page_init() in kern/pmap.c to avoid adding the page at MPENTRY_PADDR to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated check_page_free_list() test (but might fail the updated check_kern_pgdir() test, which we will fix soon). 

It is implemented by just avoid adding the physical page at `MPENTRY_ADDR` to free page list.

## Question 1

> Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S?

The problem is that, code in `kern/mpentry.S` are moved to a different physical address `MPENTRY_ADDR` before executing in `boot_aps()`.

```c
code = KADDR(MPENTRY_PADDR);
memmove(code, mpentry_start, mpentry_end - mpentry_start);
```

Therefore, the link address and load address of `kern/mpentry.S` are different, so we cannot rely on the linker to calculate variable addresses for us, and we have to do the relocation manually using `MPBOOTPHYS`. Also, because paging has not been enabled on the processor executing `kern/mpentry.S`, so the macro calculates the physical address, not virtual address.

## Exercise 3

> **Exercise 3**. Modify mem_init_mp() (in kern/pmap.c) to map per-CPU stacks starting at KSTACKTOP, as shown in inc/memlayout.h. The size of each stack is KSTKSIZE bytes plus KSTKGAP bytes of unmapped guard pages. Your code should pass the new check in check_kern_pgdir(). 

```c
static void mem_init_mp(void)
{
	for (int i = 0; i < NCPU; ++i) {
		uintptr_t stk = KSTACKTOP - i * (KSTKSIZE + KSTKGAP) - KSTKSIZE;
		boot_map_region(kern_pgdir, stk, KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);
	}
}
```

## Exercise 4

> **Exercise 4.** The code in trap_init_percpu() (kern/trap.c) initializes the TSS and TSS descriptor for the BSP. It worked in Lab 3, but is incorrect when running on other CPUs. Change the code so that it can work on all CPUs. (Note: your new code should not use the global ts variable any more.) 

What we need to do is to replace existing code in `trap_init_percpu()` to make it work in any CPU. We have to replace `ts` with `thiscpu->cpu_ts`, change the index of GDT to `(GD_TSS0 >> 3) + cpunum()]` to set this CPU's TSS segment. Finally, we have to load this TSS segment for this CPU using `ltr` instruction.

```c
void trap_init_percpu(void)
{
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpunum() * (KSTKSIZE + KSTKGAP);
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

	gdt[(GD_TSS0 >> 3) + cpunum()] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + cpunum()].sd_s = 0;

	ltr(GD_TSS0 + (cpunum() << 3));

	lidt(&idt_pd);
}
```

## Exercise 5

> **Exercise 5.** Apply the big kernel lock as described above, by calling lock_kernel() and unlock_kernel() at the proper locations. 

It is just about simply following instructions in the Lab writeup. Nothing special to pay attention to.

## Question 2

> It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock. 

When one CPU is processing an interrupt using its stack kernel, another CPU might also received an interrupt and enter into the kernel interrupt handler. If they concurrently use the same kernel stack to store interrupt, the stack will be corrupted even with the big kernel lock, because the lock cannot assure that only one CPU at a time can enter the kernel using interrupt.

## Exercise 6

> **Exercise 6.** Implement round-robin scheduling in sched_yield() as described above. Don't forget to modify syscall() to dispatch sys_yield(). 

Here is my round-robin scheduling code

```c
void sched_yield(void)
{
	struct Env *idle;

	// LAB 4: Your code here.
	envid_t curid = curenv ? (ENVX(curenv->env_id) + 1) % NENV : 0;
	for (uint32_t i = 0; i < NENV; ++i) {
		envid_t id = (curid + i) % NENV;
		if (envs[id].env_status == ENV_RUNNABLE) {
			env_run(&envs[id]);
		}
	}

	if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	}

	// sched_halt never returns
	sched_halt();
}
```

The tricky part is that you have to handle the two cases taht `curenv` exists and `curenv` does not exists. The above solution is a graceful way to do that.

## Question 3

> In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

Because when setting up page tables for a user environment, we have simply copied all kernel space address mappings to the user page table in `env_setup_vm()`.

```c
for (uint32_t i = PDX(UTOP); i < NPDENTRIES; ++i) {
	e->env_pgdir[i] = kern_pgdir[i];
}
```

So when we are in kernel mode, whatever user page table is loaded, the kernel code can always access kernel data like using the kernel page table.

## Question 4

> Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

The kernel have to ensure all the registers of a user environment are saved properly for it to execute later. Since all user environment must enter kernel mode using an interrupt, so all the register saving work has already been done in interrupt handling code (`trapentry.S` and `trap.c`).

## Exercise 7

> **Exercise 7.** Implement the system calls described above in kern/syscall.c and make sure syscall() calls them. You will need to use various functions in kern/pmap.c and kern/env.c, particularly envid2env(). For now, whenever you call envid2env(), pass 1 in the checkperm parameter. Be sure you check for any invalid system call arguments, returning -E_INVAL in that case. Test your JOS kernel with user/dumbfork and make sure it works before proceeding. 

What we need to do is to implement a set of system calls using the functions that we have already implemented in Lab 2 and Lab 3. Since we cannot make any assumption on how user environments use our system call, we have to do a lot of sanity checks on parameters to ensure bad system call will not harm our OS. We can achieve this by following the instructions in `syscall.c`.

After finishing all the specified system calls, the output of `user/dumbfork` are the following with 1 CPU.

```
[00000000] new env 00001000
[00001000] new env 00001001
0: I am the parent!
0: I am the child!
1: I am the parent!
1: I am the child!
2: I am the parent!
2: I am the child!
3: I am the parent!
3: I am the child!
4: I am the parent!
4: I am the child!
5: I am the parent!
5: I am the child!
6: I am the parent!
6: I am the child!
7: I am the parent!
7: I am the child!
8: I am the parent!
8: I am the child!
9: I am the parent!
9: I am the child!
[00001000] exiting gracefully
[00001000] free env 00001000
10: I am the child!
11: I am the child!
12: I am the child!
13: I am the child!
14: I am the child!
15: I am the child!
16: I am the child!
17: I am the child!
18: I am the child!
19: I am the child!
[00001001] exiting gracefully
[00001001] free env 00001001
```

## Exercise 8

> **Exercise 8.** Implement the sys_env_set_pgfault_upcall system call. Be sure to enable permission checking when looking up the environment ID of the target environment, since this is a "dangerous" system call. 

```c
static int sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	struct Env *e;
	int32_t ret;
	if ((ret = envid2env(envid, &e, 1)) < 0) {
		return ret; // -E_BAD_ENV
	}
	e->env_pgfault_upcall = func;
	return 0;
}
```

## Exercise 9

> **Exercise 9.** Implement the code in page_fault_handler in kern/trap.c required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?) 

```c
struct UTrapframe *utf;
if (curenv->env_pgfault_upcall != NULL) {
	if (UXSTACKTOP - PGSIZE <= tf->tf_esp && tf->tf_esp <=UXSTACKTOP - 1) {
		utf = (struct UTrapframe *)(tf->tf_esp - sizeof(structUTrapframe) - 4);
	} else {	
		utf = (struct UTrapframe *)(UXSTACKTOP - sizeof(structUTrapframe));
	}
	user_mem_assert(curenv, utf, sizeof(struct UTrapframe), PTE_W);
	if ((uintptr_t)utf > UXSTACKTOP - PGSIZE) {
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;
		utf->utf_eip = tf->tf_eip;
		utf->utf_regs = tf->tf_regs;
		utf->utf_err = tf->tf_err;
		utf->utf_fault_va = fault_va;
		curenv->env_tf.tf_esp = (uintptr_t)utf;
		curenv->env_tf.tf_eip = (uintptr_tcurenv->env_pgfault_upcall;
		env_run(curenv);
	}
}
// Destroy the environment that caused the fault.
cprintf("[%08x] user fault va %08x ip %08x\n",
	curenv->env_id, fault_va, tf->tf_eip);
print_trapframe(tf);
env_destroy(curenv);
```

In my implementation, the kernel will check user exception stack memory if `cur->env_pgfault_upcall` is not null, and write to this stack. The position of this user trap frame depends on whether it is a recursive trap or not. When user environment run out of space on the exception stack, the kernel will just kill this environment.

One of the funny thing is that I have to write the `user_mem_assert()` in exactly the way above in order to pass tests. The tests do not allow other kind of memory assert (like asserting the availability of the whole exception stack page).

## Exercise 10

> **Exercise 10.** Implement the _pgfault_upcall routine in lib/pfentry.S. The interesting part is returning to the original point in the user code that caused the page fault. You'll return directly there, without going back through the kernel. The hard part is simultaneously switching stacks and re-loading the EIP. 

First, set up return address in the original stack.

```
movl 48(%esp), %eax // trap-time esp
subl $4, %eax
movl 40(%esp), %ebx // trap-time eip
movl %ebx, (%eax)
movl %eax, 48(%esp)
```

Then restore all registers and return. `%esp` can be safely modified before `EFLAGS` is restored.

```
addl $8, %esp
popal
addl $4, %esp
popfl
popl %esp
ret
```

## Exercise 11

> **Exercise 11.** Finish set_pgfault_handler() in lib/pgfault.c. 

```c
envid_t envid = sys_getenvid();
sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P);
sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
```

It's ok not to check the return values of these system calls here, since if any of them fails, the environment will just be killed. Also, the page fault up call should be set to `_pgfault_upcall`, not `handler`.

## Exercise 12

> **Exercise 12.** Implement fork, duppage and pgfault in lib/fork.c. 

The implementation of fork in user space is tricky. First, I have to implement the page fault handler as specified in the comments. I have to round down `addr` for using it in the code or it may cause subtle bugs that are very hard to locate.

Next, in the `duppage()` function, I need to copy the page mapping carefully using `sys_page_map()` on both the source environment and the destination environment.

Finally, in the `fork()` function, I have to do all the actual page mapping and copying of other information.

## Exercise 13

> **Exercise 13.** Modify kern/trapentry.S and kern/trap.c to initialize the appropriate entries in the IDT and provide handlers for IRQs 0 through 15. Then modify the code in env_alloc() in kern/env.c to ensure that user environments are always run with interrupts enabled. Also uncomment the sti instruction in sched_halt() so that idle CPUs unmask interrupts. 

The implementation is just inserting and copy pasting a lot of interrupt handling functions. However, we have to set all IDT entry to be an interrupt gate, so that IF is reset when entering the kernel. Otherwise, the kernel will panic in `trap.c`!

## Exercise 14

> **Exercise 14.** Modify the kernel's trap_dispatch() function so that it calls sched_yield() to find and run a different environment whenever a clock interrupt takes place. 

The code to be added is dead simple.

```c
if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
	lapic_eoi();
	sched_yield();
}
```

I have also run a regression test, but found that `stresssched` is not working. After debugging I found that it's because I have not set `thisenv` correctly in `lib/fork.c` for the child process.

## Exercise 15

> **Exercise 15.** Implement sys_ipc_recv and sys_ipc_try_send in kern/syscall.c. Read the comments on both before implementing them, since they have to work together. When you call envid2env in these routines, you should set the checkperm flag to 0, meaning that any environment is allowed to send IPC messages to any other environment, and the kernel does no special permission checking other than verifying that the target envid is valid. 

When implementing IPC, you have to follow the instructions in the comments carefully. The `sys_ipc_recv()` is interesting because it never returns. When the environment finally receives something, the kernel will run it in `sched_yield()` to user mode, so you have to manually set its return value to zero in the `sys_ipc_recv` implementation.

```c
static int sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if ((uintptr_t)dstva < UTOP && (uintptr_t)dstva % PGSIZE != 0) {
		return -E_INVAL;
	}
	if ((uintptr_t)dstva < UTOP)
		curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_recving = true;
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_tf.tf_regs.reg_eax = 0;
	sys_yield();
	return 0;
}
```

After implementing this exercise I can achieve full score by running make grade.

```
dumbfork: OK (1.0s) 
Part A score: 5/5

faultread: OK (1.0s) 
faultwrite: OK (1.8s) 
faultdie: OK (1.2s) 
faultregs: OK (1.7s) 
faultalloc: OK (2.1s) 
faultallocbad: OK (2.2s) 
faultnostack: OK (1.8s) 
faultbadhandler: OK (2.1s) 
faultevilhandler: OK (2.1s) 
forktree: OK (1.7s) 
Part B score: 50/50

spin: OK (2.2s) 
stresssched: OK (2.3s) 
sendpage: OK (1.6s) 
pingpong: OK (2.1s) 
primes: OK (2.2s) 
Part C score: 25/25

Score: 80/80
```

## Challenge!

> **Challenge!** Add a less trivial scheduling policy to the kernel, such as a fixed-priority scheduler that allows each environment to be assigned a priority and ensures that higher-priority environments are always chosen in preference to lower-priority environments.

I choose to implement fix priority scheduling as described above. So I have done the following things:

1. Add a new field in `struct Env` called `env_priority`
2. Initialize this field to 0 when creating process
3. Add a new system call `sys_env_set_priority` to set priority of a given process
4. Modify `kern/sched.c` to implement fix priority scheduling
5. Create a test program to test that the new scheduler can handle priority values.
6. Run `make grade` again to ensure that the new scheduler can still handle all the test programs correctly.

The new scheduling code is this

```c
void sched_yield(void)
{
	struct Env *idle;
	envid_t curid = curenv ? (ENVX(curenv->env_id)) % NENV : 0;
	envid_t id2run = -1;

	if (curenv && curenv->env_status == ENV_RUNNING) {
		id2run = curid;
	}
	
	for (uint32_t i = 0; i < NENV; ++i) {
		envid_t id = (curid + i) % NENV;
		if (envs[id].env_status == ENV_RUNNABLE) {
			if (id2run == -1
				|| envs[id].env_priority >= envs[id2run].env_priority) {
				id2run = id;
			}
		}
	}

	if (id2run != -1) {
		// cprintf("cpunum: %d running %d(priority: %d)\n", 
		 	// cpunum(), id2run, envs[id2run].env_priority);
		env_run(&envs[id2run]);
	}

	// sched_halt never returns
	sched_halt();
}
```

The user test program I used is this

```c
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	envid_t child;
    int ret;

    ret = sys_env_set_priority(thisenv->env_id, 1);
     if (ret < 0) {
        panic("sys_env_set_priority: failed\n");
    }

	if ((child = fork()) == 0) {
		// child
        for (int i = 0; i < 5; ++i) {
		    cprintf("Spinning child...\n");
            sys_yield();
        }
        return;
	}

	// parent
    for (int i = 0; i < 5; ++i) {
        cprintf("Spinning parent...\n");
        sys_yield();
    }
    return;
}
```

By running `make run-priority` we can see this

```
hehao@hehao-lenovo:~/Desktop/MIT6.828/Labs$ make run-priority
make[1]: Entering directory '/home/hehao/Desktop/MIT6.828/Labs'
+ cc kern/init.c
+ ld obj/kern/kernel
+ mk obj/kern/kernel.img
make[1]: Leaving directory '/home/hehao/Desktop/MIT6.828/Labs'
qemu-system-i386 -drive file=obj/kern/kernel.img,index=0,media=disk,format=raw -serial mon:stdio -gdb tcp::26000 -D qemu.log -smp 1 
Physical memory: 131072K available, base = 640K, extended = 130432K
boot_alloc(): nextfree=f0284000
boot_alloc(): nextfree=f02c4000
boot_alloc(): nextfree=f02e4000
boot_alloc(): nextfree=f02e4000
boot_alloc(): nextfree=f02e4000
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
boot_alloc(): nextfree=f02e4000
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
[00001000] new env 00001001
Spinning parent...
Spinning parent...
Spinning parent...
Spinning parent...
Spinning parent...
[00001000] exiting gracefully
[00001000] free env 00001000
Spinning child...
Spinning child...
Spinning child...
Spinning child...
Spinning child...
[00001001] exiting gracefully
[00001001] free env 00001001
No runnable environments in the system!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K> 
```

We can see that the parent execute before the child because its priority is higher. You can run this over and over and still get this result.

Then run `make grade` to ensure that everything else is still working.

```
hehao@hehao-lenovo:~/Desktop/MIT6.828/Labs$ make grade
make clean
make[1]: Entering directory '/home/hehao/Desktop/MIT6.828/Labs'
rm -rf obj .gdbinit jos.in qemu.log
make[1]: Leaving directory '/home/hehao/Desktop/MIT6.828/Labs'
./grade-lab4 
make[1]: Entering directory '/home/hehao/Desktop/MIT6.828/Labs'
+ as kern/entry.S
+ cc kern/entrypgdir.c
+ cc kern/init.c
+ cc kern/console.c
+ cc kern/monitor.c
+ cc kern/pmap.c
+ cc kern/env.c
+ cc kern/kclock.c
+ cc kern/picirq.c
+ cc kern/printf.c
+ cc kern/trap.c
+ as kern/trapentry.S
+ cc kern/sched.c
+ cc kern/syscall.c
+ cc kern/kdebug.c
+ cc lib/printfmt.c
+ cc lib/readline.c
+ cc lib/string.c
+ as kern/mpentry.S
+ cc kern/mpconfig.c
+ cc kern/lapic.c
+ cc kern/spinlock.c
+ cc[USER] lib/console.c
+ cc[USER] lib/libmain.c
+ cc[USER] lib/exit.c
+ cc[USER] lib/panic.c
+ cc[USER] lib/printf.c
+ cc[USER] lib/printfmt.c
+ cc[USER] lib/readline.c
+ cc[USER] lib/string.c
+ cc[USER] lib/syscall.c
+ cc[USER] lib/pgfault.c
+ as[USER] lib/pfentry.S
+ cc[USER] lib/fork.c
+ cc[USER] lib/ipc.c
+ ar obj/lib/libjos.a
ar: creating obj/lib/libjos.a
+ cc[USER] user/hello.c
+ as[USER] lib/entry.S
+ ld obj/user/hello
+ cc[USER] user/buggyhello.c
+ ld obj/user/buggyhello
+ cc[USER] user/buggyhello2.c
+ ld obj/user/buggyhello2
+ cc[USER] user/evilhello.c
+ ld obj/user/evilhello
+ cc[USER] user/testbss.c
+ ld obj/user/testbss
+ cc[USER] user/divzero.c
+ ld obj/user/divzero
+ cc[USER] user/breakpoint.c
+ ld obj/user/breakpoint
+ cc[USER] user/softint.c
+ ld obj/user/softint
+ cc[USER] user/badsegment.c
+ ld obj/user/badsegment
+ cc[USER] user/faultread.c
+ ld obj/user/faultread
+ cc[USER] user/faultreadkernel.c
+ ld obj/user/faultreadkernel
+ cc[USER] user/faultwrite.c
+ ld obj/user/faultwrite
+ cc[USER] user/faultwritekernel.c
+ ld obj/user/faultwritekernel
+ cc[USER] user/idle.c
+ ld obj/user/idle
+ cc[USER] user/yield.c
+ ld obj/user/yield
+ cc[USER] user/dumbfork.c
+ ld obj/user/dumbfork
+ cc[USER] user/stresssched.c
+ ld obj/user/stresssched
+ cc[USER] user/faultdie.c
+ ld obj/user/faultdie
+ cc[USER] user/faultregs.c
+ ld obj/user/faultregs
+ cc[USER] user/faultalloc.c
+ ld obj/user/faultalloc
+ cc[USER] user/faultallocbad.c
+ ld obj/user/faultallocbad
+ cc[USER] user/faultnostack.c
+ ld obj/user/faultnostack
+ cc[USER] user/faultbadhandler.c
+ ld obj/user/faultbadhandler
+ cc[USER] user/faultevilhandler.c
+ ld obj/user/faultevilhandler
+ cc[USER] user/forktree.c
+ ld obj/user/forktree
+ cc[USER] user/sendpage.c
+ ld obj/user/sendpage
+ cc[USER] user/spin.c
+ ld obj/user/spin
+ cc[USER] user/fairness.c
+ ld obj/user/fairness
+ cc[USER] user/pingpong.c
+ ld obj/user/pingpong
+ cc[USER] user/pingpongs.c
+ ld obj/user/pingpongs
+ cc[USER] user/primes.c
+ ld obj/user/primes
+ cc[USER] user/priority.c
+ ld obj/user/priority
+ ld obj/kern/kernel
+ as boot/boot.S
+ cc -Os boot/main.c
+ ld boot/boot
boot block is 415 bytes (max 510)
+ mk obj/kern/kernel.img
make[1]: Leaving directory '/home/hehao/Desktop/MIT6.828/Labs'
dumbfork: OK (1.4s) 
Part A score: 5/5

faultread: OK (2.0s) 
faultwrite: OK (1.9s) 
faultdie: OK (2.2s) 
faultregs: OK (2.1s) 
faultalloc: OK (1.8s) 
faultallocbad: OK (2.1s) 
faultnostack: OK (1.9s) 
faultbadhandler: OK (2.1s) 
faultevilhandler: OK (2.1s) 
forktree: OK (2.0s) 
Part B score: 50/50

spin: OK (1.9s) 
stresssched: OK (2.1s) 
sendpage: OK (1.9s) 
pingpong: OK (2.1s) 
primes: OK (3.3s) 
Part C score: 25/25

Score: 80/80
```