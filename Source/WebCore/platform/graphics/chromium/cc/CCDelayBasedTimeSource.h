/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCDelayBasedTimeSource_h
#define CCDelayBasedTimeSource_h

#include "cc/CCTimeSource.h"
#include "cc/CCTimer.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {

class CCThread;

// This timer implements a time source that achieves the specified interval
// in face of millisecond-precision delayed callbacks and random queueing delays.
class CCDelayBasedTimeSource : public CCTimeSource, CCTimerClient {
public:
    static PassRefPtr<CCDelayBasedTimeSource> create(double intervalMs, CCThread*);

    virtual ~CCDelayBasedTimeSource() { }

    virtual void setClient(CCTimeSourceClient* client) { m_client = client; }

    virtual void setActive(bool);

    // CCTimerClient implementation.
    virtual void onTimerFired();

    // Virtual for testing.
    virtual double monotonicallyIncreasingTimeMs() const;

protected:
    CCDelayBasedTimeSource(double intervalMs, CCThread*);
    void postNextTickTask(double nowMs);

    enum State {
        STATE_INACTIVE,
        STATE_STARTING,
        STATE_ACTIVE,
    };
    CCTimeSourceClient* m_client;
    bool m_hasTickTarget;
    double m_intervalMs;
    double m_tickTarget;
    State m_state;
    CCThread* m_thread;
    CCTimer m_timer;
};

}
#endif // CCDelayBasedTimeSource_h
