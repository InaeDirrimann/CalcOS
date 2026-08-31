# GDB Initialization Script for Bare Metal Calculator
#
# Load symbols for your kernel regardless of where the build landed:
#   gdb -ex "symbol-file /path/to/kernel.elf" -x GDBInit.gdb
# (the Tools/Debug/QEMU.sh script prints this exact command for you)
# OR run this file from gdb and type: file <path/to/kernel.elf>

# Connect to QEMU's remote gdb stub on localhost:1234
target remote localhost:1234

# Set architecture to 64-bit x86
set architecture i386:x86-64

# Force disassembly style to Intel
set disassembly-flavor intel

# Add helper hooks
define show_regs
    info registers rax rbx rcx rdx rsi rdi rbp rsp rip r8 r9 r10 r11 r12 r13 r14 r15
end

document show_regs
    Display standard 64-bit general purpose registers.
end

# Set a breakpoint at the kernel start
# (pending allows it to resolve after the symbol file is loaded)
set breakpoint pending on
b kernel_main

echo \n=== GDB Configured and Connected ===\n
echo Use 'c' or 'continue' to start execution up to kernel_main.\n
