/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayVBlankMonitorThreaded.h"

#include "Logging.h"
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebKit {

DisplayVBlankMonitorThreaded::DisplayVBlankMonitorThreaded(unsigned refreshRate)
    : DisplayVBlankMonitor(refreshRate)
    , m_destroyThreadTimer(RunLoop::mainSingleton(), this, &DisplayVBlankMonitorThreaded::destroyThreadTimerFired)
{
    m_destroyThreadTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
}

DisplayVBlankMonitorThreaded::~DisplayVBlankMonitorThreaded()
{
    ASSERT(!m_thread);
}

bool DisplayVBlankMonitorThreaded::startThreadIfNeeded()
{
    if (m_thread)
        return false;

    m_thread = Thread::create("VBlankMonitor"_s, [this] {
        while (true) {
            {
                Locker locker { m_lock };
                m_condition.wait(m_lock, [this]() -> bool {
                    return m_state != State::Stop;
                });
                if (m_state == State::Invalid || m_state == State::Failed)
                    return;
            }

            if (!waitForVBlank()) {
                RELEASE_LOG_FAULT(DisplayLink, "Failed to wait for vblank");
                Locker locker { m_lock };
                m_state = State::Failed;
                return;
            }

            bool active;
            {
                Locker locker { m_lock };
                active = m_state == State::Active;
            }
            if (active)
                m_handler();
        }
    }, ThreadType::Graphics, Thread::QOS::Default);
    return true;
}

void DisplayVBlankMonitorThreaded::start()
{
    Locker locker { m_lock };
    if (m_state == State::Active)
        return;

    ASSERT(m_handler);
    m_state = State::Active;
    m_destroyThreadTimer.stop();
    if (!startThreadIfNeeded())
        m_condition.notifyAll();
}

void DisplayVBlankMonitorThreaded::stop()
{
    Locker locker { m_lock };
    if (m_state != State::Active)
        return;

    m_state = State::Stop;
    if (m_thread)
        m_destroyThreadTimer.startOneShot(30_s);
}

void DisplayVBlankMonitorThreaded::invalidate()
{
    if (!m_thread) {
        m_state = State::Invalid;
        return;
    }

    {
        Locker locker { m_lock };
        m_state = State::Invalid;
        m_condition.notifyAll();
    }
    m_thread->waitForCompletion();
    m_thread = nullptr;
}

bool DisplayVBlankMonitorThreaded::isActive()
{
    Locker locker { m_lock };
    return m_state == State::Active;
}

void DisplayVBlankMonitorThreaded::setHandler(Function<void()>&& handler)
{
    Locker locker { m_lock };
    ASSERT(m_state == State::Stop);
    DisplayVBlankMonitor::setHandler(WTFMove(handler));
}

void DisplayVBlankMonitorThreaded::destroyThreadTimerFired()
{
    if (!m_thread)
        return;

    invalidate();
}

} // namespace WebKit
