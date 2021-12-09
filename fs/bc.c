
#include "fs.h"

#define CACHE_SIZE 10
static void *block_cache[CACHE_SIZE];
static uint32_t timestamp[CACHE_SIZE];
static uint32_t current_time = 0;

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:
	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, addr, PTE_SYSCALL);
	if(r < 0){
		panic("sys_page_alloc() error: %e\n", r);
	}
	r = ide_read(blockno * BLKSECTS, addr, BLKSECTS);
	if(r < 0){
		panic("ide_read() error: %e\n", r);
	}
	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
		
	current_time ++;
	for(uint32_t i = 0; i < CACHE_SIZE; i ++){
		if (addr == block_cache[i]){
			timestamp[i] = current_time;
			return;
		}
	}
	uint32_t evict = 0;
	if(block_cache[evict] == ROUNDDOWN(super, BLKSIZE)){
		evict = 1;
	}
	for(uint32_t i = 0; i < CACHE_SIZE; i ++){
		if(current_time - timestamp[i] > current_time - timestamp[evict] && block_cache[i] != ROUNDDOWN(super, BLKSIZE)){
			evict = i;
		}
	}
	if(block_cache[evict] != NULL && block_cache[evict] != addr){
		if(uvpt[PGNUM(block_cache[evict])] & PTE_D){
			flush_block(block_cache[evict]);
		}
		r = sys_page_unmap(thisenv->env_id, block_cache[evict]);
		if(r < 0){
			panic("sys_page_unmap() error: %e\n", r);
		}
	}
	block_cache[evict] = addr;
	timestamp[evict] = current_time;
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	if(!va_is_mapped(addr)  || !va_is_dirty(addr)){
		return;
	}
	addr = ROUNDDOWN(addr, PGSIZE);
	int r = ide_write(blockno * BLKSECTS, addr, BLKSECTS);
	if(r < 0){
		panic("ide_write() error: %e\n", r);
	}
	r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL);
	if(r < 0){
		panic("sys_page_map() error: %e\n", r);
	}
	//panic("flush_block not implemented");
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	// Now repeat the same experiment, but pass an unaligned address to
	// flush_block.

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");

	// Pass an unaligned address to flush_block.
	flush_block(diskaddr(1) + 20);
	assert(va_is_mapped(diskaddr(1)));

	// Skip the !va_is_dirty() check because it makes the bug somewhat
	// obscure and hence harder to debug.
	//assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	struct Super super;
	set_pgfault_handler(bc_pgfault);
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);
}

