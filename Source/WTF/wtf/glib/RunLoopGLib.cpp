/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include <wtf/RunLoop.h>

#include <glib.h>
#include <wtf/MainThread.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WTF {

static GSourceFuncs runLoopSourceFunctions = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        if (g_source_get_ready_time(source) == -1)
            return G_SOURCE_CONTINUE;
        g_source_set_ready_time(source, -1);
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

RunLoop::RunLoop()
{
    m_mainContext = g_main_context_get_thread_default();
    if (!m_mainContext)
        m_mainContext = isMainThread() ? g_main_context_default() : adoptGRef(g_main_context_new());
    ASSERT(m_mainContext);

    GRefPtr<GMainLoop> innermostLoop = adoptGRef(g_main_loop_new(m_mainContext.get(), FALSE));
    ASSERT(innermostLoop);
    m_mainLoops.append(innermostLoop);

    m_source = adoptGRef(g_source_new(&runLoopSourceFunctions, sizeof(GSource)));
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

    for (int i = m_mainLoops.size() - 1; i >= 0; --i) {
        if (!g_main_loop_is_running(m_mainLoops[i].get()))
            continue;
        g_main_loop_quit(m_mainLoops[i].get());
    }
}

void RunLoop::run()
{
    RunLoop& runLoop = RunLoop::current();
    GMainContext* mainContext = runLoop.m_mainContext.get();

    // The innermost main loop should always be there.
    ASSERT(!runLoop.m_mainLoops.isEmpty());

    GMainLoop* innermostLoop = runLoop.m_mainLoops[0].get();
    if (!g_main_loop_is_running(innermostLoop)) {
        g_main_context_push_thread_default(mainContext);
        g_main_loop_run(innermostLoop);
        g_main_context_pop_thread_default(mainContext);
        return;
    }

    // Create and run a nested loop if the innermost one was already running.
    GMainLoop* nestedMainLoop = g_main_loop_new(mainContext, FALSE);
    runLoop.m_mainLoops.append(adoptGRef(nestedMainLoop));

    g_main_context_push_thread_default(mainContext);
    g_main_loop_run(nestedMainLoop);
    g_main_context_pop_thread_default(mainContext);

    runLoop.m_mainLoops.removeLast();
}

void RunLoop::stop()
{
    // The innermost main loop should always be there.
    ASSERT(!m_mainLoops.isEmpty());
    GRefPtr<GMainLoop> lastMainLoop = m_mainLoops.last();
    if (g_main_loop_is_running(lastMainLoop.get()))
        g_main_loop_quit(lastMainLoop.get());
}

void RunLoop::wakeUp()
{
    g_source_set_ready_time(m_source.get(), 0);
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode)
{
    g_main_context_iteration(NULL, FALSE);
    return CycleResult::Continue;
}

RunLoop::TimerBase::TimerBase(RunLoop& runLoop)
    : m_runLoop(runLoop)
    , m_source(adoptGRef(g_source_new(&runLoopSourceFunctions, sizeof(GSource))))
{
    g_source_set_priority(m_source.get(), RunLoopSourcePriority::RunLoopTimer);
    g_source_set_name(m_source.get(), "[WebKit] RunLoop::Timer work");
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

void RunLoop::TimerBase::setName(const char* name)
{
    g_source_set_name(m_source.get(), name);
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
