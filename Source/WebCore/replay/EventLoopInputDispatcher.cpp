/*
 * Copyright (C) 2011-2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EventLoopInputDispatcher.h"

#if ENABLE(WEB_REPLAY)

#include "Page.h"
#include "ReplayingInputCursor.h"
#include "WebReplayInputs.h"
#include <wtf/SetForScope.h>

#if !LOG_DISABLED
#include "Logging.h"
#include "SerializationMethods.h"
#include <replay/EncodedValue.h>
#include <wtf/text/CString.h>
#endif

namespace WebCore {

EventLoopInputDispatcher::EventLoopInputDispatcher(Page& page, ReplayingInputCursor& cursor, EventLoopInputDispatcherClient* client)
    : m_page(page)
    , m_client(client)
    , m_cursor(cursor)
    , m_timer(*this, &EventLoopInputDispatcher::timerFired)
    , m_speed(DispatchSpeed::FastForward)
{
    m_currentWork.input = nullptr;
    m_currentWork.timestamp = 0.0;
}

void EventLoopInputDispatcher::run()
{
    ASSERT(!m_running);
    m_running = true;

    LOG(WebReplay, "%-20s Starting dispatch of event loop inputs for page: %p\n", "ReplayEvents", &m_page);
    dispatchInputSoon();
}

void EventLoopInputDispatcher::pause()
{
    ASSERT(!m_dispatching);
    ASSERT(m_running);
    m_running = false;

    LOG(WebReplay, "%-20s Pausing dispatch of event loop inputs for page: %p\n", "ReplayEvents", &m_page);
    if (m_timer.isActive())
        m_timer.stop();
}

void EventLoopInputDispatcher::timerFired()
{
    dispatchInput();
}

void EventLoopInputDispatcher::dispatchInputSoon()
{
    ASSERT(m_running);

    // We may already have an input if replay was paused just before dispatching.
    if (!m_currentWork.input)
        m_currentWork = m_cursor.loadEventLoopInput();

    if (m_timer.isActive())
        m_timer.stop();

    double waitInterval = 0;

    if (m_speed == DispatchSpeed::RealTime) {
        // The goal is to reproduce the dispatch delay between inputs as it was
        // was observed during the recording. So, we need to compute how much time
        // to wait such that the elapsed time plus the wait time will equal the
        // observed delay between the previous and current input.

        if (!m_previousInputTimestamp)
            m_previousInputTimestamp = m_currentWork.timestamp;

        double targetInterval = m_currentWork.timestamp - m_previousInputTimestamp;
        double elapsed = monotonicallyIncreasingTime() - m_previousDispatchStartTime;
        waitInterval = targetInterval - elapsed;
    }

    // A negative wait time means that dispatch took longer on replay than on
    // capture. In this case, proceed without waiting at all.
    if (waitInterval < 0)
        waitInterval = 0;

    if (waitInterval > 1000.0) {
        LOG_ERROR("%-20s Tried to wait for over 1000 seconds before dispatching next event loop input; this is probably a bug.", "ReplayEvents");
        waitInterval = 0;
    }

    LOG(WebReplay, "%-20s (WAIT: %.3f ms)", "ReplayEvents", waitInterval * 1000.0);
    m_timer.startOneShot(waitInterval);
}

void EventLoopInputDispatcher::dispatchInput()
{
    ASSERT(m_currentWork.input);
    ASSERT(!m_dispatching);

    if (m_speed == DispatchSpeed::RealTime) {
        m_previousDispatchStartTime = monotonicallyIncreasingTime();
        m_previousInputTimestamp = m_currentWork.timestamp;
    }

#if !LOG_DISABLED
    EncodedValue encodedInput = EncodingTraits<NondeterministicInputBase>::encodeValue(*m_currentWork.input);
    String jsonString = encodedInput.asObject()->toJSONString();

    LOG(WebReplay, "%-20s ----------------------------------------------", "ReplayEvents");
    LOG(WebReplay, "%-20s >DISPATCH: %s %s\n", "ReplayEvents", m_currentWork.input->type().utf8().data(), jsonString.utf8().data());
#endif

    m_client->willDispatchInput(*m_currentWork.input);
    // Client could stop replay in the previous callback, so check again.
    if (!m_running)
        return;

    {
        SetForScope<bool> change(m_dispatching, true);
        m_currentWork.input->dispatch(m_page.replayController());
    }

    EventLoopInputBase* dispatchedInput = m_currentWork.input;
    m_currentWork.input = nullptr;

    // Notify clients that the event was dispatched.
    m_client->didDispatchInput(*dispatchedInput);
    if (dispatchedInput->type() == InputTraits<EndSegmentSentinel>::type()) {
        m_running = false;
        m_dispatching = false;
        m_client->didDispatchFinalInput();
        return;
    }

    // Clients could stop replay during event dispatch, or from any callback above.
    if (!m_running)
        return;

    dispatchInputSoon();
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
