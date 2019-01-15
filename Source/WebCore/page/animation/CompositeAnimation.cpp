/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CompositeAnimation.h"

#include "CSSAnimationControllerPrivate.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyNames.h"
#include "ImplicitAnimation.h"
#include "KeyframeAnimation.h"
#include "Logging.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

CompositeAnimation::CompositeAnimation(CSSAnimationControllerPrivate& animationController)
    : m_animationController(animationController)
{
    m_suspended = m_animationController.isSuspended() && !m_animationController.allowsNewAnimationsWhileSuspended();
}

CompositeAnimation::~CompositeAnimation()
{
    // Toss the refs to all animations, but make sure we remove them from
    // any waiting lists first.

    clearElement();
    m_transitions.clear();
    m_keyframeAnimations.clear();
}

void CompositeAnimation::clearElement()
{
    if (!m_transitions.isEmpty()) {
        // Clear the renderers from all running animations, in case we are in the middle of
        // an animation callback (see https://bugs.webkit.org/show_bug.cgi?id=22052)
        for (auto& transition : m_transitions.values()) {
            animationController().animationWillBeRemoved(transition.get());
            transition->clear();
        }
    }
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            animationController().animationWillBeRemoved(animation.get());
            animation->clear();
        }
    }
}

void CompositeAnimation::updateTransitions(Element& element, const RenderStyle* currentStyle, const RenderStyle& targetStyle)
{
    // If currentStyle is null or there are no old or new transitions, just skip it
    if (!currentStyle || (!targetStyle.transitions() && m_transitions.isEmpty()))
        return;

    // Mark all existing transitions as no longer active. We will mark the still active ones
    // in the next loop and then toss the ones that didn't get marked.
    for (auto& transition : m_transitions.values())
        transition->setActive(false);
        
    std::unique_ptr<RenderStyle> modifiedCurrentStyle;
    
    // Check to see if we need to update the active transitions
    if (targetStyle.transitions()) {
        for (size_t i = 0; i < targetStyle.transitions()->size(); ++i) {
            auto& animation = targetStyle.transitions()->animation(i);
            bool isActiveTransition = animation.duration() || animation.delay() > 0;

            Animation::AnimationMode mode = animation.animationMode();
            if (mode == Animation::AnimateNone || mode == Animation::AnimateUnknownProperty)
                continue;

            CSSPropertyID prop = animation.property();

            bool all = mode == Animation::AnimateAll;

            // Handle both the 'all' and single property cases. For the single prop case, we make only one pass
            // through the loop.
            for (int propertyIndex = 0; propertyIndex < CSSPropertyAnimation::getNumProperties(); ++propertyIndex) {
                if (all) {
                    // Get the next property which is not a shorthand.
                    Optional<bool> isShorthand;
                    prop = CSSPropertyAnimation::getPropertyAtIndex(propertyIndex, isShorthand);
                    if (isShorthand && *isShorthand)
                        continue;
                }

                if (prop == CSSPropertyInvalid) {
                    if (!all)
                        break;
                    continue;
                }
                
                // ImplicitAnimations are always hashed by actual properties, never animateAll.
                ASSERT(prop >= firstCSSProperty && prop < (firstCSSProperty + numCSSProperties));

                // If there is a running animation for this property, the transition is overridden
                // and we have to use the unanimatedStyle from the animation. We do the test
                // against the unanimated style here, but we "override" the transition later.
                auto* keyframeAnimation = animationForProperty(prop);
                auto* fromStyle = keyframeAnimation ? &keyframeAnimation->unanimatedStyle() : currentStyle;

                // See if there is a current transition for this prop
                ImplicitAnimation* implAnim = m_transitions.get(prop);
                bool equal = true;

                if (implAnim) {
                    // If we are post active don't bother setting the active flag. This will cause
                    // this animation to get removed at the end of this function.
                    if (!implAnim->postActive())
                        implAnim->setActive(true);
                    
                    // This might be a transition that is just finishing. That would be the case
                    // if it were postActive. But we still need to check for equality because
                    // it could be just finishing AND changing to a new goal state.
                    //
                    // This implAnim might also not be an already running transition. It might be
                    // newly added to the list in a previous iteration. This would happen if
                    // you have both an explicit transition-property and 'all' in the same
                    // list. In this case, the latter one overrides the earlier one, so we
                    // behave as though this is a running animation being replaced.
                    if (!implAnim->isTargetPropertyEqual(prop, &targetStyle)) {
                        // For accelerated animations we need to return a new RenderStyle with the _current_ value
                        // of the property, so that restarted transitions use the correct starting point.
                        if (CSSPropertyAnimation::animationOfPropertyIsAccelerated(prop) && implAnim->isAccelerated()) {
                            if (!modifiedCurrentStyle)
                                modifiedCurrentStyle = RenderStyle::clonePtr(*currentStyle);

                            implAnim->blendPropertyValueInStyle(prop, modifiedCurrentStyle.get());
                        }
                        LOG(Animations, "Removing existing ImplicitAnimation %p for property %s", implAnim, getPropertyName(prop));
                        animationController().animationWillBeRemoved(implAnim);
                        m_transitions.remove(prop);
                        equal = false;
                    }
                } else {
                    // We need to start a transition if it is active and the properties don't match
                    equal = !isActiveTransition || CSSPropertyAnimation::propertiesEqual(prop, fromStyle, &targetStyle) || !CSSPropertyAnimation::canPropertyBeInterpolated(prop, fromStyle, &targetStyle);
                }

                // We can be in this loop with an inactive transition (!isActiveTransition). We need
                // to do that to check to see if we are canceling a transition. But we don't want to
                // start one of the inactive transitions. So short circuit that here. (See
                // <https://bugs.webkit.org/show_bug.cgi?id=24787>
                if (!equal && isActiveTransition) {
                    // Add the new transition
                    auto implicitAnimation = ImplicitAnimation::create(animation, prop, element, *this, modifiedCurrentStyle ? *modifiedCurrentStyle : *fromStyle);
                    if (m_suspended && implicitAnimation->hasStyle())
                        implicitAnimation->updatePlayState(AnimationPlayState::Paused);

                    LOG(Animations, "Created ImplicitAnimation %p on element %p for property %s duration %.2f delay %.2f", implicitAnimation.ptr(), &element, getPropertyName(prop), animation.duration(), animation.delay());
                    m_transitions.set(prop, WTFMove(implicitAnimation));
                }

                // We only need one pass for the single prop case
                if (!all)
                    break;
            }
        }
    }

    // Make a list of transitions to be removed
    Vector<int> toBeRemoved;
    for (auto& transition : m_transitions.values()) {
        if (!transition->active()) {
            animationController().animationWillBeRemoved(transition.get());
            toBeRemoved.append(transition->animatingProperty());
            LOG(Animations, "Removing ImplicitAnimation %p from element %p for property %s", transition.get(), &element, getPropertyName(transition->animatingProperty()));
        }
    }

    // Now remove the transitions from the list
    for (auto propertyToRemove : toBeRemoved)
        m_transitions.remove(propertyToRemove);
}

