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
#include "WebAnimationUtilities.h"

#include "Animation.h"
#include "AnimationList.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "DeclarativeAnimation.h"
#include "WebAnimation.h"

namespace WebCore {

bool compareAnimationsByCompositeOrder(WebAnimation& lhsAnimation, WebAnimation& rhsAnimation, const AnimationList* cssAnimationList)
{
    bool lhsHasOwningElement = is<DeclarativeAnimation>(lhsAnimation) && downcast<DeclarativeAnimation>(lhsAnimation).owningElement();
    bool rhsHasOwningElement = is<DeclarativeAnimation>(rhsAnimation) && downcast<DeclarativeAnimation>(rhsAnimation).owningElement();

    // CSS Transitions sort first.
    bool lhsIsCSSTransition = lhsHasOwningElement && is<CSSTransition>(lhsAnimation);
    bool rhsIsCSSTransition = rhsHasOwningElement && is<CSSTransition>(rhsAnimation);
    if (lhsIsCSSTransition || rhsIsCSSTransition) {
        if (lhsIsCSSTransition == rhsIsCSSTransition) {
            // Sort transitions first by their generation time, and then by transition-property.
            // https://drafts.csswg.org/css-transitions-2/#animation-composite-order
            auto& lhsCSSTransition = downcast<CSSTransition>(lhsAnimation);
            auto& rhsCSSTransition = downcast<CSSTransition>(rhsAnimation);
            if (lhsCSSTransition.generationTime() != rhsCSSTransition.generationTime())
                return lhsCSSTransition.generationTime() < rhsCSSTransition.generationTime();
            return lhsCSSTransition.transitionProperty().utf8() < rhsCSSTransition.transitionProperty().utf8();
        }
        return !rhsIsCSSTransition;
    }

    // CSS Animations sort next.
    bool lhsIsCSSAnimation = lhsHasOwningElement && is<CSSAnimation>(lhsAnimation);
    bool rhsIsCSSAnimation = rhsHasOwningElement && is<CSSAnimation>(rhsAnimation);
    if (lhsIsCSSAnimation || rhsIsCSSAnimation) {
        if (lhsIsCSSAnimation == rhsIsCSSAnimation) {
            // We must have a list of CSS Animations if we have CSS Animations to sort through.
            ASSERT(cssAnimationList);
            ASSERT(!cssAnimationList->isEmpty());

            // https://drafts.csswg.org/css-animations-2/#animation-composite-order
            // Sort A and B based on their position in the computed value of the animation-name property of the (common) owning element.
            auto& lhsBackingAnimation = downcast<CSSAnimation>(lhsAnimation).backingAnimation();
            auto& rhsBackingAnimation = downcast<CSSAnimation>(rhsAnimation).backingAnimation();

            for (size_t i = 0; i < cssAnimationList->size(); ++i) {
                auto& animation = cssAnimationList->animation(i);
                if (animation == lhsBackingAnimation)
                    return true;
                if (animation == rhsBackingAnimation)
                    return false;
            }

            // We should have found either of those CSS animations in the CSS animations list.
            ASSERT_NOT_REACHED();
        }
        return !rhsIsCSSAnimation;
    }

    // JS-originated animations sort last based on their position in the global animation list.
    // https://drafts.csswg.org/web-animations-1/#animation-composite-order
    return lhsAnimation.globalPosition() < rhsAnimation.globalPosition();
}

} // namespace WebCore
