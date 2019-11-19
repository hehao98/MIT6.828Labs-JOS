#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	envid_t child;
    int ret;

    ret = sys_env_set_priority(thisenv->env_id, 1);
     if (ret < 0) {
        panic("sys_env_set_priority: failed\n");
    }

	if ((child = fork()) == 0) {
		// child
        for (int i = 0; i < 5; ++i) {
		    cprintf("Spinning child...\n");
            sys_yield();
        }
        return;
	}

	// parent
    for (int i = 0; i < 5; ++i) {
        cprintf("Spinning parent...\n");
        sys_yield();
    }
    return;
}

