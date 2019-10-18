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

I have not did anything special to make this program behave correctly, because I have set the privilege level of all interruption handler except the syscall handler to be 0, so any user program that uses `int $14` will trigger an general protection fault. If the user program is allowed to use `int` to trigger interrupts other than system calls, malicious or buggy programs can easily destory the whole system. 

## Challenge