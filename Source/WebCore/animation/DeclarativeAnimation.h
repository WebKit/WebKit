/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "AnimationEffectReadOnly.h"
#include "GenericEventQueue.h"
#include "WebAnimation.h"
#include <wtf/Ref.h>

namespace WebCore {

class Animation;
class Element;
class RenderStyle;

class DeclarativeAnimation : public WebAnimation {
public:
    ~DeclarativeAnimation();

    bool isDeclarativeAnimation() const final { return true; }

    Element& target() const { return m_target; }
    const Animation& backingAnimation() const { return m_backingAnimation; }
    void setBackingAnimation(const Animation&);
    void invalidateDOMEvents(Seconds elapsedTime = 0_s);

    void setTimeline(RefPtr<AnimationTimeline>&&) final;
    void cancel() final;

protected:
    DeclarativeAnimation(Element&, const Animation&);

    virtual void initialize(const Element&, const RenderStyle* oldStyle, const RenderStyle& newStyle);
    virtual void syncPropertiesWithBackingAnimation();

private:
    AnimationEffectReadOnly::Phase phaseWithoutEffect() const;
    void enqueueDOMEvent(const AtomicString&, Seconds);
    void remove() final;

    // ActiveDOMObject.
    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;

    Element& m_target;
    Ref<Animation> m_backingAnimation;
    bool m_wasPending { false };
    AnimationEffectReadOnly::Phase m_previousPhase { AnimationEffectReadOnly::Phase::Idle };
    double m_previousIteration;
    GenericEventQueue m_eventQueue;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(DeclarativeAnimation, isDeclarativeAnimation())
