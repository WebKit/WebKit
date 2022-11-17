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
#include "AnimationEvent.h"
#include "AnimationEventBase.h"
#include "AnimationList.h"
#include "AnimationPlaybackEvent.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "DeclarativeAnimation.h"
#include "Element.h"
#include "KeyframeEffectStack.h"
#include "TransitionEvent.h"
#include "WebAnimation.h"

namespace WebCore {

static bool compareDeclarativeAnimationOwningElementPositionsInDocumentTreeOrder(const Styleable& a, const Styleable& b)
{
    // We should not ever be calling this function with two Elements that are the same. If that were the case,
    // then comparing objects of this kind would yield inconsistent results when comparing A == B and B == A.
    // As such, this function should be called with std::stable_sort().
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(a != b);

    // With regard to pseudo-elements, the sort order is as follows:
    //
    //     - element
    //     - ::marker
    //     - ::before
    //     - any other pseudo-elements not mentioned specifically in this list, sorted in ascending order by the Unicode codepoints that make up each selector
    //     - ::after
    //     - element children
    enum SortingIndex : uint8_t { NotPseudo, Marker, Before, FirstLetter, FirstLine, Highlight, Scrollbar, Selection, After, Other };
    auto sortingIndex = [](PseudoId pseudoId) -> SortingIndex {
        switch (pseudoId) {
        case PseudoId::None:
            return NotPseudo;
        case PseudoId::Marker:
            return Marker;
        case PseudoId::Before:
            return Before;
        case PseudoId::FirstLetter:
            return FirstLetter;
        case PseudoId::FirstLine:
            return FirstLine;
        case PseudoId::Highlight:
            return Highlight;
        case PseudoId::Scrollbar:
            return Scrollbar;
        case PseudoId::Selection:
            return Selection;
        case PseudoId::After:
            return After;
        default:
            ASSERT_NOT_REACHED();
            return Other;
        }
    };

    auto& aReferenceElement = a.element;
    int aSortingIndex = sortingIndex(a.pseudoId);

    auto& bReferenceElement = b.element;
    int bSortingIndex = sortingIndex(b.pseudoId);

    if (&aReferenceElement == &bReferenceElement) {
        ASSERT(aSortingIndex != bSortingIndex);
        return aSortingIndex < bSortingIndex;
    }
    return aReferenceElement.compareDocumentPosition(bReferenceElement) & Node::DOCUMENT_POSITION_FOLLOWING;
}

static bool compareCSSTransitions(const CSSTransition& a, const CSSTransition& b)
{
    ASSERT(a.owningElement());
    ASSERT(b.owningElement());
    auto& aOwningElement = a.owningElement();
    auto& bOwningElement = b.owningElement();

    // If the owning element of A and B differs, sort A and B by tree order of their corresponding owning elements.
    if (*aOwningElement != *bOwningElement)
        return compareDeclarativeAnimationOwningElementPositionsInDocumentTreeOrder(*aOwningElement, *bOwningElement);

    // Otherwise, if A and B have different transition generation values, sort by their corresponding transition generation in ascending order.
    if (a.generationTime() != b.generationTime())
        return a.generationTime() < b.generationTime();

    // Otherwise, sort A and B in ascending order by the Unicode codepoints that make up the expanded transition property name of each transition
    // (i.e. without attempting case conversion and such that ‘-moz-column-width’ sorts before ‘column-width’).
    return codePointCompareLessThan(a.transitionProperty(), b.transitionProperty());
}

static bool compareCSSAnimations(const CSSAnimation& a, const CSSAnimation& b)
{
    // https://drafts.csswg.org/css-animations-2/#animation-composite-order
    ASSERT(a.owningElement());
    ASSERT(b.owningElement());
    auto& aOwningElement = a.owningElement();
    auto& bOwningElement = b.owningElement();

    // If the owning element of A and B differs, sort A and B by tree order of their corresponding owning elements.
    if (*aOwningElement != *bOwningElement)
        return compareDeclarativeAnimationOwningElementPositionsInDocumentTreeOrder(*aOwningElement, *bOwningElement);

    // Sort A and B based on their position in the computed value of the animation-name property of the (common) owning element.
    auto* cssAnimationList = aOwningElement->ensureKeyframeEffectStack().cssAnimationList();
    ASSERT(cssAnimationList);
    ASSERT(!cssAnimationList->isEmpty());

    auto& aBackingAnimation = a.backingAnimation();
    auto& bBackingAnimation = b.backingAnimation();
    for (auto& animation : *cssAnimationList) {
        if (animation.ptr() == &aBackingAnimation)
            return true;
        if (animation.ptr() == &bBackingAnimation)
            return false;
    }

    // We should have found either of those CSS animations in the CSS animations list.
    RELEASE_ASSERT_NOT_REACHED();
}

bool compareAnimationsByCompositeOrder(const WebAnimation& a, const WebAnimation& b)
{
    // We should not ever be calling this function with two WebAnimation objects that are the same. If that were the case,
    // then comparing objects of this kind would yield inconsistent results when comparing A == B and B == A. As such,
    // this function should be called with std::stable_sort().
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&a != &b);

    bool aHasOwningElement = is<DeclarativeAnimation>(a) && downcast<DeclarativeAnimation>(a).owningElement();
    bool bHasOwningElement = is<DeclarativeAnimation>(b) && downcast<DeclarativeAnimation>(b).owningElement();

    // CSS Transitions sort first.
    bool aIsCSSTransition = aHasOwningElement && is<CSSTransition>(a);
    bool bIsCSSTransition = bHasOwningElement && is<CSSTransition>(b);
    if (aIsCSSTransition || bIsCSSTransition) {
        if (aIsCSSTransition == bIsCSSTransition)
            return compareCSSTransitions(downcast<CSSTransition>(a), downcast<CSSTransition>(b));
        return !bIsCSSTransition;
    }

