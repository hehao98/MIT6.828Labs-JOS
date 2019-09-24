# Report for Lab 2, Hao He

## Summary of Results

All exercises compelted and all question answered.

```
running JOS: (1.2s) 
  Physical page allocator: OK 
  Page management: OK 
  Kernel page directory: OK 
  Page management 2: OK 
Score: 70/70
```

## Exercise 1

> **Exercise 1.** In the file `kern/pmap.c`, you must implement code for the following functions (probably in the order given).
> `boot_alloc()`
> `mem_init()` (only up to the call to check_page_free_list(1))
> `page_init()`  
> `page_alloc()`
> `page_free()`

The implementation of Exercise 1 is relatively simple and straight forward, which is basically linked list manipulation stuff and is also very similar to memory allocation code in xv6. However, when programming, special attention need to be paid on whether a value is a physical address or a kernel address. It will be very helpful if we can distinguish them using `intptr_t` and `phyaddr_t`. The implementation of `page_init()` is a little bit tricky because you have to identify the top of kernel memory using `boot_alloc(0)`.

## Exercise 2

> **Exercise 2.** Look at chapters 5 and 6 of the Intel 80386 Reference Manual, if you haven't done so already. Read the sections about page translation and page-based protection closely (5.2 and 6.4). We recommend that you also skim the sections about segmentation; while JOS uses the paging hardware for virtual memory and protection, segment translation and segment-based protection cannot be disabled on the x86, so you will need a basic understanding of it. 

The address translation mechanism in x86 is shown as follows.

```
           Selector  +--------------+         +-----------+
          ---------->|              |         |           |
                     | Segmentation |         |  Paging   |
Software             |              |-------->|           |---------->  RAM
            Offset   |  Mechanism   |         | Mechanism |
          ---------->|              |         |           |
                     +--------------+         +-----------+
            Virtual                   Linear                Physical
```

In JOS, we installed an GDT that we installed a Global Descriptor Table (GDT) that effectively disabled segment translation by setting all segment base addresses to 0 and limits to 0xffffffff. However, we have not set something about privilege levels yet, which is needed when we implement user processes.

When programming this lab, we also need to understand flags in PTE entrys in greater detail. We need to manually set `PTE_P`, `PTE_U` and  `PTE_W` flags when setting up page table.

## Exercise 3

> **Exercise 3.** While GDB can only access QEMU's memory by virtual address, it's often useful to be able to inspect physical memory while setting up virtual memory. Review the QEMU monitor commands from the lab tools guide, especially the xp command, which lets you inspect physical memory. To access the QEMU monitor, press Ctrl-a c in the terminal (the same binding returns to the serial console).
> Use the xp command in the QEMU monitor and the x command in GDB to inspect memory at corresponding physical and virtual addresses and make sure you see the same data.
> Our patched version of QEMU provides an info pg command that may also prove useful: it shows a compact but detailed representation of the current page tables, including all mapped memory ranges, permissions, and flags. Stock QEMU also provides an info mem command that shows an overview of which ranges of virtual addresses are mapped and with what permissions. 

After JOS has entered the kernel monitor, we set a breakpoint and inspect memory in `qemu` and in `gdb` as follows.

```
(qemu) xp/12x 0x100000
0000000000100000: 0x1badb002 0x00000000 0xe4524ffe 0x7205c766
0000000000100010: 0x34000004 0x5000b812 0x220f0011 0xc0200fd8
0000000000100020: 0x0100010d 0xc0220f80 0x10002fb8 0xbde0fff0
(gdb) x/12x 0xf0100000
0xf0100000 <_start+4026531828>: 0x1badb002 0x00000000 0xe4524ffe 0x7205c766
0xf0100010 <entry+4>:   0x34000004 0x5000b812 0x220f0011 0xc0200fd8
0xf0100020 <entry+20>:  0x0100010d 0xc0220f80 0x10002fb8 0xbde0fff0
```
We can see that the physical address 0x100000 and virtual address 0xf0100000 points to the same data, and can be successfully inspected.

By running `info pg` in `qemu` we can inspect current page setup in more detail.

