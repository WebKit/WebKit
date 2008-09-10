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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "AnimationController.h"
#include "ImplicitAnimation.h"
#include "KeyframeAnimation.h"
#include "RenderStyle.h"
#include "CSSPropertyNames.h"

namespace WebCore {

CompositeAnimation::~CompositeAnimation()
{
    deleteAllValues(m_transitions);
    deleteAllValues(m_keyframeAnimations);
}
    
void CompositeAnimation::updateTransitions(RenderObject* renderer, const RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // If currentStyle is null, we don't do transitions
    if (!currentStyle || !targetStyle->transitions())
        return;
        
    // Check to see if we need to update the active transitions
    for (size_t i = 0; i < targetStyle->transitions()->size(); ++i) {
        const Animation* anim = (*targetStyle->transitions())[i].get();
        double duration = anim->duration();
        double delay = anim->delay();

        // If this is an empty transition, skip it
        if (duration == 0 && delay <= 0)
            continue;
         
        int prop = anim->property();
        bool all = prop == cAnimateAll;
        
        // Handle both the 'all' and single property cases. For the single prop case, we make only one pass
        // through the loop
        for (int propertyIndex = 0; ; ++propertyIndex) {
            if (all) {
                if (propertyIndex >= AnimationBase::getNumProperties())
                    break;
                // get the next prop
                prop = AnimationBase::getPropertyAtIndex(propertyIndex);
            }

            // ImplicitAnimations are always hashed by actual properties, never cAnimateAll
            ASSERT(prop > firstCSSProperty && prop < (firstCSSProperty + numCSSProperties));

            // See if there is a current transition for this prop
            ImplicitAnimation* implAnim = m_transitions.get(prop);
            bool equal = true;
            
            if (implAnim) {
                // There is one, has our target changed?
                if (!implAnim->isTargetPropertyEqual(prop, targetStyle)) {
                    implAnim->reset(renderer);
                    delete implAnim;
                    m_transitions.remove(prop);
                    equal = false;
                }
            } else {
                // See if we need to start a new transition
                equal = AnimationBase::propertiesEqual(prop, currentStyle, targetStyle);
            }
            
            if (!equal) {
                // Add the new transition
                ImplicitAnimation* animation = new ImplicitAnimation(const_cast<Animation*>(anim), prop, renderer, this);
                m_transitions.set(prop, animation);
            }
            
            // We only need one pass for the single prop case
            if (!all)
                break;
        }
    }
}

void CompositeAnimation::updateKeyframeAnimations(RenderObject* renderer, const RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // Nothing to do if we don't have any animations, and didn't have any before
    if (m_keyframeAnimations.isEmpty() && !targetStyle->hasAnimations())
        return;
    
    // Nothing to do if the current and target animations are the same
    if (currentStyle && currentStyle->hasAnimations() && targetStyle->hasAnimations() && *(currentStyle->animations()) == *(targetStyle->animations()))
        return;
        
    int numAnims = 0;
    bool animsChanged = false;
    
    // see if the lists match
    if (targetStyle->animations()) {
        for (size_t i = 0; i < targetStyle->animations()->size(); ++i) {
            const Animation* anim = (*targetStyle->animations())[i].get();
            
            if (!anim->isValidAnimation())
                animsChanged = true;
            else {
                AtomicString name(anim->name());
                KeyframeAnimation* kfAnim = m_keyframeAnimations.get(name.impl());
                if (!kfAnim || !kfAnim->animationsMatch(anim))
                    animsChanged = true;
                else
                    if (anim) {
                        // animations match, but play states may differ. update if needed
                        kfAnim->updatePlayState(anim->playState() == AnimPlayStatePlaying);
                        
                        // set the saved animation to this new one, just in case the play state has changed
                        kfAnim->setAnimation(anim);
                    }
            }
            ++numAnims;
        }
    }
    
    if (!animsChanged && m_keyframeAnimations.size() != numAnims)
        animsChanged = true;

    if (!animsChanged)
        return;

    // animations have changed, update the list
    resetAnimations(renderer);

    if (!targetStyle->animations())
        return;

    // add all the new animations
    int index = 0;
    for (size_t i = 0; i < targetStyle->animations()->size(); ++i) {
        const Animation* anim = (*targetStyle->animations())[i].get();

        if (!anim->isValidAnimation())
            continue;
            
        // don't bother adding the animation if it has no keyframes or won't animate
        if ((anim->duration() || anim->delay()) && anim->iterationCount() &&
                                            anim->keyframeList().get() && !anim->keyframeList()->isEmpty()) {
            KeyframeAnimation* kfanim = new KeyframeAnimation(const_cast<Animation*>(anim), renderer, index++, this);
            m_keyframeAnimations.set(kfanim->name().impl(), kfanim);
        }
    }
}

KeyframeAnimation* CompositeAnimation::findKeyframeAnimation(const AtomicString& name)
{
    return m_keyframeAnimations.get(name.impl());
}

RenderStyle* CompositeAnimation::animate(RenderObject* renderer, const RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    RenderStyle* resultStyle = 0;
    
    // We don't do any transitions if we don't have a currentStyle (on startup)
    updateTransitions(renderer, currentStyle, targetStyle);
    
    if (currentStyle) {
        // Now that we have transition objects ready, let them know about the new goal state.  We want them
        // to fill in a RenderStyle*& only if needed.
        CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
            ImplicitAnimation*  anim = it->second;
            if (anim) {
                anim->animate(this, renderer, currentStyle, targetStyle, resultStyle);
            }
        }
    }

