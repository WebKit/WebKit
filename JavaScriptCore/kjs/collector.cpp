// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "collector.h"

#include "value.h"
#include "internal.h"

#if APPLE_CHANGES
#include <CoreFoundation/CoreFoundation.h>
#include <cxxabi.h>
#endif

#include <collector.h>
#include <value.h>
#include <internal.h>

namespace KJS {

// tunable parameters
const int MINIMUM_CELL_SIZE = 56;
const int BLOCK_SIZE = (8 * 4096);
const int SPARE_EMPTY_BLOCKS = 2;
const int MIN_ARRAY_SIZE = 14;
const int GROWTH_FACTOR = 2;
const int LOW_WATER_FACTOR = 4;
const int ALLOCATIONS_PER_COLLECTION = 1000;

// derived constants
const int CELL_ARRAY_LENGTH = (MINIMUM_CELL_SIZE / sizeof(double)) + (MINIMUM_CELL_SIZE % sizeof(double) != 0 ? sizeof(double) : 0);
const int CELL_SIZE = CELL_ARRAY_LENGTH * sizeof(double);
const int CELLS_PER_BLOCK = ((BLOCK_SIZE * 8 - sizeof(int32_t) * 8 - sizeof(void *) * 8) / (CELL_SIZE * 8));



struct CollectorCell {
  union {
    double memory[CELL_ARRAY_LENGTH];
    struct {
      void *zeroIfFree;
      CollectorCell *next;
    } freeCell;
  } u;
};


struct CollectorBlock {
  CollectorCell cells[CELLS_PER_BLOCK];
  int32_t usedCells;
  CollectorCell *freeList;
};

struct CollectorHeap {
  CollectorBlock **blocks;
  int numBlocks;
  int usedBlocks;
  int firstBlockWithPossibleSpace;
  
  CollectorCell **oversizeCells;
  int numOversizeCells;
  int usedOversizeCells;

