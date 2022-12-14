/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ReducedResolutionSeconds.h"
#include "Timer.h"

namespace WebCore {

class CaretAnimator;
class Document;
class Page;

class CaretAnimationClient {
public:
    virtual ~CaretAnimationClient() = default;

    virtual void caretAnimationDidUpdate(CaretAnimator&) { }

    virtual Document* document() = 0;
};

class CaretAnimator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct PresentationProperties {
        enum class BlinkState : bool { 
            Off, On
        };

        BlinkState blinkState { BlinkState::On };
        float opacity { 1.0 };
    };

    virtual ~CaretAnimator() = default;

    virtual void start(ReducedResolutionSeconds currentTime) = 0;

    void stop()
    {
        if (!m_isActive)
            return;
        didEnd();
    }

    bool isActive() const { return m_isActive; }

    void serviceCaretAnimation(ReducedResolutionSeconds);

    virtual String debugDescription() const = 0;

    virtual void setBlinkingSuspended(bool suspended) { m_isBlinkingSuspended = suspended; }
    bool isBlinkingSuspended() const { return m_isBlinkingSuspended; }

    virtual void setVisible(bool) = 0;

    PresentationProperties presentationProperties() const { return m_presentationProperties; }

protected:
    explicit CaretAnimator(CaretAnimationClient& client)
        : m_client(client)
        , m_blinkTimer(*this, &CaretAnimator::scheduleAnimation)
    { }

    virtual void updateAnimationProperties(ReducedResolutionSeconds) = 0;

    void didStart(ReducedResolutionSeconds currentTime, Seconds interval)
    {
        m_startTime = currentTime;
        m_isActive = true;
        m_blinkTimer.startOneShot(interval);
    }

    void didEnd()
    {
        m_isActive = false;
        m_blinkTimer.stop();
    }

    Page* page() const;

    CaretAnimationClient& m_client;
    ReducedResolutionSeconds m_startTime;
    Timer m_blinkTimer;
    PresentationProperties m_presentationProperties { };

private:
    void scheduleAnimation();

    bool m_isActive { false };
    bool m_isBlinkingSuspended { false };
};

static inline CaretAnimator::PresentationProperties::BlinkState operator!(CaretAnimator::PresentationProperties::BlinkState blinkState)
{
    using BlinkState = CaretAnimator::PresentationProperties::BlinkState;
    return blinkState == BlinkState::Off ? BlinkState::On : BlinkState::Off;
}

} // namespace WebCore
