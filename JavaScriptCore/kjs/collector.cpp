// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#include <wtf/FastMalloc.h>
#include <wtf/FastMallocInternal.h>
#include <wtf/HashCountedSet.h>
#include <wtf/UnusedParam.h>
#include "internal.h"
#include "list.h"
#include "value.h"

#include <setjmp.h>
#include <algorithm>

#include <pthread.h>

#if PLATFORM(DARWIN)

#include <mach/mach_port.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#elif PLATFORM(WIN_OS)

#include <windows.h>

#elif PLATFORM(UNIX)

#include <stdlib.h>
#include <sys/mman.h>

#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#endif

#define DEBUG_COLLECTOR 0

using std::max;

namespace KJS {


// tunable parameters

const size_t SPARE_EMPTY_BLOCKS = 2;
const size_t MIN_ARRAY_SIZE = 14;
const size_t GROWTH_FACTOR = 2;
const size_t LOW_WATER_FACTOR = 4;
const size_t ALLOCATIONS_PER_COLLECTION = 4000;

struct CollectorHeap {
  CollectorBlock **blocks;
  size_t numBlocks;
  size_t usedBlocks;
  size_t firstBlockWithPossibleSpace;
  
  size_t numLiveObjects;
  size_t numLiveObjectsAtLastCollect;
  size_t extraCost;
};

static CollectorHeap heap = {NULL, 0, 0, 0, 0, 0, 0};

size_t Collector::mainThreadOnlyObjectCount = 0;
bool Collector::memoryFull = false;

#ifndef NDEBUG

class GCLock {
    static bool isLocked;

public:
    GCLock()
    {
        ASSERT(!isLocked);
        isLocked = true;
    }
    
