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
#include "Threading.h"

#if USE(PTHREADS)

#include <errno.h>
#include <wtf/CurrentTime.h>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RawPointer.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadFunctionInvocation.h>
#include <wtf/ThreadHolder.h>
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

#endif

namespace WTF {

Thread::Thread()
{
#if !OS(DARWIN)
    sem_init(&m_semaphoreForSuspendResume, /* Only available in this process. */ 0, /* Initial value for the semaphore. */ 0);
#endif
}

Thread::~Thread()
{
#if !OS(DARWIN)
    sem_destroy(&m_semaphoreForSuspendResume);
#endif
}

#if !OS(DARWIN)

// We use SIGUSR1 to suspend and resume machine threads in JavaScriptCore.
static constexpr const int SigThreadSuspendResume = SIGUSR1;
static std::atomic<Thread*> targetThread { nullptr };
static StaticWordLock globalSuspendLock;

#if COMPILER(GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
#endif // COMPILER(GCC)

static UNUSED_FUNCTION NEVER_INLINE void* getApproximateStackPointer()
{
    volatile void* stackLocation = nullptr;
    return &stackLocation;
}

#if COMPILER(GCC)
#pragma GCC diagnostic pop
#endif // COMPILER(GCC)

static UNUSED_FUNCTION bool isOnAlternativeSignalStack()
{
    stack_t stack { };
    int ret = sigaltstack(nullptr, &stack);
    RELEASE_ASSERT(!ret);
    return stack.ss_flags == SS_ONSTACK;
}

void Thread::signalHandlerSuspendResume(int, siginfo_t*, void* ucontext)
{
    // Touching thread local atomic types from signal handlers is allowed.
    Thread* thread = targetThread.load();

    if (thread->m_suspended.load(std::memory_order_acquire)) {
        // This is signal handler invocation that is intended to be used to resume sigsuspend.
        // So this handler invocation itself should not process.
        //
        // When signal comes, first, the system calls signal handler. And later, sigsuspend will be resumed. Signal handler invocation always precedes.
        // So, the problem never happens that suspended.store(true, ...) will be executed before the handler is called.
        // http://pubs.opengroup.org/onlinepubs/009695399/functions/sigsuspend.html
        return;
    }

    ucontext_t* userContext = static_cast<ucontext_t*>(ucontext);
    ASSERT_WITH_MESSAGE(!isOnAlternativeSignalStack(), "Using an alternative signal stack is not supported. Consider disabling the concurrent GC.");

#if HAVE(MACHINE_CONTEXT)
#if CPU(PPC)
    thread->m_platformRegisters = PlatformRegisters { *userContext->uc_mcontext.uc_regs };
#else
    thread->m_platformRegisters = PlatformRegisters { userContext->uc_mcontext };
#endif
#else
    thread->m_platformRegisters = PlatformRegisters { getApproximateStackPointer() };
#endif

    // Allow suspend caller to see that this thread is suspended.
    // sem_post is async-signal-safe function. It means that we can call this from a signal handler.
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03
    //
    // And sem_post emits memory barrier that ensures that suspendedMachineContext is correctly saved.
    // http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_11
    sem_post(&thread->m_semaphoreForSuspendResume);

    // Reaching here, SigThreadSuspendResume is blocked in this handler (this is configured by sigaction's sa_mask).
    // So before calling sigsuspend, SigThreadSuspendResume to this thread is deferred. This ensures that the handler is not executed recursively.
    sigset_t blockedSignalSet;
    sigfillset(&blockedSignalSet);
    sigdelset(&blockedSignalSet, SigThreadSuspendResume);
    sigsuspend(&blockedSignalSet);

    // Allow resume caller to see that this thread is resumed.
    sem_post(&thread->m_semaphoreForSuspendResume);
}

#endif // !OS(DARWIN)

void Thread::initializePlatformThreading()
{
#if !OS(DARWIN)
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

static void initializeCurrentThreadEvenIfNonWTFCreated()
{
#if !OS(DARWIN)
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SigThreadSuspendResume);
    pthread_sigmask(SIG_UNBLOCK, &mask, 0);
#endif
}

static void* wtfThreadEntryPoint(void* param)
{
    // Balanced by .leakPtr() in Thread::createInternal.
    auto invocation = std::unique_ptr<ThreadFunctionInvocation>(static_cast<ThreadFunctionInvocation*>(param));

    ThreadHolder::initialize(*invocation->thread);
    invocation->thread = nullptr;

    invocation->function(invocation->data);
    return nullptr;
}

RefPtr<Thread> Thread::createInternal(ThreadFunction entryPoint, void* data, const char*)
{
    RefPtr<Thread> thread = adoptRef(new Thread());
    auto invocation = std::make_unique<ThreadFunctionInvocation>(entryPoint, thread.get(), data);
    pthread_t threadHandle;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
#if HAVE(QOS_CLASSES)
    pthread_attr_set_qos_class_np(&attr, adjustedQOSClass(QOS_CLASS_USER_INITIATED), 0);
#endif
    int error = pthread_create(&threadHandle, &attr, wtfThreadEntryPoint, invocation.get());
    pthread_attr_destroy(&attr);
    if (error) {
        LOG_ERROR("Failed to create pthread at entry point %p with data %p", wtfThreadEntryPoint, invocation.get());
        return nullptr;
    }

    // Balanced by std::unique_ptr constructor in wtfThreadEntryPoint.
    ThreadFunctionInvocation* leakedInvocation = invocation.release();
    UNUSED_PARAM(leakedInvocation);

    thread->establish(threadHandle);
    return thread;
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
    std::unique_lock<std::mutex> locker(m_mutex);

    int policy;
    struct sched_param param;

    if (pthread_getschedparam(m_handle, &policy, &param))
        return;

    param.sched_priority += delta;

    pthread_setschedparam(m_handle, policy, &param);
}

int Thread::waitForCompletion()
{
    pthread_t handle;
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        handle = m_handle;
    }

