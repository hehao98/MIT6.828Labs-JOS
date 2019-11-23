# Report for Lab 5, Hao He

## Summary of Results

## Exercise 1

> **Exercise 1.** i386_init identifies the file system environment by passing the type ENV_TYPE_FS to your environment creation function, env_create. Modify env_create in env.c, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment. 

This can be implemented by adding the following code in `env_create()`.

```c
env->env_tf.tf_eflags &= ~FL_IOPL_MASK;
if (type == ENV_TYPE_FS) {
	env->env_tf.tf_eflags |= FL_IOPL_3;
} else {
	env->env_tf.tf_eflags |= FL_IOPL_0;
}
```

## Question 1

> Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why? 

No, because each process has its own `EFLAGS` register, and their `IOPL` bits are no longer modified in other part of kernel code during context switch.

## Exercise 2

> **Exercise 2.** Implement the bc_pgfault and flush_block functions in fs/bc.c. bc_pgfault is a page fault handler, just like the one your wrote in the previous lab for copy-on-write fork, except that its job is to load pages in from the disk in response to a page fault. When writing this, keep in mind that (1) addr may not be aligned to a block boundary and (2) ide_read operates in sectors, not blocks. 

It is very important to keep address rounded to `PGSIZE` and clear the dirty bits after new page loaded or page is flushed to disk.

```c
// In bc_pgfault()
void *rounded_addr = (void *)ROUNDDOWN((uintptr_t)addr, PGSIZE);
r = sys_page_alloc(thisenv->env_id, rounded_addr, PTE_P|PTE_W|PTE_U);
if (r < 0) panic("in bc_pgfault, sys_page_alloc: %e\n", r);
r = ide_read(blockno * BLKSECTS, rounded_addr, BLKSECTS);
if (r < 0) panic("in bc_pgfault, ide_read: %d\n", r);

// Clear the dirty bit for the disk block page since we just read the
// block from disk
if ((r = sys_page_map(0, rounded_addr, 0, rounded_addr, 
		uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
	panic("in bc_pgfault, sys_page_map: %e", r);
```

```c
// In flush_block()
void *rounded_addr = (void *)ROUNDDOWN((uintptr_t)addr, PGSIZE);
int r;
if (!va_is_mapped(addr) || !va_is_dirty(addr)) 
	return;
if ((r = ide_write(blockno * BLKSECTS, rounded_addr, BLKSECTS)) < 0)
	panic("in flush_block(): ide_write: %d\n", r);
if ((r = sys_page_map(0, rounded_addr, 0, rounded_addr, 
		uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
	panic("in flush_block(), sys_page_map: %e", r);
```
