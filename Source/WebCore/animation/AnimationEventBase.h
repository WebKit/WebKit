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
    virtual ~AnimationEventBase();

    virtual bool isAnimationPlaybackEvent() const { return false; }
    virtual bool isCSSAnimationEvent() const { return false; }
    virtual bool isCSSTransitionEvent() const { return false; }

    WebAnimation* animation() const { return m_animation.get(); }
    std::optional<Seconds> scheduledTime() const { return m_scheduledTime; }

protected:
    AnimationEventBase(const AtomString&, WebAnimation*, std::optional<Seconds> scheduledTime);
    AnimationEventBase(const AtomString&, const EventInit&, IsTrusted);

private:
    RefPtr<WebAnimation> m_animation;
    Markable<Seconds, Seconds::MarkableTraits> m_scheduledTime;
};

}

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_EVENT_BASE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationEventBase& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

