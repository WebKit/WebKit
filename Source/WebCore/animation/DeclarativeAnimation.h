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
#include "Styleable.h"
#include "WebAnimation.h"
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Animation;
class AnimationEventBase;
class Element;
class RenderStyle;

class DeclarativeAnimation : public WebAnimation {
    WTF_MAKE_ISO_ALLOCATED(DeclarativeAnimation);
public:
    ~DeclarativeAnimation();

    bool isDeclarativeAnimation() const final { return true; }

    const std::optional<const Styleable> owningElement() const;
    const Animation& backingAnimation() const { return m_backingAnimation; }
    void setBackingAnimation(const Animation&);
    void cancelFromStyle();

    std::optional<double> bindingsStartTime() const final;
    void setBindingsStartTime(std::optional<double>) override;
    std::optional<double> bindingsCurrentTime() const final;
    ExceptionOr<void> setBindingsCurrentTime(std::optional<double>) final;
    WebAnimation::PlayState bindingsPlayState() const final;
    WebAnimation::ReplaceState bindingsReplaceState() const final;
    bool bindingsPending() const final;
    WebAnimation::ReadyPromise& bindingsReady() final;
    WebAnimation::FinishedPromise& bindingsFinished() final;
    ExceptionOr<void> bindingsPlay() override;
    ExceptionOr<void> bindingsPause() override;

    void setTimeline(RefPtr<AnimationTimeline>&&) final;
    void cancel() final;

    void tick() override;

    bool canHaveGlobalPosition() final;

    void flushPendingStyleChanges() const;

protected:
    DeclarativeAnimation(const Styleable&, const Animation&);

    virtual void initialize(const RenderStyle* oldStyle, const RenderStyle& newStyle, const RenderStyle* parentElementStyle);
    virtual void syncPropertiesWithBackingAnimation();
    // elapsedTime is the animation's current time at the time the event is added and is exposed through the DOM API, timelineTime is the animations'
    // timeline current time and is not exposed through the DOM API but used by the DocumentTimeline for sorting events before dispatch. 
    virtual Ref<AnimationEventBase> createEvent(const AtomString& eventType, double elapsedTime, const String& pseudoId, std::optional<Seconds> timelineTime) = 0;
    void invalidateDOMEvents(Seconds elapsedTime = 0_s);

private:
    void disassociateFromOwningElement();
    AnimationEffectPhase phaseWithoutEffect() const;
    void enqueueDOMEvent(const AtomString&, Seconds);

    bool m_wasPending { false };
    AnimationEffectPhase m_previousPhase { AnimationEffectPhase::Idle };

    WeakPtr<Element> m_owningElement;
    PseudoId m_owningPseudoId;
    Ref<Animation> m_backingAnimation;
    double m_previousIteration;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(DeclarativeAnimation, isDeclarativeAnimation())