    updateKeyframeAnimations(renderer, currentStyle, targetStyle);

    // Now that we have animation objects ready, let them know about the new goal state.  We want them
    // to fill in a RenderStyle*& only if needed.
    if (targetStyle->hasAnimations()) {
        for (size_t i = 0; i < targetStyle->animations()->size(); ++i) {
            const Animation* anim = (*targetStyle->animations())[i].get();

            if (anim->isValidAnimation()) {
                AtomicString name(anim->name());
                KeyframeAnimation* keyframeAnim = m_keyframeAnimations.get(name.impl());
                if (keyframeAnim)
                    keyframeAnim->animate(this, renderer, currentStyle, targetStyle, resultStyle);
            }
        }
    }
    
    cleanupFinishedAnimations(renderer);

    return resultStyle ? resultStyle : targetStyle;
}

// "animating" means that something is running that requires the timer to keep firing
// (e.g. a transition)
void CompositeAnimation::setAnimating(bool inAnimating)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation*  transition = it->second;
        transition->setAnimating(inAnimating);
    }

    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        anim->setAnimating(inAnimating);
    }
}

bool CompositeAnimation::animating()
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation*  transition = it->second;
        if (transition && transition->animating() && transition->running())
            return true;
    }
    
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        if (anim && !anim->paused() && anim->animating() && anim->active())
            return true;
    }

    return false;
}

void CompositeAnimation::resetTransitions(RenderObject* renderer)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation*  transition = it->second;
        transition->reset(renderer);
        delete transition;
    }
    m_transitions.clear();
}

void CompositeAnimation::resetAnimations(RenderObject* renderer)
{
    deleteAllValues(m_keyframeAnimations);
    m_keyframeAnimations.clear();
}

