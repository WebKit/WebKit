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

#include "fast_malloc.h"
#include "internal.h"
#include "list.h"
#include "value.h"

#if APPLE_CHANGES
#include <CoreFoundation/CoreFoundation.h>
#include <cxxabi.h>
#include <pthread.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#endif

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
      ptrdiff_t next;
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
  int numLiveObjectsAtLastCollect;
};

static CollectorHeap heap = {NULL, 0, 0, 0, NULL, 0, 0, 0, 0};

bool Collector::memoryFull = false;

void* Collector::allocate(size_t s)
{
  assert(Interpreter::lockCount() > 0);

  // collect if needed
  int numLiveObjects = heap.numLiveObjects;
  if (numLiveObjects - heap.numLiveObjectsAtLastCollect >= ALLOCATIONS_PER_COLLECTION) {
    collect();
    numLiveObjects = heap.numLiveObjects;
  }
  
  if (s > static_cast<size_t>(CELL_SIZE)) {
    // oversize allocator
    if (heap.usedOversizeCells == heap.numOversizeCells) {
      heap.numOversizeCells = MAX(MIN_ARRAY_SIZE, heap.numOversizeCells * GROWTH_FACTOR);
      heap.oversizeCells = (CollectorCell **)kjs_fast_realloc(heap.oversizeCells, heap.numOversizeCells * sizeof(CollectorCell *));
    }
    
    void *newCell = kjs_fast_malloc(s);
    heap.oversizeCells[heap.usedOversizeCells] = (CollectorCell *)newCell;
    heap.usedOversizeCells++;
    heap.numLiveObjects = numLiveObjects + 1;

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
      heap.blocks = (CollectorBlock **)kjs_fast_realloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
    }
    
    targetBlock = (CollectorBlock *)kjs_fast_calloc(1, sizeof(CollectorBlock));
    targetBlock->freeList = targetBlock->cells;
    heap.blocks[heap.usedBlocks] = targetBlock;
    heap.usedBlocks++;
  }
  
  // find a free spot in the block and detach it from the free list
  CollectorCell *newCell = targetBlock->freeList;
  
  // "next" field is a byte offset -- 0 means next cell, so a zeroed block is already initialized
  // could avoid the casts by using a cell offset, but this avoids a relatively-slow multiply
  targetBlock->freeList = reinterpret_cast<CollectorCell *>(reinterpret_cast<char *>(newCell + 1) + newCell->u.freeCell.next);

  targetBlock->usedCells++;
  heap.numLiveObjects = numLiveObjects + 1;

  return newCell;
}

struct Collector::Thread {
  Thread(pthread_t pthread, mach_port_t mthread) : posixThread(pthread), machThread(mthread) {}
  Thread *next;
  pthread_t posixThread;
  mach_port_t machThread;
};

pthread_key_t registeredThreadKey;
pthread_once_t registeredThreadKeyOnce = PTHREAD_ONCE_INIT;
Collector::Thread *registeredThreads;
  
static void destroyRegisteredThread(void *data) 
{
  Collector::Thread *thread = (Collector::Thread *)data;

  if (registeredThreads == thread) {
    registeredThreads = registeredThreads->next;
  } else {
    Collector::Thread *last = registeredThreads;
    for (Collector::Thread *t = registeredThreads->next; t != NULL; t = t->next) {
      if (t == thread) {
        last->next = t->next;
          break;
      }
      last = t;
    }
  }

  delete thread;
}

static void initializeRegisteredThreadKey()
{
  pthread_key_create(&registeredThreadKey, destroyRegisteredThread);
}

void Collector::registerThread()
{
  pthread_once(&registeredThreadKeyOnce, initializeRegisteredThreadKey);

  if (!pthread_getspecific(registeredThreadKey)) {
    pthread_t pthread = pthread_self();
    Collector::Thread *thread = new Collector::Thread(pthread, pthread_mach_thread_np(pthread));
    thread->next = registeredThreads;
    registeredThreads = thread;
    pthread_setspecific(registeredThreadKey, thread);
  }
}

#define IS_POINTER_ALIGNED(p) (((int)(p) & (sizeof(char *) - 1)) == 0)

// cells are 8-byte aligned
#define IS_CELL_ALIGNED(p) (((int)(p) & 7) == 0)

