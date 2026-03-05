/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/MonotonicTime.h>

namespace WebCore {

class WEBCORE_EXPORT ModelPlayerAnimationState {
public:
    ModelPlayerAnimationState() = default;
    ModelPlayerAnimationState(const ModelPlayerAnimationState&) = default;
    ModelPlayerAnimationState(ModelPlayerAnimationState&&) = default;
    ModelPlayerAnimationState& operator=(ModelPlayerAnimationState&&) = default;
    ModelPlayerAnimationState(bool autoplay, bool loop, bool paused, Seconds duration, std::optional<double> effectivePlaybackRate, std::optional<Seconds> lastCachedCurrentTime, std::optional<MonotonicTime> lastCachedClockTimestamp);

    bool NODELETE autoplay() const;
    void NODELETE setAutoplay(bool);
    bool NODELETE loop() const;
    void NODELETE setLoop(bool);
    bool NODELETE paused() const;
    void NODELETE setPaused(bool);
    Seconds NODELETE duration() const;
    void NODELETE setDuration(Seconds);
    std::optional<double> NODELETE effectivePlaybackRate() const;
    void setPlaybackRate(double);
    std::optional<Seconds> NODELETE lastCachedCurrentTime() const;
    std::optional<MonotonicTime> NODELETE lastCachedClockTimestamp() const;

    Seconds currentTime() const;
    void NODELETE setCurrentTime(Seconds, MonotonicTime clockTimestamp);

private:
    bool m_autoplay { false };
    bool m_loop { false };
    bool m_paused { true };
    Seconds m_duration { 0_s };
    std::optional<double> m_effectivePlaybackRate;
    std::optional<Seconds> m_lastCachedCurrentTime;
    std::optional<MonotonicTime> m_lastCachedClockTimestamp;
};

}
