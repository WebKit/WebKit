/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TimerEventBasedMock_h
#define TimerEventBasedMock_h

#if ENABLE(MEDIA_STREAM)

#include "Timer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class TimerEvent;

class MockNotifier : public RefCounted<MockNotifier> {
public:
    virtual ~MockNotifier() { }
    virtual void fire() = 0;
};

class TimerEventBasedMock {
public:
    void removeEvent(PassRefPtr<TimerEvent> event)
    {
        size_t pos = m_timerEvents.find(event);
        m_timerEvents.remove(pos);
    }

protected:
    Vector<RefPtr<TimerEvent> > m_timerEvents;
};

class TimerEvent : public RefCounted<TimerEvent> {
public:
    TimerEvent(TimerEventBasedMock* mock, PassRefPtr<MockNotifier> notifier)
        : m_mock(mock)
        , m_timer(this, &TimerEvent::timerFired)
        , m_notifier(notifier)
    {
        m_timer.startOneShot(0.5);
    }

    virtual ~TimerEvent() { }

    void timerFired(Timer*)
    {
        m_notifier->fire();
        m_mock->removeEvent(this);
    }

private:
    TimerEventBasedMock* m_mock;
    Timer m_timer;
    RefPtr<MockNotifier> m_notifier;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // TimerEventBasedMock_h
