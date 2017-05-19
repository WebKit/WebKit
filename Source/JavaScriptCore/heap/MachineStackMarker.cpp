/*
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2009 Acision BV. All rights reserved.
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
#include "MachineStackMarker.h"

#include "ConservativeRoots.h"
#include "GPRInfo.h"
#include "Heap.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "LLIntPCRanges.h"
#include "MacroAssembler.h"
#include "VM.h"
#include <setjmp.h>
#include <stdlib.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

using namespace WTF;

namespace JSC {

class ActiveMachineThreadsManager;
static ActiveMachineThreadsManager& activeMachineThreadsManager();

class ActiveMachineThreadsManager {
    WTF_MAKE_NONCOPYABLE(ActiveMachineThreadsManager);
public:

    class Locker {
    public:
        Locker(ActiveMachineThreadsManager& manager)
            : m_locker(manager.m_lock)
        {
        }

    private:
        LockHolder m_locker;
    };

    void add(MachineThreads* machineThreads)
    {
        LockHolder managerLock(m_lock);
        m_set.add(machineThreads);
    }

    void THREAD_SPECIFIC_CALL remove(MachineThreads* machineThreads)
    {
        LockHolder managerLock(m_lock);
        auto recordedMachineThreads = m_set.take(machineThreads);
        RELEASE_ASSERT(recordedMachineThreads == machineThreads);
    }

    bool contains(MachineThreads* machineThreads)
    {
        return m_set.contains(machineThreads);
    }

private:
    typedef HashSet<MachineThreads*> MachineThreadsSet;

    ActiveMachineThreadsManager() { }
    
    Lock m_lock;
    MachineThreadsSet m_set;

    friend ActiveMachineThreadsManager& activeMachineThreadsManager();
};

static ActiveMachineThreadsManager& activeMachineThreadsManager()
{
    static std::once_flag initializeManagerOnceFlag;
    static ActiveMachineThreadsManager* manager = nullptr;

    std::call_once(initializeManagerOnceFlag, [] {
        manager = new ActiveMachineThreadsManager();
    });
    return *manager;
}
    
MachineThreads::MachineThreads()
    : m_registeredThreads()
    , m_threadSpecificForMachineThreads(0)
{
    threadSpecificKeyCreate(&m_threadSpecificForMachineThreads, removeThread);
    activeMachineThreadsManager().add(this);
}

MachineThreads::~MachineThreads()
{
    activeMachineThreadsManager().remove(this);
    threadSpecificKeyDelete(m_threadSpecificForMachineThreads);

    LockHolder registeredThreadsLock(m_registeredThreadsMutex);
    for (MachineThread* current = m_registeredThreads.head(); current;) {
        MachineThread* next = current->next();
        delete current;
        current = next;
    }
}

void MachineThreads::addCurrentThread()
{
    if (threadSpecificGet(m_threadSpecificForMachineThreads)) {
#ifndef NDEBUG
        LockHolder lock(m_registeredThreadsMutex);
        ASSERT(threadSpecificGet(m_threadSpecificForMachineThreads) == this);
#endif
        return;
    }

    MachineThread* thread = new MachineThread();
    threadSpecificSet(m_threadSpecificForMachineThreads, this);

    LockHolder lock(m_registeredThreadsMutex);

    m_registeredThreads.append(thread);
}

auto MachineThreads::machineThreadForCurrentThread() -> MachineThread*
{
    LockHolder lock(m_registeredThreadsMutex);
    ThreadIdentifier id = currentThread();
    for (MachineThread* thread = m_registeredThreads.head(); thread; thread = thread->next()) {
        if (thread->threadID() == id)
            return thread;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

void THREAD_SPECIFIC_CALL MachineThreads::removeThread(void* p)
{
    auto& manager = activeMachineThreadsManager();
    ActiveMachineThreadsManager::Locker lock(manager);
    auto machineThreads = static_cast<MachineThreads*>(p);
    if (manager.contains(machineThreads)) {
        // There's a chance that the MachineThreads registry that this thread
        // was registered with was already destructed, and another one happened
        // to be instantiated at the same address. Hence, this thread may or
        // may not be found in this MachineThreads registry. We only need to
        // do a removal if this thread is found in it.

#if OS(WINDOWS)
        // On Windows the thread specific destructor is also called when the
        // main thread is exiting. This may lead to the main thread waiting
        // forever for the machine thread lock when exiting, if the sampling
        // profiler thread was terminated by the system while holding the
        // machine thread lock.
        if (WTF::isMainThread())
            return;
#endif

        machineThreads->removeThreadIfFound(currentThread());
    }
}

void MachineThreads::removeThreadIfFound(ThreadIdentifier id)
{
    LockHolder lock(m_registeredThreadsMutex);
    for (MachineThread* current = m_registeredThreads.head(); current; current = current->next()) {
        if (current->threadID() == id) {
            m_registeredThreads.remove(current);
            delete current;
            break;
        }
    }
}

SUPPRESS_ASAN
void MachineThreads::gatherFromCurrentThread(ConservativeRoots& conservativeRoots, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks, CurrentThreadState& currentThreadState)
{
    if (currentThreadState.registerState) {
        void* registersBegin = currentThreadState.registerState;
        void* registersEnd = reinterpret_cast<void*>(roundUpToMultipleOf<sizeof(void*)>(reinterpret_cast<uintptr_t>(currentThreadState.registerState + 1)));
        conservativeRoots.add(registersBegin, registersEnd, jitStubRoutines, codeBlocks);
    }

    conservativeRoots.add(currentThreadState.stackTop, currentThreadState.stackOrigin, jitStubRoutines, codeBlocks);
}

MachineThreads::MachineThread::MachineThread()
    : m_thread(WTF::Thread::current())
{
    auto stackBounds = wtfThreadData().stack();
    m_stackBase = stackBounds.origin();
    m_stackEnd = stackBounds.end();
}

size_t MachineThreads::MachineThread::getRegisters(MachineThread::Registers& registers)
{
    WTF::PlatformRegisters& regs = registers.regs;
    return m_thread->getRegisters(regs);
}

void* MachineThreads::MachineThread::Registers::stackPointer() const
{
    return MachineContext::stackPointer(regs);
}

#if ENABLE(SAMPLING_PROFILER)
void* MachineThreads::MachineThread::Registers::framePointer() const
{
#if OS(WINDOWS) || HAVE(MACHINE_CONTEXT)
    return MachineContext::framePointer(regs);
#else
#error Need a way to get the frame pointer for another thread on this platform
#endif
}

void* MachineThreads::MachineThread::Registers::instructionPointer() const
{
#if OS(WINDOWS) || HAVE(MACHINE_CONTEXT)
    return MachineContext::instructionPointer(regs);
#else
#error Need a way to get the instruction pointer for another thread on this platform
#endif
}

void* MachineThreads::MachineThread::Registers::llintPC() const
{
    // LLInt uses regT4 as PC.
#if OS(WINDOWS) || HAVE(MACHINE_CONTEXT)
    return MachineContext::llintInstructionPointer(regs);
#else
#error Need a way to get the LLIntPC for another thread on this platform
#endif
}
#endif // ENABLE(SAMPLING_PROFILER)

static inline int osRedZoneAdjustment()
{
    int redZoneAdjustment = 0;
#if !OS(WINDOWS)
#if CPU(X86_64)
    // See http://people.freebsd.org/~obrien/amd64-elf-abi.pdf Section 3.2.2.
    redZoneAdjustment = -128;
#elif CPU(ARM64)
    // See https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html#//apple_ref/doc/uid/TP40013702-SW7
    redZoneAdjustment = -128;
#endif
#endif // !OS(WINDOWS)
    return redZoneAdjustment;
}

std::pair<void*, size_t> MachineThreads::MachineThread::captureStack(void* stackTop)
{
    char* begin = reinterpret_cast_ptr<char*>(m_stackBase);
    char* end = bitwise_cast<char*>(WTF::roundUpToMultipleOf<sizeof(void*)>(reinterpret_cast<uintptr_t>(stackTop)));
    ASSERT(begin >= end);

    char* endWithRedZone = end + osRedZoneAdjustment();
    ASSERT(WTF::roundUpToMultipleOf<sizeof(void*)>(reinterpret_cast<uintptr_t>(endWithRedZone)) == reinterpret_cast<uintptr_t>(endWithRedZone));

    if (endWithRedZone < m_stackEnd)
        endWithRedZone = reinterpret_cast_ptr<char*>(m_stackEnd);

    std::swap(begin, endWithRedZone);
    return std::make_pair(begin, endWithRedZone - begin);
}

SUPPRESS_ASAN
static void copyMemory(void* dst, const void* src, size_t size)
{
    size_t dstAsSize = reinterpret_cast<size_t>(dst);
    size_t srcAsSize = reinterpret_cast<size_t>(src);
    RELEASE_ASSERT(dstAsSize == WTF::roundUpToMultipleOf<sizeof(intptr_t)>(dstAsSize));
    RELEASE_ASSERT(srcAsSize == WTF::roundUpToMultipleOf<sizeof(intptr_t)>(srcAsSize));
    RELEASE_ASSERT(size == WTF::roundUpToMultipleOf<sizeof(intptr_t)>(size));

    intptr_t* dstPtr = reinterpret_cast<intptr_t*>(dst);
    const intptr_t* srcPtr = reinterpret_cast<const intptr_t*>(src);
    size /= sizeof(intptr_t);
    while (size--)
        *dstPtr++ = *srcPtr++;
}
    


// This function must not call malloc(), free(), or any other function that might
// acquire a lock. Since 'thread' is suspended, trying to acquire a lock
// will deadlock if 'thread' holds that lock.
// This function, specifically the memory copying, was causing problems with Address Sanitizer in
// apps. Since we cannot blacklist the system memcpy we must use our own naive implementation,
// copyMemory, for ASan to work on either instrumented or non-instrumented builds. This is not a
// significant performance loss as tryCopyOtherThreadStack is only called as part of an O(heapsize)
// operation. As the heap is generally much larger than the stack the performance hit is minimal.
// See: https://bugs.webkit.org/show_bug.cgi?id=146297
void MachineThreads::tryCopyOtherThreadStack(MachineThread* thread, void* buffer, size_t capacity, size_t* size)
{
    MachineThread::Registers registers;
    size_t registersSize = thread->getRegisters(registers);

    // This is a workaround for <rdar://problem/27607384>. libdispatch recycles work
    // queue threads without running pthread exit destructors. This can cause us to scan a
    // thread during work queue initialization, when the stack pointer is null.
    if (UNLIKELY(!registers.stackPointer())) {
        *size = 0;
        return;
    }

    std::pair<void*, size_t> stack = thread->captureStack(registers.stackPointer());

    bool canCopy = *size + registersSize + stack.second <= capacity;

    if (canCopy)
        copyMemory(static_cast<char*>(buffer) + *size, &registers, registersSize);
    *size += registersSize;

    if (canCopy)
        copyMemory(static_cast<char*>(buffer) + *size, stack.first, stack.second);
    *size += stack.second;
}

bool MachineThreads::tryCopyOtherThreadStacks(const AbstractLocker&, void* buffer, size_t capacity, size_t* size)
{
    // Prevent two VMs from suspending each other's threads at the same time,
    // which can cause deadlock: <rdar://problem/20300842>.
    static StaticLock mutex;
    std::lock_guard<StaticLock> lock(mutex);

    *size = 0;

    ThreadIdentifier id = currentThread();
    int numberOfThreads = 0; // Using 0 to denote that we haven't counted the number of threads yet.
    int index = 1;
    DoublyLinkedList<MachineThread> threadsToBeDeleted;

    for (MachineThread* thread = m_registeredThreads.head(); thread; index++) {
        if (thread->threadID() != id) {
            auto result = thread->suspend();
#if OS(DARWIN)
            if (!result) {
                if (!numberOfThreads)
                    numberOfThreads = m_registeredThreads.size();

                ASSERT(result.error() != KERN_SUCCESS);

                WTFReportError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION,
                    "JavaScript garbage collection encountered an invalid thread (err 0x%x): Thread [%d/%d: %p] id %u.",
                    result.error(), index, numberOfThreads, thread, thread->threadID());

                // Put the invalid thread on the threadsToBeDeleted list.
                // We can't just delete it here because we have suspended other
                // threads, and they may still be holding the C heap lock which
                // we need for deleting the invalid thread. Hence, we need to
                // defer the deletion till after we have resumed all threads.
                MachineThread* nextThread = thread->next();
                m_registeredThreads.remove(thread);
                threadsToBeDeleted.append(thread);
                thread = nextThread;
                continue;
            }
#else
            UNUSED_PARAM(numberOfThreads);
            ASSERT_UNUSED(result, result);
#endif
        }
        thread = thread->next();
    }

    for (MachineThread* thread = m_registeredThreads.head(); thread; thread = thread->next()) {
        if (thread->threadID() != id)
            tryCopyOtherThreadStack(thread, buffer, capacity, size);
    }

    for (MachineThread* thread = m_registeredThreads.head(); thread; thread = thread->next()) {
        if (thread->threadID() != id)
            thread->resume();
    }

    for (MachineThread* thread = threadsToBeDeleted.head(); thread; ) {
        MachineThread* nextThread = thread->next();
        delete thread;
        thread = nextThread;
    }
    
    return *size <= capacity;
}

static void growBuffer(size_t size, void** buffer, size_t* capacity)
{
    if (*buffer)
        fastFree(*buffer);

    *capacity = WTF::roundUpToMultipleOf(WTF::pageSize(), size * 2);
    *buffer = fastMalloc(*capacity);
}

void MachineThreads::gatherConservativeRoots(ConservativeRoots& conservativeRoots, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks, CurrentThreadState* currentThreadState)
{
    if (currentThreadState)
        gatherFromCurrentThread(conservativeRoots, jitStubRoutines, codeBlocks, *currentThreadState);

    size_t size;
    size_t capacity = 0;
    void* buffer = nullptr;
    LockHolder lock(m_registeredThreadsMutex);
    while (!tryCopyOtherThreadStacks(lock, buffer, capacity, &size))
        growBuffer(size, &buffer, &capacity);

    if (!buffer)
        return;

    conservativeRoots.add(buffer, static_cast<char*>(buffer) + size, jitStubRoutines, codeBlocks);
    fastFree(buffer);
}

NEVER_INLINE int callWithCurrentThreadState(const ScopedLambda<void(CurrentThreadState&)>& lambda)
{
    DECLARE_AND_COMPUTE_CURRENT_THREAD_STATE(state);
    lambda(state);
    return 42; // Suppress tail call optimization.
}

} // namespace JSC