    // CSS Animations sort next.
    bool aIsCSSAnimation = aHasOwningElement && is<CSSAnimation>(a);
    bool bIsCSSAnimation = bHasOwningElement && is<CSSAnimation>(b);
    if (aIsCSSAnimation || bIsCSSAnimation) {
        if (aIsCSSAnimation == bIsCSSAnimation)
            return compareCSSAnimations(downcast<CSSAnimation>(a), downcast<CSSAnimation>(b));
        return !bIsCSSAnimation;
    }

    // JS-originated animations sort last based on their position in the global animation list.
    // https://drafts.csswg.org/web-animations-1/#animation-composite-order
    RELEASE_ASSERT(a.globalPosition() != b.globalPosition());
    return a.globalPosition() < b.globalPosition();
}

bool compareAnimationEventsByCompositeOrder(const AnimationEventBase& a, const AnimationEventBase& b)
{
    // AnimationPlaybackEvent instances sort first.
    bool aIsPlaybackEvent = is<AnimationPlaybackEvent>(a);
    bool bIsPlaybackEvent = is<AnimationPlaybackEvent>(b);
    if (aIsPlaybackEvent || bIsPlaybackEvent) {
        if (aIsPlaybackEvent != bIsPlaybackEvent)
            return !bIsPlaybackEvent;

        if (a.animation() == b.animation())
            return false;

        // https://drafts.csswg.org/web-animations-1/#update-animations-and-send-events
        // 1. Sort the events by their scheduled event time such that events that were scheduled to occur earlier, sort before
        // events scheduled to occur later and events whose scheduled event time is unresolved sort before events with a
        // resolved scheduled event time.
        auto aScheduledTime = downcast<AnimationPlaybackEvent>(a).scheduledTime();
        auto bScheduledTime = downcast<AnimationPlaybackEvent>(b).scheduledTime();
        if (aScheduledTime != bScheduledTime) {
            if (aScheduledTime && bScheduledTime)
                return *aScheduledTime < *bScheduledTime;
            return !bScheduledTime;
        }

        // 2. Within events with equal scheduled event times, sort by their composite order.

        // CSS Transitions sort first.
        bool aIsCSSTransition = is<CSSTransition>(a.animation());
        bool bIsCSSTransition = is<CSSTransition>(b.animation());
        if (aIsCSSTransition || bIsCSSTransition) {
            if (aIsCSSTransition == bIsCSSTransition)
                return false;
            return !bIsCSSTransition;
        }

        // CSS Animations sort next.
        bool aIsCSSAnimation = is<CSSAnimation>(a.animation());
        bool bIsCSSAnimation = is<CSSAnimation>(b.animation());
        if (aIsCSSAnimation || bIsCSSAnimation) {
            if (aIsCSSAnimation == bIsCSSAnimation)
                return false;
            return !bIsCSSAnimation;
        }

        // JS-originated animations sort last based on their position in the global animation list.
        auto* aAnimation = a.animation();
        auto* bAnimation = b.animation();
        if (aAnimation == bAnimation)
            return false;

        RELEASE_ASSERT(aAnimation);
        RELEASE_ASSERT(bAnimation);
        RELEASE_ASSERT(aAnimation->globalPosition() != bAnimation->globalPosition());
        return aAnimation->globalPosition() < bAnimation->globalPosition();
    }

    // TransitionEvent instances sort next.
    bool aIsCSSTransition = is<TransitionEvent>(a);
    bool bIsCSSTransition = is<TransitionEvent>(b);
    if (aIsCSSTransition || bIsCSSTransition) {
        if (aIsCSSTransition == bIsCSSTransition)
            return false;
        return !bIsCSSTransition;
    }

    // AnimationEvent instances sort last.
    bool aIsCSSAnimation = is<AnimationEvent>(a);
    bool bIsCSSAnimation = is<AnimationEvent>(b);
    if (aIsCSSAnimation || bIsCSSAnimation) {
        if (aIsCSSAnimation == bIsCSSAnimation)
            return false;
        return !bIsCSSAnimation;
    }

    return false;
}

String pseudoIdAsString(PseudoId pseudoId)
{
    static NeverDestroyed<const String> after(MAKE_STATIC_STRING_IMPL("::after"));
    static NeverDestroyed<const String> before(MAKE_STATIC_STRING_IMPL("::before"));
    static NeverDestroyed<const String> firstLetter(MAKE_STATIC_STRING_IMPL("::first-letter"));
    static NeverDestroyed<const String> firstLine(MAKE_STATIC_STRING_IMPL("::first-line"));
    static NeverDestroyed<const String> highlight(MAKE_STATIC_STRING_IMPL("::highlight"));
    static NeverDestroyed<const String> marker(MAKE_STATIC_STRING_IMPL("::marker"));
    static NeverDestroyed<const String> selection(MAKE_STATIC_STRING_IMPL("::selection"));
    static NeverDestroyed<const String> scrollbar(MAKE_STATIC_STRING_IMPL("::scrollbar"));
    switch (pseudoId) {
    case PseudoId::After:
        return after;
    case PseudoId::Before:
        return before;
    case PseudoId::FirstLetter:
        return firstLetter;
    case PseudoId::FirstLine:
        return firstLine;
    case PseudoId::Highlight:
        return highlight;
    case PseudoId::Marker:
        return marker;
    case PseudoId::Selection:
        return selection;
    case PseudoId::Scrollbar:
        return scrollbar;
    default:
        return emptyString();
    }
}

} // namespace WebCore
