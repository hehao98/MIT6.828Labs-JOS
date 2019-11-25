// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	envid_t envid = sys_getenvid();

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pte = uvpt[(uintptr_t)addr >> PGSHIFT];
	if (!(err & FEC_WR)) {
		panic("Page Fault at %08x which is not writeable\n", addr);
	}
	if (!(pte & PTE_COW)) {
		cprintf("%08x\n", utf->utf_eip);
		panic("Page Fault at %08x but not a COW page\n", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	uint32_t perm =  PTE_U | PTE_P | PTE_W;
	if ((r = sys_page_alloc(envid, PFTEMP, perm)) < 0) {
		panic("sys_page_alloc: %e\n", r);
	}
	memmove((void*)PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	if ((r = sys_page_alloc(envid, ROUNDDOWN(addr, PGSIZE), perm)) < 0) {
		panic("sys_page_alloc: %e\n", r);
	}
	memmove(ROUNDDOWN(addr, PGSIZE), (void*)PFTEMP, PGSIZE);
	if ((r = sys_page_unmap(envid, (void*)PFTEMP)) < 0) {
		panic("sys_page_umap: %e\n", r);
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	uintptr_t addr = pn * PGSIZE;
	pte_t pte = uvpt[pn];
	envid_t srcid = sys_getenvid();
	int perm;

	// LAB 4: Your code here.
	if (((pte & PTE_W) || (pte & PTE_COW)) && !(pte & PTE_SHARE)) {
		pte = (pte & (~PTE_W)) | PTE_COW;
		perm = pte & PTE_SYSCALL;
		if ((r = sys_page_map(srcid, (void*)addr, envid, (void*)addr, perm)) < 0) {
			panic("sys_page_map: %e\n", r);
		}
		if ((r = sys_page_map(srcid, (void*)addr, srcid, (void*)addr, perm)) < 0) {
			panic("sys_page_map: %e\n", r);
		}
	} else {
		perm = pte & PTE_SYSCALL;
		if ((r = sys_page_map(srcid, (void*)addr, envid, (void*)addr, perm)) < 0) {
			panic("sys_page_map: %e\n", r);
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t child;

	set_pgfault_handler(pgfault);
	
	if ((child = sys_exofork()) < 0) {
		panic("sys_exofork: %e\n", child);
	}
	
	if (child == 0) { // child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// parent
	for (int i = 0; i < NPDENTRIES; ++i) {
		if (!(uvpd[i] & PTE_P)) continue;
		for (int j = 0; j < NPTENTRIES; ++j) {
			uint32_t pn = i * NPDENTRIES + j;
			if (pn == ((UXSTACKTOP - PGSIZE) >> PGSHIFT)) continue;
			if (pn * PGSIZE >= UTOP) continue;
			if (!(uvpt[pn] & PTE_P)) continue;
			duppage(child, pn);
		}
	}
	
	extern void *_pgfault_upcall();
	sys_env_set_pgfault_upcall(child, _pgfault_upcall);
	sys_page_alloc(child, (void*)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P);
	sys_env_set_status(child, ENV_RUNNABLE);
	return child;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
