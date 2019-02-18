/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood <jhaygood@reaktix.com>
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <mutex>
#include <stdint.h>
#include <wtf/Atomics.h>
#include <wtf/Expected.h>
#include <wtf/FastTLS.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/StackBounds.h>
#include <wtf/StackStats.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Vector.h>
#include <wtf/WordLock.h>
#include <wtf/text/AtomicStringTable.h>

#if USE(PTHREADS) && !OS(DARWIN)
#include <signal.h>
#endif

namespace WTF {

class AbstractLocker;
class ThreadMessageData;

enum class ThreadGroupAddResult;

class ThreadGroup;
class PrintStream;

// This function can be called from any threads.
WTF_EXPORT_PRIVATE void initializeThreading();

#if USE(PTHREADS)

// We use SIGUSR1 to suspend and resume machine threads in JavaScriptCore.
constexpr const int SigThreadSuspendResume = SIGUSR1;

#endif

// FIXME: The following functions remain because they are used from WebKit Windows support library,
// WebKitQuartzCoreAdditions.dll. When updating the support library, we should use new API instead
// and the following workaound should be removed. And new code should not use the following APIs.
// Remove this workaround code when <rdar://problem/31793213> is fixed.
#if OS(WINDOWS)
WTF_EXPORT_PRIVATE ThreadIdentifier createThread(ThreadFunction, void*, const char* threadName);
WTF_EXPORT_PRIVATE int waitForThreadCompletion(ThreadIdentifier);
#endif

class Thread : public ThreadSafeRefCounted<Thread> {
public:
    friend class ThreadGroup;
    friend WTF_EXPORT_PRIVATE void initializeThreading();
#if OS(WINDOWS)
    friend WTF_EXPORT_PRIVATE int waitForThreadCompletion(ThreadIdentifier);
#endif

    WTF_EXPORT_PRIVATE ~Thread();

    // Returns nullptr if thread creation failed.
    // The thread name must be a literal since on some platforms it's passed in to the thread.
    WTF_EXPORT_PRIVATE static Ref<Thread> create(const char* threadName, Function<void()>&&);

    // Returns Thread object.
    static Thread& current();

    // Set of all WTF::Thread created threads.
    WTF_EXPORT_PRIVATE static HashSet<Thread*>& allThreads(const LockHolder&);
    WTF_EXPORT_PRIVATE static Lock& allThreadsMutex();

#if OS(WINDOWS)
    // Returns ThreadIdentifier directly. It is useful if the user only cares about identity
    // of threads. At that time, users should know that holding this ThreadIdentifier does not ensure
    // that the thread information is alive. While Thread::current() is not safe if it is called
    // from the destructor of the other TLS data, currentID() always returns meaningful thread ID.
    WTF_EXPORT_PRIVATE static ThreadIdentifier currentID();

    ThreadIdentifier id() const { return m_id; }
#endif

    WTF_EXPORT_PRIVATE void changePriority(int);
    WTF_EXPORT_PRIVATE int waitForCompletion();
    WTF_EXPORT_PRIVATE void detach();

#if OS(DARWIN)
    using PlatformSuspendError = kern_return_t;
#elif USE(PTHREADS)
    using PlatformSuspendError = int;
#elif OS(WINDOWS)
    using PlatformSuspendError = DWORD;
#endif

    WTF_EXPORT_PRIVATE Expected<void, PlatformSuspendError> suspend();
    WTF_EXPORT_PRIVATE void resume();
    WTF_EXPORT_PRIVATE size_t getRegisters(PlatformRegisters&);

#if USE(PTHREADS)
    WTF_EXPORT_PRIVATE bool signal(int signalNumber);
#endif

