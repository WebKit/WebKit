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

#include "config.h"
#include "CSSTransition.h"

#include "Animation.h"
#include "CSSTransitionEvent.h"
#include "DocumentTimeline.h"
#include "InspectorInstrumentation.h"
#include "KeyframeEffect.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSTransition);

Ref<CSSTransition> CSSTransition::create(const Styleable& owningElement, AnimatableProperty property, MonotonicTime generationTime, const Animation& backingAnimation, const RenderStyle& oldStyle, const RenderStyle& newStyle, Seconds delay, Seconds duration, const RenderStyle& reversingAdjustedStartStyle, double reversingShorteningFactor)
{
    auto result = adoptRef(*new CSSTransition(owningElement, property, generationTime, backingAnimation, oldStyle, newStyle, reversingAdjustedStartStyle, reversingShorteningFactor));
    result->initialize(&oldStyle, newStyle, { nullptr });
    result->setTimingProperties(delay, duration);

    InspectorInstrumentation::didCreateWebAnimation(result.get());

    return result;
}

CSSTransition::CSSTransition(const Styleable& styleable, AnimatableProperty property, MonotonicTime generationTime, const Animation& backingAnimation, const RenderStyle& oldStyle, const RenderStyle& targetStyle, const RenderStyle& reversingAdjustedStartStyle, double reversingShorteningFactor)
    : DeclarativeAnimation(styleable, backingAnimation)
    , m_property(property)
    , m_generationTime(generationTime)
    , m_timelineTimeAtCreation(styleable.element.document().timeline().currentTime())
    , m_targetStyle(RenderStyle::clonePtr(targetStyle))
    , m_currentStyle(RenderStyle::clonePtr(oldStyle))
    , m_reversingAdjustedStartStyle(RenderStyle::clonePtr(reversingAdjustedStartStyle))
    , m_reversingShorteningFactor(reversingShorteningFactor)
{
}

void CSSTransition::resolve(RenderStyle& targetStyle, const Style::ResolutionContext& resolutionContext, std::optional<Seconds> startTime)
{
    DeclarativeAnimation::resolve(targetStyle, resolutionContext, startTime);
    m_currentStyle = RenderStyle::clonePtr(targetStyle);
}

void CSSTransition::animationDidFinish()
{
    if (auto owningElement = this->owningElement())
        owningElement->removeDeclarativeAnimationFromListsForOwningElement(*this);
}

void CSSTransition::setTimingProperties(Seconds delay, Seconds duration)
{
    suspendEffectInvalidation();

    // This method is only called from CSSTransition::create() where we're guaranteed to have an effect.
    auto* animationEffect = effect();
    ASSERT(animationEffect);

    // In order for CSS Transitions to be seeked backwards, they need to have their fill mode set to backwards
    // such that the original CSS value applied prior to the transition is used for a negative current time.
    animationEffect->setFill(FillMode::Backwards);
    animationEffect->setDelay(delay);
    animationEffect->setIterationDuration(duration);
    animationEffect->setTimingFunction(backingAnimation().timingFunction());
    animationEffect->updateStaticTimingProperties();
    effectTimingDidChange();

    unsuspendEffectInvalidation();
}

Ref<DeclarativeAnimationEvent> CSSTransition::createEvent(const AtomString& eventType, std::optional<Seconds> scheduledTime, double elapsedTime, PseudoId pseudoId)
{
    return CSSTransitionEvent::create(eventType, this, scheduledTime, elapsedTime, pseudoId, transitionProperty());
}

const AtomString CSSTransition::transitionProperty() const
{
    return WTF::switchOn(m_property,
        [] (CSSPropertyID cssProperty) {
            return nameString(cssProperty);
        },
        [] (const AtomString& customProperty) {
            return customProperty;
        }
    );
}

} // namespace WebCore
