/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#include <cstring>
#include <wtf/DateMath.h>
#include <wtf/Gigacage.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PrintStream.h>
#include <wtf/ThreadGroup.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WTFConfig.h>
#include <wtf/text/AtomString.h>
#include <wtf/threads/Signals.h>

#if HAVE(QOS_CLASSES)
#include <bmalloc/bmalloc.h>
#endif

#if OS(LINUX)
#include <wtf/linux/RealTimeThreads.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/darwin/LibraryPathDiagnostics.h>
#endif

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/BPlatform.h>
#if BENABLE(LIBPAS)
#define USE_LIBPAS_THREAD_SUSPEND_LOCK 1
#include <bmalloc/pas_thread_suspend_lock.h>
#endif
#endif

namespace WTF {

Lock Thread::s_allThreadsLock;


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
#if USE(LIBPAS_THREAD_SUSPEND_LOCK)
ThreadSuspendLocker::ThreadSuspendLocker()
{
    pas_thread_suspend_lock_lock();
}

ThreadSuspendLocker::~ThreadSuspendLocker()
{
    pas_thread_suspend_lock_unlock();
}
#else
static Lock globalSuspendLock;

ThreadSuspendLocker::ThreadSuspendLocker()
{
    globalSuspendLock.lock();
}

ThreadSuspendLocker::~ThreadSuspendLocker()
{
    globalSuspendLock.unlock();
}
#endif

static std::optional<size_t> stackSize(ThreadType threadType)
{
    // Return the stack size for the created thread based on its type.
    // If the stack size is not specified, then use the system default. Platforms can tune the values here.
    // Enable STACK_STATS in StackStats.h to create a build that will track the information for tuning.
#if PLATFORM(PLAYSTATION)
    if (threadType == ThreadType::JavaScript)
        return 512 * KB;
#elif OS(DARWIN) && ASAN_ENABLED
    if (threadType == ThreadType::Compiler)
        return 1 * MB; // ASan needs more stack space (especially on Debug builds).
#else
    UNUSED_PARAM(threadType);
#endif

#if defined(DEFAULT_THREAD_STACK_SIZE_IN_KB) && DEFAULT_THREAD_STACK_SIZE_IN_KB > 0
    return DEFAULT_THREAD_STACK_SIZE_IN_KB * 1024;
#elif OS(LINUX) && !defined(__BIONIC__) && !defined(__GLIBC__)
    // on libcs other than glibc and bionic (e.g. musl) we are either unsure how big
    // the default thread stack is, or we know it's too small - pick a robust default
    return 1 * MB;
#else
    // Use the platform's default stack size
    return std::nullopt;
#endif
}

std::atomic<uint32_t> ThreadLike::s_uid;

uint32_t ThreadLike::currentSequence()
{
#if PLATFORM(COCOA)
    if (uint32_t uid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dispatch_get_specific(&s_uid))))
        return uid;
#endif
    return Thread::current().uid();
}

struct Thread::NewThreadContext : public ThreadSafeRefCounted<NewThreadContext> {
public:
    NewThreadContext(const char* name, Function<void()>&& entryPoint, Ref<Thread>&& thread)
        : name(name)
        , entryPoint(WTFMove(entryPoint))
        , thread(WTFMove(thread))
    {
    }

    enum class Stage { Start, EstablishedHandle, Initialized };
    Stage stage { Stage::Start };
    const char* name;
    Function<void()> entryPoint;
    Ref<Thread> thread;
    Mutex mutex;

#if !HAVE(STACK_BOUNDS_FOR_NEW_THREAD)
    ThreadCondition condition;
#endif
};

HashSet<Thread*>& Thread::allThreads()
{
    static LazyNeverDestroyed<HashSet<Thread*>> allThreads;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        allThreads.construct();
    });
    return allThreads;
}

Lock& Thread::allThreadsLock()
{
    return s_allThreadsLock;
}

const char* Thread::normalizeThreadName(const char* threadName)
{
#if HAVE(PTHREAD_SETNAME_NP)
    return threadName;
#else
    // This name can be com.apple.WebKit.ProcessLauncher or com.apple.CoreIPC.ReceiveQueue.
    // We are using those names for the thread name, but both are longer than the limit of
    // the platform thread name length, 32 for Windows and 16 for Linux.
    auto result = StringView::fromLatin1(threadName);
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

    m_currentAtomStringTable = &m_defaultAtomStringTable;
#if USE(WEB_THREAD)
    // On iOS, one AtomStringTable is shared between the main UI thread and the WebThread.
    if (isWebThread() || isUIThread()) {
        static LazyNeverDestroyed<AtomStringTable> sharedStringTable;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            sharedStringTable.construct();
        });
        m_currentAtomStringTable = &sharedStringTable.get();
    }
#endif

#if OS(LINUX)
    m_id = currentID();
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

