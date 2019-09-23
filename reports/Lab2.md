# Report for Lab 2, Hao He

## Summary of Results

## Exercise 1

> **Exercise 1.** In the file `kern/pmap.c`, you must implement code for the following functions (probably in the order given).
> `boot_alloc()`
> `mem_init()` (only up to the call to check_page_free_list(1))
> `page_init()`  
> `page_alloc()`
> `page_free()`

The implementation of Exercise 1 is relatively simple and straight forward, which is basically linked list manipulation stuff and is also very similar to memory allocation code in xv6. However, when programming, special attention need to be paid on whether a value is a physical address or a kernel address. The implementation of `page_init()` is a little bit tricky because you have to identify the top of kernel memory using `boot_alloc(0)`.

## Exercise 2

> **Exercise 2.** Look at chapters 5 and 6 of the Intel 80386 Reference Manual, if you haven't done so already. Read the sections about page translation and page-based protection closely (5.2 and 6.4). We recommend that you also skim the sections about segmentation; while JOS uses the paging hardware for virtual memory and protection, segment translation and segment-based protection cannot be disabled on the x86, so you will need a basic understanding of it. 



## Exercise 3
