// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(err&FEC_WR)){
		panic("Not a write fault at pgfault() address 0x%x\n", addr);
	}
	pde_t pde = uvpd[PDX(addr)];
	pte_t pte = uvpt[(uintptr_t)addr >> PGSHIFT];
	if(!(pde & PTE_P) || !(pte & PTE_COW)){
		panic("Not a COW page at pgfault() address 0x%x\n", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, (void *)PFTEMP, PTE_W | PTE_U | PTE_P);
	if(r < 0){
		panic("sys_page_alloc() error in pgfault(): %e\n", r);
	}
	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy(PFTEMP, addr, PGSIZE);	
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P);
	if (r < 0){
		panic("sys_page_map() error in pgfault() : %e\n", r);
	}

	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *addr = (void *)(pn * PGSIZE);
	pte_t pte = uvpt[pn];
	if((pte & PTE_W) || (pte & PTE_COW)){
		r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_P | PTE_U);
		if (r < 0){
			panic("COW sys_page_map(%d) error in duppage() : %e\n", envid, r);
		}
		r = sys_page_map(0, addr, 0, addr, PTE_COW | PTE_P | PTE_U);
		if (r < 0){
			panic("COW sys_page_map(0) error in duppage() : %e\n", r);
		}
	}else{
		r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U);
		if (r < 0){
			panic("sys_page_map(%d) error in duppage() : %e\n", envid, r);
		}
	}

	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	set_pgfault_handler(pgfault);
	envid_t envid = sys_exofork();
	if(envid < 0){
		panic("sys_exofork() error in fork(): %e\n", envid);
	}
	if(envid == 0){ 
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for(int i = 0; i < NPDENTRIES; i ++){
		if((i << PDXSHIFT) >= UTOP) break;
		if(!(uvpd[i] & PTE_P)) continue;
		for(int j = 0; j < NPTENTRIES; j ++) {
			uint32_t pn = i * NPDENTRIES + j;
			if(pn * PGSIZE >= UTOP) break;
			if(pn == ((UXSTACKTOP - PGSIZE) >> PGSHIFT)) continue;
			if(!(uvpt[pn] & PTE_P)) continue;
			duppage(envid, pn);
		}
	}
	
	r = sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P);
	if(r < 0){
		panic("sys_page_alloc() error in fork(): %e\n", envid);
	}
	extern void _pgfault_upcall(void);
	r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if(r < 0){
		panic("sys_env_set_pgfault_upcall() error in fork(): %e\n", envid);
	}
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if(r < 0){
		panic("sys_env_set_status() error in fork(): %e\n", envid);
	}
	return envid;

	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
