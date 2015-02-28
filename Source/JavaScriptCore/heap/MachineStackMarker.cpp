/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "Heap.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "VM.h"
#include <setjmp.h>
#include <stdlib.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)

#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#elif OS(WINDOWS)

#include <windows.h>
#include <malloc.h>

#elif OS(UNIX)

#include <sys/mman.h>
#include <unistd.h>

#if OS(SOLARIS)
#include <thread.h>
#else
#include <pthread.h>
#endif

#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#if USE(PTHREADS) && !OS(WINDOWS) && !OS(DARWIN)
#include <signal.h>
#endif

#endif

using namespace WTF;

namespace JSC {

#if OS(DARWIN)
typedef mach_port_t PlatformThread;
#elif OS(WINDOWS)
typedef HANDLE PlatformThread;
#elif USE(PTHREADS)
typedef pthread_t PlatformThread;
static const int SigThreadSuspendResume = SIGUSR2;

#if defined(SA_RESTART)
static void pthreadSignalHandlerSuspendResume(int)
{
    sigset_t signalSet;
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SigThreadSuspendResume);
    sigsuspend(&signalSet);
}
#endif
#endif

class MachineThreads::Thread {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Thread(const PlatformThread& platThread, void* base)
        : platformThread(platThread)
        , stackBase(base)
    {
#if USE(PTHREADS) && !OS(WINDOWS) && !OS(DARWIN) && defined(SA_RESTART)
        // if we have SA_RESTART, enable SIGUSR2 debugging mechanism
        struct sigaction action;
        action.sa_handler = pthreadSignalHandlerSuspendResume;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        sigaction(SigThreadSuspendResume, &action, 0);

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SigThreadSuspendResume);
        pthread_sigmask(SIG_UNBLOCK, &mask, 0);
#endif
    }

    Thread* next;
    PlatformThread platformThread;
    void* stackBase;
};

MachineThreads::MachineThreads(Heap* heap)
    : m_registeredThreads(0)
    , m_threadSpecific(0)
#if !ASSERT_DISABLED
    , m_heap(heap)
#endif
{
    UNUSED_PARAM(heap);
    threadSpecificKeyCreate(&m_threadSpecific, removeThread);
}

MachineThreads::~MachineThreads()
{
    threadSpecificKeyDelete(m_threadSpecific);

    MutexLocker registeredThreadsLock(m_registeredThreadsMutex);
    for (Thread* t = m_registeredThreads; t;) {
        Thread* next = t->next;
        delete t;
        t = next;
    }
}

static inline PlatformThread getCurrentPlatformThread()
{
#if OS(DARWIN)
    return pthread_mach_thread_np(pthread_self());
#elif OS(WINDOWS)
    return GetCurrentThread();
#elif USE(PTHREADS)
    return pthread_self();
#endif
}

static inline bool equalThread(const PlatformThread& first, const PlatformThread& second)
{
#if OS(DARWIN) || OS(WINDOWS)
    return first == second;
#elif USE(PTHREADS)
    return !!pthread_equal(first, second);
#else
#error Need a way to compare threads on this platform
#endif
}

void MachineThreads::addCurrentThread()
{
    ASSERT(!m_heap->vm()->hasExclusiveThread() || m_heap->vm()->exclusiveThread() == std::this_thread::get_id());

    if (threadSpecificGet(m_threadSpecific)) {
        ASSERT(threadSpecificGet(m_threadSpecific) == this);
        return;
    }

    threadSpecificSet(m_threadSpecific, this);
    Thread* thread = new Thread(getCurrentPlatformThread(), wtfThreadData().stack().origin());

    MutexLocker lock(m_registeredThreadsMutex);

    thread->next = m_registeredThreads;
    m_registeredThreads = thread;
}

void MachineThreads::removeThread(void* p)
{
    static_cast<MachineThreads*>(p)->removeCurrentThread();
}

void MachineThreads::removeCurrentThread()
{
    PlatformThread currentPlatformThread = getCurrentPlatformThread();

    MutexLocker lock(m_registeredThreadsMutex);
    if (equalThread(currentPlatformThread, m_registeredThreads->platformThread)) {
        Thread* t = m_registeredThreads;
        m_registeredThreads = m_registeredThreads->next;
        delete t;
    } else {
        Thread* last = m_registeredThreads;
        Thread* t;
        for (t = m_registeredThreads->next; t; t = t->next) {
            if (equalThread(t->platformThread, currentPlatformThread)) {
                last->next = t->next;
                break;
            }
            last = t;
        }
        ASSERT(t); // If t is NULL, we never found ourselves in the list.
        delete t;
    }
}
    