void CompositeAnimation::updateKeyframeAnimations(Element& element, const RenderStyle* currentStyle, const RenderStyle& targetStyle)
{
    // Nothing to do if we don't have any animations, and didn't have any before
    if (m_keyframeAnimations.isEmpty() && !targetStyle.hasAnimations())
        return;

    m_keyframeAnimations.checkConsistency();
    
    if (currentStyle && currentStyle->hasAnimations() && targetStyle.hasAnimations() && *(currentStyle->animations()) == *(targetStyle.animations()))
        return;

    AnimationNameMap newAnimations;

    // Toss the animation order map.
    m_keyframeAnimationOrderMap.clear();

    static NeverDestroyed<const AtomicString> none("none", AtomicString::ConstructFromLiteral);
    
    // Now mark any still active animations as active and add any new animations.
    if (targetStyle.animations()) {
        int numAnims = targetStyle.animations()->size();
        for (int i = 0; i < numAnims; ++i) {
            auto& animation = targetStyle.animations()->animation(i);
            AtomicString animationName(animation.name());

            if (!animation.isValidAnimation())
                continue;
            
            // See if there is a current animation for this name.
            RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(animationName.impl());
            if (keyframeAnim) {
                newAnimations.add(keyframeAnim->name().impl(), keyframeAnim);

                if (keyframeAnim->postActive())
                    continue;

                // Animations match, but play states may differ. Update if needed.
                keyframeAnim->updatePlayState(animation.playState());

                // Set the saved animation to this new one, just in case the play state has changed.
                keyframeAnim->setAnimation(animation);
            } else if ((animation.duration() || animation.delay()) && animation.iterationCount() && animationName != none) {
                keyframeAnim = KeyframeAnimation::create(animation, element, *this, targetStyle);
                LOG(Animations, "Creating KeyframeAnimation %p on element %p with keyframes %s, duration %.2f, delay %.2f, iterations %.2f", keyframeAnim.get(), &element, animation.name().utf8().data(), animation.duration(), animation.delay(), animation.iterationCount());

                if (m_suspended) {
                    keyframeAnim->updatePlayState(AnimationPlayState::Paused);
                    LOG(Animations, "  (created in suspended/paused state)");
                }
#if !LOG_DISABLED
                for (auto propertyID : keyframeAnim->keyframes().properties())
                    LOG(Animations, "  property %s", getPropertyName(propertyID));
#endif

                newAnimations.set(keyframeAnim->name().impl(), keyframeAnim);
            }
            
            // Add this to the animation order map.
            if (keyframeAnim)
                m_keyframeAnimationOrderMap.append(keyframeAnim->name().impl());
        }
    }
    
    // Make a list of animations to be removed.
    for (auto& animation : m_keyframeAnimations.values()) {
        if (!newAnimations.contains(animation->name().impl())) {
            animationController().animationWillBeRemoved(animation.get());
            animation->clear();
            LOG(Animations, "Removing KeyframeAnimation %p from element %p", animation.get(), &element);
        }
    }
    
    std::swap(newAnimations, m_keyframeAnimations);
}

