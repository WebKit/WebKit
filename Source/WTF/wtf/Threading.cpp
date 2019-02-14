/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#include <wtf/Threading.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <wtf/DateMath.h>
#include <wtf/PrintStream.h>
#include <wtf/RandomNumberSeed.h>
#include <wtf/ThreadGroup.h>
#include <wtf/ThreadMessage.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/text/AtomicStringTable.h>
#include <wtf/text/StringView.h>

#if HAVE(QOS_CLASSES)
#include <bmalloc/bmalloc.h>
#endif

namespace WTF {

struct Thread::NewThreadContext : public ThreadSafeRefCounted<NewThreadContext> {
public:
    NewThreadContext(const char* name, Function<void()>&& entryPoint, Ref<Thread>&& thread)
        : name(name)
        , entryPoint(WTFMove(entryPoint))
        , thread(WTFMove(thread))
    {
    }

    const char* name;
    Function<void()> entryPoint;
    Ref<Thread> thread;
    Mutex mutex;
    enum class Stage { Start, EstablishedHandle, Initialized };
    Stage stage { Stage::Start };

#if !HAVE(STACK_BOUNDS_FOR_NEW_THREAD)
    ThreadCondition condition;
#endif
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

void Thread::initializeInThread()
{
    if (m_stack.isEmpty())
        m_stack = StackBounds::currentThreadStackBounds();
    m_savedLastStackTop = stack().origin();

    m_currentAtomicStringTable = &m_defaultAtomicStringTable;
#if USE(WEB_THREAD)
    // On iOS, one AtomicStringTable is shared between the main UI thread and the WebThread.
    if (isWebThread() || isUIThread()) {
        static NeverDestroyed<AtomicStringTable> sharedStringTable;
        m_currentAtomicStringTable = &sharedStringTable.get();
    }
#endif
}

void Thread::entryPoint(NewThreadContext* newThreadContext)
{
    Function<void()> function;
    {
        // Ref is already incremented by Thread::create.
        Ref<NewThreadContext> context = adoptRef(*newThreadContext);
        // Block until our creating thread has completed any extra setup work, including establishing ThreadIdentifier.
        MutexLocker locker(context->mutex);
        ASSERT(context->stage == NewThreadContext::Stage::EstablishedHandle);

        Thread::initializeCurrentThreadInternal(context->name);
        function = WTFMove(context->entryPoint);
        context->thread->initializeInThread();

        Thread::initializeTLS(WTFMove(context->thread));

#if !HAVE(STACK_BOUNDS_FOR_NEW_THREAD)
        // Ack completion of initialization to the creating thread.
        context->stage = NewThreadContext::Stage::Initialized;
        context->condition.signal();
#endif
    }

    ASSERT(!Thread::current().stack().isEmpty());
    function();
}

Ref<Thread> Thread::create(const char* name, Function<void()>&& entryPoint)
{
    WTF::initializeThreading();
    Ref<Thread> thread = adoptRef(*new Thread());
    Ref<NewThreadContext> context = adoptRef(*new NewThreadContext { name, WTFMove(entryPoint), thread.copyRef() });
    // Increment the context ref on behalf of the created thread. We do not just use a unique_ptr and leak it to the created thread because both the creator and created thread has a need to keep the context alive:
    // 1. the created thread needs to keep it alive because Thread::create() can exit before the created thread has a chance to use the context.
    // 2. the creator thread (if HAVE(STACK_BOUNDS_FOR_NEW_THREAD) is false) needs to keep it alive because the created thread may exit before the creator has a chance to wake up from waiting for the completion of the created thread's initialization. This waiting uses a condition variable in the context.
    // Hence, a joint ownership model is needed if HAVE(STACK_BOUNDS_FOR_NEW_THREAD) is false. To simplify the code, we just go with joint ownership by both the creator and created threads,
    // and make the context ThreadSafeRefCounted.
    context->ref();
    {
        MutexLocker locker(context->mutex);
        bool success = thread->establishHandle(context.ptr());
        RELEASE_ASSERT(success);
        context->stage = NewThreadContext::Stage::EstablishedHandle;

#if HAVE(STACK_BOUNDS_FOR_NEW_THREAD)
        thread->m_stack = StackBounds::newThreadStackBounds(thread->m_handle);
#else
        // In platforms which do not support StackBounds::newThreadStackBounds(), we do not have a way to get stack
        // bounds outside the target thread itself. Thus, we need to initialize thread information in the target thread
        // and wait for completion of initialization in the caller side.
        while (context->stage != NewThreadContext::Stage::Initialized)
            context->condition.wait(context->mutex);
#endif
    }

    ASSERT(!thread->stack().isEmpty());
    return thread;
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
        {
            Vector<std::shared_ptr<ThreadGroup>> threadGroups;
            {
                auto locker = holdLock(m_mutex);
                for (auto& threadGroup : m_threadGroups) {
                    // If ThreadGroup is just being destroyed,
                    // we do not need to perform unregistering.
                    if (auto retained = threadGroup.lock())
                        threadGroups.append(WTFMove(retained));
                }
                m_isShuttingDown = true;
            }
            for (auto& threadGroup : threadGroups) {
                auto threadGroupLocker = holdLock(threadGroup->getLock());
                auto locker = holdLock(m_mutex);
                threadGroup->m_threads.remove(*this);
            }
        }

        // We would like to say "thread is exited" after unregistering threads from thread groups.
        // So we need to separate m_isShuttingDown from m_didExit.
        auto locker = holdLock(m_mutex);
        m_didExit = true;
    }
}

ThreadGroupAddResult Thread::addToThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup& threadGroup)
{
    UNUSED_PARAM(threadGroupLocker);
    auto locker = holdLock(m_mutex);
    if (m_isShuttingDown)
        return ThreadGroupAddResult::NotAdded;
    if (threadGroup.m_threads.add(*this).isNewEntry) {
        m_threadGroups.append(threadGroup.weakFromThis());
        return ThreadGroupAddResult::NewlyAdded;
    }
    return ThreadGroupAddResult::AlreadyAdded;
}

void Thread::removeFromThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup& threadGroup)
{
    UNUSED_PARAM(threadGroupLocker);
    auto locker = holdLock(m_mutex);
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
    out.print("Thread:", RawPointer(this));
}

#if !HAVE(FAST_TLS)
ThreadSpecificKey Thread::s_key = InvalidThreadSpecificKey;
#endif

static bool initialized = false;

bool threadingIsInitialized()
{
    return initialized;
}

void initializeThreading()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        initialized = true;
        initializeRandomNumberGenerator();
#if !HAVE(FAST_TLS)
        Thread::initializeTLSKey();
#endif
        initializeDates();
        Thread::initializePlatformThreading();
    });
}

} // namespace WTF
