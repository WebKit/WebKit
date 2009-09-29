/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "Collector.h"

#include "ArgList.h"
#include "CallFrame.h"
#include "CodeBlock.h"
#include "CollectorHeapIterator.h"
#include "Interpreter.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "JSString.h"
#include "JSValue.h"
#include "MarkStack.h"
#include "Nodes.h"
#include "Tracing.h"
#include <algorithm>
#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashCountedSet.h>
#include <wtf/UnusedParam.h>
#include <wtf/VMTags.h>

#if PLATFORM(DARWIN)

#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#elif PLATFORM(SYMBIAN)
#include <e32std.h>
#include <e32cmn.h>
#include <unistd.h>

#elif PLATFORM(WIN_OS)

#include <windows.h>
#include <malloc.h>

#elif PLATFORM(HAIKU)

#include <OS.h>

#elif PLATFORM(UNIX)

#include <stdlib.h>
#if !PLATFORM(HAIKU)
#include <sys/mman.h>
#endif
#include <unistd.h>

#if PLATFORM(SOLARIS)
#include <thread.h>
#else
#include <pthread.h>
#endif

#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#if PLATFORM(QNX)
#include <fcntl.h>
#include <sys/procfs.h>
#include <stdio.h>
#include <errno.h>
#endif

#endif

#define COLLECT_ON_EVERY_ALLOCATION 0

using std::max;

namespace JSC {

// tunable parameters

const size_t GROWTH_FACTOR = 2;
const size_t LOW_WATER_FACTOR = 4;
const size_t ALLOCATIONS_PER_COLLECTION = 4000;
// This value has to be a macro to be used in max() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define MIN_ARRAY_SIZE (static_cast<size_t>(14))

#if PLATFORM(SYMBIAN)
const size_t MAX_NUM_BLOCKS = 256; // Max size of collector heap set to 16 MB
static RHeap* userChunk = 0;
#endif

#if ENABLE(JSC_MULTIPLE_THREADS)

#if PLATFORM(DARWIN)
typedef mach_port_t PlatformThread;
#elif PLATFORM(WIN_OS)
struct PlatformThread {
    PlatformThread(DWORD _id, HANDLE _handle) : id(_id), handle(_handle) {}
    DWORD id;
    HANDLE handle;
};
#endif

class Heap::Thread {
public:
    Thread(pthread_t pthread, const PlatformThread& platThread, void* base) 
        : posixThread(pthread)
        , platformThread(platThread)
        , stackBase(base)
    {
    }