    int joinResult = pthread_join(handle, 0);

    if (joinResult == EDEADLK)
        LOG_ERROR("ThreadIdentifier %u was found to be deadlocked trying to quit", m_id);
    else if (joinResult)
        LOG_ERROR("ThreadIdentifier %u was unable to be joined.\n", m_id);

    std::unique_lock<std::mutex> locker(m_mutex);
    ASSERT(joinableState() == Joinable);

    // If the thread has already exited, then do nothing. If the thread hasn't exited yet, then just signal that we've already joined on it.
    // In both cases, ThreadHolder::destruct() will take care of destroying Thread.
    if (!hasExited())
        didJoin();

    return joinResult;
}

void Thread::detach()
{
    std::unique_lock<std::mutex> locker(m_mutex);
    int detachResult = pthread_detach(m_handle);
    if (detachResult)
        LOG_ERROR("ThreadIdentifier %u was unable to be detached\n", m_id);

    if (!hasExited())
        didBecomeDetached();
}

Thread* Thread::currentMayBeNull()
{
    ThreadHolder* data = ThreadHolder::current();
    if (data)
        return &data->thread();
    return nullptr;
}

Thread& Thread::current()
{
    if (Thread* current = currentMayBeNull())
        return *current;

    // Not a WTF-created thread, ThreadIdentifier is not established yet.
    RefPtr<Thread> thread = adoptRef(new Thread());
    thread->establish(pthread_self());
    ThreadHolder::initialize(*thread);
    initializeCurrentThreadEvenIfNonWTFCreated();
    return *thread;
}

ThreadIdentifier Thread::currentID()
{
    return current().id();
}

bool Thread::signal(int signalNumber)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    if (hasExited())
        return false;
    int errNo = pthread_kill(m_handle, signalNumber);
    return !errNo; // A 0 errNo means success.
}

