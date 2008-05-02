// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "collector.h"

#include "ExecState.h"
#include "JSGlobalObject.h"
#include "internal.h"
#include "list.h"
#include "value.h"
#include <algorithm>
#include <setjmp.h>
#include <stdlib.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashCountedSet.h>
#include <wtf/UnusedParam.h>

#if USE(MULTIPLE_THREADS)
#include <pthread.h>
#include <wtf/ThreadSpecific.h>
#endif

#if PLATFORM(DARWIN)

#include <mach/mach_port.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#include "CollectorHeapIntrospector.h"

#elif PLATFORM(WIN_OS)

#include <windows.h>

#elif PLATFORM(UNIX)

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#if PLATFORM(SOLARIS)
#include <thread.h>
#endif

#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#else
#include <pthread.h>
#endif

#endif

#define DEBUG_COLLECTOR 0
#define COLLECT_ON_EVERY_ALLOCATION 0

using std::max;
using namespace WTF;

namespace KJS {

// tunable parameters

#define SPARE_EMPTY_BLOCKS 2UL
#define MIN_ARRAY_SIZE 14UL
#define GROWTH_FACTOR 2UL
#define LOW_WATER_FACTOR 4UL
#define ALLOCATIONS_PER_COLLECTION 4000UL

Heap::Heap()
{
    memset(this, 0, sizeof(Heap));
}

Heap* Heap::threadHeap()
{
#if USE(MULTIPLE_THREADS)
    static ThreadSpecific<Heap> sharedInstance;
    return sharedInstance;
#else
    static Heap sharedInstance;
    return &sharedInstance;
#endif
}

static NEVER_INLINE CollectorBlock* allocateBlock()
{
#if PLATFORM(DARWIN)    
    vm_address_t address = 0;
    vm_map(current_task(), &address, BLOCK_SIZE, BLOCK_OFFSET_MASK, VM_FLAGS_ANYWHERE, MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
#elif PLATFORM(WIN_OS)
     // windows virtual address granularity is naturally 64k
    LPVOID address = VirtualAlloc(NULL, BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif HAVE(POSIX_MEMALIGN)
    void* address;
    posix_memalign(&address, BLOCK_SIZE, BLOCK_SIZE);
    memset(address, 0, BLOCK_SIZE);
#else
    static size_t pagesize = getpagesize();
    
    size_t extra = 0;
    if (BLOCK_SIZE > pagesize)
        extra = BLOCK_SIZE - pagesize;

    void* mmapResult = mmap(NULL, BLOCK_SIZE + extra, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    uintptr_t address = reinterpret_cast<uintptr_t>(mmapResult);

    size_t adjust = 0;
    if ((address & BLOCK_OFFSET_MASK) != 0)
        adjust = BLOCK_SIZE - (address & BLOCK_OFFSET_MASK);

    if (adjust > 0)
        munmap(reinterpret_cast<void*>(address), adjust);

    if (adjust < extra)
        munmap(reinterpret_cast<void*>(address + adjust + BLOCK_SIZE), extra - adjust);

    address += adjust;
    memset(reinterpret_cast<void*>(address), 0, BLOCK_SIZE);
#endif

    return reinterpret_cast<CollectorBlock*>(address);
}

static void freeBlock(CollectorBlock* block)
{
#if PLATFORM(DARWIN)    
    vm_deallocate(current_task(), reinterpret_cast<vm_address_t>(block), BLOCK_SIZE);
#elif PLATFORM(WIN_OS)
    VirtualFree(block, BLOCK_SIZE, MEM_RELEASE);
#elif HAVE(POSIX_MEMALIGN)
    free(block);
#else
    munmap(block, BLOCK_SIZE);
#endif
}

void Heap::recordExtraCost(size_t cost)
{
    // Our frequency of garbage collection tries to balance memory use against speed
    // by collecting based on the number of newly created values. However, for values
    // that hold on to a great deal of memory that's not in the form of other JS values,
    // that is not good enough - in some cases a lot of those objects can pile up and
    // use crazy amounts of memory without a GC happening. So we track these extra
    // memory costs. Only unusually large objects are noted, and we only keep track
    // of this extra cost until the next GC. In garbage collected languages, most values
    // are either very short lived temporaries, or have extremely long lifetimes. So
    // if a large value survives one garbage collection, there is not much point to
    // collecting more frequently as long as it stays alive.
    // NOTE: we target the primaryHeap unconditionally as JSNumber doesn't modify cost 

    primaryHeap.extraCost += cost;
}

template <Heap::HeapType heapType> struct HeapConstants;

template <> struct HeapConstants<Heap::PrimaryHeap> {
    static const size_t cellSize = CELL_SIZE;
    static const size_t cellsPerBlock = CELLS_PER_BLOCK;
    static const size_t bitmapShift = 0;
    typedef CollectorCell Cell;
    typedef CollectorBlock Block;
};

template <> struct HeapConstants<Heap::NumberHeap> {
    static const size_t cellSize = SMALL_CELL_SIZE;
    static const size_t cellsPerBlock = SMALL_CELLS_PER_BLOCK;
    static const size_t bitmapShift = 1;
    typedef SmallCollectorCell Cell;
    typedef SmallCellCollectorBlock Block;
};

template <Heap::HeapType heapType> void* Heap::heapAllocate(size_t s)
{
  typedef typename HeapConstants<heapType>::Block Block;
  typedef typename HeapConstants<heapType>::Cell Cell;

  CollectorHeap& heap = heapType == PrimaryHeap ? primaryHeap : numberHeap;
  ASSERT(JSLock::lockCount() > 0);
  ASSERT(JSLock::currentThreadIsHoldingLock());
  ASSERT(this == threadHeap());
  ASSERT(s <= HeapConstants<heapType>::cellSize);
  UNUSED_PARAM(s); // s is now only used for the above assert

  ASSERT(heap.operationInProgress == NoOperation);
  ASSERT(heapType == PrimaryHeap || heap.extraCost == 0);
  // FIXME: If another global variable access here doesn't hurt performance
  // too much, we could abort() in NDEBUG builds, which could help ensure we
  // don't spend any time debugging cases where we allocate inside an object's
  // deallocation code.

  size_t numLiveObjects = heap.numLiveObjects;
  size_t usedBlocks = heap.usedBlocks;
  size_t i = heap.firstBlockWithPossibleSpace;

#if COLLECT_ON_EVERY_ALLOCATION
  collect();
#endif

  // if we have a huge amount of extra cost, we'll try to collect even if we still have
  // free cells left.
  if (heapType == PrimaryHeap && heap.extraCost > ALLOCATIONS_PER_COLLECTION) {
      size_t numLiveObjectsAtLastCollect = heap.numLiveObjectsAtLastCollect;
      size_t numNewObjects = numLiveObjects - numLiveObjectsAtLastCollect;
      const size_t newCost = numNewObjects + heap.extraCost;
      if (newCost >= ALLOCATIONS_PER_COLLECTION && newCost >= numLiveObjectsAtLastCollect)
          goto collect;
  }

  ASSERT(heap.operationInProgress == NoOperation);
#ifndef NDEBUG
  // FIXME: Consider doing this in NDEBUG builds too (see comment above).
  heap.operationInProgress = Allocation;
#endif

scan:
  Block* targetBlock;
  size_t targetBlockUsedCells;
  if (i != usedBlocks) {
    targetBlock = (Block*)heap.blocks[i];
    targetBlockUsedCells = targetBlock->usedCells;
    ASSERT(targetBlockUsedCells <= HeapConstants<heapType>::cellsPerBlock);
    while (targetBlockUsedCells == HeapConstants<heapType>::cellsPerBlock) {
      if (++i == usedBlocks)
        goto collect;
      targetBlock = (Block*)heap.blocks[i];
      targetBlockUsedCells = targetBlock->usedCells;
      ASSERT(targetBlockUsedCells <= HeapConstants<heapType>::cellsPerBlock);
    }
    heap.firstBlockWithPossibleSpace = i;
  } else {

collect:
    size_t numLiveObjectsAtLastCollect = heap.numLiveObjectsAtLastCollect;
    size_t numNewObjects = numLiveObjects - numLiveObjectsAtLastCollect;
    const size_t newCost = numNewObjects + heap.extraCost;

    if (newCost >= ALLOCATIONS_PER_COLLECTION && newCost >= numLiveObjectsAtLastCollect) {
#ifndef NDEBUG
      heap.operationInProgress = NoOperation;
#endif
      bool collected = collect();
#ifndef NDEBUG
      heap.operationInProgress = Allocation;
#endif
      if (collected) {
        numLiveObjects = heap.numLiveObjects;
        usedBlocks = heap.usedBlocks;
        i = heap.firstBlockWithPossibleSpace;
        goto scan;
      }
    }
  
    // didn't find a block, and GC didn't reclaim anything, need to allocate a new block
    size_t numBlocks = heap.numBlocks;
    if (usedBlocks == numBlocks) {
      numBlocks = max(MIN_ARRAY_SIZE, numBlocks * GROWTH_FACTOR);
      heap.numBlocks = numBlocks;
      heap.blocks = static_cast<CollectorBlock **>(fastRealloc(heap.blocks, numBlocks * sizeof(CollectorBlock *)));
    }

    targetBlock = (Block*)allocateBlock();
    targetBlock->freeList = targetBlock->cells;
    targetBlock->heap = this;
    targetBlockUsedCells = 0;
    heap.blocks[usedBlocks] = (CollectorBlock*)targetBlock;
    heap.usedBlocks = usedBlocks + 1;
    heap.firstBlockWithPossibleSpace = usedBlocks;
  }
  
  // find a free spot in the block and detach it from the free list
  Cell *newCell = targetBlock->freeList;
  
  // "next" field is a cell offset -- 0 means next cell, so a zeroed block is already initialized
  targetBlock->freeList = (newCell + 1) + newCell->u.freeCell.next;

  targetBlock->usedCells = static_cast<uint32_t>(targetBlockUsedCells + 1);
  heap.numLiveObjects = numLiveObjects + 1;

#ifndef NDEBUG
  // FIXME: Consider doing this in NDEBUG builds too (see comment above).
  heap.operationInProgress = NoOperation;
#endif

  return newCell;
}

void* Heap::allocate(size_t s) 
{
    return heapAllocate<PrimaryHeap>(s);
}

void* Heap::allocateNumber(size_t s) 
{
    return heapAllocate<NumberHeap>(s);
}

static inline void* currentThreadStackBase()
{
#if PLATFORM(DARWIN)
    pthread_t thread = pthread_self();
    return pthread_get_stackaddr_np(thread);
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(MSVC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    __asm {
        MOV EAX, FS:[18h]
        MOV pTib, EAX
    }
    return (void*)pTib->StackBase;
#elif PLATFORM(WIN_OS) && PLATFORM(X86_64) && COMPILER(MSVC)
    PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
    return (void*)pTib->StackBase;
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(GCC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    asm ( "movl %%fs:0x18, %0\n"
          : "=r" (pTib)
        );
    return (void*)pTib->StackBase;
#elif PLATFORM(SOLARIS)
    stack_t s;
    thr_stksegment(&s);
    return s.ss_sp;
#elif PLATFORM(UNIX)
    static void* stackBase = 0;
    static size_t stackSize = 0;
    static pthread_t stackThread;
    pthread_t thread = pthread_self();
    if (stackBase == 0 || thread != stackThread) {
        pthread_attr_t sattr;
        pthread_attr_init(&sattr);
#if HAVE(PTHREAD_NP_H)
        // e.g. on FreeBSD 5.4, neundorf@kde.org
        pthread_attr_get_np(thread, &sattr);
#else
        // FIXME: this function is non-portable; other POSIX systems may have different np alternatives
        pthread_getattr_np(thread, &sattr);
#endif
        int rc = pthread_attr_getstack(&sattr, &stackBase, &stackSize);
        (void)rc; // FIXME: Deal with error code somehow? Seems fatal.
        ASSERT(stackBase);
        pthread_attr_destroy(&sattr);
        stackThread = thread;
    }
    return static_cast<char*>(stackBase) + stackSize;
#else
#error Need a way to get the stack base on this platform
#endif
}

#define IS_POINTER_ALIGNED(p) (((intptr_t)(p) & (sizeof(char *) - 1)) == 0)

// cell size needs to be a power of two for this to be valid
#define IS_HALF_CELL_ALIGNED(p) (((intptr_t)(p) & (CELL_MASK >> 1)) == 0)

void Heap::markStackObjectsConservatively(void *start, void *end)
{
  if (start > end) {
    void* tmp = start;
    start = end;
    end = tmp;
  }

  ASSERT(((char*)end - (char*)start) < 0x1000000);
  ASSERT(IS_POINTER_ALIGNED(start));
  ASSERT(IS_POINTER_ALIGNED(end));
  
  char** p = (char**)start;
  char** e = (char**)end;
    
  size_t usedPrimaryBlocks = primaryHeap.usedBlocks;
  size_t usedNumberBlocks = numberHeap.usedBlocks;
  CollectorBlock **primaryBlocks = primaryHeap.blocks;
  CollectorBlock **numberBlocks = numberHeap.blocks;

  const size_t lastCellOffset = sizeof(CollectorCell) * (CELLS_PER_BLOCK - 1);

  while (p != e) {
      char* x = *p++;
      if (IS_HALF_CELL_ALIGNED(x) && x) {
          uintptr_t xAsBits = reinterpret_cast<uintptr_t>(x);
          xAsBits &= CELL_ALIGN_MASK;
          uintptr_t offset = xAsBits & BLOCK_OFFSET_MASK;
          CollectorBlock* blockAddr = reinterpret_cast<CollectorBlock*>(xAsBits - offset);
          // Mark the the number heap, we can mark these Cells directly to avoid the virtual call cost
          for (size_t block = 0; block < usedNumberBlocks; block++) {
              if ((numberBlocks[block] == blockAddr) & (offset <= lastCellOffset)) {
                  Heap::markCell(reinterpret_cast<JSCell*>(xAsBits));
                  goto endMarkLoop;
              }
          }
          
          // Mark the primary heap
          for (size_t block = 0; block < usedPrimaryBlocks; block++) {
              if ((primaryBlocks[block] == blockAddr) & (offset <= lastCellOffset)) {
                  if (((CollectorCell*)xAsBits)->u.freeCell.zeroIfFree != 0) {
                      JSCell* imp = reinterpret_cast<JSCell*>(xAsBits);
                      if (!imp->marked())
                          imp->mark();
                  }
                  break;
              }
          }
      endMarkLoop:
          ;
      }
  }
}

void NEVER_INLINE Heap::markStackObjectsConservativelyInternal()
{
    void* dummy;
    void* stackPointer = &dummy;
    void* stackBase = currentThreadStackBase();
    markStackObjectsConservatively(stackPointer, stackBase);
}

void Heap::markStackObjectsConservatively()
{
    // setjmp forces volatile registers onto the stack
    jmp_buf registers;
#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4611)
#endif
    setjmp(registers);
#if COMPILER(MSVC)
#pragma warning(pop)
#endif

    markStackObjectsConservativelyInternal();
}

void Heap::protect(JSValue *k)
{
    ASSERT(k);
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (JSImmediate::isImmediate(k))
      return;

    protectedValues.add(k->asCell());
}

void Heap::unprotect(JSValue *k)
{
    ASSERT(k);
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (JSImmediate::isImmediate(k))
      return;

    protectedValues.remove(k->asCell());
}

Heap* Heap::heap(const JSValue* v)
{
    if (JSImmediate::isImmediate(v))
        return 0;
    // FIXME: should assert that the result equals threadHeap(), but currently, this fails as database code uses gcUnprotect from a different thread.
    // That's a race condition and should be fixed.
    return Heap::cellBlock(v->asCell())->heap;
}

void Heap::markProtectedObjects()
{
  HashCountedSet<JSCell*>::iterator end = protectedValues.end();
  for (HashCountedSet<JSCell*>::iterator it = protectedValues.begin(); it != end; ++it) {
    JSCell *val = it->first;
    if (!val->marked())
      val->mark();
  }
}

template <Heap::HeapType heapType> size_t Heap::sweep()
{
    typedef typename HeapConstants<heapType>::Block Block;
    typedef typename HeapConstants<heapType>::Cell Cell;

    // SWEEP: delete everything with a zero refcount (garbage) and unmark everything else
    CollectorHeap& heap = heapType == Heap::PrimaryHeap ? primaryHeap : numberHeap;
    
    size_t emptyBlocks = 0;
    size_t numLiveObjects = heap.numLiveObjects;
    
    for (size_t block = 0; block < heap.usedBlocks; block++) {
        Block* curBlock = (Block*)heap.blocks[block];
        
        size_t usedCells = curBlock->usedCells;
        Cell* freeList = curBlock->freeList;
        
        if (usedCells == HeapConstants<heapType>::cellsPerBlock) {
            // special case with a block where all cells are used -- testing indicates this happens often
            for (size_t i = 0; i < HeapConstants<heapType>::cellsPerBlock; i++) {
                if (!curBlock->marked.get(i >> HeapConstants<heapType>::bitmapShift)) {
                    Cell* cell = curBlock->cells + i;
                    
                    if (heapType != Heap::NumberHeap) {
                        JSCell* imp = reinterpret_cast<JSCell*>(cell);
                        // special case for allocated but uninitialized object
                        // (We don't need this check earlier because nothing prior this point 
                        // assumes the object has a valid vptr.)
                        if (cell->u.freeCell.zeroIfFree == 0)
                            continue;
                        
                        imp->~JSCell();
                    }
                    
                    --usedCells;
                    --numLiveObjects;
                    
                    // put cell on the free list
                    cell->u.freeCell.zeroIfFree = 0;
                    cell->u.freeCell.next = freeList - (cell + 1);
                    freeList = cell;
                }
            }
        } else {
            size_t minimumCellsToProcess = usedCells;
            for (size_t i = 0; (i < minimumCellsToProcess) & (i < HeapConstants<heapType>::cellsPerBlock); i++) {
                Cell *cell = curBlock->cells + i;
                if (cell->u.freeCell.zeroIfFree == 0) {
                    ++minimumCellsToProcess;
                } else {
                    if (!curBlock->marked.get(i >> HeapConstants<heapType>::bitmapShift)) {
                        if (heapType != Heap::NumberHeap) {
                            JSCell *imp = reinterpret_cast<JSCell*>(cell);
                            imp->~JSCell();
                        }
                        --usedCells;
                        --numLiveObjects;
                        
                        // put cell on the free list
                        cell->u.freeCell.zeroIfFree = 0;
                        cell->u.freeCell.next = freeList - (cell + 1); 
                        freeList = cell;
                    }
                }
            }
        }
        
        curBlock->usedCells = static_cast<uint32_t>(usedCells);
        curBlock->freeList = freeList;
        curBlock->marked.clearAll();
        
        if (usedCells == 0) {
            emptyBlocks++;
            if (emptyBlocks > SPARE_EMPTY_BLOCKS) {
#if !DEBUG_COLLECTOR
                freeBlock((CollectorBlock*)curBlock);
#endif
                // swap with the last block so we compact as we go
                heap.blocks[block] = heap.blocks[heap.usedBlocks - 1];
                heap.usedBlocks--;
                block--; // Don't move forward a step in this case
                
                if (heap.numBlocks > MIN_ARRAY_SIZE && heap.usedBlocks < heap.numBlocks / LOW_WATER_FACTOR) {
                    heap.numBlocks = heap.numBlocks / GROWTH_FACTOR; 
                    heap.blocks = (CollectorBlock**)fastRealloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
                }
            }
        }
    }
    
    if (heap.numLiveObjects != numLiveObjects)
        heap.firstBlockWithPossibleSpace = 0;
        
    heap.numLiveObjects = numLiveObjects;
    heap.numLiveObjectsAtLastCollect = numLiveObjects;
    heap.extraCost = 0;
    return numLiveObjects;
}
    
bool Heap::collect()
{
  ASSERT(JSLock::lockCount() > 0);
  ASSERT(JSLock::currentThreadIsHoldingLock());
  ASSERT(this == threadHeap());

  ASSERT((primaryHeap.operationInProgress == NoOperation) | (numberHeap.operationInProgress == NoOperation));
  if ((primaryHeap.operationInProgress != NoOperation) | (numberHeap.operationInProgress != NoOperation))
    abort();
    
  primaryHeap.operationInProgress = Collection;
  numberHeap.operationInProgress = Collection;

  // MARK: first mark all referenced objects recursively starting out from the set of root objects

#ifndef NDEBUG
  // Forbid malloc during the mark phase. Marking a thread suspends it, so 
  // a malloc inside mark() would risk a deadlock with a thread that had been 
  // suspended while holding the malloc lock.
  fastMallocForbid();
#endif

  markStackObjectsConservatively();
  markProtectedObjects();
  if (m_markListSet.size())
    List::markProtectedLists(m_markListSet);

#ifndef NDEBUG
  fastMallocAllow();
#endif
    
  size_t originalLiveObjects = primaryHeap.numLiveObjects + numberHeap.numLiveObjects;
  size_t numLiveObjects = sweep<PrimaryHeap>();
  numLiveObjects += sweep<NumberHeap>();
  
  primaryHeap.operationInProgress = NoOperation;
  numberHeap.operationInProgress = NoOperation;
  
  return numLiveObjects < originalLiveObjects;
}

size_t Heap::size() 
{
  return primaryHeap.numLiveObjects + numberHeap.numLiveObjects; 
}

size_t Heap::globalObjectCount()
{
  size_t count = 0;
  if (JSGlobalObject::head()) {
    JSGlobalObject* o = JSGlobalObject::head();
    do {
      ++count;
      o = o->next();
    } while (o != JSGlobalObject::head());
  }
  return count;
}

size_t Heap::protectedGlobalObjectCount()
{
  size_t count = 0;
  if (JSGlobalObject::head()) {
    JSGlobalObject* o = JSGlobalObject::head();
    do {
      if (protectedValues.contains(o))
        ++count;
      o = o->next();
    } while (o != JSGlobalObject::head());
  }
  return count;
}

size_t Heap::protectedObjectCount()
{
  return protectedValues.size();
}

static const char *typeName(JSCell *val)
{
  const char *name = "???";
  switch (val->type()) {
    case UnspecifiedType:
      break;
    case UndefinedType:
      name = "undefined";
      break;
    case NullType:
      name = "null";
      break;
    case BooleanType:
      name = "boolean";
      break;
    case StringType:
      name = "string";
      break;
    case NumberType:
      name = "number";
      break;
    case ObjectType: {
      const ClassInfo *info = static_cast<JSObject *>(val)->classInfo();
      name = info ? info->className : "Object";
      break;
    }
    case GetterSetterType:
      name = "gettersetter";
      break;
  }
  return name;
}

HashCountedSet<const char*>* Heap::protectedObjectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    HashCountedSet<JSCell*>::iterator end = protectedValues.end();
    for (HashCountedSet<JSCell*>::iterator it = protectedValues.begin(); it != end; ++it)
        counts->add(typeName(it->first));

    return counts;
}

bool Heap::isBusy()
{
    return (primaryHeap.operationInProgress != NoOperation) | (numberHeap.operationInProgress != NoOperation);
}

void Heap::reportOutOfMemoryToAllExecStates()
{
    if (!JSGlobalObject::head())
        return;

    JSGlobalObject* globalObject = JSGlobalObject::head();
    do {
        ExecStateStack::const_iterator end = globalObject->activeExecStates().end();
        for (ExecStateStack::const_iterator it = globalObject->activeExecStates().begin(); it != end; ++it)
            (*it)->setException(Error::create(*it, GeneralError, "Out of memory"));
        globalObject = globalObject->next();
    } while (globalObject != JSGlobalObject::head());
}

} // namespace KJS
