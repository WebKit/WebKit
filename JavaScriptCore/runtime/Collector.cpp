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
#include "JSZombie.h"
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

#if OS(DARWIN)

#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#elif OS(SYMBIAN)
#include <e32std.h>
#include <e32cmn.h>
#include <unistd.h>

#elif OS(WINDOWS)

#include <windows.h>
#include <malloc.h>

#elif OS(HAIKU)

#include <OS.h>

#elif OS(UNIX)

#include <stdlib.h>
#if !OS(HAIKU)
#include <sys/mman.h>
#endif
#include <unistd.h>

#if OS(SOLARIS)
#include <thread.h>
#else
#include <pthread.h>
#endif

#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#if OS(QNX)
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
const size_t ALLOCATIONS_PER_COLLECTION = 3600;
// This value has to be a macro to be used in max() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define MIN_ARRAY_SIZE (static_cast<size_t>(14))

#if OS(SYMBIAN)
const size_t MAX_NUM_BLOCKS = 256; // Max size of collector heap set to 16 MB
static RHeap* userChunk = 0;
#endif

#if ENABLE(JSC_MULTIPLE_THREADS)

#if OS(DARWIN)
typedef mach_port_t PlatformThread;
#elif OS(WINDOWS)
typedef HANDLE PlatformThread;
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
    
#if OS(SYMBIAN)
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
#endif // OS(SYMBIAN)
    
    memset(&m_heap, 0, sizeof(CollectorHeap));
    allocateBlock();
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

    ASSERT(!m_globalData->dynamicGlobalObject);
    ASSERT(!isBusy());
    
    // The global object is not GC protected at this point, so sweeping may delete it
    // (and thus the global data) before other objects that may use the global data.
    RefPtr<JSGlobalData> protect(m_globalData);

    delete m_markListSet;
    m_markListSet = 0;

    freeBlocks();

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

