/*
 * Copyright (C) Canon Inc. 2016
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AnimationTimeline.h"

#include "Animation.h"
#include "AnimationEffectReadOnly.h"
#include "AnimationList.h"
#include "CSSAnimation.h"
#include "CSSPropertyAnimation.h"
#include "CSSTransition.h"
#include "DocumentTimeline.h"
#include "Element.h"
#include "KeyframeEffectReadOnly.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "WebAnimationUtilities.h"
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

AnimationTimeline::AnimationTimeline(ClassType classType)
    : m_classType(classType)
{
}

AnimationTimeline::~AnimationTimeline()
{
    m_animations.clear();
    m_elementToAnimationsMap.clear();
    m_elementToCSSAnimationsMap.clear();
    m_elementToCSSTransitionsMap.clear();
    m_elementToCSSAnimationByName.clear();
    m_elementToCSSTransitionByCSSPropertyID.clear();
}

void AnimationTimeline::addAnimation(Ref<WebAnimation>&& animation)
{
    m_animations.add(WTFMove(animation));
    timingModelDidChange();
}

void AnimationTimeline::removeAnimation(Ref<WebAnimation>&& animation)
{
    m_animations.remove(WTFMove(animation));
    timingModelDidChange();
}

std::optional<double> AnimationTimeline::bindingsCurrentTime()
{
    auto time = currentTime();
    if (!time)
        return std::nullopt;
    return secondsToWebAnimationsAPITime(*time);
}

void AnimationTimeline::setCurrentTime(Seconds currentTime)
{
    m_currentTime = currentTime;
    timingModelDidChange();
}

HashMap<Element*, Vector<RefPtr<WebAnimation>>>& AnimationTimeline::relevantMapForAnimation(WebAnimation& animation)
{
    if (animation.isCSSAnimation())
        return m_elementToCSSAnimationsMap;
    if (animation.isCSSTransition())
        return m_elementToCSSTransitionsMap;
    return m_elementToAnimationsMap;
}

void AnimationTimeline::animationWasAddedToElement(WebAnimation& animation, Element& element)
{
    auto result = relevantMapForAnimation(animation).ensure(&element, [] {
        return Vector<RefPtr<WebAnimation>> { };
    });
    result.iterator->value.append(&animation);
}

void AnimationTimeline::animationWasRemovedFromElement(WebAnimation& animation, Element& element)
{
    auto& map = relevantMapForAnimation(animation);
    auto iterator = map.find(&element);
    if (iterator == map.end())
        return;

    auto& animations = iterator->value;
    animations.removeFirst(&animation);
    if (!animations.size())
        map.remove(iterator);
}

Vector<RefPtr<WebAnimation>> AnimationTimeline::animationsForElement(Element& element)
{
    Vector<RefPtr<WebAnimation>> animations;
    if (m_elementToCSSAnimationsMap.contains(&element))
        animations.appendVector(m_elementToCSSAnimationsMap.get(&element));
    if (m_elementToCSSTransitionsMap.contains(&element))
        animations.appendVector(m_elementToCSSTransitionsMap.get(&element));
    if (m_elementToAnimationsMap.contains(&element))
        animations.appendVector(m_elementToAnimationsMap.get(&element));
    return animations;
}

void AnimationTimeline::updateCSSAnimationsForElement(Element& element, const RenderStyle& newStyle, const RenderStyle* oldStyle)
{
    if (element.document().pageCacheState() != Document::NotInPageCache)
        return;

    if (element.document().renderView()->printing())
        return;

    // In case this element is newly getting a "display: none" we need to cancel all of its animations and disregard new ones.
    if (oldStyle && oldStyle->hasAnimations() && oldStyle->display() != NONE && newStyle.display() == NONE) {
        if (m_elementToCSSAnimationByName.contains(&element)) {
            for (const auto& cssAnimationsByNameMapItem : m_elementToCSSAnimationByName.take(&element))
                cancelOrRemoveDeclarativeAnimation(cssAnimationsByNameMapItem.value);
        }
        return;
    }

    // First, compile the list of animation names that were applied to this element up to this point.
    HashSet<String> namesOfPreviousAnimations;
    if (oldStyle && oldStyle->hasAnimations()) {
        auto* previousAnimations = oldStyle->animations();
        for (size_t i = 0; i < previousAnimations->size(); ++i) {
            auto& previousAnimation = previousAnimations->animation(i);
            if (previousAnimation.isValidAnimation())
                namesOfPreviousAnimations.add(previousAnimation.name());
        }
    }

    // Create or get the CSSAnimations by animation name map for this element.
    auto& cssAnimationsByName = m_elementToCSSAnimationByName.ensure(&element, [] {
        return HashMap<String, RefPtr<CSSAnimation>> { };
    }).iterator->value;

    if (auto* currentAnimations = newStyle.animations()) {
        for (size_t i = 0; i < currentAnimations->size(); ++i) {
            auto& currentAnimation = currentAnimations->animation(i);
            auto& name = currentAnimation.name();
            if (namesOfPreviousAnimations.contains(name)) {
                // We've found the name of this animation in our list of previous animations, this means we've already
                // created a CSSAnimation object for it and need to ensure that this CSSAnimation is backed by the current
                // animation object for this animation name.
                cssAnimationsByName.get(name)->setBackingAnimation(currentAnimation);
            } else if (currentAnimation.isValidAnimation()) {
                // Otherwise we are dealing with a new animation name and must create a CSSAnimation for it.
                cssAnimationsByName.set(name, CSSAnimation::create(element, currentAnimation));
            }
            // Remove the name of this animation from our list since it's now known to be current.
            namesOfPreviousAnimations.remove(name);
        }
    }

    // The animations names left in namesOfPreviousAnimations are now known to no longer apply so we need to
    // remove the CSSAnimation object created for them.
    for (const auto& nameOfAnimationToRemove : namesOfPreviousAnimations)
        cancelOrRemoveDeclarativeAnimation(cssAnimationsByName.take(nameOfAnimationToRemove));

    // Remove the map of CSSAnimations by animation name for this element if it's now empty.
    if (cssAnimationsByName.isEmpty())
        m_elementToCSSAnimationByName.remove(&element);
}

void AnimationTimeline::updateCSSTransitionsForElement(Element& element, const RenderStyle& newStyle, const RenderStyle* oldStyle)
{
    if (element.document().pageCacheState() != Document::NotInPageCache)
        return;

    if (element.document().renderView()->printing())
        return;

    // In case this element is newly getting a "display: none" we need to cancel all of its animations and disregard new ones.
    if (oldStyle && oldStyle->hasTransitions() && oldStyle->display() != NONE && newStyle.display() == NONE) {
        if (m_elementToCSSTransitionByCSSPropertyID.contains(&element)) {
            for (const auto& cssTransitionsByCSSPropertyIDMapItem : m_elementToCSSTransitionByCSSPropertyID.take(&element))
                cancelOrRemoveDeclarativeAnimation(cssTransitionsByCSSPropertyIDMapItem.value);
        }
        return;
    }

    // FIXME: We do not handle "all" transitions yet.

    // First, compile the list of backing animations and properties that were applied to this element up to this point.
    HashSet<CSSPropertyID> previousProperties;
    HashSet<const Animation*> previousBackingAnimations;
    if (oldStyle && oldStyle->hasTransitions()) {
        auto* previousTransitions = oldStyle->transitions();
        for (size_t i = 0; i < previousTransitions->size(); ++i) {
            auto& animation = previousTransitions->animation(i);
            auto previousTransitionProperty = animation.property();
            if (previousTransitionProperty != CSSPropertyInvalid) {
                previousProperties.add(previousTransitionProperty);
                previousBackingAnimations.add(&animation);
            }
        }
    }

    // Create or get the CSSTransitions by CSS property name map for this element.
    auto& cssTransitionsByProperty = m_elementToCSSTransitionByCSSPropertyID.ensure(&element, [] {
        return HashMap<CSSPropertyID, RefPtr<CSSTransition>> { };
    }).iterator->value;

    if (auto* currentTransitions = newStyle.transitions()) {
        for (size_t i = 0; i < currentTransitions->size(); ++i) {
            auto& backingAnimation = currentTransitions->animation(i);
            auto property = backingAnimation.property();
            if (property == CSSPropertyInvalid)
                continue;
            previousProperties.remove(property);
            // We've found a backing animation that we didn't know about for a valid property.
            if (!previousBackingAnimations.contains(&backingAnimation)) {
                // If we already had a CSSTransition for this property, check whether its timing properties match the current backing
                // animation's properties and whether its blending keyframes match the old and new styles. If they do, move on to the
                // next transition, otherwise delete the previous CSSTransition object, and create a new one.
                if (cssTransitionsByProperty.contains(property)) {
                    if (cssTransitionsByProperty.get(property)->matchesBackingAnimationAndStyles(backingAnimation, oldStyle, newStyle))
                        continue;
                    removeDeclarativeAnimation(cssTransitionsByProperty.take(property));
                }
                // Now we can create a new CSSTransition with the new backing animation provided it has a valid
                // duration and the from and to values are distinct.
                if (backingAnimation.duration() > 0 && oldStyle && !CSSPropertyAnimation::propertiesEqual(property, oldStyle, &newStyle))
                    cssTransitionsByProperty.set(property, CSSTransition::create(element, backingAnimation, oldStyle, newStyle));
            }
        }
    }

    // Remaining properties are no longer current and must be removed.
    for (const auto transitionPropertyToRemove : previousProperties) {
        if (cssTransitionsByProperty.contains(transitionPropertyToRemove))
            cancelOrRemoveDeclarativeAnimation(cssTransitionsByProperty.take(transitionPropertyToRemove));
    }

    // Remove the map of CSSTransitions by property for this element if it's now empty.
    if (cssTransitionsByProperty.isEmpty())
        m_elementToCSSTransitionByCSSPropertyID.remove(&element);
}

void AnimationTimeline::removeDeclarativeAnimation(RefPtr<DeclarativeAnimation> animation)
{
    animation->setEffect(nullptr);
    removeAnimation(animation.releaseNonNull());
}

void AnimationTimeline::cancelOrRemoveDeclarativeAnimation(RefPtr<DeclarativeAnimation> animation)
{
    auto phase = animation->effect()->phase();
    if (phase != AnimationEffectReadOnly::Phase::Idle && phase != AnimationEffectReadOnly::Phase::After)
        animation->cancel();
    else
        removeDeclarativeAnimation(animation);
}

String AnimationTimeline::description()
{
    TextStream stream;
    int count = 1;
    stream << (m_classType == DocumentTimelineClass ? "DocumentTimeline" : "AnimationTimeline") << " with " << m_animations.size() << " animations:";
    stream << "\n";
    for (const auto& animation : m_animations) {
        writeIndent(stream, 1);
        stream << count << ". " << animation->description();
        stream << "\n";
        count++;
    }
    return stream.release();
}

} // namespace WebCore