    ~GCLock()
    {
        ASSERT(isLocked);
        isLocked = false;
    }
};

bool GCLock::isLocked = false;
#endif

static CollectorBlock* allocateBlock()
{
#if PLATFORM(DARWIN)    
    vm_address_t address = 0;
    vm_map(current_task(), &address, BLOCK_SIZE, BLOCK_OFFSET_MASK, VM_FLAGS_ANYWHERE, MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
#elif PLATFORM(WIN_OS)
     // windows virtual address granularity is naturally 64k
    LPVOID address = VirtualAlloc(NULL, BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif HAVE(POSIX_MEMALIGN)
    void* address;
    posix_memalign(address, BLOCK_SIZE, BLOCK_SIZE);
    memset(reinterpret_cast<void*>(address), 0, BLOCK_SIZE);
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

void Collector::recordExtraCost(size_t cost)
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

    heap.extraCost += cost;
}

void* Collector::allocate(size_t s)
{
  ASSERT(JSLock::lockCount() > 0);
  ASSERT(JSLock::currentThreadIsHoldingLock());
  ASSERT(s <= CELL_SIZE);
  UNUSED_PARAM(s); // s is now only used for the above assert

  // collect if needed
  size_t numLiveObjects = heap.numLiveObjects;
  size_t numLiveObjectsAtLastCollect = heap.numLiveObjectsAtLastCollect;
  size_t numNewObjects = numLiveObjects - numLiveObjectsAtLastCollect;
  size_t newCost = numNewObjects + heap.extraCost;

  if (newCost >= ALLOCATIONS_PER_COLLECTION && newCost >= numLiveObjectsAtLastCollect) {
    collect();
    numLiveObjects = heap.numLiveObjects;
  }
  
#ifndef NDEBUG
  GCLock lock;
#endif
  
  // slab allocator
  
  size_t usedBlocks = heap.usedBlocks;

  size_t i = heap.firstBlockWithPossibleSpace;
  CollectorBlock *targetBlock;
  size_t targetBlockUsedCells;
  if (i != usedBlocks) {
    targetBlock = heap.blocks[i];
    targetBlockUsedCells = targetBlock->usedCells;
    ASSERT(targetBlockUsedCells <= CELLS_PER_BLOCK);
    while (targetBlockUsedCells == CELLS_PER_BLOCK) {
      if (++i == usedBlocks)
        goto allocateNewBlock;
      targetBlock = heap.blocks[i];
      targetBlockUsedCells = targetBlock->usedCells;
      ASSERT(targetBlockUsedCells <= CELLS_PER_BLOCK);
    }
    heap.firstBlockWithPossibleSpace = i;
  } else {
allocateNewBlock:
    // didn't find one, need to allocate a new block
    size_t numBlocks = heap.numBlocks;
    if (usedBlocks == numBlocks) {
      numBlocks = max(MIN_ARRAY_SIZE, numBlocks * GROWTH_FACTOR);
      heap.numBlocks = numBlocks;
      heap.blocks = static_cast<CollectorBlock **>(fastRealloc(heap.blocks, numBlocks * sizeof(CollectorBlock *)));
    }

    targetBlock = allocateBlock();
    targetBlock->freeList = targetBlock->cells;
    targetBlockUsedCells = 0;
    heap.blocks[usedBlocks] = targetBlock;
    heap.usedBlocks = usedBlocks + 1;
    heap.firstBlockWithPossibleSpace = usedBlocks;
  }
  
  // find a free spot in the block and detach it from the free list
  CollectorCell *newCell = targetBlock->freeList;
  
  // "next" field is a byte offset -- 0 means next cell, so a zeroed block is already initialized
  // could avoid the casts by using a cell offset, but this avoids a relatively-slow multiply
  targetBlock->freeList = reinterpret_cast<CollectorCell *>(reinterpret_cast<char *>(newCell + 1) + newCell->u.freeCell.next);

  targetBlock->usedCells = static_cast<uint32_t>(targetBlockUsedCells + 1);
  heap.numLiveObjects = numLiveObjects + 1;

  return newCell;
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
#elif PLATFORM(UNIX)
    static void *stackBase = 0;
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
        (void)rc; // FIXME: deal with error code somehow?  seems fatal...
        ASSERT(stackBase);
        pthread_attr_destroy(&sattr);
        stackThread = thread;
    }
    return (void*)(size_t(stackBase) + stackSize);
#else
#error Need a way to get the stack base on this platform
#endif
}

static pthread_t mainThread;

void Collector::registerAsMainThread()
{
    mainThread = pthread_self();
}

static inline bool onMainThread()
{
#if PLATFORM(DARWIN)
    return pthread_main_np();
#else
    return !!pthread_equal(pthread_self(), mainThread);
#endif
}

#if USE(MULTIPLE_THREADS)

#if PLATFORM(DARWIN)
typedef mach_port_t PlatformThread;
#elif PLATFORM(WIN_OS)
struct PlatformThread {
    PlatformThread(DWORD _id, HANDLE _handle) : id(_id), handle(_handle) {}
    DWORD id;
    HANDLE handle;
};
#endif

static inline PlatformThread getCurrentPlatformThread()
{
#if PLATFORM(DARWIN)
    return pthread_mach_thread_np(pthread_self());
#elif PLATFORM(WIN_OS)
    HANDLE threadHandle = pthread_getw32threadhandle_np(pthread_self());
    return PlatformThread(GetCurrentThreadId(), threadHandle);
#endif
}

class Collector::Thread {
public:
  Thread(pthread_t pthread, const PlatformThread& platThread) : posixThread(pthread), platformThread(platThread) {}
  Thread* next;
  pthread_t posixThread;
  PlatformThread platformThread;
};

pthread_key_t registeredThreadKey;
pthread_once_t registeredThreadKeyOnce = PTHREAD_ONCE_INIT;
Collector::Thread* registeredThreads;

static void destroyRegisteredThread(void* data) 
{
  Collector::Thread* thread = (Collector::Thread*)data;

  // Can't use JSLock convenience object here because we don't want to re-register
  // an exiting thread.
  JSLock::lock();
  
  if (registeredThreads == thread) {
    registeredThreads = registeredThreads->next;
  } else {
    Collector::Thread *last = registeredThreads;
    Collector::Thread *t;
    for (t = registeredThreads->next; t != NULL; t = t->next) {
      if (t == thread) {          
          last->next = t->next;
          break;
      }
      last = t;
    }
    ASSERT(t); // If t is NULL, we never found ourselves in the list.
  }

  JSLock::unlock();

  delete thread;
}

static void initializeRegisteredThreadKey()
{
  pthread_key_create(&registeredThreadKey, destroyRegisteredThread);
}

void Collector::registerThread()
{
  ASSERT(JSLock::lockCount() > 0);
  ASSERT(JSLock::currentThreadIsHoldingLock());
  
  pthread_once(&registeredThreadKeyOnce, initializeRegisteredThreadKey);

  if (!pthread_getspecific(registeredThreadKey)) {
    if (!onMainThread())
        WTF::fastMallocSetIsMultiThreaded();
    Collector::Thread *thread = new Collector::Thread(pthread_self(), getCurrentPlatformThread());

    thread->next = registeredThreads;
    registeredThreads = thread;
    pthread_setspecific(registeredThreadKey, thread);
  }
}

#endif

#define IS_POINTER_ALIGNED(p) (((intptr_t)(p) & (sizeof(char *) - 1)) == 0)

// cell size needs to be a power of two for this to be valid
#define IS_CELL_ALIGNED(p) (((intptr_t)(p) & CELL_MASK) == 0)

void Collector::markStackObjectsConservatively(void *start, void *end)
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
  
  size_t usedBlocks = heap.usedBlocks;
  CollectorBlock **blocks = heap.blocks;

  const size_t lastCellOffset = sizeof(CollectorCell) * (CELLS_PER_BLOCK - 1);

  while (p != e) {
    char* x = *p++;
    if (IS_CELL_ALIGNED(x) && x) {
      uintptr_t offset = reinterpret_cast<uintptr_t>(x) & BLOCK_OFFSET_MASK;
      CollectorBlock* blockAddr = reinterpret_cast<CollectorBlock*>(x - offset);
      for (size_t block = 0; block < usedBlocks; block++) {
        if ((blocks[block] == blockAddr) & (offset <= lastCellOffset)) {
          if (((CollectorCell*)x)->u.freeCell.zeroIfFree != 0) {
            JSCell* imp = reinterpret_cast<JSCell*>(x);
            if (!imp->marked())
              imp->mark();
          }
          break;
        }
      }
    }
  }
}

void Collector::markCurrentThreadConservatively()
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

