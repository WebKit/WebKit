// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#if APPLE_CHANGES
#define _COLLECTOR
#include <CoreFoundation/CoreFoundation.h>
#include <cxxabi.h>
#endif

#include <collector.h>
#include <value.h>
#include <internal.h>

using namespace KJS;

// tunable parameters
static const int CELL_SIZE = 64;
static const int BLOCK_SIZE = (4 * 4096);
static const int SPARE_EMPTY_BLOCKS = 1;
static const int MIN_ARRAY_SIZE = 14;
static const int GROWTH_FACTOR = 2;
static const int LOW_WATER_FACTOR = 4;

// derived constants
static const int WORD_SIZE = sizeof(uint32_t);
static const int BITS_PER_WORD = WORD_SIZE * 8;
static const uint32_t ALL_BITS_ON = 0xffffffff;
static const int CELLS_PER_BLOCK = ((BLOCK_SIZE * 8 - 32) / (CELL_SIZE * 8 + 1));
static const int BITMAP_SIZE = (CELLS_PER_BLOCK / BITS_PER_WORD) + (CELLS_PER_BLOCK % BITS_PER_WORD != 0 ? 1 : 0);


struct CollectorCell {
  char memory[CELL_SIZE];
};

static const int ALLOCATIONS_PER_COLLECTION = 1000;

struct CollectorBlock {
  CollectorBlock() : usedCells(0) { memset(bitmap, 0, BITMAP_SIZE * WORD_SIZE); }
  
  uint32_t bitmap[BITMAP_SIZE];
  int32_t usedCells;
  CollectorCell cells[CELLS_PER_BLOCK];
};

struct CollectorHeap {
  CollectorBlock **blocks;
  int numBlocks;
  int usedBlocks;
  
  CollectorCell **oversizeCells;
  int numOversizeCells;
  int usedOversizeCells;

  int numLiveObjects;
  int numAllocationsSinceLastCollect;
};

static CollectorHeap heap = {NULL, 0, 0, NULL, 0, 0, 0, 0};

bool Collector::memoryFull = false;


void* Collector::allocate(size_t s)
{
  if (s == 0)
    return 0L;
  
  // collect if needed
  if (++heap.numAllocationsSinceLastCollect >= ALLOCATIONS_PER_COLLECTION)
    collect();
  
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
    
    return (void *)newCell;
  }
  
  // slab allocator
  
  CollectorBlock *targetBlock = NULL;
  
  // try to find free space in an existing block
  for (int i = 0; i < heap.usedBlocks; i++) {
    if (heap.blocks[i]->usedCells < CELLS_PER_BLOCK) {
      targetBlock = heap.blocks[i];
      break;
    }
  }
  
  if (targetBlock == NULL) {
    // didn't find one, need to allocate a new block
    
    if (heap.usedBlocks == heap.numBlocks) {
      heap.numBlocks = MAX(MIN_ARRAY_SIZE, heap.numBlocks * GROWTH_FACTOR);
      heap.blocks = (CollectorBlock **)realloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
    }
    
    targetBlock = new CollectorBlock();
    heap.blocks[heap.usedBlocks] = targetBlock;
    heap.usedBlocks++;
  }
  
  // find a free spot in the block
  for (int wordInBitmap = 0; wordInBitmap < BITMAP_SIZE; wordInBitmap++) {
    uint32_t word = targetBlock->bitmap[wordInBitmap];
    if (word == ALL_BITS_ON) {
      continue;
    }
    for (int bitInWord = 0; bitInWord < BITS_PER_WORD; bitInWord++) {
      if ((word & (1 << bitInWord)) == 0) {
	int cellPos = BITS_PER_WORD * wordInBitmap + bitInWord;
	if (cellPos < CELLS_PER_BLOCK) {
	  targetBlock->bitmap[wordInBitmap] |= (1 << bitInWord);
	  targetBlock->usedCells++;
          heap.numLiveObjects++;
	  return (void *)(targetBlock->cells + cellPos);
	}
      }
    }
  }
  // unreachable, if the free count wasn't 0 there has to be a free
  // cell in the block
  assert(false);

  return false;
}

