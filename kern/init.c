/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>


// Test the stack backtrace function (lab 1 only)
void
test_backtrace(int x)
{
	cprintf("entering test_backtrace %d\n", x);
	if (x > 0)
		test_backtrace(x-1);
	else
		mon_backtrace(0, 0, 0);
	cprintf("leaving test_backtrace %d\n", x);
}

void
lab1_test(void)
{
	// Print some strange stuff
	cprintf("6828 decimal is %o octal!\n", 6828);
	unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s\n", 57616, &i);
	cprintf("Printing colored strings: ");
	cprintf("\x1b[31;40mRed ");
	cprintf("\x1b[32;40mGreen ");
	cprintf("\x1b[33;40mYellow ");
	cprintf("\x1b[34;40mBlue ");
	cprintf("\x1b[35;40mMagenta ");
	cprintf("\x1b[36;40mCyan ");
	cprintf("\x1b[37;40mWhite");
	cprintf("\n");
	cprintf("With background color: ");
	cprintf("\x1b[31;32mRed ");
	cprintf("\x1b[32;33mGreen ");
	cprintf("\x1b[33;34mYellow ");
	cprintf("\x1b[34;35mBlue ");
	cprintf("\x1b[35;36mMagenta ");
	cprintf("\x1b[36;37mCyan ");
	cprintf("\x1b[37;40mWhite");
	cprintf("\n");

	// Test the stack backtrace function (lab 1 only)
	test_backtrace(5);
}

void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	// lab1_test();

	// Lab2: Initialize memory
	mem_init();

	// Lab 3 user environment initialization functions
	env_init();
	trap_init();

#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	// Touch all you want.
	ENV_CREATE(user_hello, ENV_TYPE_USER);
#endif // TEST*

	// We only have one user environment for now, so just run it.
	env_run(&envs[0]);
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	asm volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
