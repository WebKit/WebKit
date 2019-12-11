/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "KeyframeEffectStack.h"

#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "KeyframeEffect.h"
#include "WebAnimation.h"

namespace WebCore {

KeyframeEffectStack::KeyframeEffectStack()
{
}

KeyframeEffectStack::~KeyframeEffectStack()
{
    ASSERT(m_effects.isEmpty());
}

void KeyframeEffectStack::addEffect(KeyframeEffect& effect)
{
    // To qualify for membership in an effect stack, an effect must have a target, an animation and a timeline.
    // This method will be called in WebAnimation and KeyframeEffect as those properties change.
    if (!effect.target() || !effect.animation() || !effect.animation()->timeline())
        return;

    m_effects.append(makeWeakPtr(&effect));
    m_isSorted = false;
}

void KeyframeEffectStack::removeEffect(KeyframeEffect& effect)
{
    m_effects.removeFirst(&effect);
}

Vector<WeakPtr<KeyframeEffect>> KeyframeEffectStack::sortedEffects()
{
    ensureEffectsAreSorted();
    return m_effects;
}

void KeyframeEffectStack::ensureEffectsAreSorted()
{
    if (m_isSorted || m_effects.size() < 2)
        return;

    std::sort(m_effects.begin(), m_effects.end(), [&](auto& lhs, auto& rhs) {
        auto* lhsAnimation = lhs->animation();
        auto* rhsAnimation = rhs->animation();

        ASSERT(lhsAnimation);
        ASSERT(rhsAnimation);

        // CSS Transitions sort first.
        bool lhsIsCSSTransition = is<CSSTransition>(lhsAnimation);
        bool rhsIsCSSTransition = is<CSSTransition>(rhsAnimation);
        if (lhsIsCSSTransition || rhsIsCSSTransition) {
            if (lhsIsCSSTransition == rhsIsCSSTransition) {
                // Sort transitions first by their generation time, and then by transition-property.
                // https://drafts.csswg.org/css-transitions-2/#animation-composite-order
                auto* lhsCSSTransition = downcast<CSSTransition>(lhsAnimation);
                auto* rhsCSSTransition = downcast<CSSTransition>(rhsAnimation);
                if (lhsCSSTransition->generationTime() != rhsCSSTransition->generationTime())
                    return lhsCSSTransition->generationTime() < rhsCSSTransition->generationTime();
                return lhsCSSTransition->transitionProperty().utf8() < rhsCSSTransition->transitionProperty().utf8();
            }
            return !rhsIsCSSTransition;
        }

        // CSS Animations sort next.
        bool lhsIsCSSAnimation = is<CSSAnimation>(lhsAnimation);
        bool rhsIsCSSAnimation = is<CSSAnimation>(rhsAnimation);
        if (lhsIsCSSAnimation || rhsIsCSSAnimation) {
            if (lhsIsCSSAnimation == rhsIsCSSAnimation) {
                // https://drafts.csswg.org/css-animations-2/#animation-composite-order
                // Sort A and B based on their position in the computed value of the animation-name property of the (common) owning element.
                auto& lhsCSSAnimationName = downcast<CSSAnimation>(lhsAnimation)->backingAnimation().name();
                auto& rhsCSSAnimationName = downcast<CSSAnimation>(rhsAnimation)->backingAnimation().name();

                for (auto& animationName : m_cssAnimationNames) {
                    if (animationName == lhsCSSAnimationName)
                        return true;
                    if (animationName == rhsCSSAnimationName)
                        return false;
                }
                // We should have found either of those CSS animations in the CSS animations list.
                ASSERT_NOT_REACHED();
            }
            return !rhsIsCSSAnimation;
        }

        // JS-originated animations sort last based on their position in the global animation list.
        // https://drafts.csswg.org/web-animations-1/#animation-composite-order
        return lhsAnimation->globalPosition() < rhsAnimation->globalPosition();
    });

    m_isSorted = true;
}

void KeyframeEffectStack::setCSSAnimationNames(Vector<String>&& animationNames)
{
    m_cssAnimationNames = WTFMove(animationNames);
    // Since the list of animation names has changed, the sorting order of the animation effects may have changed as well.
    m_isSorted = false;
}

} // namespace WebCore
