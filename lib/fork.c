// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
extern void _pgfault_upcall(void);
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
	pte_t * p = (pte_t *) UVPT;
	if (!(err & FEC_WR && (p[PGNUM(addr)] & (PTE_COW | PTE_W)))) panic("fork gg\n");


	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, ROUNDDOWN(PFTEMP, PGSIZE), PTE_W | PTE_U | PTE_P);
	if (r < 0) panic("gg %d\n", r);
	memcpy((void *) ROUNDDOWN(PFTEMP, PGSIZE), (void *) ROUNDDOWN(addr, PGSIZE), PGSIZE);

	r = sys_page_map(0, (void *) ROUNDDOWN(PFTEMP, PGSIZE), 0, (void *) ROUNDDOWN(addr, PGSIZE), PTE_W | PTE_P | PTE_U);
	if (r < 0) panic("gg %d\n", r);
	r = sys_page_unmap(0, (void *) ROUNDDOWN(PFTEMP, PGSIZE));
	if (r < 0) panic("gg %d\n", r);
	// panic("pgfault not implemented");
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

	r = sys_page_map(0, (void *) (pn * PGSIZE), envid, (void *) (pn * PGSIZE), PTE_COW | PTE_U | PTE_P);
	if (r < 0) return r;
	r = sys_page_map(0, (void *) (pn * PGSIZE), 0, (void *) (pn * PGSIZE), PTE_COW | PTE_U | PTE_P);
	if (r < 0) return r;	
	// LAB 4: Your code here.
	// panic("duppage not implemented");
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
	int r;
	set_pgfault_handler(pgfault);
	envid_t env = sys_exofork();
	if (env < 0) panic("fork fail\n");

	if (env == 0){
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	pte_t * p = (pte_t *) UVPT;
	for(size_t i = 0; i < PGNUM(UTOP) - 1; ++i){
		if (!((uvpd[(i/1024)] & PTE_P) && (p[i] & PTE_P))) continue;

		if (p[i] & (PTE_COW | PTE_W)){
			r = duppage(env, i);
			if (r < 0) panic("gg\n");
		}
		else {
			sys_page_map(0, (void *) (i * PGSIZE), env, (void *) (i * PGSIZE), p[i] & PTE_SYSCALL);
		}
	}
	r = sys_page_alloc(env, (void *) (UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P | PTE_AVAIL);
	if (r < 0) panic("gg\n");
	r = sys_env_set_pgfault_upcall(env, _pgfault_upcall);
	if (r < 0) panic("gg\n");
	r = sys_env_set_status(env, ENV_RUNNABLE);
	if (r < 0) panic("gg\n");
 	return env;
	// LAB 4: Your code here.
	// panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
