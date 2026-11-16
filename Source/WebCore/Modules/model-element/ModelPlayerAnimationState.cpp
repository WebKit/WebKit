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

#include "config.h"
#include "ModelPlayerAnimationState.h"

namespace WebCore {

ModelPlayerAnimationState::ModelPlayerAnimationState(bool autoplay, bool loop, bool paused, Seconds duration, std::optional<double> effectivePlaybackRate, std::optional<Seconds> lastCachedCurrentTime, std::optional<MonotonicTime> lastCachedClockTimestamp)
    : m_autoplay(autoplay)
    , m_loop(loop)
    , m_paused(paused)
    , m_duration(duration)
    , m_effectivePlaybackRate(effectivePlaybackRate)
    , m_lastCachedCurrentTime(lastCachedCurrentTime)
    , m_lastCachedClockTimestamp(lastCachedClockTimestamp)
{
}

bool ModelPlayerAnimationState::autoplay() const
{
    return m_autoplay;
}

void ModelPlayerAnimationState::setAutoplay(bool autoplay)
{
    m_autoplay = autoplay;
}

bool ModelPlayerAnimationState::loop() const
{
    return m_loop;
}

void ModelPlayerAnimationState::setLoop(bool loop)
{
    m_loop = loop;
}

bool ModelPlayerAnimationState::paused() const
{
    return m_paused;
}

void ModelPlayerAnimationState::setPaused(bool paused)
{
    m_paused = paused;
}

Seconds ModelPlayerAnimationState::duration() const
{
    return m_duration;
}

void ModelPlayerAnimationState::setDuration(Seconds duration)
{
    m_duration = duration;
}

std::optional<double> ModelPlayerAnimationState::effectivePlaybackRate() const
{
    return m_effectivePlaybackRate;
}

void ModelPlayerAnimationState::setPlaybackRate(double playbackRate)
{
    m_effectivePlaybackRate = playbackRate;
}

std::optional<Seconds> ModelPlayerAnimationState::lastCachedCurrentTime() const
{
    return m_lastCachedCurrentTime;
}

std::optional<MonotonicTime> ModelPlayerAnimationState::lastCachedClockTimestamp() const
{
    return m_lastCachedClockTimestamp;
}

Seconds ModelPlayerAnimationState::currentTime() const
{
    if (!m_duration || !m_lastCachedCurrentTime || !m_lastCachedClockTimestamp)
        return 0_s;

    Seconds lastCachedCurrentTime = *m_lastCachedCurrentTime;
    if (m_paused)
        return lastCachedCurrentTime;

    // Approximate based on last cached animation time, clock timestamp, and playbackRate
    MonotonicTime lastCachedTimestamp = *m_lastCachedClockTimestamp;
    double playbackRate = m_effectivePlaybackRate ? *m_effectivePlaybackRate : 1.0;
    Seconds timePassedSinceLastSync = MonotonicTime::now() - lastCachedTimestamp;
    Seconds animationTimePassed = Seconds::fromMilliseconds(floor((timePassedSinceLastSync * playbackRate).milliseconds()));
    Seconds estimatedCurrentTime = lastCachedCurrentTime + animationTimePassed;
    if (estimatedCurrentTime > m_duration)
        estimatedCurrentTime = m_loop ? estimatedCurrentTime % m_duration : m_duration;
    else if (estimatedCurrentTime < 0_s)
        estimatedCurrentTime = m_loop ? m_duration + (estimatedCurrentTime % m_duration) : 0_s;
    return estimatedCurrentTime;
}

void ModelPlayerAnimationState::setCurrentTime(Seconds currentTime, MonotonicTime clockTimestamp)
{
    m_lastCachedCurrentTime = currentTime;
    m_lastCachedClockTimestamp = clockTimestamp;
}

}
