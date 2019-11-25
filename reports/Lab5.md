# Report for Lab 5, Hao He

## Summary of Results

All exercises finished and questions answered. By running `make grade` we can get the following output.

```
hehao@hehao-lenovo:~/Desktop/MIT6.828/Labs$ make grade
make clean
make[1]: Entering directory '/home/hehao/Desktop/MIT6.828/Labs'
rm -rf obj .gdbinit jos.in qemu.log
make[1]: Leaving directory '/home/hehao/Desktop/MIT6.828/Labs'
./grade-lab5 
make[1]: Entering directory '/home/hehao/Desktop/MIT6.828/Labs'
...A lot of compiling and linking output ignored
make[1]: Leaving directory '/home/hehao/Desktop/MIT6.828/Labs'
internal FS tests [fs/test.c]: OK (0.8s) 
  fs i/o: OK 
  check_bc: OK 
  check_super: OK 
  check_bitmap: OK 
  alloc_block: OK 
  file_open: OK 
  file_get_block: OK 
  file_flush/file_truncate/file rewrite: OK 
testfile: OK (2.3s) 
  serve_open/file_stat/file_close: OK 
  file_read: OK 
  file_write: OK 
  file_read after file_write: OK 
  open: OK 
  large file: OK 
spawn via spawnhello: OK (0.7s) 
Protection I/O space: OK (1.9s) 
PTE_SHARE [testpteshare]: OK (2.3s) 
PTE_SHARE [testfdsharing]: OK (1.6s) 
start the shell [icode]: Timeout! OK (32.1s) 
testshell: OK (2.0s) 
primespipe: OK (4.4s) 
Score: 150/150
```

For challenge, I choose to implement eviction policy for the disk block cache. See the challenge section below for more details.

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

## Exercise 3

> **Exercise 3.** Use free_block as a model to implement alloc_block in fs/fs.c, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with flush_block, to help file system consistency.

We should set the corresponding bit to zero if we want to mark one block in use.

```c
for (uint32_t i = 0; i < super->s_nblocks; ++i) {
	if ((bitmap[i / 32] & (1 << (i % 32))) == 0) continue;
	bitmap[i / 32] &= ~(1 << (i % 32));
	flush_block(bitmap + i / 32);
	return i;
}
return -E_NO_DISK;
```

## Exercise 4

> **Exercise 4.** Implement file_block_walk and file_get_block. file_block_walk maps from a block offset within a file to the pointer for that block in the struct File or the indirect block, very much like what pgdir_walk did for page tables. file_get_block goes one step further and maps to the actual disk block, allocating a new one if necessary. 

When implementing these two functions, it's important to keep in mind that the `struct File` are storing **block numbers**, and you have to convert them to address in order to access the real block.

```c
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
    // LAB 5: Your code here.
    if (filebno > NDIRECT + NINDIRECT)
		return -E_INVAL;
	if (filebno < NDIRECT) {
		*ppdiskbno = &f->f_direct[filebno];
		return 0;
	}
	if (f->f_indirect == 0 && !alloc)
		return -E_NOT_FOUND;

	if (f->f_indirect == 0) {
		int blockno = alloc_block();
		if (blockno < 0) return blockno;
		f->f_indirect = blockno;
		memset(diskaddr(f->f_indirect), 0, BLKSIZE);
	}

	uint32_t *addr = (uint32_t *)diskaddr(f->f_indirect);
	*ppdiskbno = &addr[filebno - NDIRECT];
	return 0;
}
```

```c
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
    // LAB 5: Your code here.
	uint32_t *pdiskbno;
	int r;

    if (filebno > NDIRECT + NINDIRECT)
		return -E_INVAL;
	if ((r = file_block_walk(f, filebno, &pdiskbno, true)) < 0)
		return r; // -E_NO_DISK
	if (*pdiskbno == 0) {
		if ((r = alloc_block()) < 0)
			return r; // -E_NO_DISK
		*pdiskbno = r;
	}
	*blk = (char *)diskaddr(*pdiskbno);
	return 0;
}
```

## Exercise 5

> **Exercise 5.** Implement serve_read in fs/serv.c. 

The `serve_read()` function should first find the corresponding `OpenFile` structure, and then use `file_read()` to read the data. There is no need to do parameter sanity checks since the user library wrappers will do that.

```c
int
serve_read(envid_t envid, union Fsipc *ipc)
{
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;
	struct OpenFile *of;
	int r;

	if (debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// Lab 5: Your code here:
	if ((r = openfile_lookup(envid, req->req_fileid, &of)) < 0)
		return r;
	if ((r = file_read(of->o_file, ret->ret_buf, req->req_n, of->o_fd->fd_offset)) < 0)
		return r;
	of->o_fd->fd_offset += r;
	return r;
}
```

## Exercise 6

> **Exercise 6.** Implement serve_write in fs/serv.c and devfile_write in lib/file.c. 

The implementation of `serve_write()` is almost identical to `serve_read()`.

```c
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
	if (debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// LAB 5: Your code here.
	int r;
	struct OpenFile *of;

	if ((r = openfile_lookup(envid, req->req_fileid, &of)) < 0)
		return r;
	if ((r = file_write(of->o_file, req->req_buf, req->req_n, of->o_fd->fd_offset)) < 0)
		return r;
	of->o_fd->fd_offset += r;
	return r;
}
```

In `devfile_write()`, we can always write fewer bytes than requested, so it's ok to modify n if it is too large.

```c
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
	int r;
	int bufsize = sizeof(fsipcbuf.write.req_buf);

	n = MIN(bufsize, n);
	memmove(fsipcbuf.write.req_buf, buf, n);
	fsipcbuf.write.req_fileid = fd->fd_file.id;
	fsipcbuf.write.req_n = n;
	if ((r = fsipc(FSREQ_WRITE, NULL)) < 0)
		return r;
	assert(r <= n);
	assert(r <= bufsize);
	return r;
}
```