    // Mark the current thread as requiring UI responsiveness.
    // relativePriority is a value in the range [-15, 0] where a lower value indicates a lower priority.
    WTF_EXPORT_PRIVATE static void setCurrentThreadIsUserInteractive(int relativePriority = 0);
    WTF_EXPORT_PRIVATE static void setCurrentThreadIsUserInitiated(int relativePriority = 0);

#if HAVE(QOS_CLASSES)
    WTF_EXPORT_PRIVATE static void setGlobalMaxQOSClass(qos_class_t);
    WTF_EXPORT_PRIVATE static qos_class_t adjustedQOSClass(qos_class_t);
#endif

    // Called in the thread during initialization.
    // Helpful for platforms where the thread name must be set from within the thread.
    static void initializeCurrentThreadInternal(const char* threadName);
    static void initializeCurrentThreadEvenIfNonWTFCreated();
    
    WTF_EXPORT_PRIVATE static const unsigned lockSpinLimit;
    WTF_EXPORT_PRIVATE static void yield();

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;

    static void initializePlatformThreading();

    const StackBounds& stack() const
    {
        return m_stack;
    }

    AtomicStringTable* atomicStringTable()
    {
        return m_currentAtomicStringTable;
    }

    AtomicStringTable* setCurrentAtomicStringTable(AtomicStringTable* atomicStringTable)
    {
        AtomicStringTable* oldAtomicStringTable = m_currentAtomicStringTable;
        m_currentAtomicStringTable = atomicStringTable;
        return oldAtomicStringTable;
    }

#if ENABLE(STACK_STATS)
    StackStats::PerThreadStats& stackStats()
    {
        return m_stackStats;
    }
#endif

    void* savedStackPointerAtVMEntry()
    {
        return m_savedStackPointerAtVMEntry;
    }

    void setSavedStackPointerAtVMEntry(void* stackPointerAtVMEntry)
    {
        m_savedStackPointerAtVMEntry = stackPointerAtVMEntry;
    }

    void* savedLastStackTop()
    {
        return m_savedLastStackTop;
    }

    void setSavedLastStackTop(void* lastStackTop)
    {
        m_savedLastStackTop = lastStackTop;
    }

#if OS(DARWIN)
    mach_port_t machThread() { return m_platformThread; }
#endif

    struct NewThreadContext;
    static void entryPoint(NewThreadContext*);
protected:
    Thread() = default;

    void initializeInThread();

    // Internal platform-specific Thread establishment implementation.
    bool establishHandle(NewThreadContext*);

#if USE(PTHREADS)
    void establishPlatformSpecificHandle(PlatformThreadHandle);
#else
    void establishPlatformSpecificHandle(PlatformThreadHandle, ThreadIdentifier);
#endif

#if USE(PTHREADS) && !OS(DARWIN)
    static void signalHandlerSuspendResume(int, siginfo_t*, void* ucontext);
#endif

    static const char* normalizeThreadName(const char* threadName);

    enum JoinableState : uint8_t {
        // The default thread state. The thread can be joined on.
        Joinable,

        // Somebody waited on this thread to exit and this thread finally exited. This state is here because there can be a
        // period of time between when the thread exits (which causes pthread_join to return and the remainder of waitOnThreadCompletion to run)
        // and when threadDidExit is called. We need threadDidExit to take charge and delete the thread data since there's
        // nobody else to pick up the slack in this case (since waitOnThreadCompletion has already returned).
        Joined,

        // The thread has been detached and can no longer be joined on. At this point, the thread must take care of cleaning up after itself.
        Detached,
    };

    JoinableState joinableState() { return m_joinableState; }
    void didBecomeDetached() { m_joinableState = Detached; }
    void didExit();
    void didJoin() { m_joinableState = Joined; }
    bool hasExited() { return m_didExit; }

    // These functions are only called from ThreadGroup.
    ThreadGroupAddResult addToThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup&);
    void removeFromThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup&);

    // The Thread instance is ref'ed and held in thread-specific storage. It will be deref'ed by destructTLS at thread destruction time.
    // For pthread, it employs pthreads-specific 2-pass destruction to reliably remove Thread.
    // For Windows, we use thread_local to defer thread TLS destruction. It assumes regular ThreadSpecific
    // types don't use multiple-pass destruction.