void CompositeAnimation::cleanupFinishedAnimations(RenderObject* renderer)
{
    if (suspended())
        return;
    
    // Make a list of transitions to be deleted
    Vector<int> finishedTransitions;
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();

    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (!anim)
            continue;
        if (anim->postActive() && !anim->waitingForEndEvent())
            finishedTransitions.append(anim->animatingProperty());
    }
    
    // Delete them
    for (Vector<int>::iterator it = finishedTransitions.begin(); it != finishedTransitions.end(); ++it) {
        ImplicitAnimation* anim = m_transitions.get(*it);
        if (anim) {
            anim->reset(renderer);
            delete anim;
        }
        m_transitions.remove(*it);
    }

    // Make a list of animations to be deleted
    Vector<AtomicStringImpl*> finishedAnimations;
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();

    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        if (!anim)
            continue;
        if (anim->postActive() && !anim->waitingForEndEvent())
            finishedAnimations.append(anim->name().impl());
    }
    
    // delete them
    for (Vector<AtomicStringImpl*>::iterator it = finishedAnimations.begin(); it != finishedAnimations.end(); ++it) {
        KeyframeAnimation* kfanim = m_keyframeAnimations.get(*it);
        if (kfanim) {
            kfanim->reset(renderer);
            delete kfanim;
        }
        m_keyframeAnimations.remove(*it);
    }
}

void CompositeAnimation::setAnimationStartTime(double t)
{
    // set start time on all animations waiting for it
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        if (anim && anim->waitingForStartTime())
            anim->updateStateMachine(AnimationBase::AnimationStateInputStartTimeSet, t);
    }
}

void CompositeAnimation::setTransitionStartTime(int property, double t)
{
    // Set the start time for given property transition
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->waitingForStartTime() && anim->animatingProperty() == property)
            anim->updateStateMachine(AnimationBase::AnimationStateInputStartTimeSet, t);
    }
}

void CompositeAnimation::suspendAnimations()
{
    if (m_suspended)
        return;
    
    m_suspended = true;

    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        if (anim)
            anim->updatePlayState(false);
    }

    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->hasStyle())
            anim->updatePlayState(false);
    }
}

void CompositeAnimation::resumeAnimations()
{
    if (!m_suspended)
        return;
    
    m_suspended = false;
    
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        if (anim && anim->playStatePlaying())
            anim->updatePlayState(true);
    }
    
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->hasStyle())
            anim->updatePlayState(true);
    }
}

void CompositeAnimation::overrideImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->animatingProperty() == property)
            anim->setOverridden(true);
    }
}

void CompositeAnimation::resumeOverriddenImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->animatingProperty() == property)
            anim->setOverridden(false);
    }
}

static inline bool compareAnimationIndices(const KeyframeAnimation* a, const KeyframeAnimation* b)
{
    return a->index() < b->index();
}

void CompositeAnimation::styleAvailable()
{
    if (m_numStyleAvailableWaiters == 0)
        return;
    
    // We have to go through animations in the order in which they appear in
    // the style, because order matters for additivity.
    Vector<KeyframeAnimation*> animations(m_keyframeAnimations.size());
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    size_t i = 0;
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        // We can't just insert based on anim->index() because invalid animations don't
        // make it into the hash.
        animations[i++] = anim;
    }

    if (animations.size() > 1)
        std::stable_sort(animations.begin(), animations.end(), compareAnimationIndices);
    
    for (i = 0; i < animations.size(); ++i) {
        KeyframeAnimation* anim = animations[i];
        if (anim && anim->waitingForStyleAvailable())
            anim->updateStateMachine(AnimationBase::AnimationStateInputStyleAvailable, -1);
    }
    
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->waitingForStyleAvailable())
            anim->updateStateMachine(AnimationBase::AnimationStateInputStyleAvailable, -1);
    }
}

bool CompositeAnimation::isAnimatingProperty(int property, bool isRunningNow) const
{
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* anim = it->second;
        if (anim && anim->isAnimatingProperty(property, isRunningNow))
            return true;
    }
    
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second;
        if (anim && anim->isAnimatingProperty(property, isRunningNow))
            return true;
    }
    return false;
}
    
void CompositeAnimation::setWaitingForStyleAvailable(bool waiting)
{
    if (waiting)
        m_numStyleAvailableWaiters++;
    else
        m_numStyleAvailableWaiters--;
    m_animationController->setWaitingForStyleAvailable(waiting);
}
    
}
