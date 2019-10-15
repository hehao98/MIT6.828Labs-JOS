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

    


