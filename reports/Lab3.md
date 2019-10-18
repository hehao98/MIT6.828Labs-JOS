# Report for Lab 3, Hao He

## Summary

## Exercise 1

> **Exercise 1.** Modify `mem_init()` in `kern/pmap.c` to allocate and map the `envs` array.

Make `envs` point to an array of size `NENV` of `struct Env`.

```c
envs = boot_alloc(NENV * sizeof(struct Env));
memset(envs, 0, NENV * sizeof(struct Env));
```

Map the `envs` array read-only by the user at linear address `UENVS`.

```c
boot_map_region(kern_pgdir, UENVS, PTSIZE, PADDR(envs), PTE_U);
```

## Exercise 2

> **Exercise 2.** In the file `env.c`, finish coding the following functions.

Again, like previous programming exercises, the code to write is not that much, but you have to be extremely careful to avoid subtle bugs. And when there are bugs, it's hard and painful to debug.

The implementation of `env_init()` is straightforward, but the iteration order is intentionally reversed so that the enviroment in the free list are the same order they are in the envs array.

```c
// env_init()
for (int32_t i = NENV - 1; i >= 0; --i) {
	envs[i].env_id = 0;
	envs[i].env_link = env_free_list;
	env_free_list = &envs[i];
}
```

The implementation of `env_setup_vm()` is a little bit tricky. My implementation just copies contents from kernel directory entries. There is no need to modifiy the permisson bits, though, if the permisson bits have been set correctly.

```c
// env_setup_vm()
p->pp_ref++;
e->env_pgdir = page2kva(p);
for (uint32_t i = PDX(UTOP); i < NPDENTRIES; ++i) {
	e->env_pgdir[i] = kern_pgdir[i];
}
```

To implement `load_icode()` and `region_alloc()`, we need to utilize functions previously implemented in `pmap.c`. Also, we have to deal with corner cases carefully. For `region_alloc()`, I have used `page_alloc()` and `page_insert()` implemented in Lab 2 and also applied a lot of checks to make this code more robust to subtle bugs. FOr `load_icode()`, the implementation is similar to the code in boot loader. For convenience, I first loaded environment page directory and later load kernel page directory back.

The implementation of `env_create()` and `env_run()` can be done by just following the instructions.

## Exercise 3

> **Exercise 3.** Read Chapter 9, Exceptions and Interrupts in the 80386 Programmer's Manual (or Chapter 5 of the IA-32 Developer's Manual), if you haven't already.

The interruption and exception mechanism has been partly covered in CSAPP, and also in the operating system course. The important thing is that an operating system need to set up IDT and write handler programs that follow x86 hardware conventions.

## Exercise 4

> **Exercise 4.** Implement the basic exception handling feature described above.

When implementing code for Exercise 4, there are a number of specific details that we need to pay attention to, like which interrupt has an error code and whether to set the `istrap` flag in IDT descriptor. For setting up IDT and determine which interrupt uses error code, I have found this page(http://wiki.osdev.org/Exceptions) more useful than 80386 manual.

The code is just a duplication of `TRAPHANDLER` and `SETGATE` macros like this.

```

TRAPHANDLER_NOEC(handler_div, T_DIVIDE)
TRAPHANDLER_NOEC(handler_debug, T_DEBUG)
TRAPHANDLER_NOEC(handler_nmi, T_NMI)
TRAPHANDLER_NOEC(handler_brkpt, T_BRKPT)
TRAPHANDLER_NOEC(handler_oflow, T_OFLOW)
TRAPHANDLER_NOEC(handler_bound, T_BOUND)
...
TRAPHANDLER_NOEC(handler_syscall, T_SYSCALL)
TRAPHANDLER_NOEC(handler_default, T_DEFAULT)
```

I have to admit my implementation for this is very ugly, which I decide to improve in the following challenge.

## Questions

> 1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)

Without a seperate handler for each interrupt we have no way to know the interruption number. In x86 there is no hardware mechanism for that, so the operating system has to do this.