void Collector::markStackObjectsConservatively(void *start, void *end)
{
  if (start > end) {
    void *tmp = start;
    start = end;
    end = tmp;
  }

  assert(((char *)end - (char *)start) < 0x1000000);
  assert(IS_POINTER_ALIGNED(start));
  assert(IS_POINTER_ALIGNED(end));
  
  char **p = (char **)start;
  char **e = (char **)end;
  
  while (p != e) {
    char *x = *p++;
    if (IS_CELL_ALIGNED(x) && x) {
      bool good = false;
      for (int block = 0; block < heap.usedBlocks; block++) {
	size_t offset = x - (char *)heap.blocks[block];
	const size_t lastCellOffset = sizeof(CollectorCell) * (CELLS_PER_BLOCK - 1);
	if (offset <= lastCellOffset && offset % sizeof(CollectorCell) == 0) {
	  good = true;
	  break;
	}
      }
      
      if (!good) {
	int n = heap.usedOversizeCells;
	for (int i = 0; i != n; i++) {
	  if (x == (char *)heap.oversizeCells[i]) {
	    good = true;
	    break;
	  }
	}
      }
      
      if (good && ((CollectorCell *)x)->u.freeCell.zeroIfFree != 0) {
	ValueImp *imp = (ValueImp *)x;
	if (!imp->marked())
	  imp->mark();
      }
    }
  }
}

void Collector::markCurrentThreadConservatively()
{
  jmp_buf registers;
  setjmp(registers);

  pthread_t thread = pthread_self();
  void *stackBase = pthread_get_stackaddr_np(thread);
  int dummy;
  void *stackPointer = &dummy;
  markStackObjectsConservatively(stackPointer, stackBase);
}

typedef unsigned long usword_t; // word size, assumed to be either 32 or 64 bit

void Collector::markOtherThreadConservatively(Thread *thread)
{
  thread_suspend(thread->machThread);

#if defined(__i386__)
  i386_thread_state_t regs;
  unsigned user_count = sizeof(regs)/sizeof(int);
  thread_state_flavor_t flavor = i386_THREAD_STATE;
#elif defined(__ppc__)
  ppc_thread_state_t  regs;
  unsigned user_count = PPC_THREAD_STATE_COUNT;
  thread_state_flavor_t flavor = PPC_THREAD_STATE;
#elif defined(__ppc64__)
  ppc_thread_state64_t  regs;
  unsigned user_count = PPC_THREAD_STATE64_COUNT;
  thread_state_flavor_t flavor = PPC_THREAD_STATE64;
#else
#error Unknown Architecture
#endif
  // get the thread register state
  thread_get_state(thread->machThread, flavor, (thread_state_t)&regs, &user_count);
  
  // scan the registers
  markStackObjectsConservatively((void *)&regs, (void *)((char *)&regs + (user_count * sizeof(usword_t))));
  
  // scan the stack
#if defined(__i386__)
  markStackObjectsConservatively((void *)regs.esp, pthread_get_stackaddr_np(thread->posixThread));
#elif defined(__ppc__) || defined(__ppc64__)
  markStackObjectsConservatively((void *)regs.r1, pthread_get_stackaddr_np(thread->posixThread));
#else
#error Unknown Architecture
#endif

  thread_resume(thread->machThread);
}

void Collector::markStackObjectsConservatively()
{
  markCurrentThreadConservatively();

  for (Thread *thread = registeredThreads; thread != NULL; thread = thread->next) {
    if (thread->posixThread != pthread_self()) {
      markOtherThreadConservatively(thread);
    }
  }
}

void Collector::markProtectedObjects()
{
  int size = ProtectedValues::_tableSize;
  ProtectedValues::KeyValue *table = ProtectedValues::_table;
  for (int i = 0; i < size; i++) {
    ValueImp *val = table[i].key;
    if (val && !val->marked()) {
      val->mark();
    }
  }
}

