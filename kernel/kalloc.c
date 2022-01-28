// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end,int coreId);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

typedef struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
kmem freelist_core[NCPU];
char* core_freelist_name[8]={"kmem_core0","kmem_core1","kmem_core2","kmem_core3","kmem_core4","kmem_core5","kmem_core6","kmem_core7"};
#define SIZEPERCORE(x) ((void*)PHYSTOP-(void*)(x))/NCPU
struct run* findNextFreelist(int coreId);
void kfree_init(void* pa,int coreId);
void
kinit()
{
  int per_size = SIZEPERCORE(end);
  for(int i =0;i<NCPU;i++)
  {

    initlock(&freelist_core[i].lock,core_freelist_name[i]);
    freerange(end + per_size*i, end + (per_size*(i+1)-1),i);
  }
  //initlock(&kmem.lock, "kmem");
  //freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end,int coreId)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_init(p,coreId);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  push_off();
  int coreId = cpuid();
  //printf("coreId:%d\n",coreId);
  pop_off();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&freelist_core[coreId].lock);
  r->next = freelist_core[coreId].freelist;
  freelist_core[coreId].freelist = r;
  release(&freelist_core[coreId].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int coreId = cpuid();
  pop_off();

  acquire(&freelist_core[coreId].lock);
  r = freelist_core[coreId].freelist;
  if(r)
  {
    freelist_core[coreId].freelist = r->next;
    release(&freelist_core[coreId].lock);
  }
  else
  {
    //printf("%d core empty\n",coreId);
    release(&freelist_core[coreId].lock);
    r = findNextFreelist(coreId);
    //printf("r=%p\n",r);
  }
 

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
struct run*  findNextFreelist(int coreId)
{
  int cId = coreId;
  struct run *r = 0;
  if(cId < NCPU - 1)
  {
    cId = cId+1;
  }
  else
  {
    cId = 0;
  }
  while(cId != coreId)
  {
    acquire(&freelist_core[cId].lock);
    r = freelist_core[cId].freelist;
    if(r)
    {
      
      freelist_core[cId].freelist = r->next;
      release(&freelist_core[cId].lock);

      break;
    }

    //printf("current CoreID:%d\n",cId);
    release(&freelist_core[cId].lock);
    //cId ++;
    if(cId < NCPU - 1)
    {
      cId = cId+1;
    }
    else
    {
      cId = 0;
    }
    //findNextFreelist(cId,r);
  }
  return r;
}
void kfree_init(void* pa,int coreId)
{
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&freelist_core[coreId].lock);
  r->next = freelist_core[coreId].freelist;
  freelist_core[coreId].freelist = r;
  release(&freelist_core[coreId].lock);
}