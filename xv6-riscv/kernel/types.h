typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;
// PA4: one node per physical frame, linked into the global LRU list.
#ifndef PA4_STRUCT_PAGE
#define PA4_STRUCT_PAGE
struct page {
  struct page *next, *prev;
  uint64       pagetable;   // owning user page table (0 = not on list)
  uint64       vaddr;       // user VA this frame is mapped to
};
#endif