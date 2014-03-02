/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef HysteresisActivity_h
#define HysteresisActivity_h

#include <wtf/RunLoop.h>

namespace WebCore {

static const double DefaultHysteresisSeconds = 5.0;

template<typename Delegate>
class HysteresisActivity {
public:
    explicit HysteresisActivity(Delegate& delegate, double hysteresisSeconds = DefaultHysteresisSeconds)
        : m_delegate(delegate)
        , m_hysteresisSeconds(hysteresisSeconds)
        , m_active(false)
        , m_timer(RunLoop::main(), this, &HysteresisActivity<Delegate>::hysteresisTimerFired)
    {
    }

    void start()
    {
        if (m_active)
            return;
        m_active = true;

        if (m_timer.isActive())
            m_timer.stop();
        else
            m_delegate.started();
    }

    void stop()
    {
        if (!m_active)
            return;
        m_active = false;

        m_timer.startOneShot(m_hysteresisSeconds);
    }

    void impulse()
    {
        if (!m_active) {
            start();
            stop();
        }
    }

private:
    void hysteresisTimerFired()
    {
        m_delegate.stopped();
        m_timer.stop();
    }

    Delegate& m_delegate;
    double m_hysteresisSeconds;
    bool m_active;
    RunLoop::Timer<HysteresisActivity> m_timer;
};

} // namespace WebCore

#endif // HysteresisActivity_h
