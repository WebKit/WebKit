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

#include "AnimationControllerPrivate.h"
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

CompositeAnimation::CompositeAnimation(AnimationControllerPrivate& animationController)
    : m_animationController(animationController)
{
    m_suspended = m_animationController.isSuspended() && !m_animationController.allowsNewAnimationsWhileSuspended();
}

CompositeAnimation::~CompositeAnimation()
{
    // Toss the refs to all animations, but make sure we remove them from
    // any waiting lists first.

    clearRenderer();
    m_transitions.clear();
    m_keyframeAnimations.clear();
}

void CompositeAnimation::clearRenderer()
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

void CompositeAnimation::updateTransitions(RenderElement* renderer, const RenderStyle* currentStyle, const RenderStyle* targetStyle)
{
    // If currentStyle is null or there are no old or new transitions, just skip it
    if (!currentStyle || (!targetStyle->transitions() && m_transitions.isEmpty()))
        return;

    // Mark all existing transitions as no longer active. We will mark the still active ones
    // in the next loop and then toss the ones that didn't get marked.
    for (auto& transition : m_transitions.values())
        transition->setActive(false);
        
    std::unique_ptr<RenderStyle> modifiedCurrentStyle;
    
    // Check to see if we need to update the active transitions
    if (targetStyle->transitions()) {
        for (size_t i = 0; i < targetStyle->transitions()->size(); ++i) {
            auto& animation = targetStyle->transitions()->animation(i);
            bool isActiveTransition = !m_suspended && (animation.duration() || animation.delay() > 0);

            Animation::AnimationMode mode = animation.animationMode();
            if (mode == Animation::AnimateNone)
                continue;

            CSSPropertyID prop = animation.property();

            bool all = mode == Animation::AnimateAll;

            // Handle both the 'all' and single property cases. For the single prop case, we make only one pass
            // through the loop.
            for (int propertyIndex = 0; propertyIndex < CSSPropertyAnimation::getNumProperties(); ++propertyIndex) {
                if (all) {
                    // Get the next property which is not a shorthand.
                    bool isShorthand;
                    prop = CSSPropertyAnimation::getPropertyAtIndex(propertyIndex, isShorthand);
                    if (isShorthand)
                        continue;
                }

                // ImplicitAnimations are always hashed by actual properties, never animateAll.
                ASSERT(prop >= firstCSSProperty && prop < (firstCSSProperty + numCSSProperties));

                // If there is a running animation for this property, the transition is overridden
                // and we have to use the unanimatedStyle from the animation. We do the test
                // against the unanimated style here, but we "override" the transition later.
                RefPtr<KeyframeAnimation> keyframeAnim = getAnimationForProperty(prop);
                auto* fromStyle = keyframeAnim ? keyframeAnim->unanimatedStyle() : currentStyle;

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
                    if (!implAnim->isTargetPropertyEqual(prop, targetStyle)) {
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
                    equal = !isActiveTransition || CSSPropertyAnimation::propertiesEqual(prop, fromStyle, targetStyle);
                }

                // We can be in this loop with an inactive transition (!isActiveTransition). We need
                // to do that to check to see if we are canceling a transition. But we don't want to
                // start one of the inactive transitions. So short circuit that here. (See
                // <https://bugs.webkit.org/show_bug.cgi?id=24787>
                if (!equal && isActiveTransition) {
                    // Add the new transition
                    auto implicitAnimation = ImplicitAnimation::create(animation, prop, renderer, this, modifiedCurrentStyle ? modifiedCurrentStyle.get() : fromStyle);
                    LOG(Animations, "Created ImplicitAnimation %p on renderer %p for property %s duration %.2f delay %.2f", implicitAnimation.ptr(), renderer, getPropertyName(prop), animation.duration(), animation.delay());
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
            LOG(Animations, "Removing ImplicitAnimation %p from renderer %p for property %s", transition.get(), renderer, getPropertyName(transition->animatingProperty()));
        }
    }

    // Now remove the transitions from the list
    for (auto propertyToRemove : toBeRemoved)
        m_transitions.remove(propertyToRemove);
}

void CompositeAnimation::updateKeyframeAnimations(RenderElement* renderer, const RenderStyle* currentStyle, const RenderStyle* targetStyle)
{
    // Nothing to do if we don't have any animations, and didn't have any before
    if (m_keyframeAnimations.isEmpty() && !targetStyle->hasAnimations())
        return;

    m_keyframeAnimations.checkConsistency();
    
    if (currentStyle && currentStyle->hasAnimations() && targetStyle->hasAnimations() && *(currentStyle->animations()) == *(targetStyle->animations()))
        return;

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    m_hasScrollTriggeredAnimation = false;
#endif

    AnimationNameMap newAnimations;

    // Toss the animation order map.
    m_keyframeAnimationOrderMap.clear();

    static NeverDestroyed<const AtomicString> none("none", AtomicString::ConstructFromLiteral);
    
    // Now mark any still active animations as active and add any new animations.
    if (targetStyle->animations()) {
        int numAnims = targetStyle->animations()->size();
        for (int i = 0; i < numAnims; ++i) {
            auto& animation = targetStyle->animations()->animation(i);
            AtomicString animationName(animation.name());

            if (!animation.isValidAnimation())
                continue;
            
            // See if there is a current animation for this name.
            RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(animationName.impl());
            if (keyframeAnim) {
                newAnimations.add(keyframeAnim->name().impl(), keyframeAnim);

                if (keyframeAnim->postActive())
                    continue;

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
                if (animation.trigger()->isScrollAnimationTrigger())
                    m_hasScrollTriggeredAnimation = true;
#endif

                // Animations match, but play states may differ. Update if needed.
                keyframeAnim->updatePlayState(animation.playState());

                // Set the saved animation to this new one, just in case the play state has changed.
                keyframeAnim->setAnimation(animation);
            } else if ((animation.duration() || animation.delay()) && animation.iterationCount() && animationName != none) {
                keyframeAnim = KeyframeAnimation::create(animation, renderer, this, targetStyle);
                LOG(Animations, "Creating KeyframeAnimation %p on renderer %p with keyframes %s, duration %.2f, delay %.2f, iterations %.2f", keyframeAnim.get(), renderer, animation.name().utf8().data(), animation.duration(), animation.delay(), animation.iterationCount());

                if (m_suspended) {
                    keyframeAnim->updatePlayState(AnimPlayStatePaused);
                    LOG(Animations, "  (created in suspended/paused state)");
                }
#if !LOG_DISABLED
                for (auto propertyID : keyframeAnim->keyframes().properties())
                    LOG(Animations, "  property %s", getPropertyName(propertyID));
#endif

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
                if (animation.trigger()->isScrollAnimationTrigger())
                    m_hasScrollTriggeredAnimation = true;
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
            LOG(Animations, "Removing KeyframeAnimation %p from renderer %p", animation.get(), renderer);
        }
    }
    
    std::swap(newAnimations, m_keyframeAnimations);
}

bool CompositeAnimation::animate(RenderElement& renderer, const RenderStyle* currentStyle, const RenderStyle& targetStyle, std::unique_ptr<RenderStyle>& blendedStyle)
{
    // We don't do any transitions if we don't have a currentStyle (on startup).
    updateTransitions(&renderer, currentStyle, &targetStyle);
    updateKeyframeAnimations(&renderer, currentStyle, &targetStyle);
    m_keyframeAnimations.checkConsistency();

    bool animationStateChanged = false;

    if (currentStyle) {
        // Now that we have transition objects ready, let them know about the new goal state.  We want them
        // to fill in a RenderStyle*& only if needed.
        for (auto& transition : m_transitions.values()) {
            if (transition->animate(this, &renderer, currentStyle, &targetStyle, blendedStyle))
                animationStateChanged = true;
        }
    }

    // Now that we have animation objects ready, let them know about the new goal state.  We want them
    // to fill in a RenderStyle*& only if needed.
    for (auto& name : m_keyframeAnimationOrderMap) {
        RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(name);
        if (keyframeAnim && keyframeAnim->animate(this, &renderer, currentStyle, &targetStyle, blendedStyle))
            animationStateChanged = true;
    }

    return animationStateChanged;
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

double CompositeAnimation::timeToNextService() const
{
    // Returns the time at which next service is required. -1 means no service is required. 0 means 
    // service is required now, and > 0 means service is required that many seconds in the future.
    double minT = -1;
    
    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            double t = transition->timeToNextService();
            if (t < minT || minT == -1)
                minT = t;
            if (minT == 0)
                return 0;
        }
    }
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            double t = animation->timeToNextService();
            if (t < minT || minT == -1)
                minT = t;
            if (minT == 0)
                return 0;
        }
    }

    return minT;
}

