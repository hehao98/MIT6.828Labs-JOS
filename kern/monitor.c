// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/types.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display function backtrace info", mon_backtrace },
	{ "showmapping", "Display physical page mappings", mon_showmapping },
	{ "phymem", "Display physical memory", mon_phymem },
	{ "virtmem", "Display virtual memory", mon_virtmem },
	{ "chgperm", "Change permisson of any memory mapping", mon_chgperm },
};

/***** Implementations of basic kernel monitor commands *****/

static uint32_t
mon_atoi(const char *str)
{
	uint32_t x = 0;
	if (str[0] == '0' && str[1] == 'x') {
		str += 2;
	}
	while (*str) {
		uint32_t val = 0;
		if (*str >= '0' && *str <= '9') {
			val = *str - '0';
		} else if (*str >= 'A' && *str <= 'F') {
			val = *str - 'A' + 10;
		} else if (*str >= 'a' && *str <= 'f') {
			val = *str - 'a' + 10;
		} else {
			cprintf("input is not a hex number!\n");
			return -1;
		}
		x = x * 16 + val;
		str++;
	} 
	return x;
}

static void
mon_printmem(uintptr_t addr_begin, uintptr_t addr_end, bool show_phy)
{
	pde_t *pgdir = get_curr_pde();

	for (uintptr_t addr = ROUNDDOWN(addr_begin, 16); addr < addr_end; addr += 16) {
		if (show_phy) {
			cprintf("0x%08x: ", addr - KERNBASE);
		} else {
			cprintf("0x%08x: ", addr);
		}
		
		if (pgdir_walk(pgdir, (void *)addr, false) == NULL) {
			cprintf("UNALLOCATED\n");
			if (ROUNDUP(addr + 1, PGSIZE) == 0) {
				break; // Avoid infinite loop caused by overflow		
			} 
			addr = ROUNDUP(addr + 1, PGSIZE) - 16; // To next page
			continue;
		}
		uint32_t *p = (uint32_t *)addr;
		cprintf("0x%08x 0x%08x 0x%08x 0x%08x\n", p[0], p[1], p[2], p[3]);
	}
}

static 
const char *pg_permstr(pte_t entry) 
{
	static char buf[] =  "---------";
	static char buf2[] = "gpdacwuwp";
	uint32_t perm = entry & 0xFFF;
	for (int i = 0; i < 9; ++i) {
		if (perm & (1 << i)) {
			buf[8 - i] = buf2[8 - i];
		} else {
			buf[8 - i] = '-';
		}
	}
	return buf;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack backtrace:\n");

	uint32_t ebp = read_ebp();
	while (ebp != 0) {
		// Print stack info
		uint32_t *p = (uint32_t *) ebp;
		uint32_t eip = p[1];
		cprintf("ebp %08x eip %08x args", ebp, eip);
		for (int i = 0; i < 5; ++i) {
			cprintf(" %08x", p[i + 2]);
		}
		cprintf("\n\t");

		// Print context info
		struct Eipdebuginfo info;
		int ret = debuginfo_eip(eip, &info);
		cprintf("%s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
			info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);

		ebp = *p;
	}

	return 0;
}

int
mon_showmapping(int argc, char **argv, struct Trapframe *tf)
{
	pde_t *pgdir = get_curr_pde();
	uintptr_t addr_begin = 0;
	uintptr_t addr_end = 0;

	cprintf("Current Page Table Address: 0x%08x\n", pgdir);

	if (argc < 3) {
		cprintf("Usage: showmapping addr_begin addr_end\n");
		return -1;
	} else {
		addr_begin = mon_atoi(argv[1]);
		addr_end = mon_atoi(argv[2]);
	}
	
	bool pde_printed[(1 << 10)] = { false };

	for (uintptr_t addr = ROUNDDOWN(addr_begin, PGSIZE);
			addr < addr_end; addr += PGSIZE) {

		// print PDE info
		if (!pde_printed[PDX(addr)]) {
			pde_printed[PDX(addr)] = true;	
			if ((pgdir[PDX(addr)] & PTE_P) == 0) {
				cprintf("[%08x-%08x]: UNALLOCATED\n", PDX(addr) << PDXSHIFT, 
					((PDX(addr) + 1) << PDXSHIFT) - 1);
				addr = ROUNDUP(addr, PTSIZE);
				if (addr != 0) continue; else break;
			} else {
				cprintf("[%08x-%08x]:\n", PDX(addr) << PDXSHIFT, 
					((PDX(addr) + 1) << PDXSHIFT) - 1);
			}
		}

		// print PTE info
		pte_t *pte = pgdir_walk(pgdir, (void *)addr, false);
		if (pte != NULL) {
			cprintf("    %08x: %08x %s\n", addr & (~0xFFF),
				PTE_ADDR(*pte), pg_permstr(*pte));
		} else {
			cprintf("    %08x: UNALLOCATED\n", addr & (~0xFFF));
		}
	}

	return 0;
}

int 
mon_phymem(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3) {
		cprintf("Usage: phymem addr_begin addr_end\n");
		return -1;
	}

	uintptr_t addr_begin = mon_atoi(argv[1]);
	uintptr_t addr_end = mon_atoi(argv[2]);

	if (addr_begin > addr_end) {
		uintptr_t temp = addr_begin;
		addr_begin = addr_end;
		addr_end = temp;
	}

	if (addr_end > 0xFFFFFFFF - KERNBASE) {
		cprintf("Physical memory larger than 256MB is not supported.\n");
		return -1;
	}

	mon_printmem(addr_begin + KERNBASE, addr_end + KERNBASE, true);

	return 0;
}

int 
mon_virtmem(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3) {
		cprintf("Not enough parameters!"
				"Should have two hex numbers that specify address range.\n");
		return -1;
	}

	uintptr_t addr_begin = mon_atoi(argv[1]);
	uintptr_t addr_end = mon_atoi(argv[2]);

	if (addr_begin > addr_end) {
		uintptr_t temp = addr_begin;
		addr_begin = addr_end;
		addr_end = temp;
	}
	
	mon_printmem(addr_begin, addr_end, false);

	return 0;
}

int 
mon_chgperm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3) {
		cprintf("Usage: chgperm addr perm\n");
		return -1;
	}

	uintptr_t addr = mon_atoi(argv[1]);
	uintptr_t perm = mon_atoi(argv[2]);
	pde_t *pgdir = get_curr_pde();
	pte_t *pte = pgdir_walk(pgdir, (void*)addr, false);

	if (pte == NULL) {
		cprintf("Virtual address %08x is unmapped\n", addr);
		return -1;
	}

	*pte = (*pte & (~0xFFF)) | (perm & 0xFFF);

	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