    void* dummy;
    void* stackPointer = &dummy;
    void* stackBase = currentThreadStackBase();

    markStackObjectsConservatively(stackPointer, stackBase);
}

#if USE(MULTIPLE_THREADS)

static inline void suspendThread(const PlatformThread& platformThread)
{
#if PLATFORM(DARWIN)
  thread_suspend(platformThread);
#elif PLATFORM(WIN_OS)
  SuspendThread(platformThread.handle);
#else
#error Need a way to suspend threads on this platform
#endif
}

static inline void resumeThread(const PlatformThread& platformThread)
{
#if PLATFORM(DARWIN)
  thread_resume(platformThread);
#elif PLATFORM(WIN_OS)
  ResumeThread(platformThread.handle);
#else
#error Need a way to resume threads on this platform
#endif
}

typedef unsigned long usword_t; // word size, assumed to be either 32 or 64 bit

#if PLATFORM(DARWIN)

#if     PLATFORM(X86)
typedef i386_thread_state_t PlatformThreadRegisters;
#elif   PLATFORM(X86_64)
typedef x86_thread_state64_t PlatformThreadRegisters;
#elif   PLATFORM(PPC)
typedef ppc_thread_state_t PlatformThreadRegisters;
#elif   PLATFORM(PPC64)
typedef ppc_thread_state64_t PlatformThreadRegisters;
#else
#error Unknown Architecture
#endif

#elif PLATFORM(WIN_OS)&& PLATFORM(X86)
typedef CONTEXT PlatformThreadRegisters;
#else
#error Need a thread register struct for this platform
#endif

