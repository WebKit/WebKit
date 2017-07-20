/*
 * Copyright (C) 2008, 2009, 2014 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <wtf/DateMath.h>
#include <wtf/PrintStream.h>
#include <wtf/RandomNumberSeed.h>
#include <wtf/ThreadGroup.h>
#include <wtf/ThreadHolder.h>
#include <wtf/ThreadMessage.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WTFThreadData.h>
#include <wtf/text/StringView.h>

#if HAVE(QOS_CLASSES)
#include <bmalloc/bmalloc.h>
#endif

namespace WTF {

enum class Stage {
    Start, EstablishedHandle, Initialized
};

struct Thread::NewThreadContext {
    const char* name;
    Function<void()> entryPoint;
    Stage stage;
    Mutex mutex;
    ThreadCondition condition;
    Thread& thread;
};

const char* Thread::normalizeThreadName(const char* threadName)
{
#if HAVE(PTHREAD_SETNAME_NP)
    return threadName;
#else
    // This name can be com.apple.WebKit.ProcessLauncher or com.apple.CoreIPC.ReceiveQueue.
    // We are using those names for the thread name, but both are longer than the limit of
    // the platform thread name length, 32 for Windows and 16 for Linux.
    StringView result(threadName);
    size_t size = result.reverseFind('.');
    if (size != notFound)
        result = result.substring(size + 1);

#if OS(WINDOWS)
    constexpr const size_t kVisualStudioThreadNameLimit = 32 - 1;
    if (result.length() > kVisualStudioThreadNameLimit)
        result = result.right(kVisualStudioThreadNameLimit);
#elif OS(LINUX)
    constexpr const size_t kLinuxThreadNameLimit = 16 - 1;
    if (result.length() > kLinuxThreadNameLimit)
        result = result.right(kLinuxThreadNameLimit);
#endif
    ASSERT(result.characters8()[result.length()] == '\0');
    return reinterpret_cast<const char*>(result.characters8());
#endif
}

void Thread::entryPoint(NewThreadContext* context)
{
    Function<void()> function;
    {
        // Block until our creating thread has completed any extra setup work, including establishing ThreadIdentifier.
        MutexLocker locker(context->mutex);
        ASSERT(context->stage == Stage::EstablishedHandle);

        // Initialize thread holder with established ID.
        ThreadHolder::initialize(context->thread);

        Thread::initializeCurrentThreadInternal(context->thread, context->name);
        function = WTFMove(context->entryPoint);

        // Ack completion of initialization to the creating thread.
        context->stage = Stage::Initialized;
        context->condition.signal();
    }
    function();
}

RefPtr<Thread> Thread::create(const char* name, Function<void()>&& entryPoint)
{
    Ref<Thread> thread = adoptRef(*new Thread());
    NewThreadContext context { name, WTFMove(entryPoint), Stage::Start, { }, { }, thread.get() };
    MutexLocker locker(context.mutex);
    if (!thread->establishHandle(&context))
        return nullptr;
    context.stage = Stage::EstablishedHandle;
    // After establishing Thread, release the mutex and wait for completion of initialization.
    while (context.stage != Stage::Initialized)
        context.condition.wait(context.mutex);
    return WTFMove(thread);
}

Thread* Thread::currentMayBeNull()
{
    ThreadHolder* data = ThreadHolder::current();
    if (data)
        return &data->thread();
    return nullptr;
}

void Thread::initialize()
{
    m_stack = StackBounds::currentThreadStackBounds();
}

static bool shouldRemoveThreadFromThreadGroup()
{
#if OS(WINDOWS)
    // On Windows the thread specific destructor is also called when the
    // main thread is exiting. This may lead to the main thread waiting
    // forever for the thread group lock when exiting, if the sampling
    // profiler thread was terminated by the system while holding the
    // thread group lock.
    if (WTF::isMainThread())
        return false;
#endif
    return true;
}

void Thread::didExit()
{
    if (shouldRemoveThreadFromThreadGroup()) {
        Vector<std::shared_ptr<ThreadGroup>> threadGroups;
        {
            std::lock_guard<std::mutex> locker(m_mutex);
            for (auto& threadGroup : m_threadGroups) {
                // If ThreadGroup is just being destroyed,
                // we do not need to perform unregistering.
                if (auto retained = threadGroup.lock())
                    threadGroups.append(WTFMove(retained));
            }
            m_isShuttingDown = true;
        }
        for (auto& threadGroup : threadGroups) {
            std::lock_guard<std::mutex> threadGroupLocker(threadGroup->getLock());
            std::lock_guard<std::mutex> locker(m_mutex);
            threadGroup->m_threads.remove(*this);
        }
    }
    // We would like to say "thread is exited" after unregistering threads from thread groups.
    // So we need to separate m_isShuttingDown from m_didExit.
    std::lock_guard<std::mutex> locker(m_mutex);
    m_didExit = true;
}

bool Thread::addToThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup& threadGroup)
{
    UNUSED_PARAM(threadGroupLocker);
    std::lock_guard<std::mutex> locker(m_mutex);
    if (m_isShuttingDown)
        return false;
    if (threadGroup.m_threads.add(*this).isNewEntry)
        m_threadGroups.append(threadGroup.weakFromThis());
    return true;
}

void Thread::removeFromThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup& threadGroup)
{
    UNUSED_PARAM(threadGroupLocker);
    std::lock_guard<std::mutex> locker(m_mutex);
    if (m_isShuttingDown)
        return;
    m_threadGroups.removeFirstMatching([&] (auto weakPtr) {
        if (auto sharedPtr = weakPtr.lock())
            return sharedPtr.get() == &threadGroup;
        return false;
    });
}

void Thread::setCurrentThreadIsUserInteractive(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(adjustedQOSClass(QOS_CLASS_USER_INTERACTIVE), relativePriority);
#else
    UNUSED_PARAM(relativePriority);
#endif
}

void Thread::setCurrentThreadIsUserInitiated(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(adjustedQOSClass(QOS_CLASS_USER_INITIATED), relativePriority);
#else
    UNUSED_PARAM(relativePriority);
#endif
}

#if HAVE(QOS_CLASSES)
static qos_class_t globalMaxQOSclass { QOS_CLASS_UNSPECIFIED };

void Thread::setGlobalMaxQOSClass(qos_class_t maxClass)
{
    bmalloc::api::setScavengerThreadQOSClass(maxClass);
    globalMaxQOSclass = maxClass;
}

qos_class_t Thread::adjustedQOSClass(qos_class_t originalClass)
{
    if (globalMaxQOSclass != QOS_CLASS_UNSPECIFIED)
        return std::min(originalClass, globalMaxQOSclass);
    return originalClass;
}
#endif

void Thread::dump(PrintStream& out) const
{
    out.print(m_id);
}

void initializeThreading()
{
    static std::once_flag initializeKey;
    std::call_once(initializeKey, [] {
        ThreadHolder::initializeOnce();
        initializeRandomNumberGenerator();
        wtfThreadData();
        initializeDates();
        Thread::initializePlatformThreading();
    });
}

} // namespace WTF