#if !HAVE(FAST_TLS)
    static WTF_EXPORT_PRIVATE ThreadSpecificKey s_key;
    // One time initialization for this class as a whole.
    // This method must be called before initializeTLS() and it is not thread-safe.
    static void initializeTLSKey();
#endif

    // Creates and puts an instance of Thread into thread-specific storage.
    static Thread& initializeTLS(Ref<Thread>&&);
    WTF_EXPORT_PRIVATE static Thread& initializeCurrentTLS();

    // Returns nullptr if thread-specific storage was not initialized.
    static Thread* currentMayBeNull();

#if OS(WINDOWS)
    WTF_EXPORT_PRIVATE static Thread* currentDying();
    static RefPtr<Thread> get(ThreadIdentifier);
#endif

    // This thread-specific destructor is called 2 times when thread terminates:
    // - first, when all the other thread-specific destructors are called, it simply remembers it was 'destroyed once'
    // and (1) re-sets itself into the thread-specific slot or (2) constructs thread local value to call it again later.
    // - second, after all thread-specific destructors were invoked, it gets called again - this time, we remove the
    // Thread from the threadMap, completing the cleanup.
    static void THREAD_SPECIFIC_CALL destructTLS(void* data);

    JoinableState m_joinableState { Joinable };
    bool m_isShuttingDown { false };
    bool m_didExit { false };
    bool m_isDestroyedOnce { false };

    // Lock & ParkingLot rely on ThreadSpecific. But Thread object can be destroyed even after ThreadSpecific things are destroyed.
    // Use WordLock since WordLock does not depend on ThreadSpecific and this "Thread".
    WordLock m_mutex;
    StackBounds m_stack { StackBounds::emptyBounds() };
    Vector<std::weak_ptr<ThreadGroup>> m_threadGroups;
    PlatformThreadHandle m_handle;
#if OS(WINDOWS)
    ThreadIdentifier m_id { 0 };
#elif OS(DARWIN)
    mach_port_t m_platformThread { MACH_PORT_NULL };
#elif USE(PTHREADS)
    PlatformRegisters* m_platformRegisters { nullptr };
    unsigned m_suspendCount { 0 };
#endif

    AtomicStringTable* m_currentAtomicStringTable { nullptr };
    AtomicStringTable m_defaultAtomicStringTable;

#if ENABLE(STACK_STATS)
    StackStats::PerThreadStats m_stackStats;
#endif
    void* m_savedStackPointerAtVMEntry { nullptr };
    void* m_savedLastStackTop;
public:
    void* m_apiData { nullptr };
};

inline Thread* Thread::currentMayBeNull()
{
#if !HAVE(FAST_TLS)
    ASSERT(s_key != InvalidThreadSpecificKey);
    return static_cast<Thread*>(threadSpecificGet(s_key));
#else
    return static_cast<Thread*>(_pthread_getspecific_direct(WTF_THREAD_DATA_KEY));
#endif
}

inline Thread& Thread::current()
{
    // WRT WebCore:
    //    Thread::current() is used on main thread before it could possibly be used
    //    on secondary ones, so there is no need for synchronization here.
    // WRT JavaScriptCore:
    //    Thread::initializeTLSKey() is initially called from initializeThreading(), ensuring
    //    this is initially called in a std::call_once locked context.
#if !HAVE(FAST_TLS)
    if (UNLIKELY(Thread::s_key == InvalidThreadSpecificKey))
        WTF::initializeThreading();
#endif
    if (auto* thread = currentMayBeNull())
        return *thread;
#if OS(WINDOWS)
    if (auto* thread = currentDying())
        return *thread;
#endif
    return initializeCurrentTLS();
}

} // namespace WTF

using WTF::Thread;

#if OS(WINDOWS)
using WTF::ThreadIdentifier;
using WTF::createThread;
using WTF::waitForThreadCompletion;
#endif
