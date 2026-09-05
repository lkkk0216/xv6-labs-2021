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
} kmem[NCPU];

void
kinit()
{
  initlock(&kmem[0].lock, "kmem0");
  initlock(&kmem[1].lock, "kmem1");
  initlock(&kmem[2].lock, "kmem2");
  initlock(&kmem[3].lock, "kmem3");
  initlock(&kmem[4].lock, "kmem4");
  initlock(&kmem[5].lock, "kmem5");
  initlock(&kmem[6].lock, "kmem6");
  initlock(&kmem[7].lock, "kmem7");
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

// Free the page of physical memory pointed at by v,
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

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  // 本CPU没有空闲页时，从其他CPU拿一批过来
  if(r == 0){
    for(int i = 0; i < NCPU; i++){
      if(i == id)
        continue;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r){
        struct run *last = r;
        for(int n = 1; n < 64 && last->next; n++)
          last = last->next;
        kmem[i].freelist = last->next;
        last->next = 0;
      }
      release(&kmem[i].lock);
      if(r){
        struct run *extra = r->next;
        r->next = 0;
        if(extra){
          acquire(&kmem[id].lock);
          struct run *last = extra;
          while(last->next)
            last = last->next;
          last->next = kmem[id].freelist;
          kmem[id].freelist = extra;
          release(&kmem[id].lock);
        }
        break;
      }
    }
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
