/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AnimationPlaybackEvent.h"

#include "WebAnimationUtilities.h"

namespace WebCore {

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomicString& type, const AnimationPlaybackEventInit& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
{
    if (initializer.currentTime == WTF::nullopt)
        m_currentTime = WTF::nullopt;
    else
        m_currentTime = Seconds::fromMilliseconds(initializer.currentTime.value());

    if (initializer.timelineTime == WTF::nullopt)
        m_timelineTime = WTF::nullopt;
    else
        m_timelineTime = Seconds::fromMilliseconds(initializer.timelineTime.value());
}

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomicString& type, Optional<Seconds> currentTime, Optional<Seconds> timelineTime)
    : Event(type, CanBubble::Yes, IsCancelable::No)
    , m_currentTime(currentTime)
    , m_timelineTime(timelineTime)
{
}

AnimationPlaybackEvent::~AnimationPlaybackEvent() = default;

Optional<double> AnimationPlaybackEvent::bindingsCurrentTime() const
{
    if (!m_currentTime)
        return WTF::nullopt;
    return secondsToWebAnimationsAPITime(m_currentTime.value());
}

Optional<double> AnimationPlaybackEvent::bindingsTimelineTime() const
{
    if (!m_timelineTime)
        return WTF::nullopt;
    return secondsToWebAnimationsAPITime(m_timelineTime.value());
}

} // namespace WebCore
