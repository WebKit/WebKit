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

#include <bmalloc/ThreadSuspend.h>
#include <errno.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WTFConfig.h>

#if OS(HAIKU)
#include <OS.h>
#endif

#if OS(LINUX)
#include <sched.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <wtf/linux/RealTimeThreads.h>
#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK 0x40000000
#endif
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

#if OS(DARWIN)
#include <mach/mach_traps.h>
#include <mach/thread_switch.h>
#endif

#if OS(QNX)
#define SA_RESTART 0
#endif

namespace WTF {

Thread::~Thread() = default;

void Thread::initializePlatformThreading()
{
    if (!g_wtfConfig.isUserSpecifiedThreadSuspendResumeSignalConfigured) {
        g_wtfConfig.sigThreadSuspendResume = SIGUSR1;
        if (const char* string = getenv("JSC_SIGNAL_FOR_GC")) {
            int32_t value = 0;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            if (sscanf(string, "%d", &value) == 1)
                g_wtfConfig.sigThreadSuspendResume = value;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        }
    }
    g_wtfConfig.isThreadSuspendResumeSignalConfigured = true;
#if !OS(DARWIN)
    bmalloc::api::setThreadSuspendSignal(g_wtfConfig.sigThreadSuspendResume);
#endif // !OS(DARWIN)
    bmalloc::api::threadSuspendSignalHandlerInstall();
#if !OS(DARWIN)
    ASSERT(g_wtfConfig.sigThreadSuspendResume == bmalloc::api::threadSuspendSignalNumber());
#endif
}

#if OS(LINUX)
ThreadIdentifier Thread::currentID()
{
    return static_cast<ThreadIdentifier>(syscall(SYS_gettid));
}
#endif

void Thread::initializeCurrentThreadEvenIfNonWTFCreated()
{
#if !OS(DARWIN)
    RELEASE_ASSERT(g_wtfConfig.isThreadSuspendResumeSignalConfigured);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, g_wtfConfig.sigThreadSuspendResume);
    pthread_sigmask(SIG_UNBLOCK, &mask, 0);
#endif
}

static void* wtfThreadEntryPoint(void* context)
{
    Thread::entryPoint(reinterpret_cast<Thread::NewThreadContext*>(context));
    return nullptr;
}

#if HAVE(QOS_CLASSES)
dispatch_qos_class_t Thread::dispatchQOSClass(QOS qos)
{
    switch (qos) {
    case QOS::UserInteractive:
        return adjustedQOSClass(QOS_CLASS_USER_INTERACTIVE);
    case QOS::UserInitiated:
        return adjustedQOSClass(QOS_CLASS_USER_INITIATED);
    case QOS::Default:
        return adjustedQOSClass(QOS_CLASS_DEFAULT);
    case QOS::Utility:
        return adjustedQOSClass(QOS_CLASS_UTILITY);
    case QOS::Background:
        return adjustedQOSClass(QOS_CLASS_BACKGROUND);
    }
}
#endif

#if HAVE(SCHEDULING_POLICIES) || OS(LINUX)
static int NODELETE schedPolicy(Thread::SchedulingPolicy schedulingPolicy)
{
    switch (schedulingPolicy) {
    case Thread::SchedulingPolicy::FIFO:
        return SCHED_FIFO;
    case Thread::SchedulingPolicy::Realtime:
        return SCHED_RR;
    case Thread::SchedulingPolicy::Other:
        return SCHED_OTHER;
    }
    ASSERT_NOT_REACHED();
    return SCHED_OTHER;
}
#endif