> 2. Did you have to do anything to make the `user/softint` program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but softint's code says `int $14`. Why should this produce interrupt vector 13? What happens if the kernel actually allows softint's `int $14` instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

I have not did anything special to make this program behave correctly, because I have set the privilege level of all interruption handler except  syscall and breakpoint to be 0, so any user program that uses `int $14` will trigger an general protection fault. If the user program is allowed to use `int` to trigger interrupts other than system calls, malicious or buggy programs can easily destory the whole system. 

## Exercise 5

> **Exercise 5.** Modify trap_dispatch() to dispatch page fault exceptions to page_fault_handler(). 

Implementation is straightforward.

```c
if (tf->tf_trapno == T_PGFLT) {
	page_fault_handler(tf);
}
```

## Exercise 6

> **Exercise 6.** Modify trap_dispatch() to make breakpoint exceptions invoke the kernel monitor.

Implementation is straightforward.

```c
if (tf->tf_trapno == T_BRKPT) {
	monitor(tf);
}
```

## Questions

> 3. The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the IDT (i.e., your call to SETGATE from trap_init). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?

If the privilege level of the corresponding IDT entry is not set to 3, then `int $3` will generate a general protection fault. In order to allow user programs to use this interrupt you have to set its privilege level to 3 like this.

```
SETGATE(idt[T_BRKPT], true, GD_KT, handler_brkpt, 3);
```

> 4. What do you think is the point of these mechanisms, particularly in light of what the user/softint test program does?

These mechanisms are used to protect the operating system from malicious or buggy user programs. Operating system should not be taken down because of this.

## Challenge

> **Challenge!** Modify the JOS kernel monitor so that you can 'continue' execution from the current location (e.g., after the int3, if the kernel monitor was invoked via the breakpoint exception), and so that you can single-step one instruction at a time. You will need to understand certain bits of the EFLAGS register in order to implement single-stepping. 

I further implemented continue and single step command for kernel monitor with the following code.

```c
int mon_continue(int argc, char **argv, struct Trapframe *tf) 
{
	if (tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG) {
		cprintf("The monitor is not invoked from a breakpoint/debug trap!\n");
		return -1;
	}

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}

int mon_singlestep(int argc, char **argv, struct Trapframe *tf) 
{
	if (tf->tf_trapno != T_BRKPT && tf->tf_trapno != T_DEBUG) {
		cprintf("The monitor is not invoked from a breakpoint/debug trap!\n");
		return -1;
	}

	uint32_t tf_mask = 0x100;
	tf->tf_eflags ^= tf_mask; // flipping TF flag
	if (tf->tf_eflags & tf_mask) {
		cprintf("Enabled Single Step\n");
	} else {
		cprintf("Disabled Single Step\n");
	}

	return 0;
}
```

The implementation of single step utilized the TF flag in the EFLAG control register. By setting it to 1, the processor will automatically generate a `T_DEBUG` interrupt (trap number 1).

To test these commands, I have rewritten `user/breakpoint.c` to the following:

```c
#include <inc/lib.h>

void umain(int argc, char **argv)
{
	asm volatile("int $3");
	asm volatile("addl %eax, %eax");
	asm volatile("addl %eax, %eax");
	asm volatile("addl %eax, %eax");
	asm volatile("int $3");
	asm volatile("addl %eax, %eax");
}
```

Then we can use `continue` and `singlestep` command in the kernel monitor to control the execution of this program. Here is an example run (some output is omitted to make it clearer).