  int numLiveObjects;
  int numAllocationsSinceLastCollect;
};

static CollectorHeap heap = {NULL, 0, 0, 0, NULL, 0, 0, 0, 0};

bool Collector::memoryFull = false;

void* Collector::allocate(size_t s)
{
  assert(Interpreter::lockCount() > 0);

  if (s == 0)
    return 0L;
  
  // collect if needed
  if (++heap.numAllocationsSinceLastCollect >= ALLOCATIONS_PER_COLLECTION) {
    collect();
  }
  
  if (s > (unsigned)CELL_SIZE) {
    // oversize allocator
    if (heap.usedOversizeCells == heap.numOversizeCells) {
      heap.numOversizeCells = MAX(MIN_ARRAY_SIZE, heap.numOversizeCells * GROWTH_FACTOR);
      heap.oversizeCells = (CollectorCell **)realloc(heap.oversizeCells, heap.numOversizeCells * sizeof(CollectorCell *));
    }
    
    void *newCell = malloc(s);
    heap.oversizeCells[heap.usedOversizeCells] = (CollectorCell *)newCell;
    heap.usedOversizeCells++;
    heap.numLiveObjects++;

    ((ValueImp *)(newCell))->_flags = 0;
    return newCell;
  }
  
  // slab allocator
  
  CollectorBlock *targetBlock = NULL;
  
  int i;
  for (i = heap.firstBlockWithPossibleSpace; i < heap.usedBlocks; i++) {
    if (heap.blocks[i]->usedCells < CELLS_PER_BLOCK) {
      targetBlock = heap.blocks[i];
      break;
    }
  }

  heap.firstBlockWithPossibleSpace = i;
  
  if (targetBlock == NULL) {
    // didn't find one, need to allocate a new block
    
    if (heap.usedBlocks == heap.numBlocks) {
      heap.numBlocks = MAX(MIN_ARRAY_SIZE, heap.numBlocks * GROWTH_FACTOR);
      heap.blocks = (CollectorBlock **)realloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
    }
    
    targetBlock = (CollectorBlock *)calloc(1, sizeof(CollectorBlock));
    targetBlock->freeList = targetBlock->cells;
    heap.blocks[heap.usedBlocks] = targetBlock;
    heap.usedBlocks++;
  }
  
  // find a free spot in the block and detach it from the free list
  CollectorCell *newCell = targetBlock->freeList;

  if (newCell->u.freeCell.next != NULL) {
    targetBlock->freeList = newCell->u.freeCell.next;
  } else if (targetBlock->usedCells == (CELLS_PER_BLOCK - 1)) {
    // last cell in this block
    targetBlock->freeList = NULL;
  } else {
    // all next pointers are initially 0, meaning "next cell"
    targetBlock->freeList = newCell + 1;
  }

  targetBlock->usedCells++;
  heap.numLiveObjects++;

  ((ValueImp *)(newCell))->_flags = 0;
  return (void *)(newCell);
}

bool Collector::collect()
{
  assert(Interpreter::lockCount() > 0);

  bool deleted = false;

  // MARK: first mark all referenced objects recursively
  // starting out from the set of root objects
  if (InterpreterImp::s_hook) {
    InterpreterImp *scr = InterpreterImp::s_hook;
    do {
      //fprintf( stderr, "Collector marking interpreter %p\n",(void*)scr);
      scr->mark();
      scr = scr->next;
    } while (scr != InterpreterImp::s_hook);
  }
  
  // mark any other objects that we wouldn't delete anyway
  for (int block = 0; block < heap.usedBlocks; block++) {

    int minimumCellsToProcess = heap.blocks[block]->usedCells;
    CollectorBlock *curBlock = heap.blocks[block];

    for (int cell = 0; cell < CELLS_PER_BLOCK; cell++) {
      if (minimumCellsToProcess < cell) {
	goto skip_block_mark;
      }
	
      ValueImp *imp = (ValueImp *)(curBlock->cells + cell);

      if (((CollectorCell *)imp)->u.freeCell.zeroIfFree != 0) {
	
	if ((imp->_flags & (ValueImp::VI_CREATED|ValueImp::VI_MARKED)) == ValueImp::VI_CREATED &&
	    ((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0)) {
	  imp->mark();
	}
      } else {
	minimumCellsToProcess++;
      }
    }
  skip_block_mark: ;
  }
  
  for (int cell = 0; cell < heap.usedOversizeCells; cell++) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
    if ((imp->_flags & (ValueImp::VI_CREATED|ValueImp::VI_MARKED)) == ValueImp::VI_CREATED &&
	((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0)) {
      imp->mark();
    }
  }

  // SWEEP: delete everything with a zero refcount (garbage) and unmark everything else
  
  int emptyBlocks = 0;

  for (int block = 0; block < heap.usedBlocks; block++) {
    CollectorBlock *curBlock = heap.blocks[block];

    int minimumCellsToProcess = curBlock->usedCells;

    for (int cell = 0; cell < CELLS_PER_BLOCK; cell++) {
      if (minimumCellsToProcess < cell) {
	goto skip_block_sweep;
      }

      ValueImp *imp = (ValueImp *)(curBlock->cells + cell);

      if (((CollectorCell *)imp)->u.freeCell.zeroIfFree != 0) {
	if (!imp->refcount && imp->_flags == (ValueImp::VI_GCALLOWED | ValueImp::VI_CREATED)) {
	  //fprintf( stderr, "Collector::deleting ValueImp %p (%s)\n", (void*)imp, typeid(*imp).name());
	  // emulate destructing part of 'operator delete()'
	  imp->~ValueImp();
	  curBlock->usedCells--;
	  heap.numLiveObjects--;
	  deleted = true;

	  // put it on the free list
	  ((CollectorCell *)imp)->u.freeCell.zeroIfFree = 0;
	  ((CollectorCell *)imp)->u.freeCell.next = curBlock->freeList;
	  curBlock->freeList = (CollectorCell *)imp;

	} else {
	  imp->_flags &= ~ValueImp::VI_MARKED;
	}
      } else {
	minimumCellsToProcess++;
      }
    }

  skip_block_sweep:

    if (heap.blocks[block]->usedCells == 0) {
      emptyBlocks++;
      if (emptyBlocks > SPARE_EMPTY_BLOCKS) {
#if !DEBUG_COLLECTOR
	free(heap.blocks[block]);
#endif
	// swap with the last block so we compact as we go
	heap.blocks[block] = heap.blocks[heap.usedBlocks - 1];
	heap.usedBlocks--;
	block--; // Don't move forward a step in this case

	if (heap.numBlocks > MIN_ARRAY_SIZE && heap.usedBlocks < heap.numBlocks / LOW_WATER_FACTOR) {
	  heap.numBlocks = heap.numBlocks / GROWTH_FACTOR; 
	  heap.blocks = (CollectorBlock **)realloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
	}
      } 
    }
  }

  if (deleted) {
    heap.firstBlockWithPossibleSpace = 0;
  }
  
  int cell = 0;
  while (cell < heap.usedOversizeCells) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
    
    if (!imp->refcount && 
	imp->_flags == (ValueImp::VI_GCALLOWED | ValueImp::VI_CREATED)) {
      
      imp->~ValueImp();
#if DEBUG_COLLECTOR
      heap.oversizeCells[cell]->u.freeCell.zeroIfFree = 0;
#else
      free((void *)imp);
#endif

      // swap with the last oversize cell so we compact as we go
      heap.oversizeCells[cell] = heap.oversizeCells[heap.usedOversizeCells - 1];

      heap.usedOversizeCells--;
      deleted = true;
      heap.numLiveObjects--;

      if (heap.numOversizeCells > MIN_ARRAY_SIZE && heap.usedOversizeCells < heap.numOversizeCells / LOW_WATER_FACTOR) {
	heap.numOversizeCells = heap.numOversizeCells / GROWTH_FACTOR; 
	heap.oversizeCells = (CollectorCell **)realloc(heap.oversizeCells, heap.numOversizeCells * sizeof(CollectorCell *));
      }

    } else {
      imp->_flags &= ~ValueImp::VI_MARKED;
      cell++;
    }
  }
  
  heap.numAllocationsSinceLastCollect = 0;
  
  memoryFull = (heap.numLiveObjects >= KJS_MEM_LIMIT);

  return deleted;
}

int Collector::size() 
{
  return heap.numLiveObjects; 
}

#ifdef KJS_DEBUG_MEM
void Collector::finalCheck()
{
}
#endif

#if APPLE_CHANGES

int Collector::numInterpreters()
{
  int count = 0;
  if (InterpreterImp::s_hook) {
    InterpreterImp *scr = InterpreterImp::s_hook;
    do {
      ++count;
      scr = scr->next;
    } while (scr != InterpreterImp::s_hook);
  }
  return count;
}

int Collector::numGCNotAllowedObjects()
{
  int count = 0;
  for (int block = 0; block < heap.usedBlocks; block++) {
    CollectorBlock *curBlock = heap.blocks[block];

    for (int cell = 0; cell < CELLS_PER_BLOCK; cell++) {
      ValueImp *imp = (ValueImp *)(curBlock->cells + cell);
      
      if (((CollectorCell *)imp)->u.freeCell.zeroIfFree != 0 &&
	  (imp->_flags & ValueImp::VI_GCALLOWED) == 0) {
	++count;
      }
    }
  }
  
  for (int cell = 0; cell < heap.usedOversizeCells; cell++) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
    if ((imp->_flags & ValueImp::VI_GCALLOWED) == 0) {
      ++count;
    }
  }