#if OS(LINUX)
static int schedPolicy(Thread::QOS qos, Thread::SchedulingPolicy schedulingPolicy)
{
    // A specific scheduling policy can override the implied policy from QOS
    auto policy = schedPolicy(schedulingPolicy);
    if (policy != SCHED_OTHER)
        return policy;

    switch (qos) {
    case Thread::QOS::UserInteractive:
        return SCHED_RR;
    case Thread::QOS::UserInitiated:
    case Thread::QOS::Default:
        return SCHED_OTHER;
    case Thread::QOS::Utility:
        return SCHED_BATCH;
    case Thread::QOS::Background:
        return SCHED_IDLE;
    }
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

bool Thread::establishHandle(NewThreadContext& context, StackAllocationSpecification stackSpec, QOS qos, SchedulingPolicy schedulingPolicy)
{
    pthread_t threadHandle;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
#if HAVE(QOS_CLASSES)
    pthread_attr_set_qos_class_np(&attr, dispatchQOSClass(qos), 0);
#endif
#if HAVE(SCHEDULING_POLICIES)
    pthread_attr_setschedpolicy(&attr, schedPolicy(schedulingPolicy));
#endif

    switch (stackSpec.kind()) {
    case StackAllocationSpecification::Kind::Default:
        break;
    case StackAllocationSpecification::Kind::SizeOnly:
        pthread_attr_setstacksize(&attr, stackSpec.sizeBytes());
        break;
    case StackAllocationSpecification::Kind::SizeAndLocation: {
        auto bounds = stackSpec.stackSpan();
        int result = pthread_attr_setstack(&attr, bounds.data(), bounds.size_bytes());
        if (result) {
            LOG_ERROR("Failed to set custom stack at %p size %zu: %s",
                bounds.data(), bounds.size_bytes(), safeStrerror(result).data());
            pthread_attr_destroy(&attr);
            return false;
        } } break;
    }

    int error = pthread_create(&threadHandle, &attr, wtfThreadEntryPoint, &context);
    pthread_attr_destroy(&attr);
    if (error) {
        LOG_ERROR("Failed to create pthread at entry point %p with context %p", wtfThreadEntryPoint, &context);
        return false;
    }

#if OS(LINUX)
    int policy = schedPolicy(qos, schedulingPolicy);
    if (policy == SCHED_RR)
        RealTimeThreads::singleton().registerThread(*this);
    else {
        struct sched_param param = { };
        error = pthread_setschedparam(threadHandle, policy | SCHED_RESET_ON_FORK, &param);
        if (error)
            LOG_ERROR("Failed to set sched policy %d for thread %ld: %s", policy, threadHandle, safeStrerror(error).data());
    }
#else
#if !HAVE(QOS_CLASSES)
    UNUSED_PARAM(qos);
#endif
#if !HAVE(SCHEDULING_POLICIES)
    UNUSED_PARAM(schedulingPolicy);
#endif
#endif

    establishPlatformSpecificHandle(threadHandle);
    return true;
}

void Thread::initializeCurrentThreadInternal(const char* threadName)
{
#if HAVE(PTHREAD_SETNAME_NP)
    pthread_setname_np(normalizeThreadName(threadName));
#elif OS(HAIKU)
    rename_thread(find_thread(nullptr), normalizeThreadName(threadName));
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
    Locker locker { m_mutex };

    int policy;
    struct sched_param param;

    if (pthread_getschedparam(m_handle, &policy, &param))
        return;

    param.sched_priority += delta;

    pthread_setschedparam(m_handle, policy, &param);
#endif
}

#if HAVE(THREAD_TIME_CONSTRAINTS)
void Thread::setThreadTimeConstraints(MonotonicTime period, MonotonicTime nominalComputation, MonotonicTime constraint, bool isPreemptable)
{
#if OS(DARWIN)
    thread_time_constraint_policy policy { };
    policy.period = period.toMachAbsoluteTime();
    policy.computation = nominalComputation.toMachAbsoluteTime();
    policy.constraint = constraint.toMachAbsoluteTime();
    policy.preemptible = isPreemptable;
    if (auto error = thread_policy_set(machThread(), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT)) {
        UNUSED_VARIABLE(error);
        LOG_ERROR("Thread %p failed to set time constraints with error %d", this, error);
    }
#else
    ASSERT_NOT_REACHED();
#endif
}
#endif

int Thread::waitForCompletion()
{
    pthread_t handle;
    {
        Locker locker { m_mutex };
        handle = m_handle;
    }

    int joinResult = pthread_join(handle, 0);

    if (joinResult == EDEADLK)
        LOG_ERROR("Thread %p was found to be deadlocked trying to quit", this);
    else if (joinResult)
        LOG_ERROR("Thread %p was unable to be joined.\n", this);

    Locker locker { m_mutex };
    ASSERT(joinableState() == Joinable);

    // If the thread has already exited, then do nothing. If the thread hasn't exited yet, then just signal that we've already joined on it.
    // In both cases, Thread::destructTLS() will take care of destroying Thread.
    if (!hasExited())
        didJoin();

    return joinResult;
}

void Thread::detach()
{
    Locker locker { m_mutex };
    int detachResult = pthread_detach(m_handle);
    if (detachResult)
        LOG_ERROR("Thread %p was unable to be detached\n", this);

    if (!hasExited())
        didBecomeDetached();
}

Thread& Thread::initializeCurrentTLS()
{
    // Not a WTF-created thread, Thread is not established yet.
    WTF::initialize();
#if PLATFORM(COCOA)
    Ref thread = adoptRef(*new Thread(SchedulingPolicy::Other, pthread_main_np() ? IsMain::Yes : IsMain::No));
#else
    Ref thread = adoptRef(*new Thread(SchedulingPolicy::Other));
#endif
    thread->establishPlatformSpecificHandle(pthread_self());
    thread->initializeInThread();
    initializeCurrentThreadEvenIfNonWTFCreated();

    return initializeTLS(WTF::move(thread));
}

bool Thread::signal(int signalNumber)
{
    Locker locker { m_mutex };
    if (hasExited())
        return false;
    int errNo = pthread_kill(m_handle, signalNumber);
    return !errNo; // A 0 errNo means success.
}

auto Thread::suspend(const ThreadSuspendLocker&) -> Expected<void, PlatformSuspendError>
{
    RELEASE_ASSERT_WITH_MESSAGE(this != &Thread::currentSingleton(), "We do not support suspending the current thread itself.");
#if OS(DARWIN)
    if (!bmalloc::api::suspendThread(m_handle))
        return makeUnexpected(-1);
#else
    if (!m_suspendCount) {
        ASSERT(!m_suspendData);
        m_suspendData.emplace(m_handle, m_stack.origin(), m_stack.end());
        if (!bmalloc::api::suspendThread(*m_suspendData)) {
            m_suspendData.reset();
            return makeUnexpected(-1);
        }
    }
    ++m_suspendCount;
#endif
    return { };
}

void Thread::resume(const ThreadSuspendLocker&)
{
#if OS(DARWIN)
    bmalloc::api::resumeThread(m_handle);
    ++m_suspendGeneration;
#else
    ASSERT(m_suspendCount);
    --m_suspendCount;
    if (!m_suspendCount) {
        ASSERT(m_suspendData);
        bmalloc::api::resumeThread(*m_suspendData);
        m_suspendData.reset();
        ++m_suspendGeneration;
    }
#endif // !OS(DARWIN)
}

#if OS(DARWIN)
struct ThreadStateMetadata {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ThreadStateMetadata);
    unsigned userCount;
    thread_state_flavor_t flavor;
};

