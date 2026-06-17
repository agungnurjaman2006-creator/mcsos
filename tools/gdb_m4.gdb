set pagination off
set disassembly-flavor intel
file build/kernel.elf
target remote :1234
break kmain
break x86_64_idt_init
break x86_64_trap_dispatch
continue