```
$ make run-breakpoint
[00000000] new env 00001000
Incoming TRAP frame at 0xefffffbc
Triggered Breakpoint at 0x00800037
TRAP frame at 0xf01d3000
  trap 0x00000003 Breakpoint
K> singlestep
Enabled Single Step
K> continue
Incoming TRAP frame at 0xefffffbc
Triggered Breakpoint at 0x00800039
TRAP frame at 0xf01d3000
  trap 0x00000001 Debug
K> continue
Incoming TRAP frame at 0xefffffbc
Triggered Breakpoint at 0x0080003b
TRAP frame at 0xf01d3000
  trap 0x00000001 Debug
K> singlestep
Disabled Single Step
K> continue
Incoming TRAP frame at 0xefffffbc
Triggered Breakpoint at 0x0080003e
TRAP frame at 0xf01d3000
  trap 0x00000003 Breakpoint
K> continue
Incoming TRAP frame at 0xefffffbc
TRAP frame at 0xf01d3000
  trap 0x00000030 System call
[00001000] free env 00001000
Destroyed the only environment - nothing more to do!
```

## Exercise 7

> **Exercise 7.** Add a handler in the kernel for interrupt vector T_SYSCALL. You will have to edit kern/trapentry.S and kern/trap.c's trap_init(). You also need to change trap_dispatch() to handle the system call interrupt by calling syscall() (defined in kern/syscall.c) with the appropriate arguments, and then arranging for the return value to be passed back to the user process in %eax. Finally, you need to implement syscall() in kern/syscall.c. Make sure syscall() returns -E_INVAL if the system call number is invalid. You should read and understand lib/syscall.c (especially the inline assembly routine) in order to confirm your understanding of the system call interface. Handle all the system calls listed in inc/syscall.h by invoking the corresponding kernel function for each call. 

To implement syscall, we have to understand the following inline assembly.

```c
asm volatile("int %1\n"
		     : "=a" (ret)
		     : "i" (T_SYSCALL),
		       "a" (num),
		       "d" (a1),
		       "c" (a2),
		       "b" (a3),
		       "D" (a4),
		       "S" (a5)
		     : "cc", "memory");
```

We need to know that this inline assembly has assigned system call number and arguments to these registers, and we can read these registers in kernel from the trap frame like this. We can also return the system call return value by setting `%eax` register in the trap frame.

```c
if (tf->tf_trapno == T_SYSCALL) {
	uint32_t ret = syscall(tf->tf_regs.reg_eax, 
		tf->tf_regs.reg_edx, tf->tf_regs.reg_ecx,
		tf->tf_regs.reg_ebx, tf->tf_regs.reg_edi,
		tf->tf_regs.reg_esi);
	tf->tf_regs.reg_eax = ret;
	return;
}
```

The implementation of the real system calls is fairly straighforward.

```c
switch (syscallno) {
case SYS_cputs:
	sys_cputs((const char *)a1, a2);
	return 0;
case SYS_cgetc:
	return sys_cgetc();
case SYS_getenvid:
	return sys_getenvid();
case SYS_env_destroy:
	return sys_env_destroy(a1);
default:
	return -E_INVAL;
}
```

## Exercise 8

> **Exercise 8.** Add the required code to the user library, then boot your kernel. You should see `user/hello` print "hello, world" and then print "i am environment 00001000". `user/hello` then attempts to "exit" by calling `sys_env_destroy()`.

Modifying `lib/libmain.c` is simple.

```c
thisenv = &envs[ENVX(sys_getenvid())];
```

However, during my implementation process a page fault is still generated at the second `cprintf` in `hello` user program. Why? By using `info pg` in `qemu` simulator I have found that the page directory corresponding to `UENV` has not set the `PTE_U` permission!

```
VPN range     Entry         Flags        Physical page
[eec00-ef3ff]  PDE[3bb-3bc] --------WP
  [eec00-ef3ff]  PTE[000-3ff] -------U-P 001d3-005d2 00193-00592
```

So there is a hidden bug from Lab 2, in `pgdir_walk()`! Although the comments say that we can make permissions in page directory entry more permissive, I have missed to assign `PTE_U` flag to each diectory entry. Therefore I made the following modification.

```c
pgdir[pdx] = page2pa(pi) | PTE_P | PTE_W | PTE_U;
```

Then the `hello` program can pass the test.