PassRefPtr<KeyframeAnimation> CompositeAnimation::getAnimationForProperty(CSSPropertyID property) const
{
    RefPtr<KeyframeAnimation> retval;
    
    // We want to send back the last animation with the property if there are multiples.
    // So we need to iterate through all animations
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            if (animation->hasAnimationForProperty(property))
                retval = animation;
        }
    }
    
    return retval;
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
            animation->updatePlayState(AnimPlayStatePaused);
    }

    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->hasStyle())
                transition->updatePlayState(AnimPlayStatePaused);
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
                animation->updatePlayState(AnimPlayStatePlaying);
        }
    }

    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->hasStyle())
                transition->updatePlayState(AnimPlayStatePlaying);
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

bool CompositeAnimation::isAnimatingProperty(CSSPropertyID property, bool acceleratedOnly, AnimationBase::RunningState runningState) const
{
    if (!m_keyframeAnimations.isEmpty()) {
        m_keyframeAnimations.checkConsistency();
        for (auto& animation : m_keyframeAnimations.values()) {
            if (animation->isAnimatingProperty(property, acceleratedOnly, runningState))
                return true;
        }
    }

    if (!m_transitions.isEmpty()) {
        for (auto& transition : m_transitions.values()) {
            if (transition->isAnimatingProperty(property, acceleratedOnly, runningState))
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
