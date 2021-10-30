// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	asm volatile("int $3");
    asm volatile("movw $1, %ax");
    asm volatile("movw $2, %ax");
    asm volatile("movw $3, %ax");
    asm volatile("int $3");
    asm volatile("movw $4, %ax");
    asm volatile("movw $5, %ax");
}

