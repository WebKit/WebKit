/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "Event.h"
#include <wtf/Markable.h>

namespace WebCore {

class WebAnimation;

class AnimationEventBase : public Event {
    WTF_MAKE_ISO_ALLOCATED(AnimationEventBase);
public:
    static Ref<AnimationEventBase> create(const AtomString& type, WebAnimation* animation, std::optional<Seconds> timelineTime)
    {
        return adoptRef(*new AnimationEventBase(type, animation, timelineTime));
    }

    virtual ~AnimationEventBase();

    virtual bool isAnimationPlaybackEvent() const { return false; }
    virtual bool isAnimationEvent() const { return false; }
    virtual bool isTransitionEvent() const { return false; }

    std::optional<Seconds> timelineTime() const { return m_timelineTime; }
    WebAnimation* animation() const { return m_animation.get(); }

protected:
    AnimationEventBase(const AtomString&, WebAnimation*, std::optional<Seconds>);
    AnimationEventBase(const AtomString&, const EventInit&, IsTrusted);

    RefPtr<WebAnimation> m_animation;
    Markable<Seconds, Seconds::MarkableTraits> m_timelineTime;
};

}

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_EVENT_BASE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationEventBase& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

