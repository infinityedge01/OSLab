/* See COPYRIGHT for copyright information. */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>

#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000

#define CRT_ROWS	25
#define CRT_COLS	80
#define CRT_SIZE	(CRT_ROWS * CRT_COLS)

#define COLOR_BLACK 	0
#define COLOR_BLUE 		1
#define COLOR_GREEN 	2
#define COLOR_CYAN		3
#define COLOR_RED		4
#define COLOR_MAGENTA	5
#define COLOR_YELLOW	6
#define COLOR_WHITE		7
#define COLOR_BRIGHT	8
#define DEFAULT_COLOR_ATTRIBUTE 0x0700

void cons_init(void);
int cons_getc(void);

void kbd_intr(void); // irq 1
void serial_intr(void); // irq 4

void set_foreground_color(uint16_t c);

void set_background_color(uint16_t c);
void set_default_color(void);
#endif /* _CONSOLE_H_ */