NEVER_INLINE CollectorBlock* Heap::allocateBlock()
{
#if OS(DARWIN)
    vm_address_t address = 0;
    vm_map(current_task(), &address, BLOCK_SIZE, BLOCK_OFFSET_MASK, VM_FLAGS_ANYWHERE | VM_TAG_FOR_COLLECTOR_MEMORY, MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
#elif OS(SYMBIAN)
    // Allocate a 64 kb aligned CollectorBlock
    unsigned char* mask = reinterpret_cast<unsigned char*>(userChunk->Alloc(BLOCK_SIZE));
    if (!mask)
        CRASH();
    uintptr_t address = reinterpret_cast<uintptr_t>(mask);
#elif OS(WINCE)
    void* address = VirtualAlloc(NULL, BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif OS(WINDOWS)
#if COMPILER(MINGW)
    void* address = __mingw_aligned_malloc(BLOCK_SIZE, BLOCK_SIZE);
#else
    void* address = _aligned_malloc(BLOCK_SIZE, BLOCK_SIZE);
#endif
    memset(address, 0, BLOCK_SIZE);
#elif HAVE(POSIX_MEMALIGN)
    void* address;
    posix_memalign(&address, BLOCK_SIZE, BLOCK_SIZE);
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
#endif

    // Initialize block.

    CollectorBlock* block = reinterpret_cast<CollectorBlock*>(address);
    block->heap = this;
    clearMarkBits(block);

    Structure* dummyMarkableCellStructure = m_globalData->dummyMarkableCellStructure.get();
    for (size_t i = 0; i < HeapConstants::cellsPerBlock; ++i)
        new (block->cells + i) JSCell(dummyMarkableCellStructure);
    
    // Add block to blocks vector.

    size_t numBlocks = m_heap.numBlocks;
    if (m_heap.usedBlocks == numBlocks) {
        static const size_t maxNumBlocks = ULONG_MAX / sizeof(CollectorBlock*) / GROWTH_FACTOR;
        if (numBlocks > maxNumBlocks)
            CRASH();
        numBlocks = max(MIN_ARRAY_SIZE, numBlocks * GROWTH_FACTOR);
        m_heap.numBlocks = numBlocks;
        m_heap.blocks = static_cast<CollectorBlock**>(fastRealloc(m_heap.blocks, numBlocks * sizeof(CollectorBlock*)));
    }
    m_heap.blocks[m_heap.usedBlocks++] = block;

    return block;
}

NEVER_INLINE void Heap::freeBlock(size_t block)
{
    m_heap.didShrink = true;

    ObjectIterator it(m_heap, block);
    ObjectIterator end(m_heap, block + 1);
    for ( ; it != end; ++it)
        (*it)->~JSCell();
    freeBlockPtr(m_heap.blocks[block]);

    // swap with the last block so we compact as we go
    m_heap.blocks[block] = m_heap.blocks[m_heap.usedBlocks - 1];
    m_heap.usedBlocks--;

    if (m_heap.numBlocks > MIN_ARRAY_SIZE && m_heap.usedBlocks < m_heap.numBlocks / LOW_WATER_FACTOR) {
        m_heap.numBlocks = m_heap.numBlocks / GROWTH_FACTOR; 
        m_heap.blocks = static_cast<CollectorBlock**>(fastRealloc(m_heap.blocks, m_heap.numBlocks * sizeof(CollectorBlock*)));
    }
}

NEVER_INLINE void Heap::freeBlockPtr(CollectorBlock* block)
{
#if OS(DARWIN)    
    vm_deallocate(current_task(), reinterpret_cast<vm_address_t>(block), BLOCK_SIZE);
#elif OS(SYMBIAN)
    userChunk->Free(reinterpret_cast<TAny*>(block));
#elif OS(WINCE)
    VirtualFree(block, 0, MEM_RELEASE);
#elif OS(WINDOWS)
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

void Heap::freeBlocks()
{
    ProtectCountSet protectedValuesCopy = m_protectedValues;

    clearMarkBits();
    ProtectCountSet::iterator protectedValuesEnd = protectedValuesCopy.end();
    for (ProtectCountSet::iterator it = protectedValuesCopy.begin(); it != protectedValuesEnd; ++it)
        markCell(it->first);

    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
    DeadObjectIterator it(m_heap, m_heap.nextBlock, m_heap.nextCell);
    DeadObjectIterator end(m_heap, m_heap.usedBlocks);
    for ( ; it != end; ++it)
        (*it)->~JSCell();

    ASSERT(!protectedObjectCount());

    protectedValuesEnd = protectedValuesCopy.end();
    for (ProtectCountSet::iterator it = protectedValuesCopy.begin(); it != protectedValuesEnd; ++it)
        it->first->~JSCell();

    for (size_t block = 0; block < m_heap.usedBlocks; ++block)
        freeBlockPtr(m_heap.blocks[block]);

    fastFree(m_heap.blocks);

    memset(&m_heap, 0, sizeof(CollectorHeap));
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

    if (m_heap.extraCost > maxExtraCost && m_heap.extraCost > m_heap.usedBlocks * BLOCK_SIZE / 2) {
        // If the last iteration through the heap deallocated blocks, we need
        // to clean up remaining garbage before marking. Otherwise, the conservative
        // marking mechanism might follow a pointer to unmapped memory.
        if (m_heap.didShrink)
            sweep();
        reset();
    }
    m_heap.extraCost += cost;
}

void* Heap::allocate(size_t s)
{
    typedef HeapConstants::Block Block;
    typedef HeapConstants::Cell Cell;
    
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());
    ASSERT_UNUSED(s, s <= HeapConstants::cellSize);

    ASSERT(m_heap.operationInProgress == NoOperation);

#if COLLECT_ON_EVERY_ALLOCATION
    collectAllGarbage();
    ASSERT(m_heap.operationInProgress == NoOperation);
#endif

allocate:

    // Fast case: find the next garbage cell and recycle it.

    do {
        ASSERT(m_heap.nextBlock < m_heap.usedBlocks);
        Block* block = reinterpret_cast<Block*>(m_heap.blocks[m_heap.nextBlock]);
        do {
            ASSERT(m_heap.nextCell < HeapConstants::cellsPerBlock);
            if (!block->marked.get(m_heap.nextCell)) { // Always false for the last cell in the block
                Cell* cell = block->cells + m_heap.nextCell;

                m_heap.operationInProgress = Allocation;
                JSCell* imp = reinterpret_cast<JSCell*>(cell);
                imp->~JSCell();
                m_heap.operationInProgress = NoOperation;

                ++m_heap.nextCell;
                return cell;
            }
        } while (++m_heap.nextCell != HeapConstants::cellsPerBlock);
        m_heap.nextCell = 0;
    } while (++m_heap.nextBlock != m_heap.usedBlocks);

    // Slow case: reached the end of the heap. Mark live objects and start over.

    reset();
    goto allocate;
}

void Heap::resizeBlocks()
{
    m_heap.didShrink = false;

    size_t usedCellCount = markedCells();
    size_t minCellCount = usedCellCount + max(ALLOCATIONS_PER_COLLECTION, usedCellCount);
    size_t minBlockCount = (minCellCount + HeapConstants::cellsPerBlock - 1) / HeapConstants::cellsPerBlock;

    size_t maxCellCount = 1.25f * minCellCount;
    size_t maxBlockCount = (maxCellCount + HeapConstants::cellsPerBlock - 1) / HeapConstants::cellsPerBlock;

    if (m_heap.usedBlocks < minBlockCount)
        growBlocks(minBlockCount);
    else if (m_heap.usedBlocks > maxBlockCount)
        shrinkBlocks(maxBlockCount);
}

void Heap::growBlocks(size_t neededBlocks)
{
    ASSERT(m_heap.usedBlocks < neededBlocks);
    while (m_heap.usedBlocks < neededBlocks)
        allocateBlock();
}

void Heap::shrinkBlocks(size_t neededBlocks)
{
    ASSERT(m_heap.usedBlocks > neededBlocks);
    
    // Clear the always-on last bit, so isEmpty() isn't fooled by it.
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        m_heap.blocks[i]->marked.clear(HeapConstants::cellsPerBlock - 1);

    for (size_t i = 0; i != m_heap.usedBlocks && m_heap.usedBlocks != neededBlocks; ) {
        if (m_heap.blocks[i]->marked.isEmpty()) {
            freeBlock(i);
        } else
            ++i;
    }

    // Reset the always-on last bit.
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        m_heap.blocks[i]->marked.set(HeapConstants::cellsPerBlock - 1);
}

#if OS(WINCE)
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

#if OS(QNX)
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
#if OS(DARWIN)
    pthread_t thread = pthread_self();
    return pthread_get_stackaddr_np(thread);
#elif OS(WINDOWS) && CPU(X86) && COMPILER(MSVC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    __asm {
        MOV EAX, FS:[18h]
        MOV pTib, EAX
    }
    return static_cast<void*>(pTib->StackBase);
#elif OS(WINDOWS) && CPU(X86_64) && COMPILER(MSVC)
    // FIXME: why only for MSVC?
    PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
    return reinterpret_cast<void*>(pTib->StackBase);
#elif OS(WINDOWS) && CPU(X86) && COMPILER(GCC)
    // offset 0x18 from the FS segment register gives a pointer to
    // the thread information block for the current thread
    NT_TIB* pTib;
    asm ( "movl %%fs:0x18, %0\n"
          : "=r" (pTib)
        );
    return static_cast<void*>(pTib->StackBase);
#elif OS(QNX)
    return currentThreadStackBaseQNX();
#elif OS(SOLARIS)
    stack_t s;
    thr_stksegment(&s);
    return s.ss_sp;
#elif OS(OPENBSD)
    pthread_t thread = pthread_self();
    stack_t stack;
    pthread_stackseg_np(thread, &stack);
    return stack.ss_sp;
#elif OS(SYMBIAN)
    static void* stackBase = 0;
    if (stackBase == 0) {
        TThreadStackInfo info;
        RThread thread;
        thread.StackInfo(info);
        stackBase = (void*)info.iBase;
    }
    return (void*)stackBase;
#elif OS(HAIKU)
    thread_info threadInfo;
    get_thread_info(find_thread(NULL), &threadInfo);
    return threadInfo.stack_end;
#elif OS(UNIX)
    static void* stackBase = 0;
    static size_t stackSize = 0;
    static pthread_t stackThread;
    pthread_t thread = pthread_self();
    if (stackBase == 0 || thread != stackThread) {
        pthread_attr_t sattr;
        pthread_attr_init(&sattr);
#if HAVE(PTHREAD_NP_H) || OS(NETBSD)
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
#elif OS(WINCE)
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
#if OS(DARWIN)
    return pthread_mach_thread_np(pthread_self());
#elif OS(WINDOWS)
    return pthread_getw32threadhandle_np(pthread_self());
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

inline bool isPointerAligned(void* p)
{
    return (((intptr_t)(p) & (sizeof(char*) - 1)) == 0);
}

// Cell size needs to be a power of two for isPossibleCell to be valid.
COMPILE_ASSERT(sizeof(CollectorCell) % 2 == 0, Collector_cell_size_is_power_of_two);

#if USE(JSVALUE32)
static bool isHalfCellAligned(void *p)
{
    return (((intptr_t)(p) & (CELL_MASK >> 1)) == 0);
}

static inline bool isPossibleCell(void* p)
{
    return isHalfCellAligned(p) && p;
}

#else

static inline bool isCellAligned(void *p)
{
    return (((intptr_t)(p) & CELL_MASK) == 0);
}

static inline bool isPossibleCell(void* p)
{
    return isCellAligned(p) && p;
}
#endif // USE(JSVALUE32)

void Heap::markConservatively(MarkStack& markStack, void* start, void* end)
{
    if (start > end) {
        void* tmp = start;
        start = end;
        end = tmp;
    }

    ASSERT((static_cast<char*>(end) - static_cast<char*>(start)) < 0x1000000);
    ASSERT(isPointerAligned(start));
    ASSERT(isPointerAligned(end));

    char** p = static_cast<char**>(start);
    char** e = static_cast<char**>(end);

    CollectorBlock** blocks = m_heap.blocks;
    while (p != e) {
        char* x = *p++;
        if (isPossibleCell(x)) {
            size_t usedBlocks;
            uintptr_t xAsBits = reinterpret_cast<uintptr_t>(x);
            xAsBits &= CELL_ALIGN_MASK;

            uintptr_t offset = xAsBits & BLOCK_OFFSET_MASK;
            const size_t lastCellOffset = sizeof(CollectorCell) * (CELLS_PER_BLOCK - 1);
            if (offset > lastCellOffset)
                continue;

            CollectorBlock* blockAddr = reinterpret_cast<CollectorBlock*>(xAsBits - offset);
            usedBlocks = m_heap.usedBlocks;
            for (size_t block = 0; block < usedBlocks; block++) {
                if (blocks[block] != blockAddr)
                    continue;
                markStack.append(reinterpret_cast<JSCell*>(xAsBits));
                markStack.drain();
            }
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
#if OS(DARWIN)
    thread_suspend(platformThread);
#elif OS(WINDOWS)
    SuspendThread(platformThread);
#else
#error Need a way to suspend threads on this platform
#endif
}

static inline void resumeThread(const PlatformThread& platformThread)
{
#if OS(DARWIN)
    thread_resume(platformThread);
#elif OS(WINDOWS)
    ResumeThread(platformThread);
#else
#error Need a way to resume threads on this platform
#endif
}

typedef unsigned long usword_t; // word size, assumed to be either 32 or 64 bit

#if OS(DARWIN)

#if CPU(X86)
typedef i386_thread_state_t PlatformThreadRegisters;
#elif CPU(X86_64)
typedef x86_thread_state64_t PlatformThreadRegisters;
#elif CPU(PPC)
typedef ppc_thread_state_t PlatformThreadRegisters;
#elif CPU(PPC64)
typedef ppc_thread_state64_t PlatformThreadRegisters;
#elif CPU(ARM)
typedef arm_thread_state_t PlatformThreadRegisters;
#else
#error Unknown Architecture
#endif

#elif OS(WINDOWS) && CPU(X86)
typedef CONTEXT PlatformThreadRegisters;
#else
#error Need a thread register struct for this platform
#endif

static size_t getPlatformThreadRegisters(const PlatformThread& platformThread, PlatformThreadRegisters& regs)
{
#if OS(DARWIN)

#if CPU(X86)
    unsigned user_count = sizeof(regs)/sizeof(int);
    thread_state_flavor_t flavor = i386_THREAD_STATE;
#elif CPU(X86_64)
    unsigned user_count = x86_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
#elif CPU(PPC) 
    unsigned user_count = PPC_THREAD_STATE_COUNT;
    thread_state_flavor_t flavor = PPC_THREAD_STATE;
#elif CPU(PPC64)
    unsigned user_count = PPC_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = PPC_THREAD_STATE64;
#elif CPU(ARM)
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
// end OS(DARWIN)

#elif OS(WINDOWS) && CPU(X86)
    regs.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_SEGMENTS;
    GetThreadContext(platformThread, &regs);
    return sizeof(CONTEXT);
#else
#error Need a way to get thread registers on this platform
#endif
}

static inline void* otherThreadStackPointer(const PlatformThreadRegisters& regs)
{
#if OS(DARWIN)

#if __DARWIN_UNIX03

#if CPU(X86)
    return reinterpret_cast<void*>(regs.__esp);
#elif CPU(X86_64)
    return reinterpret_cast<void*>(regs.__rsp);
#elif CPU(PPC) || CPU(PPC64)
    return reinterpret_cast<void*>(regs.__r1);
#elif CPU(ARM)
    return reinterpret_cast<void*>(regs.__sp);
#else
#error Unknown Architecture
#endif

#else // !__DARWIN_UNIX03

#if CPU(X86)
    return reinterpret_cast<void*>(regs.esp);
#elif CPU(X86_64)
    return reinterpret_cast<void*>(regs.rsp);
#elif CPU(PPC) || CPU(PPC64)
    return reinterpret_cast<void*>(regs.r1);
#else
#error Unknown Architecture
#endif

#endif // __DARWIN_UNIX03

// end OS(DARWIN)
#elif CPU(X86) && OS(WINDOWS)
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

void Heap::protect(JSValue k)
{
    ASSERT(k);
    ASSERT(JSLock::currentThreadIsHoldingLock() || !m_globalData->isSharedInstance);

    if (!k.isCell())
        return;

    m_protectedValues.add(k.asCell());
}

void Heap::unprotect(JSValue k)
{
    ASSERT(k);
    ASSERT(JSLock::currentThreadIsHoldingLock() || !m_globalData->isSharedInstance);

    if (!k.isCell())
        return;

    m_protectedValues.remove(k.asCell());
}

void Heap::markProtectedObjects(MarkStack& markStack)
{
    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it) {
        markStack.append(it->first);
        markStack.drain();
    }
}

void Heap::clearMarkBits()
{
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        clearMarkBits(m_heap.blocks[i]);
}

void Heap::clearMarkBits(CollectorBlock* block)
{
    // allocate assumes that the last cell in every block is marked.
    block->marked.clearAll();
    block->marked.set(HeapConstants::cellsPerBlock - 1);
}

size_t Heap::markedCells(size_t startBlock, size_t startCell) const
{
    ASSERT(startBlock <= m_heap.usedBlocks);
    ASSERT(startCell < HeapConstants::cellsPerBlock);

    if (startBlock >= m_heap.usedBlocks)
        return 0;

    size_t result = 0;
    result += m_heap.blocks[startBlock]->marked.count(startCell);
    for (size_t i = startBlock + 1; i < m_heap.usedBlocks; ++i)
        result += m_heap.blocks[i]->marked.count();

    return result;
}

void Heap::sweep()
{
    ASSERT(m_heap.operationInProgress == NoOperation);
    if (m_heap.operationInProgress != NoOperation)
        CRASH();
    m_heap.operationInProgress = Collection;
    
#if !ENABLE(JSC_ZOMBIES)
    Structure* dummyMarkableCellStructure = m_globalData->dummyMarkableCellStructure.get();
#endif

    DeadObjectIterator it(m_heap, m_heap.nextBlock, m_heap.nextCell);
    DeadObjectIterator end(m_heap, m_heap.usedBlocks);
    for ( ; it != end; ++it) {
        JSCell* cell = *it;
#if ENABLE(JSC_ZOMBIES)
        if (!cell->isZombie()) {
            const ClassInfo* info = cell->classInfo();
            cell->~JSCell();
            new (cell) JSZombie(info, JSZombie::leakedZombieStructure());
            Heap::markCell(cell);
        }
#else
        cell->~JSCell();
        // Callers of sweep assume it's safe to mark any cell in the heap.
        new (cell) JSCell(dummyMarkableCellStructure);
#endif
    }

    m_heap.operationInProgress = NoOperation;
}

void Heap::markRoots()
{
#ifndef NDEBUG
    if (m_globalData->isSharedInstance) {
        ASSERT(JSLock::lockCount() > 0);
        ASSERT(JSLock::currentThreadIsHoldingLock());
    }
#endif

    ASSERT(m_heap.operationInProgress == NoOperation);
    if (m_heap.operationInProgress != NoOperation)
        CRASH();

    m_heap.operationInProgress = Collection;

    MarkStack& markStack = m_globalData->markStack;

    // Reset mark bits.
    clearMarkBits();

    // Mark stack roots.
    markStackObjectsConservatively(markStack);
    m_globalData->interpreter->registerFile().markCallFrames(markStack, this);

    // Mark explicitly registered roots.
    markProtectedObjects(markStack);

    // Mark misc. other roots.
    if (m_markListSet && m_markListSet->size())
        MarkedArgumentBuffer::markLists(markStack, *m_markListSet);
    if (m_globalData->exception)
        markStack.append(m_globalData->exception);
    if (m_globalData->functionCodeBlockBeingReparsed)
        m_globalData->functionCodeBlockBeingReparsed->markAggregate(markStack);
    if (m_globalData->firstStringifierToMark)
        JSONObject::markStringifiers(markStack, m_globalData->firstStringifierToMark);

    // Mark the small strings cache last, since it will clear itself if nothing
    // else has marked it.
    m_globalData->smallStrings.markChildren(markStack);

    markStack.drain();
    markStack.compact();

    m_heap.operationInProgress = NoOperation;
}

size_t Heap::objectCount() const
{
    return m_heap.nextBlock * HeapConstants::cellsPerBlock // allocated full blocks
           + m_heap.nextCell // allocated cells in current block
           + markedCells(m_heap.nextBlock, m_heap.nextCell) // marked cells in remainder of m_heap
           - m_heap.usedBlocks; // 1 cell per block is a dummy sentinel
}

void Heap::addToStatistics(Heap::Statistics& statistics) const
{
    statistics.size += m_heap.usedBlocks * BLOCK_SIZE;
    statistics.free += m_heap.usedBlocks * BLOCK_SIZE - (objectCount() * HeapConstants::cellSize);
}

Heap::Statistics Heap::statistics() const
{
    Statistics statistics = { 0, 0 };
    addToStatistics(statistics);
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
    size_t count = 0;
    if (JSGlobalObject* head = m_globalData->head) {
        JSGlobalObject* o = head;
        do {
            if (m_protectedValues.contains(o))
                ++count;
            o = o->next();
        } while (o != head);
    }

    return count;
}

size_t Heap::protectedObjectCount()
{
    return m_protectedValues.size();
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
        return "Getter-Setter";
    if (cell->isAPIValueWrapper())
        return "API wrapper";
    if (cell->isPropertyNameIterator())
        return "For-in iterator";
    if (!cell->isObject())
        return "[empty cell]";
    const ClassInfo* info = cell->classInfo();
    return info ? info->className : "Object";
}

HashCountedSet<const char*>* Heap::protectedObjectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
        counts->add(typeName(it->first));

    return counts;
}

HashCountedSet<const char*>* Heap::objectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    LiveObjectIterator it = primaryHeapBegin();
    LiveObjectIterator heapEnd = primaryHeapEnd();
    for ( ; it != heapEnd; ++it)
        counts->add(typeName(*it));

    return counts;
}

bool Heap::isBusy()
{
    return m_heap.operationInProgress != NoOperation;
}

void Heap::reset()
{
    JAVASCRIPTCORE_GC_BEGIN();

    markRoots();

    JAVASCRIPTCORE_GC_MARKED();

    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
    m_heap.nextNumber = 0;
    m_heap.extraCost = 0;
#if ENABLE(JSC_ZOMBIES)
    sweep();
#endif
    resizeBlocks();

    JAVASCRIPTCORE_GC_END();
}

void Heap::collectAllGarbage()
{
    JAVASCRIPTCORE_GC_BEGIN();

    // If the last iteration through the heap deallocated blocks, we need
    // to clean up remaining garbage before marking. Otherwise, the conservative
    // marking mechanism might follow a pointer to unmapped memory.
    if (m_heap.didShrink)
        sweep();

    markRoots();

    JAVASCRIPTCORE_GC_MARKED();

    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
    m_heap.nextNumber = 0;
    m_heap.extraCost = 0;
    sweep();
    resizeBlocks();

    JAVASCRIPTCORE_GC_END();
}

LiveObjectIterator Heap::primaryHeapBegin()
{
    return LiveObjectIterator(m_heap, 0);
}

LiveObjectIterator Heap::primaryHeapEnd()
{
    return LiveObjectIterator(m_heap, m_heap.usedBlocks);
}

} // namespace JSC