  return count;
}

int Collector::numReferencedObjects()
{
  int count = 0;
  for (int block = 0; block < heap.usedBlocks; block++) {
    CollectorBlock *curBlock = heap.blocks[block];

    for (int cell = 0; cell < CELLS_PER_BLOCK; cell++) {
      ValueImp *imp = (ValueImp *)(curBlock->cells + cell);
      
      if (((CollectorCell *)imp)->u.freeCell.zeroIfFree != 0 &&
	  imp->refcount != 0) {
	++count;
      }
    }
  }
  
  for (int cell = 0; cell < heap.usedOversizeCells; cell++) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
      if (imp->refcount != 0) {
        ++count;
      }
  }

  return count;
}

const void *Collector::rootObjectClasses()
{
  CFMutableSetRef classes = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
  
  for (int block = 0; block < heap.usedBlocks; block++) {
    CollectorBlock *curBlock = heap.blocks[block];
    for (int cell = 0; cell < CELLS_PER_BLOCK; cell++) {
      ValueImp *imp = (ValueImp *)(curBlock->cells + cell);
      
      if (((CollectorCell *)imp)->u.freeCell.zeroIfFree != 0 &&
	  ((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0)) {
	const char *mangled_name = typeid(*imp).name();
	int status;
	char *demangled_name = __cxxabiv1::__cxa_demangle (mangled_name, NULL, NULL, &status);
	
	CFStringRef className = CFStringCreateWithCString(NULL, demangled_name, kCFStringEncodingASCII);
	free(demangled_name);
	CFSetAddValue(classes, className);
	CFRelease(className);
      }
    }
  }
  
  for (int cell = 0; cell < heap.usedOversizeCells; cell++) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
    
    if ((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0) {
      const char *mangled_name = typeid(*imp).name();
      int status;
      char *demangled_name = __cxxabiv1::__cxa_demangle (mangled_name, NULL, NULL, &status);
      
      CFStringRef className = CFStringCreateWithCString(NULL, demangled_name, kCFStringEncodingASCII);
      free(demangled_name);
      CFSetAddValue(classes, className);
      CFRelease(className);
    }
  }
  
  return classes;
}

#endif // APPLE_CHANGES

} // namespace KJS
