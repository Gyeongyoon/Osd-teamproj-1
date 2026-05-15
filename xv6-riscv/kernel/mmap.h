/* AI was used
Asked AI how to define and manage a global mmap_area array shared across processes,
and how to implement find_mmap() to locate a slot by virtual address and process pointer 
*/

#pragma once

#include "types.h"
#include "param.h"

struct file;
struct proc;

struct mmap_area {
  struct file *f;     // file pointer (NULL for anonymous mapping)
  uint64 addr;        // start VA of mapping (= MMAPBASE + addr argument)
  int length;         // size of mapping in bytes
  int offset;         // file offset
  int prot;           // protection flags (PROT_READ / PROT_WRITE)
  int flags;          // mapping flags (MAP_ANONYMOUS / MAP_POPULATE)
  struct proc *p;     // owner process (0 if slot is empty)
};

extern struct mmap_area mmap_area_list[MAXMMAP];

struct mmap_area* find_mmap(uint64 va, struct proc *p);