size_t getPlatformThreadRegisters(const PlatformThread& platformThread, PlatformThreadRegisters& regs)
{
#if PLATFORM(DARWIN)

#if     PLATFORM(X86)
  unsigned user_count = sizeof(regs)/sizeof(int);
  thread_state_flavor_t flavor = i386_THREAD_STATE;
#elif   PLATFORM(X86_64)
  unsigned user_count = x86_THREAD_STATE64_COUNT;
  thread_state_flavor_t flavor = x86_THREAD_STATE64;
#elif   PLATFORM(PPC) 
  unsigned user_count = PPC_THREAD_STATE_COUNT;
  thread_state_flavor_t flavor = PPC_THREAD_STATE;
#elif   PLATFORM(PPC64)
  unsigned user_count = PPC_THREAD_STATE64_COUNT;
  thread_state_flavor_t flavor = PPC_THREAD_STATE64;
#else
#error Unknown Architecture
#endif

  kern_return_t result = thread_get_state(platformThread, flavor, (thread_state_t)&regs, &user_count);
  if (result != KERN_SUCCESS) {
    WTFReportFatalError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, 
                        "JavaScript garbage collection failed because thread_get_state returned an error (%d). This is probably the result of running inside Rosetta, which is not supported.", result);
    CRASH();
  }
  return user_count * sizeof(usword_t);
// end PLATFORM(DARWIN)

#elif PLATFORM(WIN_OS) && PLATFORM(X86)
  regs.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_SEGMENTS;
  GetThreadContext(platformThread.handle, &regs);
  return sizeof(CONTEXT);
#else
#error Need a way to get thread registers on this platform
#endif
}

static inline void* otherThreadStackPointer(const PlatformThreadRegisters& regs)
{
#if PLATFORM(DARWIN)

#if __DARWIN_UNIX03

#if PLATFORM(X86)
  return (void*)regs.__esp;
#elif PLATFORM(X86_64)
  return (void*)regs.__rsp;
#elif PLATFORM(PPC) || PLATFORM(PPC64)
  return (void*)regs.__r1;
#else
#error Unknown Architecture
#endif

#else // !__DARWIN_UNIX03

#if PLATFORM(X86)
  return (void*)regs.esp;
#elif PLATFORM(X86_64)
  return (void*)regs.rsp;
#elif (PLATFORM(PPC) || PLATFORM(PPC64))
  return (void*)regs.r1;
#else
#error Unknown Architecture
#endif

#endif // __DARWIN_UNIX03

// end PLATFORM(DARWIN)
#elif PLATFORM(X86) && PLATFORM(WIN_OS)
  return (void*)(uintptr_t)regs.Esp;
#else
#error Need a way to get the stack pointer for another thread on this platform
#endif
}

static inline void* otherThreadStackBase(const PlatformThreadRegisters& regs, Collector::Thread* thread)
{
#if PLATFORM(DARWIN)
  (void)regs;
  return pthread_get_stackaddr_np(thread->posixThread);
// end PLATFORM(DARWIN);
#elif PLATFORM(X86) && PLATFORM(WIN_OS)
  LDT_ENTRY desc;
  NT_TIB* tib;
  GetThreadSelectorEntry(thread->platformThread.handle, regs.SegFs, &desc);
  tib = (NT_TIB*)(uintptr_t)(desc.BaseLow | desc.HighWord.Bytes.BaseMid << 16 | desc.HighWord.Bytes.BaseHi << 24);
  ASSERT(tib == tib->Self);
  return tib->StackBase;
#else
#error Need a way to get the stack pointer for another thread on this platform
#endif
}

void Collector::markOtherThreadConservatively(Thread* thread)
{
  suspendThread(thread->platformThread);

  PlatformThreadRegisters regs;
  size_t regSize = getPlatformThreadRegisters(thread->platformThread, regs);

  // mark the thread's registers
  markStackObjectsConservatively((void*)&regs, (void*)((char*)&regs + regSize));
 
  void* stackPointer = otherThreadStackPointer(regs);
  void* stackBase = otherThreadStackBase(regs, thread);
  markStackObjectsConservatively(stackPointer, stackBase);

  resumeThread(thread->platformThread);
}

#endif

void Collector::markStackObjectsConservatively()
{
  markCurrentThreadConservatively();

#if USE(MULTIPLE_THREADS)
  for (Thread *thread = registeredThreads; thread != NULL; thread = thread->next) {
    if (!pthread_equal(thread->posixThread, pthread_self())) {
      markOtherThreadConservatively(thread);
    }
  }
#endif
}

typedef HashCountedSet<JSCell*> ProtectCountSet;

static ProtectCountSet& protectedValues()
{
    static ProtectCountSet staticProtectCountSet;
    return staticProtectCountSet;
}

