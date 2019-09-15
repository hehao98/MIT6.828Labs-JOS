# Report for Lab1, Hao He

## Environment Configuration

```
Hardware Environment:
Memory:        16GB
Processor:     Intel® Core™ i7-7700 CPU @ 3.60GHz × 8
Graphics:	   NV137
OS Type:	   64 bit
Disk:          128GB

Software Environment:
OS:            Ubuntu 18.04 LTS(x86_64)
gcc:           gcc version 7.4.0
Make:          GNU Make 4.1
gdb:           GNU gdb 8.1.0
```

### Test Compiler Toolchain

```
hehao@hehao-lenovo:~$ objdump -i
...
elf32-i386
 (header little endian, data little endian)
  i386
...
hehao@hehao-lenovo:~$ gcc -m32 -print-libgcc-file-name
/usr/lib/gcc/x86_64-linux-gnu/7/32/libgcc.a
```

### QEMU Emulator

The QEMU emulator is installed by running the following commands.

```
$ git clone https://github.com/mit-pdos/6.828-qemu.git qemu 
$ cd qemu
$ ./configure --disable-kvm --target-list="i386-softmmu x86_64-softmmu"
$ make
$ sudo make install
```

## PC Bootstrap

### Get JOS and compile the kernel

```
$ git clone https://pdos.csail.mit.edu/6.828/2018/jos.git Labs
$ cd Labs
$ make
```

After the compilation we can see the compiled disk image at `obj/kern/kernel.img`.

### Running the Kernel Using QEMU

Below is my output after running the JOS using QEMU and testing its supported commands.

```
hehao@hehao-lenovo:~/Desktop/MIT6.828/Labs$ make qemu
qemu-system-i386 -drive file=obj/kern/kernel.img,index=0,media=disk,format=raw -serial mon:stdio -gdb tcp::26000 -D qemu.log 
VNC server running on `127.0.0.1:5900'
6828 decimal is XXX octal!
entering test_backtrace 5
entering test_backtrace 4
entering test_backtrace 3
entering test_backtrace 2
entering test_backtrace 1
entering test_backtrace 0
leaving test_backtrace 0
leaving test_backtrace 1
leaving test_backtrace 2
leaving test_backtrace 3
leaving test_backtrace 4
leaving test_backtrace 5
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K> help
help - Display this list of commands
kerninfo - Display information about the kernel
K> kerninfo
Special kernel symbols:
  _start                  0010000c (phys)
  entry  f010000c (virt)  0010000c (phys)
  etext  f01019e9 (virt)  001019e9 (phys)
  edata  f0113060 (virt)  00113060 (phys)
  end    f01136a0 (virt)  001136a0 (phys)
