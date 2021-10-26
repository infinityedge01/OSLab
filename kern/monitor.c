// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>
#include <kern/trap.h>

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
	{ "backtrace", "Display information about the backtrace", mon_backtrace },
	{ "showmappings", "Display information about the memory mappings", mon_showmappings },
	{ "chperm", "Change the permission of a virtual memory page", mon_chperm },
	{ "dumpvmem", "Dump the virtual memory", mon_dumpvmem },
	{ "dumppmem", "Dump the physical memory", mon_dumppmem },
};

/***** Implementations of basic kernel monitor commands *****/

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
	// Your code here.
	int* ebp;
	struct Eipdebuginfo info;
	cprintf("Stack backtrace:\n");
	ebp = (int *)read_ebp();
	while((unsigned)ebp != 0){
		cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", ebp, ebp[1], ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);
		if(debuginfo_eip(ebp[1], &info) == 0){
			cprintf("\t\t%s:%d: ", info.eip_file, info.eip_line);
			cprintf("%.*s", info.eip_fn_namelen, info.eip_fn_name);
			cprintf("+%d\n", ebp[1] - info.eip_fn_addr);
		}
		ebp = (int*)*ebp;
	}
	return 0;
}


void mon_showmappings_help(){
	cprintf("Usage: showmappings st ed\n");
	cprintf("st, ed are virtual address in hex, starts with 0x\n");
}

int parse_hex_values(char* ch, uintptr_t* val){
	size_t len = strlen(ch), i;
	*val = 0;
	if(len > 10 || len <= 2 || ch[0] != '0' || ch[1] != 'x'){
		return -1;
	}
	for(i = 2; i < 10 && i < len; i ++) {
		*val = *val * 16 + ch[i];
		if(ch[i] >= 'a' && ch[i] <= 'f') *val -= 'a' - 10;
		else if(ch[i] >= 'A' && ch[i] <= 'F') *val -= 'A' - 10;
		else if(ch[i] >= '0' && ch[i] <= '9') *val -= '0'; 
		else{
			return -1;
		}
	}
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t st, ed;
	size_t i, j;
	if (argc != 3) {
		mon_showmappings_help();
		return 0;
	}
	if (parse_hex_values(argv[1], &st) == -1 || parse_hex_values(argv[2], &ed) == -1){
		mon_showmappings_help();
		return 0;
	}
	if(st > ed){
		mon_showmappings_help();
		return 0;
	}
	pmap_showmappings(st, ed);
	return 0;
}

void mon_chperm_help(){
	cprintf("Usage: showmappings vaddr perm\n");
	cprintf("vaddr, perm are in hex, starts with 0x\n");
}
int
mon_chperm(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va, perm;
	size_t i, j;
	if (argc != 3) {
		mon_chperm_help();
		return 0;
	}
	if (parse_hex_values(argv[1], &va) == -1 || parse_hex_values(argv[2], &perm) == -1){
		mon_chperm_help();
		return 0;
	}
	pmap_chperm(va, perm);
	return 0;
}

void mon_dumpvmem_help(){
	cprintf("Usage: dumpvmem st ed\n");
	cprintf("st, ed are virtual address in hex, starts with 0x\n");
}

int
mon_dumpvmem(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t st, ed;
	size_t i, j;
	if (argc != 3) {
		mon_dumpvmem_help();
		return 0;
	}
	if (parse_hex_values(argv[1], &st) == -1 || parse_hex_values(argv[2], &ed) == -1){
		mon_dumpvmem_help();
		return 0;
	}
	if(st > ed){
		mon_dumpvmem_help();
		return 0;
	}
	pmap_dumpvmem(st, ed);
	return 0;
}


void mon_dumppmem_help(){
	cprintf("Usage: dumppmem st ed\n");
	cprintf("st, ed are physical address in hex, starts with 0x\n");
}

int
mon_dumppmem(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t st, ed;
	size_t i, j;
	if (argc != 3) {
		mon_dumppmem_help();
		return 0;
	}
	if (parse_hex_values(argv[1], &st) == -1 || parse_hex_values(argv[2], &ed) == -1){
		mon_dumppmem_help();
		return 0;
	}
	if(st > ed){
		mon_dumppmem_help();
		return 0;
	}
	pmap_dumppmem(st, ed);
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
	set_foreground_color(COLOR_BLACK);
	set_background_color(COLOR_WHITE | COLOR_BRIGHT);
	
	cprintf("Good night. And see you tomorrow, Miss Diana.\n");
	set_foreground_color(COLOR_BLUE);
	cprintf("Blue ");
	set_foreground_color(COLOR_GREEN);
	cprintf("Green ");
	set_foreground_color(COLOR_RED);
	cprintf("Red ");
	set_background_color(COLOR_BLACK);
	set_foreground_color(COLOR_CYAN | COLOR_BRIGHT);
	cprintf("Bright Cyan ");
	set_foreground_color(COLOR_MAGENTA | COLOR_BRIGHT);
	cprintf("Bright Magenta ");
	set_foreground_color(COLOR_YELLOW | COLOR_BRIGHT);
	cprintf("Bright Yellow\n");

	set_default_color();
	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
