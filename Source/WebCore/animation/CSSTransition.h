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

#include "CSSPropertyNames.h"
#include "StyleOriginatedAnimation.h"
#include "StyleTransition.h"
#include "Styleable.h"
#include "WebAnimationTypes.h"
#include <wtf/Markable.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>

namespace WebCore {

namespace Style {
class ComputedStyle;
}

class CSSTransition final : public StyleOriginatedAnimation {
    WTF_MAKE_TZONE_ALLOCATED(CSSTransition);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CSSTransition);
public:
    static Ref<CSSTransition> create(const Styleable&, const AnimatableCSSProperty&, MonotonicTime generationTime, const Style::Transition&, const Style::ComputedStyle& oldStyle, const Style::ComputedStyle& newStyle, Seconds delay, Seconds duration, const Style::ComputedStyle& reversingAdjustedStartStyle, double);

    virtual ~CSSTransition();

    const AtomString transitionProperty() const;
    AnimatableCSSProperty property() const { return m_property; }
    MonotonicTime generationTime() const { return m_generationTime; }
    const Style::ComputedStyle& targetStyle() const LIFETIME_BOUND { return *m_targetStyle; }
    const Style::ComputedStyle& reversingAdjustedStartStyle() const LIFETIME_BOUND { return *m_reversingAdjustedStartStyle; }
    double reversingShorteningFactor() const { return m_reversingShorteningFactor; }

    const Style::Transition& backingStyleTransition() const LIFETIME_BOUND { return m_backingStyleTransition; }

private:
    CSSTransition(const Styleable&, const AnimatableCSSProperty&, MonotonicTime generationTime, const Style::Transition&, const Style::ComputedStyle& targetStyle, const Style::ComputedStyle& reversingAdjustedStartStyle, double);
    void setTimingProperties(Seconds delay, Seconds duration);
    Ref<StyleOriginatedAnimationEvent> createEvent(const AtomString& eventType, std::optional<Seconds> scheduledTime, double elapsedTime, const std::optional<Style::PseudoElementIdentifier>&) final;
    void animationDidFinish() final;
    bool isCSSTransition() const final { return true; }

    AnimationPlayState backingAnimationPlayState() const final;
    TimingFunction* backingAnimationTimingFunction() const final;

    AnimatableCSSProperty m_property;
    MonotonicTime m_generationTime;
    std::unique_ptr<Style::ComputedStyle> m_targetStyle;
    std::unique_ptr<Style::ComputedStyle> m_reversingAdjustedStartStyle;
    double m_reversingShorteningFactor;

    Style::Transition m_backingStyleTransition;

};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(CSSTransition, isCSSTransition())