```
(qemu) info pg
VPN range     Entry         Flags        Physical page
[00000-003ff]  PDE[000]     ----A----P
  [00000-00000]  PTE[000]     --------WP 00000
  [00001-0009f]  PTE[001-09f] ---DA---WP 00001-0009f
  [000a0-000b7]  PTE[0a0-0b7] --------WP 000a0-000b7
  [000b8-000b8]  PTE[0b8]     ---DA---WP 000b8
  [000b9-000ff]  PTE[0b9-0ff] --------WP 000b9-000ff
  [00100-00103]  PTE[100-103] ----A---WP 00100-00103
  [00104-00111]  PTE[104-111] --------WP 00104-00111
  [00112-00112]  PTE[112]     ---DA---WP 00112
  [00113-00113]  PTE[113]     --------WP 00113
  [00114-00114]  PTE[114]     ----A---WP 00114
  [00115-00115]  PTE[115]     --------WP 00115
  [00116-003ff]  PTE[116-3ff] ---DA---WP 00116-003ff
[f0000-f03ff]  PDE[3c0]     ----A---WP
  [f0000-f0000]  PTE[000]     --------WP 00000
  [f0001-f009f]  PTE[001-09f] ---DA---WP 00001-0009f
  [f00a0-f00b7]  PTE[0a0-0b7] --------WP 000a0-000b7
  [f00b8-f00b8]  PTE[0b8]     ---DA---WP 000b8
  [f00b9-f00ff]  PTE[0b9-0ff] --------WP 000b9-000ff
  [f0100-f0103]  PTE[100-103] ----A---WP 00100-00103
  [f0104-f0111]  PTE[104-111] --------WP 00104-00111
  [f0112-f0112]  PTE[112]     ---DA---WP 00112
  [f0113-f0113]  PTE[113]     --------WP 00113
  [f0114-f0114]  PTE[114]     ----A---WP 00114
  [f0115-f0115]  PTE[115]     --------WP 00115
  [f0116-f03ff]  PTE[116-3ff] ---DA---WP 00116-003ff
```

## Exercise 4

> **Exercise 4.** In the file kern/pmap.c, you must implement code for the following functions. `pgdir_walk()` `boot_map_region()` `page_lookup()` `page_remove()` `page_insert()`

When implementing `pgdir_walk()`, you must be very careful in order to follow the specifications mentioned in the comments. For example, when creating a new page table entry, you have to increment the reference count of physical memory and handle the case of physical memory running out.

The implementation of `page_remove()` and `page_lookup()` is relatively straightforward.

I have fallen into a trap when implementing `boot_map_region()` because I have failed to consider integer overflow bugs! This is my initial verision.

```c
for (uintptr_t p = va; p < va + size; p += PGSIZE) {
	pte_t *pte = pgdir_walk(pgdir, (void *)p, true);
	*pte = PTE_ADDR(pa) | perm | PTE_P;
	pa += PGSIZE;
}
```

This version of `boot_map_region()` is wrong because it will map nothing on this call due to integer overflow.

```c
boot_map_region(kern_pgdir, KERNBASE, 0xFFFFFFFF - KERNBASE + 1, 0, PTE_W);
```

This is the correct version.

```c
for (size_t i = 0; i < size; i += PGSIZE) {
	pte_t *pte = pgdir_walk(pgdir, (void *)(va + i), true);
	*pte = PTE_ADDR(pa + i) | perm | PTE_P;
}
```

The implementation of `page_insert()` need some careful consideration. As specified in the comment, you have to handle the corner case in one code path. More specifically, we need to handle the case when the same `pp` is re-inserted at the same virtual address in the same `pgdir`, but the `perm` might be different. We also need to invalidate TLB where needed. However, we can see that `page_remove()` can handle these for us! Also, if we increment `pp_ref` prior to `page_remove()`, the code works in all cases! Therefore, my final implementation is the following.

```c
int page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	pte_t *pte = pgdir_walk(pgdir, va, true);
	if (pte == NULL) return -E_NO_MEM;
	pp->pp_ref++;
	if (*pte & PTE_P) page_remove(pgdir, va);
	*pte = page2pa(pp) | perm | PTE_P;
	pgdir[PDX(va)] |= perm;
	return 0;
}
```

## Exercise 5

> **Exercise 5.** Fill in the missing code in `mem_init()` after the call to `check_page()`.

This three lines of code will work.

```c
	boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), PTE_U);
	boot_map_region(kern_pgdir, KSTACKTOP - KSTKSIZE, KSTKSIZE, PADDR(bootstack), PTE_W);
	boot_map_region(kern_pgdir, KERNBASE, 0xFFFFFFFF - KERNBASE + 1, 0, PTE_W);
```

## Answer to Questions

> Assuming that the following JOS kernel code is correct, what type should variable `x` have, `uintptr_t` or `physaddr_t`?

```c
 mystery_t x;
 char* value = return_a_pointer();
 *value = 10;
 x = (mystery_t) value;
 ```

The type of `x` should be `uintptr_t` because `value` is a pointer that can be dereferenced, so `x` stores the value of a virtual address.