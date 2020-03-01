// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char * alloc_list [1000];
  int alloc_count;
} kmem;



extern char end[]; // first address after kernel loaded from ELF file

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  initlock(&kmem.lock, "kmem");
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += 2*PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    kmem.alloc_list[kmem.alloc_count] = (char*)r;
    kmem.alloc_count++;
  }
  release(&kmem.lock);
  return (char*)r;
}

// dump the pages which have been allocated
//      frames: a pointer to an allocated array of integers that will be filled in by the kernel
//              with a list of all the frame numbers that are currently allocated
//      numframes: The previous numframes allocated frames whose information we are asking for.
int dump_allocated(int *frames, int numframes)
{
  int i,j = 0;
  
  if(numframes >= kmem.alloc_count) return -1;
  for(i = kmem.alloc_count-1; i >= kmem.alloc_count-numframes; i--)
  {
    frames[j] = (int)kmem.alloc_list[i];
    j++;
  }
  return 0;
}