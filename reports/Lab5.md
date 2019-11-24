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