AnimationUpdate CompositeAnimation::animate(Element& element, const RenderStyle* currentStyle, const RenderStyle& targetStyle)
{
    // We don't do any transitions if we don't have a currentStyle (on startup).
    updateTransitions(element, currentStyle, targetStyle);
    updateKeyframeAnimations(element, currentStyle, targetStyle);
    m_keyframeAnimations.checkConsistency();

    bool animationStateChanged = false;
    bool forceStackingContext = false;

    std::unique_ptr<RenderStyle> animatedStyle;

    if (currentStyle) {
        // Now that we have transition objects ready, let them know about the new goal state.  We want them
        // to fill in a RenderStyle*& only if needed.
        bool checkForStackingContext = false;
        for (auto& transition : m_transitions.values()) {
            bool didBlendStyle = false;
            if (transition->animate(*this, targetStyle, animatedStyle, didBlendStyle))
                animationStateChanged = true;

            if (didBlendStyle)
                checkForStackingContext |= WillChangeData::propertyCreatesStackingContext(transition->animatingProperty());
        }

        if (animatedStyle && checkForStackingContext) {
            // Note that this is similar to code in StyleResolver::adjustRenderStyle() but only needs to consult
            // animatable properties that can trigger stacking context.
            if (animatedStyle->opacity() < 1.0f
                || animatedStyle->hasTransformRelatedProperty()
                || animatedStyle->hasMask()
                || animatedStyle->clipPath()
                || animatedStyle->boxReflect()
                || animatedStyle->hasFilter()
#if ENABLE(FILTERS_LEVEL_2)
                || animatedStyle->hasBackdropFilter()
#endif
                )
            forceStackingContext = true;
        }
    }

    // Now that we have animation objects ready, let them know about the new goal state.  We want them
    // to fill in a RenderStyle*& only if needed.
    for (auto& name : m_keyframeAnimationOrderMap) {
        RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(name);
        if (keyframeAnim) {
            bool didBlendStyle = false;
            if (keyframeAnim->animate(*this, targetStyle, animatedStyle, didBlendStyle))
                animationStateChanged = true;

            forceStackingContext |= didBlendStyle && keyframeAnim->triggersStackingContext();
            m_hasAnimationThatDependsOnLayout |= keyframeAnim->dependsOnLayout();
        }
    }

    // https://drafts.csswg.org/css-animations-1/
    // While an animation is applied but has not finished, or has finished but has an animation-fill-mode of forwards or both,
    // the user agent must act as if the will-change property ([css-will-change-1]) on the element additionally
    // includes all the properties animated by the animation.
    if (forceStackingContext && animatedStyle) {
        if (animatedStyle->hasAutoZIndex())
            animatedStyle->setZIndex(0);
    }

    return { WTFMove(animatedStyle), animationStateChanged };
}

std::unique_ptr<RenderStyle> CompositeAnimation::getAnimatedStyle() const
{
    std::unique_ptr<RenderStyle> resultStyle;
    for (auto& transition : m_transitions.values())
        transition->getAnimatedStyle(resultStyle);

    m_keyframeAnimations.checkConsistency();

    for (auto& name : m_keyframeAnimationOrderMap) {
        RefPtr<KeyframeAnimation> keyframeAnimation = m_keyframeAnimations.get(name);
        if (keyframeAnimation)
            keyframeAnimation->getAnimatedStyle(resultStyle);
    }
    
    return resultStyle;
}

