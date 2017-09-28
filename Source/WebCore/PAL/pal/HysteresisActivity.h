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

#pragma once

#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>

namespace PAL {

static const Seconds defaultHysteresisDuration { 5_s };

enum class HysteresisState {
    Started,
    Stopped
};

class HysteresisActivity {
public:
    explicit HysteresisActivity(WTF::Function<void(HysteresisState)>&& callback = [](HysteresisState) { }, Seconds hysteresisSeconds = defaultHysteresisDuration)
        : m_callback(WTFMove(callback))
        , m_hysteresisSeconds(hysteresisSeconds)
        , m_active(false)
        , m_timer(RunLoop::main(), this, &HysteresisActivity::hysteresisTimerFired)
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
            m_callback(HysteresisState::Started);
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

    HysteresisState state() const
    {
        return m_active || m_timer.isActive() ? HysteresisState::Started : HysteresisState::Stopped;
    }
    
private:
    void hysteresisTimerFired()
    {
        m_timer.stop();
        m_callback(HysteresisState::Stopped);
    }

    WTF::Function<void(HysteresisState)> m_callback;
    Seconds m_hysteresisSeconds;
    bool m_active;
    RunLoop::Timer<HysteresisActivity> m_timer;
};

} // namespace PAL
