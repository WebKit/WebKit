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
#include "RunLoop.h"

#include <glib.h>
#include <wtf/MainThread.h>

namespace WTF {

RunLoop::RunLoop()
{
    m_mainContext = g_main_context_get_thread_default();
    if (!m_mainContext)
        m_mainContext = isMainThread() ? g_main_context_default() : adoptGRef(g_main_context_new());
    ASSERT(m_mainContext);

    GRefPtr<GMainLoop> innermostLoop = adoptGRef(g_main_loop_new(m_mainContext.get(), FALSE));
    ASSERT(innermostLoop);
    m_mainLoops.append(innermostLoop);
}

RunLoop::~RunLoop()
{
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
    RefPtr<RunLoop> runLoop(this);
    GMainLoopSource::scheduleAndDeleteOnDestroy("[WebKit] RunLoop work", std::function<void()>([runLoop] {
        runLoop->performWork();
    }), G_PRIORITY_DEFAULT, nullptr, m_mainContext.get());
    g_main_context_wakeup(m_mainContext.get());
}

RunLoop::TimerBase::TimerBase(RunLoop& runLoop)
    : m_runLoop(runLoop)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::start(double fireInterval, bool repeat)
{
    m_timerSource.scheduleAfterDelay("[WebKit] RunLoop::Timer", std::function<bool ()>([this, repeat] { fired(); return repeat; }),
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(fireInterval)), G_PRIORITY_DEFAULT, nullptr, m_runLoop.m_mainContext.get());
}

void RunLoop::TimerBase::stop()
{
    m_timerSource.cancel();
}

bool RunLoop::TimerBase::isActive() const
{
    return m_timerSource.isScheduled();
}

} // namespace WTF