void MachineThreads::gatherFromCurrentThread(ConservativeRoots& conservativeRoots, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks, void* stackCurrent, RegisterState& registers)
{
    void* registersBegin = &registers;
    void* registersEnd = reinterpret_cast<void*>(roundUpToMultipleOf<sizeof(void*)>(reinterpret_cast<uintptr_t>(&registers + 1)));
    conservativeRoots.add(registersBegin, registersEnd, jitStubRoutines, codeBlocks);

    void* stackBegin = stackCurrent;
    void* stackEnd = wtfThreadData().stack().origin();
    conservativeRoots.add(stackBegin, stackEnd, jitStubRoutines, codeBlocks);
}

static inline bool suspendThread(const PlatformThread& platformThread)
{
#if OS(DARWIN)
    kern_return_t result = thread_suspend(platformThread);
    return result == KERN_SUCCESS;
#elif OS(WINDOWS)
    bool threadIsSuspended = (SuspendThread(platformThread) != (DWORD)-1);
    ASSERT(threadIsSuspended);
    return threadIsSuspended;
#elif USE(PTHREADS)
    pthread_kill(platformThread, SigThreadSuspendResume);
    return true;
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
#elif USE(PTHREADS)
    pthread_kill(platformThread, SigThreadSuspendResume);
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
#elif CPU(ARM64)
typedef arm_thread_state64_t PlatformThreadRegisters;
#else
#error Unknown Architecture
#endif

#elif OS(WINDOWS)
typedef CONTEXT PlatformThreadRegisters;
#elif USE(PTHREADS)
typedef pthread_attr_t PlatformThreadRegisters;
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
#elif CPU(ARM64)
    unsigned user_count = ARM_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = ARM_THREAD_STATE64;
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

#elif OS(WINDOWS)
    regs.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    GetThreadContext(platformThread, &regs);
    return sizeof(CONTEXT);
#elif USE(PTHREADS)
    pthread_attr_init(&regs);
#if HAVE(PTHREAD_NP_H) || OS(NETBSD)
#if !OS(OPENBSD)
    // e.g. on FreeBSD 5.4, neundorf@kde.org
    pthread_attr_get_np(platformThread, &regs);
#endif
#else
    // FIXME: this function is non-portable; other POSIX systems may have different np alternatives
    pthread_getattr_np(platformThread, &regs);
#endif
    return 0;
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
#elif CPU(ARM64)
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
#elif OS(WINDOWS)

#if CPU(ARM)
    return reinterpret_cast<void*>((uintptr_t) regs.Sp);
#elif CPU(MIPS)
    return reinterpret_cast<void*>((uintptr_t) regs.IntSp);
#elif CPU(X86)
    return reinterpret_cast<void*>((uintptr_t) regs.Esp);
#elif CPU(X86_64)
    return reinterpret_cast<void*>((uintptr_t) regs.Rsp);
#else
#error Unknown Architecture
#endif

#elif USE(PTHREADS)
    void* stackBase = 0;
    size_t stackSize = 0;
#if OS(OPENBSD)
    stack_t ss;
    int rc = pthread_stackseg_np(pthread_self(), &ss);
    stackBase = (void*)((size_t) ss.ss_sp - ss.ss_size);
    stackSize = ss.ss_size;
#else
    int rc = pthread_attr_getstack(&regs, &stackBase, &stackSize);
#endif
    (void)rc; // FIXME: Deal with error code somehow? Seems fatal.
    ASSERT(stackBase);
    return static_cast<char*>(stackBase) + stackSize;
#else
#error Need a way to get the stack pointer for another thread on this platform
#endif
}

static void freePlatformThreadRegisters(PlatformThreadRegisters& regs)
{
#if USE(PTHREADS) && !OS(WINDOWS) && !OS(DARWIN)
    pthread_attr_destroy(&regs);
#else
    UNUSED_PARAM(regs);
#endif
}

static std::pair<void*, size_t> otherThreadStack(void* stackBase, const PlatformThreadRegisters& registers)
{
    void* begin = stackBase;
    void* end = reinterpret_cast<void*>(
        WTF::roundUpToMultipleOf<sizeof(void*)>(
            reinterpret_cast<uintptr_t>(
                otherThreadStackPointer(registers))));
    if (begin > end)
        std::swap(begin, end);
    return std::make_pair(begin, static_cast<char*>(end) - static_cast<char*>(begin));
}