Ref<Thread> Thread::create(const char* name, Function<void()>&& entryPoint, ThreadType threadType, QOS qos)
{
    WTF::initialize();
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
        bool success = thread->establishHandle(context.ptr(), stackSize(threadType), qos);
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

    // We must register threads here since threads registered in allThreads are expected to have complete thread data which can be initialized in launched thread side.
    // However, it is also possible that the launched thread has finished its execution before it is registered in allThreads here! In this case, the thread has already
    // called Thread::didExit to unregister itself from allThreads. Registering such a thread will register a stale thread pointer to allThreads, which will not be removed
    // even after Thread is destroyed. Register a thread only when it has not unregistered itself from allThreads yet.
    {
        Locker locker { allThreadsLock() };
        if (!thread->m_didUnregisterFromAllThreads)
            allThreads().add(thread.ptr());
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
    {
        Locker locker { allThreadsLock() };
        allThreads().remove(this);
        m_didUnregisterFromAllThreads = true;
    }

    if (shouldRemoveThreadFromThreadGroup()) {
        {
            Vector<Ref<ThreadGroup>> threadGroups;
            {
                Locker locker { m_mutex };
                threadGroups = m_threadGroups.values();
                m_isShuttingDown = true;
            }
            for (auto& threadGroup : threadGroups) {
                Locker threadGroupLocker { threadGroup->getLock() };
                Locker locker { m_mutex };
                threadGroup->m_threads.remove(*this);
            }
        }

        // We would like to say "thread is exited" after unregistering threads from thread groups.
        // So we need to separate m_isShuttingDown from m_didExit.
        Locker locker { m_mutex };
        m_didExit = true;
    }
}

ThreadGroupAddResult Thread::addToThreadGroup(const AbstractLocker& threadGroupLocker, ThreadGroup& threadGroup)
{
    UNUSED_PARAM(threadGroupLocker);
    Locker locker { m_mutex };
    if (m_isShuttingDown)
        return ThreadGroupAddResult::NotAdded;
    if (threadGroup.m_threads.add(*this).isNewEntry) {
        m_threadGroups.add(threadGroup);
        return ThreadGroupAddResult::NewlyAdded;
    }
    return ThreadGroupAddResult::AlreadyAdded;
}

unsigned Thread::numberOfThreadGroups()
{
    Locker locker { m_mutex };
    return m_threadGroups.values().size();
}

bool Thread::exchangeIsCompilationThread(bool newValue)
{
    auto& thread = Thread::current();
    bool oldValue = thread.m_isCompilationThread;
    thread.m_isCompilationThread = newValue;
    return oldValue;
}

void Thread::registerGCThread(GCThreadType gcThreadType)
{
    Thread::current().m_gcThreadType = static_cast<unsigned>(gcThreadType);
}

bool Thread::mayBeGCThread()
{
    return Thread::current().gcThreadType() != GCThreadType::None;
}

void Thread::registerJSThread(Thread& thread)
{
    ASSERT(&thread == &Thread::current());
    thread.m_isJSThread = true;
}

void Thread::setCurrentThreadIsUserInteractive(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(adjustedQOSClass(QOS_CLASS_USER_INTERACTIVE), relativePriority);
#elif OS(LINUX)
    // We don't allow to make the main thread real time. This is used by secondary processes to match the
    // UI process, but in linux the UI process is not real time.
    if (!isMainThread())
        RealTimeThreads::singleton().registerThread(current());
    UNUSED_PARAM(relativePriority);
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
static Thread::QOS toQOS(qos_class_t qosClass)
{
    switch (qosClass) {
    case QOS_CLASS_USER_INTERACTIVE:
        return Thread::QOS::UserInteractive;
    case QOS_CLASS_USER_INITIATED:
        return Thread::QOS::UserInitiated;
    case QOS_CLASS_UTILITY:
        return Thread::QOS::Utility;
    case QOS_CLASS_BACKGROUND:
        return Thread::QOS::Background;
    case QOS_CLASS_UNSPECIFIED:
    case QOS_CLASS_DEFAULT:
    default:
        return Thread::QOS::Default;
    }
}
#endif

auto Thread::currentThreadQOS() -> QOS
{
#if HAVE(QOS_CLASSES)
    qos_class_t qos = QOS_CLASS_DEFAULT;
    int relativePriority;
    pthread_get_qos_class_np(pthread_self(), &qos, &relativePriority);
    return toQOS(qos);
#else
    return QOS::Default;
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

#if !HAVE(FAST_TLS) && !OS(WINDOWS)
ThreadSpecificKey Thread::s_key = InvalidThreadSpecificKey;
#endif

void initialize()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        setPermissionsOfConfigPage();
        Gigacage::ensureGigacage();
        Config::AssertNotFrozenScope assertScope;
#if !HAVE(FAST_TLS) && !OS(WINDOWS)
        Thread::initializeTLSKey();
#endif
        initializeDates();
        Thread::initializePlatformThreading();
#if USE(PTHREADS) && HAVE(MACHINE_CONTEXT)
        SignalHandlers::initialize();
#endif
#if PLATFORM(COCOA)
        initializeLibraryPathDiagnostics();
#endif
    });
}

// This is a compatibility hack to prevent linkage errors when launching older
// versions of Safari. initialize() used to be named initializeThreading(), and
// Safari.framework used to call it directly from NotificationAgentMain.
WTF_EXPORT_PRIVATE void initializeThreading();
void initializeThreading()
{
    initialize();
}

} // namespace WTF