void Collector::protect(JSValue *k)
{
    ASSERT(k);
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (JSImmediate::isImmediate(k))
      return;

    protectedValues().add(k->asCell());
}

void Collector::unprotect(JSValue *k)
{
    ASSERT(k);
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (JSImmediate::isImmediate(k))
      return;

    protectedValues().remove(k->asCell());
}

void Collector::collectOnMainThreadOnly(JSValue* value)
{
    ASSERT(value);
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (JSImmediate::isImmediate(value))
      return;

    JSCell* cell = value->asCell();
    cellBlock(cell)->collectOnMainThreadOnly.set(cellOffset(cell));
    ++mainThreadOnlyObjectCount;
}

void Collector::markProtectedObjects()
{
  ProtectCountSet& protectedValues = KJS::protectedValues();
  ProtectCountSet::iterator end = protectedValues.end();
  for (ProtectCountSet::iterator it = protectedValues.begin(); it != end; ++it) {
    JSCell *val = it->first;
    if (!val->marked())
      val->mark();
  }
}

void Collector::markMainThreadOnlyObjects()
{
#if USE(MULTIPLE_THREADS)
    ASSERT(!onMainThread());
#endif

    // Optimization for clients that never register "main thread only" objects.
    if (!mainThreadOnlyObjectCount)
        return;

    // FIXME: We can optimize this marking algorithm by keeping an exact set of 
    // "main thread only" objects when the "main thread only" object count is 
    // small. We don't want to keep an exact set all the time, because WebCore 
    // tends to create lots of "main thread only" objects, and all that set 
    // thrashing can be expensive.
    
    size_t count = 0;
    
    for (size_t block = 0; block < heap.usedBlocks; block++) {
        ASSERT(count < mainThreadOnlyObjectCount);
        
        CollectorBlock* curBlock = heap.blocks[block];
        size_t minimumCellsToProcess = curBlock->usedCells;
        for (size_t i = 0; (i < minimumCellsToProcess) & (i < CELLS_PER_BLOCK); i++) {
            CollectorCell* cell = curBlock->cells + i;
            if (cell->u.freeCell.zeroIfFree == 0)
                ++minimumCellsToProcess;
            else {
                if (curBlock->collectOnMainThreadOnly.get(i)) {
                    if (!curBlock->marked.get(i)) {
                        JSCell* imp = reinterpret_cast<JSCell*>(cell);
                        imp->mark();
                    }
                    if (++count == mainThreadOnlyObjectCount)
                        return;
                }
            }
        }
    }
}

