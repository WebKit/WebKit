/*
 * Copyright (C) 2007, 2008, 2010, 2014 Apple Inc. All rights reserved.
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

#ifndef Threading_h
#define Threading_h

#include <mutex>
#include <stdint.h>
#include <wtf/Atomics.h>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(PTHREADS) && !OS(DARWIN)
#include <semaphore.h>
#include <signal.h>
#endif

namespace WTF {

class ThreadMessageData;

using ThreadIdentifier = uint32_t;
typedef void (*ThreadFunction)(void* argument);

class ThreadHolder;
class PrintStream;

class Thread : public ThreadSafeRefCounted<Thread> {
public:
    friend class ThreadHolder;

    WTF_EXPORT_PRIVATE ~Thread();

    // Returns nullptr if thread creation failed.
    // The thread name must be a literal since on some platforms it's passed in to the thread.
    WTF_EXPORT_PRIVATE static RefPtr<Thread> create(const char* threadName, Function<void()>&&);

    // Returns Thread object.
    WTF_EXPORT_PRIVATE static Thread& current();
    WTF_EXPORT_PRIVATE static Thread* currentMayBeNull();

    // Returns ThreadIdentifier directly. It is useful if the user only cares about identity
    // of threads. At that time, users should know that holding this ThreadIdentifier does not ensure
    // that the thread information is alive. While Thread::current() is not safe if it is called
    // from the destructor of the other TLS data, currentID() always returns meaningful thread ID.
    WTF_EXPORT_PRIVATE static ThreadIdentifier currentID();

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

    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;

    ThreadIdentifier id() const { return m_id; }

    bool operator==(const Thread& thread)
    {
        return id() == thread.id();
    }

    bool operator!=(const Thread& thread)
    {
        return id() != thread.id();
    }

    static void initializePlatformThreading();

#if OS(DARWIN)
    mach_port_t machThread() { return m_platformThread; }
#endif

protected:
    Thread();

    // Internal platform-specific Thread::create implementation.
    static RefPtr<Thread> createInternal(ThreadFunction, void*, const char* threadName);

#if USE(PTHREADS)
    void establish(pthread_t);
#else
    void establish(HANDLE, ThreadIdentifier);
#endif

#if USE(PTHREADS) && !OS(DARWIN)
    static void signalHandlerSuspendResume(int, siginfo_t*, void* ucontext);
#endif

    static const char* normalizeThreadName(const char* threadName);

    enum JoinableState {
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

    // WordLock & Lock rely on ThreadSpecific. But Thread object can be destroyed even after ThreadSpecific things are destroyed.
    std::mutex m_mutex;
    ThreadIdentifier m_id { 0 };
    JoinableState m_joinableState { Joinable };
    bool m_didExit { false };
#if USE(PTHREADS)
    pthread_t m_handle;

#if OS(DARWIN)
    mach_port_t m_platformThread;
#else
    sem_t m_semaphoreForSuspendResume;
    PlatformRegisters m_platformRegisters;
    unsigned m_suspendCount { 0 };
    std::atomic<bool> m_suspended { false };
#endif
#elif OS(WINDOWS)
    HANDLE m_handle { INVALID_HANDLE_VALUE };
#else
#error Unknown System
#endif
};

// This function must be called from the main thread. It is safe to call it repeatedly.
// Darwin is an exception to this rule: it is OK to call it from any thread, the only
// requirement is that the calls are not reentrant.
WTF_EXPORT_PRIVATE void initializeThreading();

inline ThreadIdentifier currentThread()
{
    return Thread::currentID();
}


// FIXME: The following functions remain because they are used from WebKit Windows support library,
// WebKitQuartzCoreAdditions.dll. When updating the support library, we should use new API instead
// and the following workaound should be removed. And new code should not use the following APIs.
// Remove this workaround code when <rdar://problem/31793213> is fixed.
#if OS(WINDOWS)
WTF_EXPORT_PRIVATE ThreadIdentifier createThread(ThreadFunction, void*, const char* threadName);
WTF_EXPORT_PRIVATE int waitForThreadCompletion(ThreadIdentifier);
#endif

} // namespace WTF

using WTF::ThreadIdentifier;
using WTF::Thread;
using WTF::currentThread;

#if OS(WINDOWS)
using WTF::createThread;
using WTF::waitForThreadCompletion;
#endif

#endif // Threading_h