Kernel executable memory footprint: 78KB
K> 
```

### Debug Kernel using QEMU and GDB

In two different kernels, run `make gdb` and `make qemu-gdb` separately, then we can use `gdb` to control the execution of our kernel. For example, execute instructions step by step like this

```
[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
(gdb) si
[f000:e05b]    0xfe05b:	cmpl   $0x0,%cs:0x6ac8
0x0000e05b in ?? ()
(gdb) si
[f000:e062]    0xfe062:	jne    0xfd2e1
0x0000e062 in ?? ()
```

## Solution to the Exercises

### Exercise 1

> **Exercise 1**. Familiarize yourself with the assembly language materials available on [the 6.828 reference page](https://pdos.csail.mit.edu/6.828/2018/reference.html).  You don't have to read them now, but you'll almost certainly want to refer to some of this material when reading and writing x86 assembly. We do recommend reading the section "The Syntax" in [Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html). It gives a good (and quite brief) description of the AT&T assembly syntax we'll be using with the GNU assembler in JOS. 

This part has already been well covered in the Introduction to Computer Systems and Operating Systems course, so I won't repeat them here. However, I've found that the introduction to `gcc` inline assembly in [Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html) is extremely useful.

### Exercise 2

> **Exercise 2**. Use GDB's `si` (Step Instruction) command to trace into the ROM BIOS for a few more instructions, and try to guess what it might be doing. You might want to look at [Phil Storrs I/O Ports Description](http://web.archive.org/web/20040404164813/members.iweb.net.au/~pstorr/pcbook/book2/book2.htm), as well as other materials on the [6.828 reference materials page](https://pdos.csail.mit.edu/6.828/2018/reference.html). No need to figure out all the details - just the general idea of what the BIOS is doing first.

**What BIOS Do at a High Level**

1. Set up an interrupt descriptor table.
2. Initialize devices such as the VGA display and the PCI bus.
3. Search for a bootable device such as a floppy, hard drive or CD-ROM.
4. When BIOS find a bootable device, it reads the boot loader from the disk and transfer control to it.

### Exercise 3

>**Exercise 3**. Take a look at the [lab tools guide](https://pdos.csail.mit.edu/6.828/2018/labguide.html), especially the section on GDB commands. Even if you're familiar with GDB, this includes some esoteric GDB commands that are useful for OS work. 
>
>Set a breakpoint at address 0x7c00, which is where the boot sector will be loaded. Continue execution until that breakpoint. Trace through the code in `boot/boot.S`, using the source code and the disassembly file `obj/boot/boot.asm` to keep track of where you are. Also use the `x/i` command in GDB to disassemble sequences of instructions in the boot loader, and compare the original boot loader source code with both the disassembly in `obj/boot/boot.asm` and GDB. 
>
>Trace into `bootmain()` in `boot/main.c`, and then into `readsect()`. Identify the exact assembly instructions that correspond to each of the statements in `readsect()`. Trace through the rest of `readsect()` and back out into `bootmain()`, and identify the begin and end of the `for` loop that reads the remaining sectors of the kernel from the disk. Find out what code will run when the loop is finished, set a breakpoint there, and continue to that breakpoint. Then step through the remainder of the boot loader. 
>
> Be able to answer the following questions: 

**- At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?**

From line 60 in `boot.S` the processor really start executing 32-bit code. However, some prior configurations are needed to make this switch work.

First, the boot loader configures Global Descriptor Table (GDT) using `lgdt` instructions, which takes the address of `gdtdesc` as input and set up GDT using data stored in `gdtdesc`. The detailed behavior of `lgdt` can be found in [this x86 reference page](https://c9x.me/x86/html/file_module_x86_id_156.html). 

```assembly
# Switch from real to protected mode, using a bootstrap GDT
# and segment translation that makes virtual addresses 
# identical to their physical addresses, so that the 
# effective memory map does not change during the switch.
lgdt    gdtdesc
```

After that, the boot loader need to setup CR register to turn on protected mode like the following.

```assembly
movl    %cr0, %eax
orl     $CR0_PE_ON, %eax
movl    %eax, %cr0
```

Here is where the switch from 16- to 32-bit code really happens, because he jump uses 32 bit code segment to jump to the next instruction.

```assembly
ljmp    $PROT_MODE_CSEG, $protcseg
```

It can be proved by the following GDB trace.

```
(gdb) si
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x7c32
0x00007c2d in ?? ()
(gdb) si
The target architecture is assumed to be i386 # Switch happens
=> 0x7c32:	mov    $0x10,%ax
0x00007c32 in ?? ()
```

**- What is the *last* instruction of the boot loader executed, and what is the *first* instruction of the kernel it just loaded?**

The last instruction of the boot loader is

```assembly
7d6b:	ff 15 18 00 01 00    	call   *0x10018
```

Which corresponds to the following C statement in `boot/main.c`

```c
((void (*)(void)) (ELFHDR->e_entry))();
```

The first instruction of the kernel is (from `kernel.asm`)

```assembly
0x10000c:	movw   $0x1234,0x472
```

**- *Where* is the first instruction of the kernel?**

From `kernel.asm` we can find the first instruction of kernel, which can be confirmed by the following GDB trace.

```
(gdb) b *0x7d6b
Breakpoint 1 at 0x7d6b
(gdb) c
Continuing.
The target architecture is assumed to be i386
=> 0x7d6b:	call   *0x10018

Breakpoint 1, 0x00007d6b in ?? ()
(gdb) si
=> 0x10000c:	movw   $0x1234,0x472
0x0010000c in ?? ()
```

**- How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?**

The information is acquired from the kernel ELF header. A ELF file header always contain information about how large this ELF file is. Details about the ELF file format can be found [here](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format). More specifically, ELF Header contains an array of program headers, and its length is stored in the `e_phnum` field. In this code, `ph` is used to iterate over all the program headers and use the `ph->p_pa` field to find out where this program header is located.

```c
ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
eph = ph + ELFHDR->e_phnum;
for (; ph < eph; ph++)
	// p_pa is the load address of this segment (as well
	// as the physical address)
	readseg(ph->p_pa, ph->p_memsz, ph->p_offset);
```

### Exercise 4

>**Exercise 4**. Read about programming with pointers in C. 

I'm already familiar with C and pointers in C.

### Exercise 5

>**Exercise 5**. Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong. Then change the link address in `boot/Makefrag` to something wrong, run `make clean`, recompile the lab with `make`, and trace into the boot loader again to see what happens. Don't forget to change the link address back and `make clean` again afterward!

After the modification, some jump instructions still work because they are relative jumps. However, the problem comes in when it comes to the `lgdt` instruction, which will load data from an absolute address different from it should be (in my case `0x7024`). In `boot.asm its content should be 

```
00007d24 <gdtdesc>:
    7d24:	17                   	pop    %ss
    7d25:	00                   	.byte 0x0
    7d26:	0c 7d                	or     $0x7d,%al
```

However the real data becomes

```
(gdb) x/2x 0x7d24
0x7d24:	0xb1e80001	0x83ffffff
```

This will completely break the GDT up and make behavior unpredictable in protected mode. In my case the program went into an infinite loop at this instruction because the processor triggered a triple fault.

```
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x7cf2
```

### Exercise 6

>**Exercise 6**. We can examine memory using GDB's x command. The [GDB manual](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html) has full details, but for now, it is enough to know that the command x/*N*x *ADDR* prints *N* words of memory at *ADDR*. (Note that both '`x`'s in the command are lowercase.) *Warning*: The size of a word is not a universal standard. In GNU assembly, a word is two bytes (the 'w' in xorw, which stands for word, means 2 bytes). 	
>
>Reset the machine (exit QEMU/GDB and start them again). Examine the 8 words of memory at 0x00100000  at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint? (You do not really need to use QEMU to answer this question. Just think.) 

When the BIOS enters the boot loader

```
(gdb) x/8x 0x00100000
0x100000:	0x00000000	0x00000000	0x00000000	0x00000000
0x100010:	0x00000000	0x00000000	0x00000000	0x00000000
```

When the boot loader enters the kernel

```
(gdb) x/10x 0x00100000
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x2000b812	0x220f0011	0xc0200fd8
0x100020:	0x0100010d	0xc0220f80
```

It's the kernel code that has been loaded into physical memory when the boot loader is running. The boot loader loads the kernel to 0x100000.

