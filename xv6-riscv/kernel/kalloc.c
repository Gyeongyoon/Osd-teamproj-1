// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/* AI was used (Claude - Anthropic)
   Asked AI how to split kalloc into a swap-capable path (for user pages)
   and a swap-free path (kalloc_noswap, for page-table pages and other
   allocations made while holding a spinlock), to avoid sleeping on disk
   I/O under a lock ("sched locks").
*/

// internal: pull one page off the freelist (never triggers swap)
static void *
kalloc_raw(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// swap-free allocation: freelist only, returns 0 if empty.
// Use for allocations made while holding a lock (page-table pages, etc.).
void *
kalloc_noswap(void)
{
  return kalloc_raw();
}

// normal allocation: if the freelist is empty, evict a user page via swap.
// Use only in lock-free contexts (uvmalloc, exec, uvmcopy data copy, ...).
/* AI was used (Claude - Anthropic)
   Asked AI how kalloc should react under memory pressure: try the normal
   free-list path first, and only when that is empty call swap_out to evict
   a page and hand the freed frame back. swap_out is called with no kmem lock
   held so the disk I/O inside it never sleeps under a spinlock. */
void *
kalloc(void)
{
  void *p = kalloc_raw();
  if(p)
    return p;

  p = swap_out();
  if(p == 0){
    printf("kalloc: out of memory\n");
    return 0;
  }
  memset((char*)p, 5, PGSIZE);
  return p;
}

/*AI was used (Claude - Anthropic)
  Asked AI how to traverse kmem.freelist and calculate free memory in bytes
*/
// Returns the amount of free memory in bytes
// by counting free pages in kmem.freelist and multiplying by PGSIZE.
uint64
meminfo(void)
{
  struct run *r;
  uint64 count = 0;

  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r) {
    count++;
    r = r->next;
  }
  release(&kmem.lock);

  return count * PGSIZE;
}
/*
AI was used.
Asked AI how to traverse kmem.freelist to count the number of free physical pages
*/
// Returns the number of free physical memory pages
int
freemem(void)
{
  struct run *r;
  int count = 0;

  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r){
    count++;
    r = r->next;
  }
  release(&kmem.lock);

  return count;
}