#if ASAN_ENABLED
void asanUnsafeMemcpy(void* dst, const void* src, size_t);
void asanUnsafeMemcpy(void* dst, const void* src, size_t size)
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
    
#define memcpy asanUnsafeMemcpy
#endif

// This function must not call malloc(), free(), or any other function that might
// acquire a lock. Since 'thread' is suspended, trying to acquire a lock
// will deadlock if 'thread' holds that lock.
void MachineThreads::tryCopyOtherThreadStack(Thread* thread, void* buffer, size_t capacity, size_t* size)
{
    PlatformThreadRegisters registers;
    size_t registersSize = getPlatformThreadRegisters(thread->platformThread, registers);
    std::pair<void*, size_t> stack = otherThreadStack(thread->stackBase, registers);

    bool canCopy = *size + registersSize + stack.second <= capacity;

    if (canCopy)
        memcpy(static_cast<char*>(buffer) + *size, &registers, registersSize);
    *size += registersSize;

    if (canCopy)
        memcpy(static_cast<char*>(buffer) + *size, stack.first, stack.second);
    *size += stack.second;

    freePlatformThreadRegisters(registers);
}

bool MachineThreads::tryCopyOtherThreadStacks(MutexLocker&, void* buffer, size_t capacity, size_t* size)
{
    *size = 0;

    PlatformThread currentPlatformThread = getCurrentPlatformThread();
    int numberOfThreads = 0; // Using 0 to denote that we haven't counted the number of threads yet.
    int index = 1;
    Thread* threadsToBeDeleted = nullptr;

    Thread* previousThread = nullptr;
    for (Thread* thread = m_registeredThreads; thread; index++) {
        if (!equalThread(thread->platformThread, currentPlatformThread)) {
            bool success = suspendThread(thread->platformThread);
#if OS(DARWIN)
            if (!success) {
                if (!numberOfThreads) {
                    for (Thread* countedThread = m_registeredThreads; countedThread; countedThread = countedThread->next)
                        numberOfThreads++;
                }
                
                // Re-do the suspension to get the actual failure result for logging.
                kern_return_t error = thread_suspend(thread->platformThread);
                ASSERT(error != KERN_SUCCESS);

                WTFReportError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION,
                    "JavaScript garbage collection encountered an invalid thread (err 0x%x): Thread [%d/%d: %p] platformThread %p.",
                    error, index, numberOfThreads, thread, reinterpret_cast<void*>(thread->platformThread));

                // Put the invalid thread on the threadsToBeDeleted list.
                // We can't just delete it here because we have suspended other
                // threads, and they may still be holding the C heap lock which
                // we need for deleting the invalid thread. Hence, we need to
                // defer the deletion till after we have resumed all threads.
                Thread* nextThread = thread->next;
                thread->next = threadsToBeDeleted;
                threadsToBeDeleted = thread;

                if (previousThread)
                    previousThread->next = nextThread;
                else
                    m_registeredThreads = nextThread;
                thread = nextThread;
                continue;
            }
#else
            UNUSED_PARAM(numberOfThreads);
            UNUSED_PARAM(previousThread);
            ASSERT_UNUSED(success, success);
#endif
        }
        previousThread = thread;
        thread = thread->next;
    }

    for (Thread* thread = m_registeredThreads; thread; thread = thread->next) {
        if (!equalThread(thread->platformThread, currentPlatformThread))
            tryCopyOtherThreadStack(thread, buffer, capacity, size);
    }

    for (Thread* thread = m_registeredThreads; thread; thread = thread->next) {
        if (!equalThread(thread->platformThread, currentPlatformThread))
            resumeThread(thread->platformThread);
    }

    for (Thread* thread = threadsToBeDeleted; thread; ) {
        Thread* nextThread = thread->next;
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

void MachineThreads::gatherConservativeRoots(ConservativeRoots& conservativeRoots, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks, void* stackCurrent, RegisterState& currentThreadRegisters)
{
    gatherFromCurrentThread(conservativeRoots, jitStubRoutines, codeBlocks, stackCurrent, currentThreadRegisters);

    size_t size;
    size_t capacity = 0;
    void* buffer = nullptr;
    MutexLocker lock(m_registeredThreadsMutex);
    while (!tryCopyOtherThreadStacks(lock, buffer, capacity, &size))
        growBuffer(size, &buffer, &capacity);

    if (!buffer)
        return;

    conservativeRoots.add(buffer, static_cast<char*>(buffer) + size, jitStubRoutines, codeBlocks);
    fastFree(buffer);
}

} // namespace JSC