    Thread* next;
    pthread_t posixThread;
    PlatformThread platformThread;
    void* stackBase;
};

#endif

Heap::Heap(JSGlobalData* globalData)
    : m_markListSet(0)
#if ENABLE(JSC_MULTIPLE_THREADS)
    , m_registeredThreads(0)
    , m_currentThreadRegistrar(0)
#endif
    , m_globalData(globalData)
{
    ASSERT(globalData);

#if PLATFORM(SYMBIAN)
    // Symbian OpenC supports mmap but currently not the MAP_ANON flag.
    // Using fastMalloc() does not properly align blocks on 64k boundaries
    // and previous implementation was flawed/incomplete.
    // UserHeap::ChunkHeap allows allocation of continuous memory and specification
    // of alignment value for (symbian) cells within that heap.
    //
    // Clarification and mapping of terminology:
    // RHeap (created by UserHeap::ChunkHeap below) is continuos memory chunk,
    // which can dynamically grow up to 8 MB,
    // that holds all CollectorBlocks of this session (static).
    // Each symbian cell within RHeap maps to a 64kb aligned CollectorBlock.
    // JSCell objects are maintained as usual within CollectorBlocks.
    if (!userChunk) {
        userChunk = UserHeap::ChunkHeap(0, 0, MAX_NUM_BLOCKS * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
        if (!userChunk)
            CRASH();
    }
#endif // PLATFORM(SYMBIAN)
    
    memset(&primaryHeap, 0, sizeof(CollectorHeap));
    memset(&numberHeap, 0, sizeof(CollectorHeap));
}

Heap::~Heap()
{
    // The destroy function must already have been called, so assert this.
    ASSERT(!m_globalData);
}

void Heap::destroy()
{
    JSLock lock(SilenceAssertionsOnly);

    if (!m_globalData)
        return;

    // The global object is not GC protected at this point, so sweeping may delete it
    // (and thus the global data) before other objects that may use the global data.
    RefPtr<JSGlobalData> protect(m_globalData);

    delete m_markListSet;
    m_markListSet = 0;

    sweep<PrimaryHeap>();
    // No need to sweep number heap, because the JSNumber destructor doesn't do anything.

    ASSERT(!primaryHeap.numLiveObjects);

    freeBlocks(&primaryHeap);
    freeBlocks(&numberHeap);

#if ENABLE(JSC_MULTIPLE_THREADS)
    if (m_currentThreadRegistrar) {
        int error = pthread_key_delete(m_currentThreadRegistrar);
        ASSERT_UNUSED(error, !error);
    }

    MutexLocker registeredThreadsLock(m_registeredThreadsMutex);
    for (Heap::Thread* t = m_registeredThreads; t;) {
        Heap::Thread* next = t->next;
        delete t;
        t = next;
    }
#endif

    m_globalData = 0;
}

template <HeapType heapType>
NEVER_INLINE CollectorBlock* Heap::allocateBlock()
{
#if PLATFORM(DARWIN)
    vm_address_t address = 0;
    // FIXME: tag the region as a JavaScriptCore heap when we get a registered VM tag: <rdar://problem/6054788>.
    vm_map(current_task(), &address, BLOCK_SIZE, BLOCK_OFFSET_MASK, VM_FLAGS_ANYWHERE | VM_TAG_FOR_COLLECTOR_MEMORY, MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
#elif PLATFORM(SYMBIAN)
    // Allocate a 64 kb aligned CollectorBlock
    unsigned char* mask = reinterpret_cast<unsigned char*>(userChunk->Alloc(BLOCK_SIZE));
    if (!mask)
        CRASH();
    uintptr_t address = reinterpret_cast<uintptr_t>(mask);

    memset(reinterpret_cast<void*>(address), 0, BLOCK_SIZE);
#elif PLATFORM(WINCE)
    void* address = VirtualAlloc(NULL, BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif PLATFORM(WIN_OS)
#if COMPILER(MINGW)
    void* address = __mingw_aligned_malloc(BLOCK_SIZE, BLOCK_SIZE);
#else
    void* address = _aligned_malloc(BLOCK_SIZE, BLOCK_SIZE);
#endif
    memset(address, 0, BLOCK_SIZE);
#elif HAVE(POSIX_MEMALIGN)
    void* address;
    posix_memalign(&address, BLOCK_SIZE, BLOCK_SIZE);
    memset(address, 0, BLOCK_SIZE);
#else

#if ENABLE(JSC_MULTIPLE_THREADS)
#error Need to initialize pagesize safely.
#endif
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
        munmap(reinterpret_cast<char*>(address), adjust);

    if (adjust < extra)
        munmap(reinterpret_cast<char*>(address + adjust + BLOCK_SIZE), extra - adjust);

    address += adjust;
    memset(reinterpret_cast<void*>(address), 0, BLOCK_SIZE);
#endif

    CollectorBlock* block = reinterpret_cast<CollectorBlock*>(address);
    block->freeList = block->cells;
    block->heap = this;
    block->type = heapType;

    CollectorHeap& heap = heapType == PrimaryHeap ? primaryHeap : numberHeap;
    size_t numBlocks = heap.numBlocks;
    if (heap.usedBlocks == numBlocks) {
        static const size_t maxNumBlocks = ULONG_MAX / sizeof(CollectorBlock*) / GROWTH_FACTOR;
        if (numBlocks > maxNumBlocks)
            CRASH();
        numBlocks = max(MIN_ARRAY_SIZE, numBlocks * GROWTH_FACTOR);
        heap.numBlocks = numBlocks;
        heap.blocks = static_cast<CollectorBlock**>(fastRealloc(heap.blocks, numBlocks * sizeof(CollectorBlock*)));
    }
    heap.blocks[heap.usedBlocks++] = block;

    return block;
}

template <HeapType heapType>
NEVER_INLINE void Heap::freeBlock(size_t block)
{
    CollectorHeap& heap = heapType == PrimaryHeap ? primaryHeap : numberHeap;

    freeBlock(heap.blocks[block]);

    // swap with the last block so we compact as we go
    heap.blocks[block] = heap.blocks[heap.usedBlocks - 1];
    heap.usedBlocks--;

    if (heap.numBlocks > MIN_ARRAY_SIZE && heap.usedBlocks < heap.numBlocks / LOW_WATER_FACTOR) {
        heap.numBlocks = heap.numBlocks / GROWTH_FACTOR; 
        heap.blocks = static_cast<CollectorBlock**>(fastRealloc(heap.blocks, heap.numBlocks * sizeof(CollectorBlock*)));
    }
}

NEVER_INLINE void Heap::freeBlock(CollectorBlock* block)
{
#if PLATFORM(DARWIN)    
    vm_deallocate(current_task(), reinterpret_cast<vm_address_t>(block), BLOCK_SIZE);
#elif PLATFORM(SYMBIAN)
    userChunk->Free(reinterpret_cast<TAny*>(block));
#elif PLATFORM(WINCE)
    VirtualFree(block, 0, MEM_RELEASE);
#elif PLATFORM(WIN_OS)
#if COMPILER(MINGW)
    __mingw_aligned_free(block);
#else
    _aligned_free(block);
#endif
#elif HAVE(POSIX_MEMALIGN)
    free(block);
#else
    munmap(reinterpret_cast<char*>(block), BLOCK_SIZE);
#endif
}

void Heap::freeBlocks(CollectorHeap* heap)
{
    for (size_t i = 0; i < heap->usedBlocks; ++i)
        if (heap->blocks[i])
            freeBlock(heap->blocks[i]);
    fastFree(heap->blocks);
    memset(heap, 0, sizeof(CollectorHeap));
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

template <HeapType heapType> ALWAYS_INLINE void* Heap::heapAllocate(size_t s)
{
    typedef typename HeapConstants<heapType>::Block Block;
    typedef typename HeapConstants<heapType>::Cell Cell;

    CollectorHeap& heap = heapType == PrimaryHeap ? primaryHeap : numberHeap;
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());
    ASSERT_UNUSED(s, s <= HeapConstants<heapType>::cellSize);

    ASSERT(heap.operationInProgress == NoOperation);
    ASSERT(heapType == PrimaryHeap || heap.extraCost == 0);
    // FIXME: If another global variable access here doesn't hurt performance
    // too much, we could CRASH() in NDEBUG builds, which could help ensure we
    // don't spend any time debugging cases where we allocate inside an object's
    // deallocation code.

#if COLLECT_ON_EVERY_ALLOCATION
    collect();
#endif

    size_t numLiveObjects = heap.numLiveObjects;
    size_t usedBlocks = heap.usedBlocks;
    size_t i = heap.firstBlockWithPossibleSpace;

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
        targetBlock = reinterpret_cast<Block*>(heap.blocks[i]);
        targetBlockUsedCells = targetBlock->usedCells;
        ASSERT(targetBlockUsedCells <= HeapConstants<heapType>::cellsPerBlock);
        while (targetBlockUsedCells == HeapConstants<heapType>::cellsPerBlock) {
            if (++i == usedBlocks)
                goto collect;
            targetBlock = reinterpret_cast<Block*>(heap.blocks[i]);
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
            bool foundGarbage = collect();
            numLiveObjects = heap.numLiveObjects;
            usedBlocks = heap.usedBlocks;
            i = heap.firstBlockWithPossibleSpace;
#ifndef NDEBUG
            heap.operationInProgress = Allocation;
#endif
            if (foundGarbage)
                goto scan;
        }

        // didn't find a block, and GC didn't reclaim anything, need to allocate a new block
        targetBlock = reinterpret_cast<Block*>(allocateBlock<heapType>());
        heap.firstBlockWithPossibleSpace = heap.usedBlocks - 1;
        targetBlockUsedCells = 0;
    }

    // find a free spot in the block and detach it from the free list
    Cell* newCell = targetBlock->freeList;

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

#if PLATFORM(WINCE)
void* g_stackBase = 0;

inline bool isPageWritable(void* page)
{
    MEMORY_BASIC_INFORMATION memoryInformation;
    DWORD result = VirtualQuery(page, &memoryInformation, sizeof(memoryInformation));

    // return false on error, including ptr outside memory
    if (result != sizeof(memoryInformation))
        return false;

    DWORD protect = memoryInformation.Protect & ~(PAGE_GUARD | PAGE_NOCACHE);
    return protect == PAGE_READWRITE
        || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

static void* getStackBase(void* previousFrame)
{
    // find the address of this stack frame by taking the address of a local variable
    bool isGrowingDownward;
    void* thisFrame = (void*)(&isGrowingDownward);

    isGrowingDownward = previousFrame < &thisFrame;
    static DWORD pageSize = 0;
    if (!pageSize) {
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        pageSize = systemInfo.dwPageSize;
    }

    // scan all of memory starting from this frame, and return the last writeable page found
    register char* currentPage = (char*)((DWORD)thisFrame & ~(pageSize - 1));
    if (isGrowingDownward) {
        while (currentPage > 0) {
            // check for underflow
            if (currentPage >= (char*)pageSize)
                currentPage -= pageSize;
            else
                currentPage = 0;
            if (!isPageWritable(currentPage))
                return currentPage + pageSize;
        }
        return 0;
    } else {
        while (true) {
            // guaranteed to complete because isPageWritable returns false at end of memory
            currentPage += pageSize;
            if (!isPageWritable(currentPage))
                return currentPage;
        }
    }
}
#endif

#if PLATFORM(QNX)
static inline void *currentThreadStackBaseQNX()
{
    static void* stackBase = 0;
    static size_t stackSize = 0;
    static pthread_t stackThread;
    pthread_t thread = pthread_self();
    if (stackBase == 0 || thread != stackThread) {
        struct _debug_thread_info threadInfo;
        memset(&threadInfo, 0, sizeof(threadInfo));
        threadInfo.tid = pthread_self();
        int fd = open("/proc/self", O_RDONLY);
        if (fd == -1) {
            LOG_ERROR("Unable to open /proc/self (errno: %d)", errno);
            return 0;
        }
        devctl(fd, DCMD_PROC_TIDSTATUS, &threadInfo, sizeof(threadInfo), 0);
        close(fd);
        stackBase = reinterpret_cast<void*>(threadInfo.stkbase);
        stackSize = threadInfo.stksize;
        ASSERT(stackBase);
        stackThread = thread;
    }
    return static_cast<char*>(stackBase) + stackSize;
}
#endif

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
    return static_cast<void*>(pTib->StackBase);
#elif PLATFORM(WIN_OS) && PLATFORM(X86_64) && COMPILER(MSVC)
    PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
    return reinterpret_cast<void*>(pTib->StackBase);
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(GCC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    asm ( "movl %%fs:0x18, %0\n"
          : "=r" (pTib)
        );
    return static_cast<void*>(pTib->StackBase);
#elif PLATFORM(QNX)
    return currentThreadStackBaseQNX();
#elif PLATFORM(SOLARIS)
    stack_t s;
    thr_stksegment(&s);
    return s.ss_sp;
#elif PLATFORM(OPENBSD)
    pthread_t thread = pthread_self();
    stack_t stack;
    pthread_stackseg_np(thread, &stack);
    return stack.ss_sp;
#elif PLATFORM(SYMBIAN)
    static void* stackBase = 0;
    if (stackBase == 0) {
        TThreadStackInfo info;
        RThread thread;
        thread.StackInfo(info);
        stackBase = (void*)info.iBase;
    }
    return (void*)stackBase;
#elif PLATFORM(HAIKU)
    thread_info threadInfo;
    get_thread_info(find_thread(NULL), &threadInfo);
    return threadInfo.stack_end;
#elif PLATFORM(UNIX)
    static void* stackBase = 0;
    static size_t stackSize = 0;
    static pthread_t stackThread;
    pthread_t thread = pthread_self();
    if (stackBase == 0 || thread != stackThread) {
        pthread_attr_t sattr;
        pthread_attr_init(&sattr);
#if HAVE(PTHREAD_NP_H) || PLATFORM(NETBSD)
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
#elif PLATFORM(WINCE)
    if (g_stackBase)
        return g_stackBase;
    else {
        int dummy;
        return getStackBase(&dummy);
    }
#else
#error Need a way to get the stack base on this platform
#endif
}

#if ENABLE(JSC_MULTIPLE_THREADS)

static inline PlatformThread getCurrentPlatformThread()
{
#if PLATFORM(DARWIN)
    return pthread_mach_thread_np(pthread_self());
#elif PLATFORM(WIN_OS)
    HANDLE threadHandle = pthread_getw32threadhandle_np(pthread_self());
    return PlatformThread(GetCurrentThreadId(), threadHandle);
#endif
}

void Heap::makeUsableFromMultipleThreads()
{
    if (m_currentThreadRegistrar)
        return;

    int error = pthread_key_create(&m_currentThreadRegistrar, unregisterThread);
    if (error)
        CRASH();
}

void Heap::registerThread()
{
    ASSERT(!m_globalData->mainThreadOnly || isMainThread());

    if (!m_currentThreadRegistrar || pthread_getspecific(m_currentThreadRegistrar))
        return;

    pthread_setspecific(m_currentThreadRegistrar, this);
    Heap::Thread* thread = new Heap::Thread(pthread_self(), getCurrentPlatformThread(), currentThreadStackBase());

    MutexLocker lock(m_registeredThreadsMutex);

    thread->next = m_registeredThreads;
    m_registeredThreads = thread;
}

void Heap::unregisterThread(void* p)
{
    if (p)
        static_cast<Heap*>(p)->unregisterThread();
}

void Heap::unregisterThread()
{
    pthread_t currentPosixThread = pthread_self();

    MutexLocker lock(m_registeredThreadsMutex);

    if (pthread_equal(currentPosixThread, m_registeredThreads->posixThread)) {
        Thread* t = m_registeredThreads;
        m_registeredThreads = m_registeredThreads->next;
        delete t;
    } else {
        Heap::Thread* last = m_registeredThreads;
        Heap::Thread* t;
        for (t = m_registeredThreads->next; t; t = t->next) {
            if (pthread_equal(t->posixThread, currentPosixThread)) {
                last->next = t->next;
                break;
            }
            last = t;
        }
        ASSERT(t); // If t is NULL, we never found ourselves in the list.
        delete t;
    }
}

#else // ENABLE(JSC_MULTIPLE_THREADS)

void Heap::registerThread()
{
}

#endif

#define IS_POINTER_ALIGNED(p) (((intptr_t)(p) & (sizeof(char*) - 1)) == 0)

// cell size needs to be a power of two for this to be valid
#define IS_HALF_CELL_ALIGNED(p) (((intptr_t)(p) & (CELL_MASK >> 1)) == 0)

void Heap::markConservatively(MarkStack& markStack, void* start, void* end)
{
    if (start > end) {
        void* tmp = start;
        start = end;
        end = tmp;
    }

    ASSERT((static_cast<char*>(end) - static_cast<char*>(start)) < 0x1000000);
    ASSERT(IS_POINTER_ALIGNED(start));
    ASSERT(IS_POINTER_ALIGNED(end));

    char** p = static_cast<char**>(start);
    char** e = static_cast<char**>(end);

    size_t usedPrimaryBlocks = primaryHeap.usedBlocks;
    size_t usedNumberBlocks = numberHeap.usedBlocks;
    CollectorBlock** primaryBlocks = primaryHeap.blocks;
    CollectorBlock** numberBlocks = numberHeap.blocks;

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
                    if (reinterpret_cast<CollectorCell*>(xAsBits)->u.freeCell.zeroIfFree) {
                        markStack.append(reinterpret_cast<JSCell*>(xAsBits));
                        markStack.drain();
                    }
                    break;
                }
            }
        endMarkLoop:
            ;
        }
    }
}

void NEVER_INLINE Heap::markCurrentThreadConservativelyInternal(MarkStack& markStack)
{
    void* dummy;
    void* stackPointer = &dummy;
    void* stackBase = currentThreadStackBase();
    markConservatively(markStack, stackPointer, stackBase);
}

#if COMPILER(GCC)
#define REGISTER_BUFFER_ALIGNMENT __attribute__ ((aligned (sizeof(void*))))
#else
#define REGISTER_BUFFER_ALIGNMENT
#endif

void Heap::markCurrentThreadConservatively(MarkStack& markStack)
{
    // setjmp forces volatile registers onto the stack
    jmp_buf registers REGISTER_BUFFER_ALIGNMENT;
#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4611)
#endif
    setjmp(registers);
#if COMPILER(MSVC)
#pragma warning(pop)
#endif

    markCurrentThreadConservativelyInternal(markStack);
}

#if ENABLE(JSC_MULTIPLE_THREADS)

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

#if PLATFORM(X86)
typedef i386_thread_state_t PlatformThreadRegisters;
#elif PLATFORM(X86_64)
typedef x86_thread_state64_t PlatformThreadRegisters;
#elif PLATFORM(PPC)
typedef ppc_thread_state_t PlatformThreadRegisters;
#elif PLATFORM(PPC64)
typedef ppc_thread_state64_t PlatformThreadRegisters;
#elif PLATFORM(ARM)
typedef arm_thread_state_t PlatformThreadRegisters;
#else
#error Unknown Architecture
#endif

#elif PLATFORM(WIN_OS)&& PLATFORM(X86)
typedef CONTEXT PlatformThreadRegisters;
#else
#error Need a thread register struct for this platform
#endif

static size_t getPlatformThreadRegisters(const PlatformThread& platformThread, PlatformThreadRegisters& regs)
{
#if PLATFORM(DARWIN)

#if PLATFORM(X86)
    unsigned user_count = sizeof(regs)/sizeof(int);
    thread_state_flavor_t flavor = i386_THREAD_STATE;
#elif PLATFORM(X86_64)
    unsigned user_count = x86_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
#elif PLATFORM(PPC) 
    unsigned user_count = PPC_THREAD_STATE_COUNT;
    thread_state_flavor_t flavor = PPC_THREAD_STATE;
#elif PLATFORM(PPC64)
    unsigned user_count = PPC_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = PPC_THREAD_STATE64;
#elif PLATFORM(ARM)
    unsigned user_count = ARM_THREAD_STATE_COUNT;
    thread_state_flavor_t flavor = ARM_THREAD_STATE;
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
    return reinterpret_cast<void*>(regs.__esp);
#elif PLATFORM(X86_64)
    return reinterpret_cast<void*>(regs.__rsp);
#elif PLATFORM(PPC) || PLATFORM(PPC64)
    return reinterpret_cast<void*>(regs.__r1);
#elif PLATFORM(ARM)
    return reinterpret_cast<void*>(regs.__sp);
#else
#error Unknown Architecture
#endif

#else // !__DARWIN_UNIX03

#if PLATFORM(X86)
    return reinterpret_cast<void*>(regs.esp);
#elif PLATFORM(X86_64)
    return reinterpret_cast<void*>(regs.rsp);
#elif (PLATFORM(PPC) || PLATFORM(PPC64))
    return reinterpret_cast<void*>(regs.r1);
#else
#error Unknown Architecture
#endif

#endif // __DARWIN_UNIX03

// end PLATFORM(DARWIN)
#elif PLATFORM(X86) && PLATFORM(WIN_OS)
    return reinterpret_cast<void*>((uintptr_t) regs.Esp);
#else
#error Need a way to get the stack pointer for another thread on this platform
#endif
}

void Heap::markOtherThreadConservatively(MarkStack& markStack, Thread* thread)
{
    suspendThread(thread->platformThread);

    PlatformThreadRegisters regs;
    size_t regSize = getPlatformThreadRegisters(thread->platformThread, regs);

    // mark the thread's registers
    markConservatively(markStack, static_cast<void*>(&regs), static_cast<void*>(reinterpret_cast<char*>(&regs) + regSize));

    void* stackPointer = otherThreadStackPointer(regs);
    markConservatively(markStack, stackPointer, thread->stackBase);

    resumeThread(thread->platformThread);
}

#endif

void Heap::markStackObjectsConservatively(MarkStack& markStack)
{
    markCurrentThreadConservatively(markStack);

#if ENABLE(JSC_MULTIPLE_THREADS)

    if (m_currentThreadRegistrar) {

        MutexLocker lock(m_registeredThreadsMutex);

#ifndef NDEBUG
        // Forbid malloc during the mark phase. Marking a thread suspends it, so 
        // a malloc inside markChildren() would risk a deadlock with a thread that had been 
        // suspended while holding the malloc lock.
        fastMallocForbid();
#endif
        // It is safe to access the registeredThreads list, because we earlier asserted that locks are being held,
        // and since this is a shared heap, they are real locks.
        for (Thread* thread = m_registeredThreads; thread; thread = thread->next) {
            if (!pthread_equal(thread->posixThread, pthread_self()))
                markOtherThreadConservatively(markStack, thread);
        }
#ifndef NDEBUG
        fastMallocAllow();
#endif
    }
#endif
}

void Heap::setGCProtectNeedsLocking()
{
    // Most clients do not need to call this, with the notable exception of WebCore.
    // Clients that use shared heap have JSLock protection, while others are supposed
    // to do explicit locking. WebCore violates this contract in Database code,
    // which calls gcUnprotect from a secondary thread.
    if (!m_protectedValuesMutex)
        m_protectedValuesMutex.set(new Mutex);
}

void Heap::protect(JSValue k)
{
    ASSERT(k);
    ASSERT(JSLock::currentThreadIsHoldingLock() || !m_globalData->isSharedInstance);

    if (!k.isCell())
        return;

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->lock();

    m_protectedValues.add(k.asCell());

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->unlock();
}

void Heap::unprotect(JSValue k)
{
    ASSERT(k);
    ASSERT(JSLock::currentThreadIsHoldingLock() || !m_globalData->isSharedInstance);

    if (!k.isCell())
        return;

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->lock();

    m_protectedValues.remove(k.asCell());

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->unlock();
}

void Heap::markProtectedObjects(MarkStack& markStack)
{
    if (m_protectedValuesMutex)
        m_protectedValuesMutex->lock();

    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it) {
        markStack.append(it->first);
        markStack.drain();
    }

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->unlock();
}

template <HeapType heapType> size_t Heap::sweep()
{
    typedef typename HeapConstants<heapType>::Block Block;
    typedef typename HeapConstants<heapType>::Cell Cell;

    // SWEEP: delete everything with a zero refcount (garbage) and unmark everything else
    CollectorHeap& heap = heapType == PrimaryHeap ? primaryHeap : numberHeap;
    
    size_t emptyBlocks = 0;
    size_t numLiveObjects = heap.numLiveObjects;
    
    for (size_t block = 0; block < heap.usedBlocks; block++) {
        Block* curBlock = reinterpret_cast<Block*>(heap.blocks[block]);
        
        size_t usedCells = curBlock->usedCells;
        Cell* freeList = curBlock->freeList;
        
        if (usedCells == HeapConstants<heapType>::cellsPerBlock) {
            // special case with a block where all cells are used -- testing indicates this happens often
            for (size_t i = 0; i < HeapConstants<heapType>::cellsPerBlock; i++) {
                if (!curBlock->marked.get(i >> HeapConstants<heapType>::bitmapShift)) {
                    Cell* cell = curBlock->cells + i;
                    
                    if (heapType != NumberHeap) {
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
                Cell* cell = curBlock->cells + i;
                if (cell->u.freeCell.zeroIfFree == 0) {
                    ++minimumCellsToProcess;
                } else {
                    if (!curBlock->marked.get(i >> HeapConstants<heapType>::bitmapShift)) {
                        if (heapType != NumberHeap) {
                            JSCell* imp = reinterpret_cast<JSCell*>(cell);
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
        
        if (!usedCells)
            ++emptyBlocks;
    }
    
    if (heap.numLiveObjects != numLiveObjects)
        heap.firstBlockWithPossibleSpace = 0;
    
    heap.numLiveObjects = numLiveObjects;
    heap.numLiveObjectsAtLastCollect = numLiveObjects;
    heap.extraCost = 0;
    
    if (!emptyBlocks)
        return numLiveObjects;

    size_t neededCells = 1.25f * (numLiveObjects + max(ALLOCATIONS_PER_COLLECTION, numLiveObjects));
    size_t neededBlocks = (neededCells + HeapConstants<heapType>::cellsPerBlock - 1) / HeapConstants<heapType>::cellsPerBlock;
    for (size_t block = 0; block < heap.usedBlocks; block++) {
        if (heap.usedBlocks <= neededBlocks)
            break;

        Block* curBlock = reinterpret_cast<Block*>(heap.blocks[block]);
        if (curBlock->usedCells)
            continue;

        freeBlock<heapType>(block);
        block--; // Don't move forward a step in this case
    }

    return numLiveObjects;
}

bool Heap::collect()
{
#ifndef NDEBUG
    if (m_globalData->isSharedInstance) {
        ASSERT(JSLock::lockCount() > 0);
        ASSERT(JSLock::currentThreadIsHoldingLock());
    }
#endif

    ASSERT((primaryHeap.operationInProgress == NoOperation) | (numberHeap.operationInProgress == NoOperation));
    if ((primaryHeap.operationInProgress != NoOperation) | (numberHeap.operationInProgress != NoOperation))
        CRASH();

    JAVASCRIPTCORE_GC_BEGIN();
    primaryHeap.operationInProgress = Collection;
    numberHeap.operationInProgress = Collection;

    // MARK: first mark all referenced objects recursively starting out from the set of root objects
    MarkStack& markStack = m_globalData->markStack;
    markStackObjectsConservatively(markStack);
    markProtectedObjects(markStack);
    if (m_markListSet && m_markListSet->size())
        MarkedArgumentBuffer::markLists(markStack, *m_markListSet);
    if (m_globalData->exception)
        markStack.append(m_globalData->exception);
    m_globalData->interpreter->registerFile().markCallFrames(markStack, this);
    m_globalData->smallStrings.markChildren(markStack);
    if (m_globalData->functionCodeBlockBeingReparsed)
        m_globalData->functionCodeBlockBeingReparsed->markAggregate(markStack);
    if (m_globalData->firstStringifierToMark)
        JSONObject::markStringifiers(markStack, m_globalData->firstStringifierToMark);

    markStack.drain();
    markStack.compact();
    JAVASCRIPTCORE_GC_MARKED();

    size_t originalLiveObjects = primaryHeap.numLiveObjects + numberHeap.numLiveObjects;
    size_t numLiveObjects = sweep<PrimaryHeap>();
    numLiveObjects += sweep<NumberHeap>();

    primaryHeap.operationInProgress = NoOperation;
    numberHeap.operationInProgress = NoOperation;
    JAVASCRIPTCORE_GC_END(originalLiveObjects, numLiveObjects);

    return numLiveObjects < originalLiveObjects;
}

size_t Heap::objectCount() 
{
    return primaryHeap.numLiveObjects + numberHeap.numLiveObjects - m_globalData->smallStrings.count(); 
}

template <HeapType heapType> 
static void addToStatistics(Heap::Statistics& statistics, const CollectorHeap& heap)
{
    typedef HeapConstants<heapType> HC;
    for (size_t i = 0; i < heap.usedBlocks; ++i) {
        if (heap.blocks[i]) {
            statistics.size += BLOCK_SIZE;
            statistics.free += (HC::cellsPerBlock - heap.blocks[i]->usedCells) * HC::cellSize;
        }
    }
}

Heap::Statistics Heap::statistics() const
{
    Statistics statistics = { 0, 0 };
    JSC::addToStatistics<PrimaryHeap>(statistics, primaryHeap);
    JSC::addToStatistics<NumberHeap>(statistics, numberHeap);
    return statistics;
}

size_t Heap::globalObjectCount()
{
    size_t count = 0;
    if (JSGlobalObject* head = m_globalData->head) {
        JSGlobalObject* o = head;
        do {
            ++count;
            o = o->next();
        } while (o != head);
    }
    return count;
}

size_t Heap::protectedGlobalObjectCount()
{
    if (m_protectedValuesMutex)
        m_protectedValuesMutex->lock();

    size_t count = 0;
    if (JSGlobalObject* head = m_globalData->head) {
        JSGlobalObject* o = head;
        do {
            if (m_protectedValues.contains(o))
                ++count;
            o = o->next();
        } while (o != head);
    }

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->unlock();

    return count;
}

size_t Heap::protectedObjectCount()
{
    if (m_protectedValuesMutex)
        m_protectedValuesMutex->lock();

    size_t result = m_protectedValues.size();

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->unlock();

    return result;
}

static const char* typeName(JSCell* cell)
{
    if (cell->isString())
        return "string";
#if USE(JSVALUE32)
    if (cell->isNumber())
        return "number";
#endif
    if (cell->isGetterSetter())
        return "gettersetter";
    ASSERT(cell->isObject());
    const ClassInfo* info = cell->classInfo();
    return info ? info->className : "Object";
}

HashCountedSet<const char*>* Heap::protectedObjectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->lock();

    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
        counts->add(typeName(it->first));

    if (m_protectedValuesMutex)
        m_protectedValuesMutex->unlock();

    return counts;
}

bool Heap::isBusy()
{
    return (primaryHeap.operationInProgress != NoOperation) | (numberHeap.operationInProgress != NoOperation);
}

Heap::iterator Heap::primaryHeapBegin()
{
    return iterator(primaryHeap.blocks, primaryHeap.blocks + primaryHeap.usedBlocks);
}

Heap::iterator Heap::primaryHeapEnd()
{
    return iterator(primaryHeap.blocks + primaryHeap.usedBlocks, primaryHeap.blocks + primaryHeap.usedBlocks);
}

} // namespace JSC
