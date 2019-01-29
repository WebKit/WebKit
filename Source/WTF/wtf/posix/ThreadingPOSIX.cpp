/*
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood <jhaygood@reaktix.com>
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include <wtf/Threading.h>

#if USE(PTHREADS)

#include <errno.h>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RawPointer.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadGroup.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WordLock.h>

#if OS(LINUX)
#include <sys/prctl.h>
#endif

#if !COMPILER(MSVC)
#include <limits.h>
#include <sched.h>
#include <sys/time.h>
#endif

#if !OS(DARWIN) && OS(UNIX)

#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#if HAVE(PTHREAD_NP_H)
#include <pthread_np.h>
#endif

#endif

namespace WTF {

static Lock globalSuspendLock;

Thread::~Thread()
{
}

#if !OS(DARWIN)
class Semaphore {
    WTF_MAKE_NONCOPYABLE(Semaphore);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit Semaphore(unsigned initialValue)
    {
        int sharedBetweenProcesses = 0;
        sem_init(&m_platformSemaphore, sharedBetweenProcesses, initialValue);
    }

    ~Semaphore()
    {
        sem_destroy(&m_platformSemaphore);
    }

    void wait()
    {
        sem_wait(&m_platformSemaphore);
    }

    void post()
    {
        sem_post(&m_platformSemaphore);
    }

private:
    sem_t m_platformSemaphore;
};
static LazyNeverDestroyed<Semaphore> globalSemaphoreForSuspendResume;

// We use SIGUSR1 to suspend and resume machine threads in JavaScriptCore.
static constexpr const int SigThreadSuspendResume = SIGUSR1;
static std::atomic<Thread*> targetThread { nullptr };

void Thread::signalHandlerSuspendResume(int, siginfo_t*, void* ucontext)
{
    // Touching a global variable atomic types from signal handlers is allowed.
    Thread* thread = targetThread.load();

    if (thread->m_suspendCount) {
        // This is signal handler invocation that is intended to be used to resume sigsuspend.
        // So this handler invocation itself should not process.
        //
        // When signal comes, first, the system calls signal handler. And later, sigsuspend will be resumed. Signal handler invocation always precedes.
        // So, the problem never happens that suspended.store(true, ...) will be executed before the handler is called.
        // http://pubs.opengroup.org/onlinepubs/009695399/functions/sigsuspend.html
        return;
    }

    void* approximateStackPointer = currentStackPointer();
    if (!thread->m_stack.contains(approximateStackPointer)) {
        // This happens if we use an alternative signal stack.
        // 1. A user-defined signal handler is invoked with an alternative signal stack.
        // 2. In the middle of the execution of the handler, we attempt to suspend the target thread.
        // 3. A nested signal handler is executed.
        // 4. The stack pointer saved in the machine context will be pointing to the alternative signal stack.
        // In this case, we back off the suspension and retry a bit later.
        thread->m_platformRegisters = nullptr;
        globalSemaphoreForSuspendResume->post();
        return;
    }

#if HAVE(MACHINE_CONTEXT)
    ucontext_t* userContext = static_cast<ucontext_t*>(ucontext);
    thread->m_platformRegisters = &registersFromUContext(userContext);
#else
    UNUSED_PARAM(ucontext);
    PlatformRegisters platformRegisters { approximateStackPointer };
    thread->m_platformRegisters = &platformRegisters;
#endif

    // Allow suspend caller to see that this thread is suspended.
    // sem_post is async-signal-safe function. It means that we can call this from a signal handler.
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03
    //
    // And sem_post emits memory barrier that ensures that PlatformRegisters are correctly saved.
    // http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_11
    globalSemaphoreForSuspendResume->post();

    // Reaching here, SigThreadSuspendResume is blocked in this handler (this is configured by sigaction's sa_mask).
    // So before calling sigsuspend, SigThreadSuspendResume to this thread is deferred. This ensures that the handler is not executed recursively.
    sigset_t blockedSignalSet;
    sigfillset(&blockedSignalSet);
    sigdelset(&blockedSignalSet, SigThreadSuspendResume);
    sigsuspend(&blockedSignalSet);

    thread->m_platformRegisters = nullptr;

    // Allow resume caller to see that this thread is resumed.
    globalSemaphoreForSuspendResume->post();
}

#endif // !OS(DARWIN)

void Thread::initializePlatformThreading()
{
#if !OS(DARWIN)
    globalSemaphoreForSuspendResume.construct(0);

    // Signal handlers are process global configuration.
    // Intentionally block SigThreadSuspendResume in the handler.
    // SigThreadSuspendResume will be allowed in the handler by sigsuspend.
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SigThreadSuspendResume);

    action.sa_sigaction = &signalHandlerSuspendResume;
    action.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SigThreadSuspendResume, &action, 0);
#endif
}

void Thread::initializeCurrentThreadEvenIfNonWTFCreated()
{
#if !OS(DARWIN)
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SigThreadSuspendResume);
    pthread_sigmask(SIG_UNBLOCK, &mask, 0);
#endif
}

static void* wtfThreadEntryPoint(void* context)
{
    Thread::entryPoint(reinterpret_cast<Thread::NewThreadContext*>(context));
    return nullptr;
}

bool Thread::establishHandle(NewThreadContext* context)
{
    pthread_t threadHandle;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
#if HAVE(QOS_CLASSES)
    pthread_attr_set_qos_class_np(&attr, adjustedQOSClass(QOS_CLASS_USER_INITIATED), 0);
#endif
    int error = pthread_create(&threadHandle, &attr, wtfThreadEntryPoint, context);
    pthread_attr_destroy(&attr);
    if (error) {
        LOG_ERROR("Failed to create pthread at entry point %p with context %p", wtfThreadEntryPoint, context);
        return false;
    }
    establishPlatformSpecificHandle(threadHandle);
    return true;
}

void Thread::initializeCurrentThreadInternal(const char* threadName)
{
#if HAVE(PTHREAD_SETNAME_NP)
    pthread_setname_np(normalizeThreadName(threadName));
#elif OS(LINUX)
    prctl(PR_SET_NAME, normalizeThreadName(threadName));
#else
    UNUSED_PARAM(threadName);
#endif
    initializeCurrentThreadEvenIfNonWTFCreated();
}

void Thread::changePriority(int delta)
{
#if HAVE(PTHREAD_SETSCHEDPARAM)
    auto locker = holdLock(m_mutex);

    int policy;
    struct sched_param param;

    if (pthread_getschedparam(m_handle, &policy, &param))
        return;

    param.sched_priority += delta;

    pthread_setschedparam(m_handle, policy, &param);
#endif
}

int Thread::waitForCompletion()
{
    pthread_t handle;
    {
        auto locker = holdLock(m_mutex);
        handle = m_handle;
    }

    int joinResult = pthread_join(handle, 0);

    if (joinResult == EDEADLK)
        LOG_ERROR("Thread %p was found to be deadlocked trying to quit", this);
    else if (joinResult)
        LOG_ERROR("Thread %p was unable to be joined.\n", this);

    auto locker = holdLock(m_mutex);
    ASSERT(joinableState() == Joinable);

    // If the thread has already exited, then do nothing. If the thread hasn't exited yet, then just signal that we've already joined on it.
    // In both cases, Thread::destructTLS() will take care of destroying Thread.
    if (!hasExited())
        didJoin();

    return joinResult;
}

void Thread::detach()
{
    auto locker = holdLock(m_mutex);
    int detachResult = pthread_detach(m_handle);
    if (detachResult)
        LOG_ERROR("Thread %p was unable to be detached\n", this);

    if (!hasExited())
        didBecomeDetached();
}

Thread& Thread::initializeCurrentTLS()
{
    // Not a WTF-created thread, Thread is not established yet.
    Ref<Thread> thread = adoptRef(*new Thread());
    thread->establishPlatformSpecificHandle(pthread_self());
    thread->initializeInThread();
    initializeCurrentThreadEvenIfNonWTFCreated();

    return initializeTLS(WTFMove(thread));
}

bool Thread::signal(int signalNumber)
{
    auto locker = holdLock(m_mutex);
    if (hasExited())
        return false;
    int errNo = pthread_kill(m_handle, signalNumber);
    return !errNo; // A 0 errNo means success.
}

auto Thread::suspend() -> Expected<void, PlatformSuspendError>
{
    RELEASE_ASSERT_WITH_MESSAGE(this != &Thread::current(), "We do not support suspending the current thread itself.");
    // During suspend, suspend or resume should not be executed from the other threads.
    // We use global lock instead of per thread lock.
    // Consider the following case, there are threads A and B.
    // And A attempt to suspend B and B attempt to suspend A.
    // A and B send signals. And later, signals are delivered to A and B.
    // In that case, both will be suspended.
    //
    // And it is important to use a global lock to suspend and resume. Let's consider using per-thread lock.
    // Your issuing thread (A) attempts to suspend the target thread (B). Then, you will suspend the thread (C) additionally.
    // This case frequently happens if you stop threads to perform stack scanning. But thread (B) may hold the lock of thread (C).
    // In that case, dead lock happens. Using global lock here avoids this dead lock.
    LockHolder locker(globalSuspendLock);
#if OS(DARWIN)
    kern_return_t result = thread_suspend(m_platformThread);
    if (result != KERN_SUCCESS)
        return makeUnexpected(result);
    return { };
#else
    if (!m_suspendCount) {
        // Ideally, we would like to use pthread_sigqueue. It allows us to pass the argument to the signal handler.
        // But it can be used in a few platforms, like Linux.
        // Instead, we use Thread* stored in a global variable to pass it to the signal handler.
        targetThread.store(this);

        while (true) {
            int result = pthread_kill(m_handle, SigThreadSuspendResume);
            if (result)
                return makeUnexpected(result);
            globalSemaphoreForSuspendResume->wait();
            if (m_platformRegisters)
                break;
            // Because of an alternative signal stack, we failed to suspend this thread.
            // Retry suspension again after yielding.
            Thread::yield();
        }
    }
    ++m_suspendCount;
    return { };
#endif
}

void Thread::resume()
{
    // During resume, suspend or resume should not be executed from the other threads.
    LockHolder locker(globalSuspendLock);
#if OS(DARWIN)
    thread_resume(m_platformThread);
#else
    if (m_suspendCount == 1) {
        // When allowing SigThreadSuspendResume interrupt in the signal handler by sigsuspend and SigThreadSuspendResume is actually issued,
        // the signal handler itself will be called once again.
        // There are several ways to distinguish the handler invocation for suspend and resume.
        // 1. Use different signal numbers. And check the signal number in the handler.
        // 2. Use some arguments to distinguish suspend and resume in the handler. If pthread_sigqueue can be used, we can take this.
        // 3. Use thread's flag.
        // In this implementaiton, we take (3). m_suspendCount is used to distinguish it.
        targetThread.store(this);
        if (pthread_kill(m_handle, SigThreadSuspendResume) == ESRCH)
            return;
        globalSemaphoreForSuspendResume->wait();
    }
    --m_suspendCount;
#endif
}

#if OS(DARWIN)
struct ThreadStateMetadata {
    unsigned userCount;
    thread_state_flavor_t flavor;
};

static ThreadStateMetadata threadStateMetadata()
{
#if CPU(X86)
    unsigned userCount = sizeof(PlatformRegisters) / sizeof(int);
    thread_state_flavor_t flavor = i386_THREAD_STATE;
#elif CPU(X86_64)
    unsigned userCount = x86_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
#elif CPU(PPC)
    unsigned userCount = PPC_THREAD_STATE_COUNT;
    thread_state_flavor_t flavor = PPC_THREAD_STATE;
#elif CPU(PPC64)
    unsigned userCount = PPC_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = PPC_THREAD_STATE64;
#elif CPU(ARM)
    unsigned userCount = ARM_THREAD_STATE_COUNT;
    thread_state_flavor_t flavor = ARM_THREAD_STATE;
#elif CPU(ARM64)
    unsigned userCount = ARM_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = ARM_THREAD_STATE64;
#else
#error Unknown Architecture
#endif
    return ThreadStateMetadata { userCount, flavor };
}
#endif // OS(DARWIN)

size_t Thread::getRegisters(PlatformRegisters& registers)
{
    LockHolder locker(globalSuspendLock);
#if OS(DARWIN)
    auto metadata = threadStateMetadata();
    kern_return_t result = thread_get_state(m_platformThread, metadata.flavor, (thread_state_t)&registers, &metadata.userCount);
    if (result != KERN_SUCCESS) {
        WTFReportFatalError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "JavaScript garbage collection failed because thread_get_state returned an error (%d). This is probably the result of running inside Rosetta, which is not supported.", result);
        CRASH();
    }
    return metadata.userCount * sizeof(uintptr_t);
#else
    ASSERT_WITH_MESSAGE(m_suspendCount, "We can get registers only if the thread is suspended.");
    ASSERT(m_platformRegisters);
    registers = *m_platformRegisters;
    return sizeof(PlatformRegisters);
#endif
}

void Thread::establishPlatformSpecificHandle(pthread_t handle)
{
    auto locker = holdLock(m_mutex);
    m_handle = handle;
#if OS(DARWIN)
    m_platformThread = pthread_mach_thread_np(handle);
#endif
}

#if !HAVE(FAST_TLS)
void Thread::initializeTLSKey()
{
    threadSpecificKeyCreate(&s_key, destructTLS);
}
#endif

Thread& Thread::initializeTLS(Ref<Thread>&& thread)
{
    // We leak the ref to keep the Thread alive while it is held in TLS. destructTLS will deref it later at thread destruction time.
    auto& threadInTLS = thread.leakRef();
#if !HAVE(FAST_TLS)
    ASSERT(s_key != InvalidThreadSpecificKey);
    threadSpecificSet(s_key, &threadInTLS);
#else
    _pthread_setspecific_direct(WTF_THREAD_DATA_KEY, &threadInTLS);
    pthread_key_init_np(WTF_THREAD_DATA_KEY, &destructTLS);
#endif
    return threadInTLS;
}

void Thread::destructTLS(void* data)
{
    Thread* thread = static_cast<Thread*>(data);
    ASSERT(thread);

    if (thread->m_isDestroyedOnce) {
        thread->didExit();
        thread->deref();
        return;
    }

    thread->m_isDestroyedOnce = true;
    // Re-setting the value for key causes another destructTLS() call after all other thread-specific destructors were called.
#if !HAVE(FAST_TLS)
    ASSERT(s_key != InvalidThreadSpecificKey);
    threadSpecificSet(s_key, thread);
#else
    _pthread_setspecific_direct(WTF_THREAD_DATA_KEY, thread);
    pthread_key_init_np(WTF_THREAD_DATA_KEY, &destructTLS);
#endif
}

Mutex::~Mutex()
{
    int result = pthread_mutex_destroy(&m_mutex);
    ASSERT_UNUSED(result, !result);
}

void Mutex::lock()
{
    int result = pthread_mutex_lock(&m_mutex);
    ASSERT_UNUSED(result, !result);
}

bool Mutex::tryLock()
{
    int result = pthread_mutex_trylock(&m_mutex);

    if (result == 0)
        return true;
    if (result == EBUSY)
        return false;

    ASSERT_NOT_REACHED();
    return false;
}

void Mutex::unlock()
{
    int result = pthread_mutex_unlock(&m_mutex);
    ASSERT_UNUSED(result, !result);
}

ThreadCondition::~ThreadCondition()
{
    pthread_cond_destroy(&m_condition);
}
    
void ThreadCondition::wait(Mutex& mutex)
{
    int result = pthread_cond_wait(&m_condition, &mutex.impl());
    ASSERT_UNUSED(result, !result);
}

bool ThreadCondition::timedWait(Mutex& mutex, WallTime absoluteTime)
{
    if (absoluteTime < WallTime::now())
        return false;

    if (absoluteTime > WallTime::fromRawSeconds(INT_MAX)) {
        wait(mutex);
        return true;
    }

    double rawSeconds = absoluteTime.secondsSinceEpoch().value();

    int timeSeconds = static_cast<int>(rawSeconds);
    int timeNanoseconds = static_cast<int>((rawSeconds - timeSeconds) * 1E9);

    timespec targetTime;
    targetTime.tv_sec = timeSeconds;
    targetTime.tv_nsec = timeNanoseconds;

    return pthread_cond_timedwait(&m_condition, &mutex.impl(), &targetTime) == 0;
}

void ThreadCondition::signal()
{
    int result = pthread_cond_signal(&m_condition);
    ASSERT_UNUSED(result, !result);
}

void ThreadCondition::broadcast()
{
    int result = pthread_cond_broadcast(&m_condition);
    ASSERT_UNUSED(result, !result);
}

void Thread::yield()
{
    sched_yield();
}

} // namespace WTF

#endif // USE(PTHREADS)