Optional<Seconds> CompositeAnimation::timeToNextService() const
{
    // Returns the time at which next service is required. WTF::nullopt means no service is required. 0 means
    // service is required now, and > 0 means service is required that many seconds in the future.
    Optional<Seconds> minT;
    
    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            Optional<Seconds> t = transition->timeToNextService();
            if (!t)
                continue;
            if (!minT || t.value() < minT.value())
                minT = t.value();
            if (minT.value() == 0_s)
                return 0_s;
        }
    }
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            Optional<Seconds> t = animation->timeToNextService();
            if (!t)
                continue;
            if (!minT || t.value() < minT.value())
                minT = t.value();
            if (minT.value() == 0_s)
                return 0_s;
        }
    }

    return minT;
}

KeyframeAnimation* CompositeAnimation::animationForProperty(CSSPropertyID property) const
{
    KeyframeAnimation* result = nullptr;

    // We want to send back the last animation with the property if there are multiples.
    // So we need to iterate through all animations
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            if (animation->hasAnimationForProperty(property))
                result = animation.get();
        }
    }

    return result;
}

bool CompositeAnimation::computeExtentOfTransformAnimation(LayoutRect& bounds) const
{
    // If more than one transition and animation affect transform, give up.
    bool seenTransformAnimation = false;
    
    for (auto& animation : m_keyframeAnimations.values()) {
        if (!animation->hasAnimationForProperty(CSSPropertyTransform))
            continue;

        if (seenTransformAnimation)
            return false;

        seenTransformAnimation = true;

        if (!animation->computeExtentOfTransformAnimation(bounds))
            return false;
    }

    for (auto& transition : m_transitions.values()) {
        if (transition->animatingProperty() != CSSPropertyTransform || !transition->hasStyle())
            continue;

        if (seenTransformAnimation)
            return false;

        if (!transition->computeExtentOfTransformAnimation(bounds))
            return false;
    }
    
    return true;
}

void CompositeAnimation::suspendAnimations()
{
    if (m_suspended)
        return;

    m_suspended = true;

    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values())
            animation->updatePlayState(AnimationPlayState::Paused);
    }

    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->hasStyle())
                transition->updatePlayState(AnimationPlayState::Paused);
        }
    }
}

void CompositeAnimation::resumeAnimations()
{
    if (!m_suspended)
        return;

    m_suspended = false;

    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            if (animation->playStatePlaying())
                animation->updatePlayState(AnimationPlayState::Playing);
        }
    }

    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->hasStyle())
                transition->updatePlayState(AnimationPlayState::Playing);
        }
    }
}

void CompositeAnimation::overrideImplicitAnimations(CSSPropertyID property)
{
    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->animatingProperty() == property)
                transition->setOverridden(true);
        }
    }
}

void CompositeAnimation::resumeOverriddenImplicitAnimations(CSSPropertyID property)
{
    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->animatingProperty() == property)
                transition->setOverridden(false);
        }
    }
}

bool CompositeAnimation::isAnimatingProperty(CSSPropertyID property, bool acceleratedOnly) const
{
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            if (animation->isAnimatingProperty(property, acceleratedOnly))
                return true;
        }
    }

    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->isAnimatingProperty(property, acceleratedOnly))
                return true;
        }
    }
    return false;
}

bool CompositeAnimation::pauseAnimationAtTime(const AtomicString& name, double t)
{
    m_keyframeAnimations.checkConsistency();

    RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(name.impl());
    if (!keyframeAnim || !keyframeAnim->running())
        return false;

    keyframeAnim->freezeAtTime(t);
    return true;
}

bool CompositeAnimation::pauseTransitionAtTime(CSSPropertyID property, double t)
{
    if ((property < firstCSSProperty) || (property >= firstCSSProperty + numCSSProperties))
        return false;

    ImplicitAnimation* implAnim = m_transitions.get(property);
    if (!implAnim) {
        // Check to see if this property is being animated via a shorthand.
        // This code is only used for testing, so performance is not critical here.
        HashSet<CSSPropertyID> shorthandProperties = CSSPropertyAnimation::animatableShorthandsAffectingProperty(property);
        bool anyPaused = false;
        for (auto propertyID : shorthandProperties) {
            if (pauseTransitionAtTime(propertyID, t))
                anyPaused = true;
        }
        return anyPaused;
    }

    if (!implAnim->running())
        return false;

    if ((t >= 0.0) && (t <= implAnim->duration())) {
        implAnim->freezeAtTime(t);
        return true;
    }

    return false;
}

unsigned CompositeAnimation::numberOfActiveAnimations() const
{
    unsigned count = 0;
    
    m_keyframeAnimations.checkConsistency();
    for (auto& animation : m_keyframeAnimations.values()) {
        if (animation->running())
            ++count;
    }

    for (auto& transition : m_transitions.values()) {
        if (transition->running())
            ++count;
    }
    
    return count;
}

} // namespace WebCore
