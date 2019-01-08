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

#include "AnimationEffect.h"
#include "AnimationEffectPhase.h"
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

    Element* owningElement() const { return m_owningElement; }
    const Animation& backingAnimation() const { return m_backingAnimation; }
    void setBackingAnimation(const Animation&);
    void cancelFromStyle();

    Optional<double> startTime() const final;
    void setStartTime(Optional<double>) final;
    Optional<double> bindingsCurrentTime() const final;
    ExceptionOr<void> setBindingsCurrentTime(Optional<double>) final;
    WebAnimation::PlayState bindingsPlayState() const final;
    bool bindingsPending() const final;
    WebAnimation::ReadyPromise& bindingsReady() final;
    WebAnimation::FinishedPromise& bindingsFinished() final;
    ExceptionOr<void> bindingsPlay() override;
    ExceptionOr<void> bindingsPause() override;

    void setTimeline(RefPtr<AnimationTimeline>&&) final;
    void cancel() final;

    bool needsTick() const override;
    void tick() override;

protected:
    DeclarativeAnimation(Element&, const Animation&);

    virtual void initialize(const RenderStyle* oldStyle, const RenderStyle& newStyle);
    virtual void syncPropertiesWithBackingAnimation();
    void invalidateDOMEvents(Seconds elapsedTime = 0_s);

private:
    void disassociateFromOwningElement();
    void flushPendingStyleChanges() const;
    AnimationEffectPhase phaseWithoutEffect() const;
    void enqueueDOMEvent(const AtomicString&, Seconds);
    void remove() final;

    // ActiveDOMObject.
    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;

    Element* m_owningElement;
    Ref<Animation> m_backingAnimation;
    bool m_wasPending { false };
    AnimationEffectPhase m_previousPhase { AnimationEffectPhase::Idle };
    double m_previousIteration;
    GenericEventQueue m_eventQueue;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(DeclarativeAnimation, isDeclarativeAnimation())
