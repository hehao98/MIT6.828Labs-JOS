# Report for Lab 4, Hao He

## Summary

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

What we need to do is to replace existing code in `trap_init_percpu()` to make it work in any CPU. We have to replace `ts` with `thiscpu->cpu_ts`, change the index of gdt to `(GD_TSS0 >> 3) + cpunum()]` to set this CPU's TSS segment. Finally, we have to load this TSS segment for this CPU using `ltr` instruction.

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

What we need to do is to implement a set of system calls using the funtions that we have already implemented in Lab 2 and Lab 3. Since we cannot make any assumption on how user environments use our system call, we have to do a lot of sanity checks on parameters to ensure bad system call will not harm our OS. We can achieve this by following the instructions in `syscall.c`.

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