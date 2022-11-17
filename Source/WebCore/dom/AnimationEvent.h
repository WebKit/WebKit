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

#include "AnimationEventBase.h"

namespace WebCore {

class AnimationEvent final : public AnimationEventBase {
    WTF_MAKE_ISO_ALLOCATED(AnimationEvent);
public:
    static Ref<AnimationEvent> create(const AtomString& type, WebAnimation* animation, double elapsedTime, const String& animationName, const String& pseudoElement)
    {
        return adoptRef(*new AnimationEvent(type, animation, elapsedTime, animationName, pseudoElement));
    }

    struct Init : EventInit {
        String animationName;
        double elapsedTime { 0 };
        String pseudoElement;
    };

    static Ref<AnimationEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new AnimationEvent(type, initializer, isTrusted));
    }

    virtual ~AnimationEvent();

    bool isAnimationEvent() const final { return true; }

    const String& animationName() const;
    double elapsedTime() const;
    const String& pseudoElement() const;

    EventInterface eventInterface() const override;

private:
    AnimationEvent(const AtomString& type, WebAnimation*, double elapsedTime, const String& animationName, const String& pseudoElement);
    AnimationEvent(const AtomString&, const Init&, IsTrusted);

    String m_animationName;
    double m_elapsedTime;
    String m_pseudoElement;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_EVENT_BASE(AnimationEvent, isAnimationEvent())