## Exercise 7

> **Exercise 7.** spawn relies on the new syscall sys_env_set_trapframe to initialize the state of the newly created environment. Implement sys_env_set_trapframe in kern/syscall.c (don't forget to dispatch the new system call in syscall()).

One important thing to mention is that you should check the correctness of the provided trap frame address by using `user_mem_check()`. Also, theoritically you should not modify user address space, so I manually set the flags after the trap frame has been copied to `Env`.

```c
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	struct Env *e;
	int32_t ret;
	if ((ret = envid2env(envid, &e, 1)) < 0) {
		return ret; // -E_BAD_ENV
	}
	if ((ret = user_mem_check(e, tf, sizeof(struct Trapframe), PTE_U)) < 0) {
		return ret;
	}
	
	memmove(&e->env_tf, tf, sizeof(struct Trapframe));
	e->env_tf.tf_ds = GD_UD | 3;
	e->env_tf.tf_es = GD_UD | 3;
	e->env_tf.tf_ss = GD_UD | 3;
	e->env_tf.tf_cs = GD_UT | 3;
	e->env_tf.tf_eflags |= FL_IF;
	e->env_tf.tf_eflags &= ~FL_IOPL_MASK;
	return 0;
}
```

## Exercise 8

> Exercise 8. Change duppage in lib/fork.c to follow the new convention. If the page table entry has the PTE_SHARE bit set, just copy the mapping directly. (You should use PTE_SYSCALL, not 0xfff, to mask out the relevant bits from the page table entry. 0xfff picks up the accessed and dirty bits as well.) Likewise, implement copy_shared_pages in lib/spawn.c. It should loop through all page table entries in the current process (just like fork did), copying any page mappings that have the PTE_SHARE bit set into the child process. 

Modifying one line of code in `duppage()` will suffice.

```c
if (((pte & PTE_W) || (pte & PTE_COW)) && !(pte & PTE_SHARE)) {
	// Copy PTE With COW
```

The implementation of `copy_shared_pages()` is similiar to `fork()`.

```c
static int
copy_shared_pages(envid_t child)
{
	// LAB 5: Your code here.
	for (int i = 0; i < NPDENTRIES; ++i) {
		if (!(uvpd[i] & PTE_P)) continue;
		for (int j = 0; j < NPTENTRIES; ++j) {
			uint32_t pn = i * NPDENTRIES + j;
			void *addr = (void *)(pn * PGSIZE);
			int r;

			if (pn == ((UXSTACKTOP - PGSIZE) >> PGSHIFT)) continue;
			if (pn * PGSIZE >= UTOP) continue;
			if (!(uvpt[pn] & PTE_P)) continue;
			if (uvpt[pn] & PTE_SHARE) {
				if ((r = sys_page_map(thisenv->env_id, addr, 
					child, addr, uvpt[pn] & PTE_SYSCALL)) < 0) {
					panic("sys_page_map: %e\n", r);
				}
			}
		}
	}
	return 0;
}
```

## Exercise 9

> **Exercise 9.** In your kern/trap.c, call kbd_intr to handle trap IRQ_OFFSET+IRQ_KBD and serial_intr to handle trap IRQ_OFFSET+IRQ_SERIAL. 

```c
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
		kbd_intr();
		return;
	}

	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
		serial_intr();
		return;
	}
```

After adding the above code in `trap.c` and running `make run-testkbd`, we can type something in a user environment like this.

```
Type a line: asdwfedsv  errwrwer
asdwfedsv  errwrwer
Type a line: sadqrqfdsfweg
sadqrqfdsfweg
```

## Exercise 10

> **Exercise 10.** The shell doesn't support I/O redirection. It would be nice to run sh < script instead of having to type in all the commands in the script by hand, as you did above. Add I/O redirection for < to user/sh.c.

The input redirection code is just mimicking output redirection code provided below.

```c
// LAB 5: Your code here.
if ((fd = open(t, O_RDONLY)) < 0) {
	cprintf("open %s for read: %e", t, fd);
	exit();
}
if (fd != 0) {
	dup(fd, 0);
	close(fd);
}
```

However, after testing I have found subtle bugs in `make run-testshell`, which turned out to be that in `page_insert()` I forgot to invalidate TLB when a page is reinserted to the same address! This will cause the file system sanity checks to fail because the `pp_ref` field of a `struct PageInfo` will be incorrect.

Here is the updated `page_insert()` code

```c
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	pte_t *pte = pgdir_walk(pgdir, va, true);
	if (pte == NULL) {
		return -E_NO_MEM;
	}
	if (PTE_ADDR(*pte) == page2pa(pp)) {
		*pte = page2pa(pp) | perm | PTE_P;
		tlb_invalidate(pgdir, va);
		return 0;
	}
	if (*pte & PTE_P) {
		page_remove(pgdir, va);
		assert(*pte == 0);
	}
	pp->pp_ref++;
	*pte = page2pa(pp) | perm | PTE_P;
	pgdir[PDX(va)] |= perm;
	return 0;
}
```

After that I can achieve a score of 150/150 in `make grade`.

## Challenge!

> **Challenge!** The block cache has no eviction policy. Once a block gets faulted in to it, it never gets removed and will remain in memory forevermore. Add eviction to the buffer cache. Using the PTE_A "accessed" bits in the page tables, which the hardware sets on any access to a page, you can track approximate usage of disk blocks without the need to modify every place in the code that accesses the disk map region. Be careful with dirty blocks. 