bool Collector::collect()
{
  ASSERT(JSLock::lockCount() > 0);
  ASSERT(JSLock::currentThreadIsHoldingLock());


#ifndef NDEBUG
  GCLock lock;
#endif
  
#if USE(MULTIPLE_THREADS)
    bool currentThreadIsMainThread = onMainThread();
#else
    bool currentThreadIsMainThread = true;
#endif
    
  // MARK: first mark all referenced objects recursively starting out from the set of root objects

#ifndef NDEBUG
  // Forbid malloc during the mark phase. Marking a thread suspends it, so 
  // a malloc inside mark() would risk a deadlock with a thread that had been 
  // suspended while holding the malloc lock.
  fastMallocForbid();
#endif

  if (Interpreter::s_hook) {
    Interpreter* scr = Interpreter::s_hook;
    do {
      scr->mark();
      scr = scr->next;
    } while (scr != Interpreter::s_hook);
  }

  markStackObjectsConservatively();
  markProtectedObjects();
  List::markProtectedLists();
#if USE(MULTIPLE_THREADS)
  if (!currentThreadIsMainThread)
    markMainThreadOnlyObjects();
#endif

#ifndef NDEBUG
  fastMallocAllow();
#endif

  // SWEEP: delete everything with a zero refcount (garbage) and unmark everything else
  
  size_t emptyBlocks = 0;
  size_t numLiveObjects = heap.numLiveObjects;

  for (size_t block = 0; block < heap.usedBlocks; block++) {
    CollectorBlock *curBlock = heap.blocks[block];

    size_t usedCells = curBlock->usedCells;
    CollectorCell *freeList = curBlock->freeList;

    if (usedCells == CELLS_PER_BLOCK) {
      // special case with a block where all cells are used -- testing indicates this happens often
      for (size_t i = 0; i < CELLS_PER_BLOCK; i++) {
        if (!curBlock->marked.get(i)) {
          CollectorCell* cell = curBlock->cells + i;

          // special case for allocated but uninitialized object
          // (We don't need this check earlier because nothing prior this point 
          // assumes the object has a valid vptr.)
          if (cell->u.freeCell.zeroIfFree == 0)
            continue;

          JSCell* imp = reinterpret_cast<JSCell*>(cell);

          ASSERT(currentThreadIsMainThread || !curBlock->collectOnMainThreadOnly.get(i));
          if (curBlock->collectOnMainThreadOnly.get(i)) {
            curBlock->collectOnMainThreadOnly.clear(i);
            --mainThreadOnlyObjectCount;
          }
          imp->~JSCell();
          --usedCells;
          --numLiveObjects;

          // put cell on the free list
          cell->u.freeCell.zeroIfFree = 0;
          cell->u.freeCell.next = reinterpret_cast<char *>(freeList) - reinterpret_cast<char *>(cell + 1);
          freeList = cell;
        }
      }
    } else {
      size_t minimumCellsToProcess = usedCells;
      for (size_t i = 0; (i < minimumCellsToProcess) & (i < CELLS_PER_BLOCK); i++) {
        CollectorCell *cell = curBlock->cells + i;
        if (cell->u.freeCell.zeroIfFree == 0) {
          ++minimumCellsToProcess;
        } else {
          if (!curBlock->marked.get(i)) {
            JSCell *imp = reinterpret_cast<JSCell *>(cell);
            ASSERT(currentThreadIsMainThread || !curBlock->collectOnMainThreadOnly.get(i));
            if (curBlock->collectOnMainThreadOnly.get(i)) {
              curBlock->collectOnMainThreadOnly.clear(i);
              --mainThreadOnlyObjectCount;
            }
            imp->~JSCell();
            --usedCells;
            --numLiveObjects;

            // put cell on the free list
            cell->u.freeCell.zeroIfFree = 0;
            cell->u.freeCell.next = reinterpret_cast<char *>(freeList) - reinterpret_cast<char *>(cell + 1);
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
        freeBlock(curBlock);
#endif
        // swap with the last block so we compact as we go
        heap.blocks[block] = heap.blocks[heap.usedBlocks - 1];
        heap.usedBlocks--;
        block--; // Don't move forward a step in this case

        if (heap.numBlocks > MIN_ARRAY_SIZE && heap.usedBlocks < heap.numBlocks / LOW_WATER_FACTOR) {
          heap.numBlocks = heap.numBlocks / GROWTH_FACTOR; 
          heap.blocks = (CollectorBlock **)fastRealloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock *));
        }
      }
    }
  }

  if (heap.numLiveObjects != numLiveObjects)
    heap.firstBlockWithPossibleSpace = 0;
  
  bool deleted = heap.numLiveObjects != numLiveObjects;

  heap.numLiveObjects = numLiveObjects;
  heap.numLiveObjectsAtLastCollect = numLiveObjects;
  heap.extraCost = 0;
  
  memoryFull = (numLiveObjects >= KJS_MEM_LIMIT);

  return deleted;
}

size_t Collector::size() 
{
  return heap.numLiveObjects; 
}

#ifdef KJS_DEBUG_MEM
void Collector::finalCheck()
{
}
#endif

size_t Collector::numInterpreters()
{
  size_t count = 0;
  if (Interpreter::s_hook) {
    Interpreter* scr = Interpreter::s_hook;
    do {
      ++count;
      scr = scr->next;
    } while (scr != Interpreter::s_hook);
  }
  return count;
}

size_t Collector::numProtectedObjects()
{
  return protectedValues().size();
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

HashCountedSet<const char*>* Collector::rootObjectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    ProtectCountSet& protectedValues = KJS::protectedValues();
    ProtectCountSet::iterator end = protectedValues.end();
    for (ProtectCountSet::iterator it = protectedValues.begin(); it != end; ++it)
        counts->add(typeName(it->first));

    return counts;
}

} // namespace KJS