auto Thread::suspend() -> Expected<void, PlatformSuspendError>
{
    RELEASE_ASSERT_WITH_MESSAGE(id() != currentThread(), "We do not support suspending the current thread itself.");
    std::unique_lock<std::mutex> locker(m_mutex);
#if OS(DARWIN)
    kern_return_t result = thread_suspend(m_platformThread);
    if (result != KERN_SUCCESS)
        return makeUnexpected(result);
    return { };
#else
    {
        // During suspend, suspend or resume should not be executed from the other threads.
        // We use global lock instead of per thread lock.
        // Consider the following case, there are threads A and B.
        // And A attempt to suspend B and B attempt to suspend A.
        // A and B send signals. And later, signals are delivered to A and B.
        // In that case, both will be suspended.
        WordLockHolder locker(globalSuspendLock);
        if (!m_suspendCount) {
            // Ideally, we would like to use pthread_sigqueue. It allows us to pass the argument to the signal handler.
            // But it can be used in a few platforms, like Linux.
            // Instead, we use Thread* stored in the thread local storage to pass it to the signal handler.
            targetThread.store(this);
            int result = pthread_kill(m_handle, SigThreadSuspendResume);
            if (result)
                return makeUnexpected(result);
            sem_wait(&m_semaphoreForSuspendResume);
            // Release barrier ensures that this operation is always executed after all the above processing is done.
            m_suspended.store(true, std::memory_order_release);
        }
        ++m_suspendCount;
        return { };
    }
#endif
}

void Thread::resume()
{
    std::unique_lock<std::mutex> locker(m_mutex);
#if OS(DARWIN)
    thread_resume(m_platformThread);
#else
    {
        // During resume, suspend or resume should not be executed from the other threads.
        WordLockHolder locker(globalSuspendLock);
        if (m_suspendCount == 1) {
            // When allowing SigThreadSuspendResume interrupt in the signal handler by sigsuspend and SigThreadSuspendResume is actually issued,
            // the signal handler itself will be called once again.
            // There are several ways to distinguish the handler invocation for suspend and resume.
            // 1. Use different signal numbers. And check the signal number in the handler.
            // 2. Use some arguments to distinguish suspend and resume in the handler. If pthread_sigqueue can be used, we can take this.
            // 3. Use thread local storage with atomic variables in the signal handler.
            // In this implementaiton, we take (3). suspended flag is used to distinguish it.
            targetThread.store(this);
            if (pthread_kill(m_handle, SigThreadSuspendResume) == ESRCH)
                return;
            sem_wait(&m_semaphoreForSuspendResume);
            // Release barrier ensures that this operation is always executed after all the above processing is done.
            m_suspended.store(false, std::memory_order_release);
        }
        --m_suspendCount;
    }
#endif
}

size_t Thread::getRegisters(PlatformRegisters& registers)
{
    std::unique_lock<std::mutex> locker(m_mutex);
#if OS(DARWIN)
#if CPU(X86)
    unsigned userCount = sizeof(registers) / sizeof(int);
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

    kern_return_t result = thread_get_state(m_platformThread, flavor, (thread_state_t)&registers, &userCount);
    if (result != KERN_SUCCESS) {
        WTFReportFatalError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "JavaScript garbage collection failed because thread_get_state returned an error (%d). This is probably the result of running inside Rosetta, which is not supported.", result);
        CRASH();
    }
    return userCount * sizeof(uintptr_t);
#else
    ASSERT_WITH_MESSAGE(m_suspendCount, "We can get registers only if the thread is suspended.");
    registers = m_platformRegisters;
    return sizeof(PlatformRegisters);
#endif
}

void Thread::establish(pthread_t handle)
{
    std::unique_lock<std::mutex> locker(m_mutex);
    m_handle = handle;
    if (!m_id) {
        static std::atomic<ThreadIdentifier> provider { 0 };
        m_id = ++provider;
#if OS(DARWIN)
        m_platformThread = pthread_mach_thread_np(handle);
#endif
    }
}

Mutex::Mutex()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);

    int result = pthread_mutex_init(&m_mutex, &attr);
    ASSERT_UNUSED(result, !result);

    pthread_mutexattr_destroy(&attr);
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

ThreadCondition::ThreadCondition()
{ 
    pthread_cond_init(&m_condition, NULL);
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

bool ThreadCondition::timedWait(Mutex& mutex, double absoluteTime)
{
    if (absoluteTime < currentTime())
        return false;

    if (absoluteTime > INT_MAX) {
        wait(mutex);
        return true;
    }

    int timeSeconds = static_cast<int>(absoluteTime);
    int timeNanoseconds = static_cast<int>((absoluteTime - timeSeconds) * 1E9);

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

} // namespace WTF

#endif // USE(PTHREADS)