bool Collector::collect()
{
  bool deleted = false;
  // MARK: first unmark everything
  for (int block = 0; block < heap.usedBlocks; block++) {
    for (int cell = 0; cell < CELLS_PER_BLOCK; cell++) {
      ((ValueImp *)(heap.blocks[block]->cells + cell))->_flags &= ~ValueImp::VI_MARKED;
    }
  }
  for (int cell = 0; cell < heap.usedOversizeCells; cell++) {
    ((ValueImp *)heap.oversizeCells[cell])->_flags &= ~ValueImp::VI_MARKED;
  }
  
  // mark all referenced objects recursively
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
    for (int wordInBitmap = 0; wordInBitmap < BITMAP_SIZE; wordInBitmap++) {
      uint32_t word = heap.blocks[block]->bitmap[wordInBitmap];
      for (int bitInWord = 0; bitInWord < BITS_PER_WORD; bitInWord++) {
	ValueImp *imp = (ValueImp *)(heap.blocks[block]->cells + BITS_PER_WORD * wordInBitmap + bitInWord);
	
	if ((word & (1 << bitInWord)) &&
	    (imp->_flags & (ValueImp::VI_CREATED|ValueImp::VI_MARKED)) == ValueImp::VI_CREATED &&
	    ((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0)) {
	  imp->mark();
	}
      }
    }
  }
  
  for (int cell = 0; cell < heap.usedOversizeCells; cell++) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
    if ((imp->_flags & (ValueImp::VI_CREATED|ValueImp::VI_MARKED)) == ValueImp::VI_CREATED &&
	((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0)) {
      imp->mark();
    }
  }
  
  // SWEEP: delete everything with a zero refcount (garbage)
  
  int emptyBlocks = 0;

  for (int block = 0; block < heap.usedBlocks; block++) {
    for (int wordInBitmap = 0; wordInBitmap < BITMAP_SIZE; wordInBitmap++) {
      uint32_t word = heap.blocks[block]->bitmap[wordInBitmap];
      for (int bitInWord = 0; bitInWord < BITS_PER_WORD; bitInWord++) {
	ValueImp *imp = (ValueImp *)(heap.blocks[block]->cells + BITS_PER_WORD * wordInBitmap + bitInWord);
	
	if ((word & (1 << bitInWord)) &&
	    !imp->refcount && imp->_flags == (ValueImp::VI_GCALLOWED | ValueImp::VI_CREATED)) {
	  // emulate destructing part of 'operator delete()'
	  //fprintf( stderr, "Collector::deleting ValueImp %p (%s)\n", (void*)imp, typeid(*imp).name());
	  imp->~ValueImp();
	  heap.blocks[block]->bitmap[wordInBitmap] &= ~(1 << bitInWord);
	  heap.blocks[block]->usedCells--;
	  heap.numLiveObjects--;
	  deleted = true;
	}
      }
    }

    if (heap.blocks[block]->usedCells == 0) {
      emptyBlocks++;
      if (emptyBlocks > SPARE_EMPTY_BLOCKS) {
	delete heap.blocks[block];

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


  
  int cell = 0;
  while (cell < heap.usedOversizeCells) {
    ValueImp *imp = (ValueImp *)heap.oversizeCells[cell];
    
    if (!imp->refcount && 
	imp->_flags == (ValueImp::VI_GCALLOWED | ValueImp::VI_CREATED)) {
      
      imp->~ValueImp();
      free((void *)imp);

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
      cell++;
    }
  }
  
  // possibly free some completely empty collector blocks ?
  // compact array of collector blocks
  
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
    for (int wordInBitmap = 0; wordInBitmap < BITMAP_SIZE; wordInBitmap++) {
      uint32_t word = heap.blocks[block]->bitmap[wordInBitmap];
      for (int bitInWord = 0; bitInWord < BITS_PER_WORD; bitInWord++) {
	ValueImp *imp = (ValueImp *)(heap.blocks[block]->cells + BITS_PER_WORD * wordInBitmap + bitInWord);
	
	if ((word & (1 << bitInWord)) &&
	    (imp->_flags & ValueImp::VI_GCALLOWED) == 0) {
	  ++count;
	}
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
    for (int wordInBitmap = 0; wordInBitmap < BITMAP_SIZE; wordInBitmap++) {
      uint32_t word = heap.blocks[block]->bitmap[wordInBitmap];
      for (int bitInWord = 0; bitInWord < BITS_PER_WORD; bitInWord++) {
	ValueImp *imp = (ValueImp *)(heap.blocks[block]->cells + BITS_PER_WORD * wordInBitmap + bitInWord);
	
	if ((word & (1 << bitInWord)) &&
	    imp->refcount != 0) {
	  ++count;
	}
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

// FIXME: Rename. Root object classes are more useful than live object classes.
CFSetRef Collector::liveObjectClasses()
{
  CFMutableSetRef classes = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

  for (int block = 0; block < heap.usedBlocks; block++) {
    for (int wordInBitmap = 0; wordInBitmap < BITMAP_SIZE; wordInBitmap++) {
      uint32_t word = heap.blocks[block]->bitmap[wordInBitmap];
      for (int bitInWord = 0; bitInWord < BITS_PER_WORD; bitInWord++) {
	ValueImp *imp = (ValueImp *)(heap.blocks[block]->cells + BITS_PER_WORD * wordInBitmap + bitInWord);
	
	if (word & (1 << bitInWord)
                && ((imp->_flags & ValueImp::VI_GCALLOWED) == 0 || imp->refcount != 0)) {
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
