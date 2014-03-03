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
#include "ReplayInputTypes.h"
#include "ReplayingInputCursor.h"
#include <wtf/TemporaryChange.h>

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
    , m_timer(this, &EventLoopInputDispatcher::timerFired)
    , m_runningInput(nullptr)
    , m_dispatching(false)
    , m_running(false)
    , m_speed(DispatchSpeed::FastForward)
    , m_previousDispatchStartTime(0.0)
    , m_previousInputTimestamp(0.0)
{
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

void EventLoopInputDispatcher::timerFired(Timer<EventLoopInputDispatcher>*)
{
    dispatchInput();
}

void EventLoopInputDispatcher::dispatchInputSoon()
{
    ASSERT(m_running);

    // We may already have an input if replay was paused just before dispatching.
    if (!m_runningInput)
        m_runningInput = safeCast<EventLoopInputBase*>(m_cursor.uncheckedLoadInput(InputQueue::EventLoopInput));

    if (m_timer.isActive())
        m_timer.stop();

    double waitInterval = 0;

    if (m_speed == DispatchSpeed::RealTime) {
        // The goal is to reproduce the dispatch delay between inputs as it was
        // was observed during the recording. So, we need to compute how much time
        // to wait such that the elapsed time plus the wait time will equal the
        // observed delay between the previous and current input.

        if (!m_previousInputTimestamp)
            m_previousInputTimestamp = m_runningInput->timestamp();

        double targetInterval = m_runningInput->timestamp() - m_previousInputTimestamp;
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
    ASSERT(m_runningInput);
    ASSERT(!m_dispatching);

    if (m_speed == DispatchSpeed::RealTime) {
        m_previousDispatchStartTime = monotonicallyIncreasingTime();
        m_previousInputTimestamp = m_runningInput->timestamp();
    }

#if !LOG_DISABLED
    EncodedValue encodedInput = EncodingTraits<NondeterministicInputBase>::encodeValue(*m_runningInput);
    String jsonString = encodedInput.asObject()->toJSONString();

    LOG(WebReplay, "%-20s ----------------------------------------------", "ReplayEvents");
    LOG(WebReplay, "%-20s >DISPATCH: %s %s\n", "ReplayEvents", m_runningInput->type().string().utf8().data(), jsonString.utf8().data());
#endif

    m_client->willDispatchInput(*m_runningInput);
    // Client could stop replay in the previous callback, so check again.
    if (!m_running)
        return;

    {
        TemporaryChange<bool> change(m_dispatching, true);
        m_runningInput->dispatch(m_page.replayController());
    }

    EventLoopInputBase* dispatchedInput = m_runningInput;
    m_runningInput = nullptr;

    // Notify clients that the event was dispatched.
    m_client->didDispatchInput(*dispatchedInput);
    if (dispatchedInput->type() == inputTypes().EndSegmentSentinel) {
        m_running = false;
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