bool Collector::collect()
{
  assert(Interpreter::lockCount() > 0);

  bool deleted = false;

  if (InterpreterImp::s_hook) {
    InterpreterImp *scr = InterpreterImp::s_hook;
    do {
      //fprintf( stderr, "Collector marking interpreter %p\n",(void*)scr);
      scr->mark();
      scr = scr->next;
    } while (scr != InterpreterImp::s_hook);
  }

  // MARK: first mark all referenced objects recursively starting out from the set of root objects

  markStackObjectsConservatively();
  markProtectedObjects();
  List::markProtectedLists();

  // SWEEP: delete everything with a zero refcount (garbage) and unmark everything else
  
  int emptyBlocks = 0;
  int numLiveObjects = heap.numLiveObjects;

  for (int block = 0; block < heap.usedBlocks; block++) {
    CollectorBlock *curBlock = heap.blocks[block];

    int minimumCellsToProcess = curBlock->usedCells;

    for (int i = 0; i < CELLS_PER_BLOCK; i++) {
      if (minimumCellsToProcess < i) {
	goto skip_block_sweep;
      }

      CollectorCell *cell = curBlock->cells + i;
      ValueImp *imp = reinterpret_cast<ValueImp *>(cell);

      if (cell->u.freeCell.zeroIfFree != 0) {
	if (!imp->_marked)
	{
	  //fprintf( stderr, "Collector::deleting ValueImp %p (%s)\n", (void*)imp, typeid(*imp).name());
	  // emulate destructing part of 'operator delete()'
	  imp->~ValueImp();
	  curBlock->usedCells--;
	  numLiveObjects--;
	  deleted = true;

	  // put it on the free list
	  cell->u.freeCell.zeroIfFree = 0;
	  cell->u.freeCell.next = reinterpret_cast<char *>(curBlock->freeList) - reinterpret_cast<char *>(cell + 1);
	  curBlock->freeList = cell;

	} else {
	  imp->_marked = false;
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
	kjs_fast_free(heap.blocks[block]);
#endif
	// swap with the last block so we compact as we go
	heap.blocks[block] = heap.blocks[heap.usedBlocks - 1];
	heap.usedBlocks--;
	block--; // Don't move forward a step in this case

	if (heap.numBlocks > MIN_ARRAY_SIZE && heap.usedBlocks < heap.numBlocks / LOW_WATER_FACTOR) {
	  heap.numBlocks = heap.numBlocks / GROWTH_FACTOR; 
	  heap.blocks = (CollectorBlock **)kjs_fast_realloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
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
    
    if (!imp->_marked) {
      imp->~ValueImp();
#if DEBUG_COLLECTOR
      heap.oversizeCells[cell]->u.freeCell.zeroIfFree = 0;
#else
      kjs_fast_free((void *)imp);
#endif

      // swap with the last oversize cell so we compact as we go
      heap.oversizeCells[cell] = heap.oversizeCells[heap.usedOversizeCells - 1];

      heap.usedOversizeCells--;
      deleted = true;
      numLiveObjects--;

      if (heap.numOversizeCells > MIN_ARRAY_SIZE && heap.usedOversizeCells < heap.numOversizeCells / LOW_WATER_FACTOR) {
	heap.numOversizeCells = heap.numOversizeCells / GROWTH_FACTOR; 
	heap.oversizeCells = (CollectorCell **)kjs_fast_realloc(heap.oversizeCells, heap.numOversizeCells * sizeof(CollectorCell *));
      }

    } else {
      imp->_marked = false;
      cell++;
    }
  }
  
  heap.numLiveObjects = numLiveObjects;
  heap.numLiveObjectsAtLastCollect = numLiveObjects;
  
  memoryFull = (numLiveObjects >= KJS_MEM_LIMIT);

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
  return 0;
}

int Collector::numReferencedObjects()
{
  int count = 0;

  int size = ProtectedValues::_tableSize;
  ProtectedValues::KeyValue *table = ProtectedValues::_table;
  for (int i = 0; i < size; i++) {
    ValueImp *val = table[i].key;
    if (val) {
      ++count;
    }
  }

  return count;
}

#if APPLE_CHANGES

const void *Collector::rootObjectClasses()
{
  CFMutableSetRef classes = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);

  int size = ProtectedValues::_tableSize;
  ProtectedValues::KeyValue *table = ProtectedValues::_table;
  for (int i = 0; i < size; i++) {
    ValueImp *val = table[i].key;
    if (val) {
      const char *mangled_name = typeid(*val).name();
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
