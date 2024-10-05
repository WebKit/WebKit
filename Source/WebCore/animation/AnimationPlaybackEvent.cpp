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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AnimationPlaybackEvent);

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomString& type, const AnimationPlaybackEventInit& initializer, IsTrusted isTrusted)
    : AnimationEventBase(EventInterfaceType::AnimationPlaybackEvent, type, initializer, isTrusted)
    , m_timelineTime(initializer.timelineTime)
    , m_currentTime(initializer.currentTime)
{
}

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomString& type, WebAnimation* animation, std::optional<Seconds> scheduledTime, std::optional<Seconds> timelineTime, std::optional<Seconds> currentTime)
    : AnimationEventBase(EventInterfaceType::AnimationPlaybackEvent, type, animation, scheduledTime)
{
    if (timelineTime)
        m_timelineTime = *timelineTime;
    if (currentTime)
        m_currentTime = *currentTime;
}

AnimationPlaybackEvent::~AnimationPlaybackEvent() = default;

} // namespace WebCore
