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
#include "Element.h"
#include "KeyframeEffectReadOnly.h"

namespace WebCore {

Ref<CSSTransition> CSSTransition::create(Element& target, CSSPropertyID property, const Animation& backingAnimation, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    auto result = adoptRef(*new CSSTransition(target, property, backingAnimation));
    result->initialize(target, oldStyle, newStyle);
    return result;
}

CSSTransition::CSSTransition(Element& element, CSSPropertyID property, const Animation& backingAnimation)
    : DeclarativeAnimation(element, backingAnimation)
    , m_property(property)
{
}

void CSSTransition::initialize(const Element& target, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
    DeclarativeAnimation::initialize(target, oldStyle, newStyle);

    suspendEffectInvalidation();

    // In order for CSS Transitions to be seeked backwards, they need to have their fill mode set to backwards
    // such that the original CSS value applied prior to the transition is used for a negative current time.
    effect()->timing()->setFill(FillMode::Backwards);

    unsuspendEffectInvalidation();
}

bool CSSTransition::matchesBackingAnimationAndStyles(const Animation& newBackingAnimation, const RenderStyle* oldStyle, const RenderStyle& newStyle) const
{
    // See if the animations match, excluding the property since we can move from an "all" transition to an explicit property transition.
    bool backingAnimationsMatch = backingAnimation().animationsMatch(newBackingAnimation, false);
    if (!oldStyle || !effect())
        return backingAnimationsMatch;
    return backingAnimationsMatch && !downcast<KeyframeEffectReadOnly>(effect())->stylesWouldYieldNewCSSTransitionsBlendingKeyframes(*oldStyle, newStyle);
}

bool CSSTransition::canBeListed() const
{
    if (!downcast<KeyframeEffectReadOnly>(effect())->hasBlendingKeyframes())
        return false;
    return WebAnimation::canBeListed();
}

} // namespace WebCore