static ThreadStateMetadata NODELETE threadStateMetadata()
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

SuspendedThreadRegisters Thread::getRegisters(const ThreadSuspendLocker&, PlatformRegisters& scratch)
{
#if OS(DARWIN)
    auto metadata = threadStateMetadata();
    kern_return_t result = thread_get_state(m_platformThread, metadata.flavor, (thread_state_t)&scratch, &metadata.userCount);
    if (result != KERN_SUCCESS) {
        WTFReportFatalError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, "JavaScript garbage collection failed because thread_get_state returned an error (%d). This is probably the result of running inside Rosetta, which is not supported.", result);
        CRASH_WITH_INFO(result, KERN_SUCCESS);
    }
    return SuspendedThreadRegisters { scratch, metadata.userCount * sizeof(uintptr_t), m_suspendGeneration };
#else
    ASSERT_WITH_MESSAGE(m_suspendCount, "We can get registers only if the thread is suspended.");
    static_assert(sizeof(PlatformRegisters) == sizeof(pas_machine_registers));
    PlatformRegisters& registers = *std::bit_cast<PlatformRegisters*>(
        m_suspendData->getRegisters(std::bit_cast<pas_machine_registers*>(&scratch)));
    return SuspendedThreadRegisters { registers, sizeof(PlatformRegisters), m_suspendGeneration };
#endif
}

void Thread::establishPlatformSpecificHandle(pthread_t handle)
{
    Locker locker { m_mutex };
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
    SUPPRESS_UNCOUNTED_LOCAL auto& threadInTLS = thread.leakRef();
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
    // Destructor of ClientData can rely on Thread::currentSingleton() (e.g. AtomStringTable).
    // We destroy it after re-setting Thread::currentSingleton() so that we can ensure destruction
    // can still access to it.
    thread->m_clientData = nullptr;
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
    if (absoluteTime.isInfinity()) {
        if (absoluteTime == -WallTime::infinity())
            return false;
        wait(mutex);
        return true;
    }

    if (absoluteTime < WallTime::now())
        return false;

    if (absoluteTime > WallTime::fromRawSeconds(static_cast<double>(std::numeric_limits<time_t>::max()))) {
        wait(mutex);
        return true;
    }

    double rawSeconds = absoluteTime.secondsSinceEpoch().value();

    time_t timeSeconds = static_cast<time_t>(rawSeconds);
    long timeNanoseconds = static_cast<long>((rawSeconds - timeSeconds) * 1E9);

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
#if OS(DARWIN)
    constexpr mach_msg_timeout_t timeoutInMS = 1;
    thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, timeoutInMS);
#else
    sched_yield();
#endif
}

} // namespace WTF

#endif // USE(PTHREADS)
