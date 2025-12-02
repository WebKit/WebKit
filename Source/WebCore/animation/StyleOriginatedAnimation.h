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

#include <WebCore/AnimationEffect.h>
#include <WebCore/AnimationEffectPhase.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/Styleable.h>
#include <WebCore/WebAnimation.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class StyleOriginatedAnimationEvent;
class Element;
class RenderStyle;

class StyleOriginatedAnimation : public WebAnimation {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(StyleOriginatedAnimation);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(StyleOriginatedAnimation);
public:
    ~StyleOriginatedAnimation();

    bool isStyleOriginatedAnimation() const final { return true; }

    const std::optional<const Styleable> owningElement() const;

    void cancelFromStyle(WebAnimation::Silently = WebAnimation::Silently::No);

    std::optional<WebAnimationTime> bindingsStartTime() const final;
    std::optional<WebAnimationTime> bindingsCurrentTime() const final;
    WebAnimation::PlayState bindingsPlayState() const final;
    WebAnimation::ReplaceState bindingsReplaceState() const final;
    bool bindingsPending() const final;
    WebAnimation::ReadyPromise& bindingsReady() final;
    WebAnimation::FinishedPromise& bindingsFinished() final;
    ExceptionOr<void> bindingsPlay() override;
    ExceptionOr<void> bindingsPause() override;

    void setTimeline(RefPtr<AnimationTimeline>&&) final;
    void cancel(WebAnimation::Silently = WebAnimation::Silently::No) final;

    void tick() override;

    bool canHaveGlobalPosition() final;

    void flushPendingStyleChanges() const;

    virtual AnimationPlayState backingAnimationPlayState() const = 0;
    virtual TimingFunction* backingAnimationTimingFunction() const = 0;

protected:
    StyleOriginatedAnimation(const Styleable&);

    void initialize(const RenderStyle* oldStyle, const RenderStyle& newStyle, const Style::ResolutionContext&);
    virtual void syncPropertiesWithBackingAnimation();
    virtual Ref<StyleOriginatedAnimationEvent> createEvent(const AtomString& eventType, std::optional<Seconds> scheduledTime, double elapsedTime, const std::optional<Style::PseudoElementIdentifier>&) = 0;

private:
    void disassociateFromOwningElement();
    AnimationEffectPhase phaseWithoutEffect() const;
    template<typename F> void invalidateDOMEvents(F&&);
    void invalidateDOMEvents(WebAnimationTime cancelationTime = 0_s);
    void enqueueDOMEvent(const AtomString&, WebAnimationTime elapsedTime, WebAnimationTime scheduledEffectTime);

    WebAnimationTime effectTimeAtStart() const;
    WebAnimationTime effectTimeAtIteration(double) const;
    WebAnimationTime effectTimeAtEnd() const;

    bool m_wasPending { false };
    AnimationEffectPhase m_previousPhase { AnimationEffectPhase::Idle };

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_owningElement;
    std::optional<Style::PseudoElementIdentifier> m_owningPseudoElementIdentifier;
    double m_previousIteration;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(StyleOriginatedAnimation, isStyleOriginatedAnimation())
