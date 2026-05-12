#include "types.h"
#include "param.h"
#include "mmap.h"

struct mmap_area mmap_area_list[MAXMMAP];

// Find mmap_area slot containing va, owned by proc p.
// Returns pointer to the slot, or 0 if not found.
struct mmap_area*
find_mmap(uint64 va, struct proc *p)
{
  for(int i = 0; i < MAXMMAP; i++){
    if(mmap_area_list[i].p == p &&
       va >= mmap_area_list[i].addr &&
       va < mmap_area_list[i].addr + mmap_area_list[i].length){
      return &mmap_area_list[i];
    }
  }
  return 0;
}