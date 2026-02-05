/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
 * Copyright (C) 2025 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Compiler.h"
#include <wtf/RunLoop.h>

#include <glib.h>
#include <wtf/BubbleSort.h>
#include <wtf/MainThread.h>
#include <wtf/SafeStrerror.h>
#include <wtf/glib/ActivityObserver.h>
#include <wtf/glib/RunLoopSourcePriority.h>

#if HAVE(TIMERFD)
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <wtf/SystemTracing.h>
#endif

namespace WTF {

typedef struct {
    GSource source;
    RunLoop* runLoop;
#if HAVE(TIMERFD)
    int timerFd;
    struct itimerspec timerFdSpec;
#endif
} RunLoopSource;

GSourceFuncs RunLoop::s_runLoopSourceFunctions = {
    // prepare
#if HAVE(TIMERFD)
    [](GSource* source, int* timeout) -> gboolean {
        auto& runLoopSource = *reinterpret_cast<RunLoopSource*>(source);

        *timeout = -1;

        if (runLoopSource.timerFd > -1) {
            struct itimerspec timerFdSpec = { };
            int64_t readyTime = g_source_get_ready_time(source);

            if (readyTime > -1) {
                timerFdSpec.it_value.tv_sec = readyTime / G_USEC_PER_SEC;
                timerFdSpec.it_value.tv_nsec = (readyTime % G_USEC_PER_SEC) * 1000L;
            }

            if (timerFdSpec.it_interval.tv_sec != runLoopSource.timerFdSpec.it_interval.tv_sec
                || timerFdSpec.it_interval.tv_nsec != runLoopSource.timerFdSpec.it_interval.tv_nsec
                || timerFdSpec.it_value.tv_sec != runLoopSource.timerFdSpec.it_value.tv_sec
                || timerFdSpec.it_value.tv_nsec != runLoopSource.timerFdSpec.it_value.tv_nsec) {
                runLoopSource.timerFdSpec = timerFdSpec;
                timerfd_settime(runLoopSource.timerFd, TFD_TIMER_ABSTIME, &runLoopSource.timerFdSpec, nullptr);
            }
        }
        return FALSE;
    },
#else
    nullptr,
#endif
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        gint64 readyTime = g_source_get_ready_time(source);
        if (readyTime == -1)
            return G_SOURCE_CONTINUE;

#if HAVE(TIMERFD) && USE(SYSPROF_CAPTURE)
        static const bool shouldEnableSourceDispatchSignposts = ([]() -> bool {
            bool shouldEnableSignposts = false;
            if (const char* envString = getenv("WEBKIT_ENABLE_SOURCE_DISPATCH_SIGNPOSTS")) {
                auto envStringView = StringView::fromLatin1(envString);
                if (envStringView == "1"_s)
                    shouldEnableSignposts = true;
            }
            return shouldEnableSignposts;
        })();

        if (shouldEnableSourceDispatchSignposts && readyTime > 0) {
            gint64 lateness = g_get_monotonic_time() - readyTime;
            WTFEmitSignpost(source, RunLoopSourceDispatch, "[%s] lateness=%ldµs", g_source_get_name(source), lateness);
        }
#endif

        g_source_set_ready_time(source, -1);
        const char* name = g_source_get_name(source);
        auto& runLoopSource = *reinterpret_cast<RunLoopSource*>(source);
        runLoopSource.runLoop->notifyEvent(RunLoop::Event::WillDispatch, name);
        auto returnValue = callback(userData);
        runLoopSource.runLoop->notifyEvent(RunLoop::Event::DidDispatch, name);
        return returnValue;
    },
    // finalize
#if HAVE(TIMERFD)
    [](GSource* source) -> void {
        auto& runLoopSource = *reinterpret_cast<RunLoopSource*>(source);
        if (runLoopSource.timerFd > -1) {
            close(runLoopSource.timerFd);
            runLoopSource.timerFd = -1;
        }
    },
#else
    nullptr,
#endif
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

RunLoop::RunLoop()
{
    m_mainContext = g_main_context_get_thread_default();
    if (!m_mainContext)
        m_mainContext = isMainThread() ? g_main_context_default() : adoptGRef(g_main_context_new());
    ASSERT(m_mainContext);

    m_source = adoptGRef(g_source_new(&RunLoop::s_runLoopSourceFunctions, sizeof(RunLoopSource)));
    auto& runLoopSource = *reinterpret_cast<RunLoopSource*>(m_source.get());
    runLoopSource.runLoop = this;
#if HAVE(TIMERFD)
    runLoopSource.timerFd = -1;
#endif
    g_source_set_priority(m_source.get(), RunLoopSourcePriority::RunLoopDispatcher);
    g_source_set_name(m_source.get(), "[WebKit] RunLoop work");
    g_source_set_can_recurse(m_source.get(), TRUE);
    g_source_set_callback(m_source.get(), [](gpointer userData) -> gboolean {
        static_cast<RunLoop*>(userData)->performWork();
        return G_SOURCE_CONTINUE;
    }, this, nullptr);
    g_source_attach(m_source.get(), m_mainContext.get());
}

RunLoop::~RunLoop()
{
    g_source_destroy(m_source.get());
    m_shouldStop = true;
}

void RunLoop::runGLibMainLoopIteration(MayBlock mayBlock)
{
    gint maxPriority = 0;
    g_main_context_prepare(m_mainContext.get(), &maxPriority);

    m_pollFDs.resize(s_pollFDsCapacity);

    gint timeoutInMilliseconds = 0;
    gint numFDs = 0;
    while ((numFDs = g_main_context_query(m_mainContext.get(), maxPriority, &timeoutInMilliseconds, m_pollFDs.mutableSpan().data(), m_pollFDs.size())) > static_cast<int>(m_pollFDs.size()))
        m_pollFDs.grow(numFDs);

    if (mayBlock == MayBlock::No)
        timeoutInMilliseconds = 0;

    notifyActivity(Activity::BeforeWaiting);

    if (numFDs || timeoutInMilliseconds) {
        auto* pollFunction = g_main_context_get_poll_func(m_mainContext.get());
        auto result = (*pollFunction)(m_pollFDs.mutableSpan().data(), numFDs, timeoutInMilliseconds);
        if (result < 0 && errno != EINTR)
            LOG_ERROR("RunLoop::runGLibMainLoopIteration() - polling failed, ignoring. Error message: %s", safeStrerror(errno).data());
    }
    notifyActivity(Activity::AfterWaiting);

    g_main_context_check(m_mainContext.get(), maxPriority, m_pollFDs.mutableSpan().data(), numFDs);
    g_main_context_dispatch(m_mainContext.get());
}

void RunLoop::runGLibMainLoop()
{
    g_main_context_push_thread_default(m_mainContext.get());
    notifyActivity(Activity::Entry);

    while (!m_shouldStop)
        runGLibMainLoopIteration(MayBlock::Yes);

    notifyActivity(Activity::Exit);
    g_main_context_pop_thread_default(m_mainContext.get());
}

void RunLoop::run()
{
    Ref runLoop = RunLoop::currentSingleton();

    ++runLoop->m_nestedLoopLevel;
    runLoop->m_shouldStop = false;

    runLoop->runGLibMainLoop();

    --runLoop->m_nestedLoopLevel;
    if (runLoop->m_nestedLoopLevel > 0)
        runLoop->m_shouldStop = false;
}

void RunLoop::stop()
{
    m_shouldStop = true;
    wakeUp();
}

void RunLoop::wakeUp()
{
    g_source_set_ready_time(m_source.get(), 0);
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode)
{
    Ref runLoop = RunLoop::currentSingleton();
    runLoop->runGLibMainLoopIteration(MayBlock::No);
    return CycleResult::Continue;
}

void RunLoop::observeEvent(const RunLoop::EventObserver& observer)
{
    Locker locker { m_eventObserversLock };
    ASSERT(!m_eventObservers.contains(observer));
    m_eventObservers.add(observer);
}

void RunLoop::observeActivity(const Ref<ActivityObserver>& observer)
{
    {
        Locker locker { m_activityObserversLock };
        ASSERT(!m_activityObservers.contains(observer));
        m_activityObservers.append(observer);
        m_activities.add(observer->activities());

        if (m_activityObservers.size() > 1) {
            // We use bubble sort here because the input is always sorted already. See BubbleSort.h.
            WTF::bubbleSort(m_activityObservers.mutableSpan(), [](const auto& a, const auto& b) {
                return a->order() < b->order();
            });
        }
    }

    wakeUp();
}

void RunLoop::unobserveActivity(const Ref<ActivityObserver>& observer)
{
    Locker locker { m_activityObserversLock };
    ASSERT(m_activityObservers.contains(observer));
    m_activityObservers.removeFirst(observer);
    m_activities.remove(observer->activities());
}

void RunLoop::notifyActivity(Activity activity)
{
    // Lock the activity observers, collect the ones to be notified.
    ActivityObservers observersToBeNotified;
    {
        Locker locker { m_activityObserversLock };
        if (m_activityObservers.isEmpty())
            return;

        if (!m_activities.contains(activity))
            return;

        for (auto& observer : m_activityObservers) {
            if (observer->activities().contains(activity))
                observersToBeNotified.append(observer);
        }
    }

    // Notify the activity observers, without holding a lock - as mutations
    // to the activity observers are allowed.
    for (auto& observer : observersToBeNotified)
        observer->notify();
}

void RunLoop::notifyEvent(RunLoop::Event event, const char* name)
{
    Locker locker { m_eventObserversLock };
    if (m_eventObservers.isEmptyIgnoringNullReferences())
        return;

    m_eventObservers.forEach([event, name = String::fromUTF8(name)](auto& observer) {
        observer(event, name);
    });
}

RunLoop::TimerBase::TimerBase(Ref<RunLoop>&& runLoop, ASCIILiteral description)
    : m_runLoop(WTF::move(runLoop))
    , m_description(description)
    , m_source(adoptGRef(g_source_new(&RunLoop::s_runLoopSourceFunctions, sizeof(RunLoopSource))))
{
    auto& runLoopSource = *reinterpret_cast<RunLoopSource*>(m_source.get());
    runLoopSource.runLoop = m_runLoop.ptr();
#if HAVE(TIMERFD)
    runLoopSource.timerFd = -1;
#endif

    g_source_set_priority(m_source.get(), RunLoopSourcePriority::RunLoopTimer);
    g_source_set_name(m_source.get(), m_description);
    g_source_set_callback(m_source.get(), [](gpointer userData) -> gboolean {
        // fired() executes the user's callback. It may destroy timer,
        // so we must check if the source is still active afterwards
        // before it is safe to dereference timer again.
        RunLoop::TimerBase* timer = static_cast<RunLoop::TimerBase*>(userData);
        GSource* source = timer->m_source.get();
        if (timer->m_isRepeating)
            timer->updateReadyTime();
        timer->fired();
        if (g_source_is_destroyed(source))
            return G_SOURCE_REMOVE;
        return G_SOURCE_CONTINUE;
    }, this, nullptr);
    g_source_attach(m_source.get(), m_runLoop->m_mainContext.get());
}

RunLoop::TimerBase::~TimerBase()
{
    g_source_destroy(m_source.get());
}

void RunLoop::TimerBase::setPriority(int priority)
{
    g_source_set_priority(m_source.get(), priority);
}

void RunLoop::TimerBase::updateReadyTime()
{
    if (!m_interval) {
        g_source_set_ready_time(m_source.get(), 0);
        return;
    }

    gint64 currentTime = g_get_monotonic_time();
    gint64 targetTime = currentTime + std::min<gint64>(G_MAXINT64 - currentTime, m_interval.microsecondsAs<gint64>());
    ASSERT(targetTime >= currentTime);
    g_source_set_ready_time(m_source.get(), targetTime);
}

void RunLoop::TimerBase::start(Seconds interval, bool repeat)
{
#if HAVE(TIMERFD)
    // Create the timerfd here so that it's created as late as possible. Some
    // timers are created but may never be triggered.
    auto& runLoopSource = *reinterpret_cast<RunLoopSource*>(m_source.get());
    if (m_interval && runLoopSource.timerFd < 0) {
        runLoopSource.timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (runLoopSource.timerFd > -1) [[likely]]
            g_source_add_unix_fd(m_source.get(), runLoopSource.timerFd, G_IO_IN);
        else
            LOG_ERROR("Could not create timerfd: %s", safeStrerror(errno).data());
    }
#endif

    m_interval = interval;
    m_isRepeating = repeat;
    updateReadyTime();
}

void RunLoop::TimerBase::stop()
{
    g_source_set_ready_time(m_source.get(), -1);
    m_interval = { };
    m_isRepeating = false;
}

bool RunLoop::TimerBase::isActive() const
{
    return g_source_get_ready_time(m_source.get()) != -1;
}

Seconds RunLoop::TimerBase::secondsUntilFire() const
{
    gint64 time = g_source_get_ready_time(m_source.get());
    if (time != -1)
        return std::max<Seconds>(Seconds::fromMicroseconds(time - g_get_monotonic_time()), 0_s);
    return 0_s;
}

} // namespace WTF
