/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
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

#ifndef EventLoopInputDispatcher_h
#define EventLoopInputDispatcher_h

#if ENABLE(WEB_REPLAY)

#include "EventLoopInput.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Page;
class ReplayingInputCursor;

enum class DispatchSpeed {
    RealTime,
    FastForward,
};

class EventLoopInputDispatcherClient {
public:
    EventLoopInputDispatcherClient() { }
    virtual ~EventLoopInputDispatcherClient() { }

    virtual void willDispatchInput(const EventLoopInputBase&) =0;
    virtual void didDispatchInput(const EventLoopInputBase&) =0;
    virtual void didDispatchFinalInput() =0;
};

class EventLoopInputDispatcher {
    WTF_MAKE_NONCOPYABLE(EventLoopInputDispatcher);
public:
    EventLoopInputDispatcher(Page&, ReplayingInputCursor&, EventLoopInputDispatcherClient*);

    void run();
    void pause();

    void setDispatchSpeed(DispatchSpeed speed) { m_speed = speed; }
    DispatchSpeed dispatchSpeed() const { return m_speed; }

    bool isRunning() const { return m_running; }
    bool isDispatching() const { return m_dispatching; }
private:
    void dispatchInputSoon();
    void dispatchInput();
    void timerFired(Timer*);

    Page& m_page;
    EventLoopInputDispatcherClient* m_client;
    ReplayingInputCursor& m_cursor;
    Timer m_timer;

    // This pointer is valid when an event loop input is presently dispatching.
    EventLoopInputBase* m_runningInput;
    // Whether the dispatcher is currently calling out to an inputs' dispatch() method.
    bool m_dispatching;
    // Whether the dispatcher is waiting to dispatch or actively dispatching inputs.
    bool m_running;

    DispatchSpeed m_speed;
    // The time at which the last input dispatch() method was called.
    double m_previousDispatchStartTime;
    // The timestamp specified by the last dispatched input.
    double m_previousInputTimestamp;
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // EventLoopInputDispatcher_h
