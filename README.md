# MIT6.828Labs-JOS

This repository contains my solution to MIT6.828 Operating System Engineering Fall 2018 Lab 1-5. You can refer to the `report/` folder for my reports of each Lab.

WARNING: DO NOT COPY THIS REPOSITORY AS YOUR SOLUTION IF YOU ARE TAKING COURSES THAT USE THESE LABS AS ASSIGNMENTS!!!

## Challenges

Most of the solutions on the Internet are missing challenges, but I have done some challenges in Lab 2-5, which might be useful for you. The challenges I solved are the following, you can refer to my reports and code for how I solved them.

### Lab 1 - Colored Output to Console

> **Challenge.** Enhance the console to allow text to be printed in different colors. The traditional way to do this is to make it interpret [ANSI escape sequences](http://rrbrandt.dee.ufcg.edu.br/en/docs/ansi/) embedded in the text strings printed to the console, but you may use any mechanism you like. There is plenty of information on [the 6.828 reference page](https://pdos.csail.mit.edu/6.828/2018/reference.html) and elsewhere on the web on programming the VGA display hardware. If you're feeling really adventurous, you could try switching the VGA hardware into a graphics mode and making the console draw text onto the graphical frame buffer.

### Lab 2 - More Useful JOS Monitor Commands

> **Challenge!** Extend the JOS kernel monitor with commands to:
>
> Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space. For example, you might enter 'showmappings 0x3000 0x5000' to display the physical page mappings and corresponding permission bits that apply to the pages at virtual addresses 0x3000, 0x4000, and 0x5000.
>
> Explicitly set, clear, or change the permissions of any mapping in the current address space.
>
> Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!
> 
> Do anything else that you think might be useful later for debugging the kernel. (There's a good chance it will be!)

### Lab 3 - Breakpoints and Single Stepping

> **Challenge!** Modify the JOS kernel monitor so that you can 'continue' execution from the current location (e.g., after the int3, if the kernel monitor was invoked via the breakpoint exception), and so that you can single-step one instruction at a time. You will need to understand certain bits of the EFLAGS register in order to implement single-stepping.

### Lab 4 - Fixed Priority Scheduling

> **Challenge!** Add a less trivial scheduling policy to the kernel, such as a fixed-priority scheduler that allows each environment to be assigned a priority and ensures that higher-priority environments are always chosen in preference to lower-priority environments.

### Lab 5 - FIFO Disk Block Cache

> **Challenge!** The block cache has no eviction policy. Once a block gets faulted in to it, it never gets removed and will remain in memory forevermore. Add eviction to the buffer cache. Using the PTE_A "accessed" bits in the page tables, which the hardware sets on any access to a page, you can track approximate usage of disk blocks without the need to modify every place in the code that accesses the disk map region. Be careful with dirty blocks. 
