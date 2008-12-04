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
#include "CSSPropertyNames.h"
#include "ImplicitAnimation.h"
#include "KeyframeAnimation.h"
#include "RenderObject.h"
#include "RenderStyle.h"

namespace WebCore {

class CompositeAnimationPrivate {
public:
    CompositeAnimationPrivate(AnimationController* animationController, CompositeAnimation* compositeAnimation)
        : m_isSuspended(false)
        , m_animationController(animationController)
        , m_compositeAnimation(compositeAnimation)
        , m_numStyleAvailableWaiters(0)
    {
    }
    
    ~CompositeAnimationPrivate();

    PassRefPtr<RenderStyle> animate(RenderObject*, RenderStyle* currentStyle, RenderStyle* targetStyle);

    void setAnimating(bool);
    bool isAnimating() const;
    
    const KeyframeAnimation* getAnimationForProperty(int property) const;

    void cleanupFinishedAnimations(RenderObject*);

    void setAnimationStartTime(double t);
    void setTransitionStartTime(int property, double t);

    void suspendAnimations();
    void resumeAnimations();
    bool isSuspended() const { return m_isSuspended; }

    void overrideImplicitAnimations(int property);
    void resumeOverriddenImplicitAnimations(int property);

    void styleAvailable();

    bool isAnimatingProperty(int property, bool isRunningNow) const;

    void setWaitingForStyleAvailable(bool);

    bool pauseAnimationAtTime(const AtomicString& name, double t);
    bool pauseTransitionAtTime(int property, double t);

protected:
    void updateTransitions(RenderObject*, RenderStyle* currentStyle, RenderStyle* targetStyle);
    void updateKeyframeAnimations(RenderObject*, RenderStyle* currentStyle, RenderStyle* targetStyle);

private:
    typedef HashMap<int, RefPtr<ImplicitAnimation> > CSSPropertyTransitionsMap;
    typedef HashMap<AtomicStringImpl*, RefPtr<KeyframeAnimation> >  AnimationNameMap;

    CSSPropertyTransitionsMap m_transitions;
    AnimationNameMap m_keyframeAnimations;
    bool m_isSuspended;
    AnimationController* m_animationController;
    CompositeAnimation* m_compositeAnimation;
    unsigned m_numStyleAvailableWaiters;
};

CompositeAnimationPrivate::~CompositeAnimationPrivate()
{
    // Clear the renderers from all running animations, in case we are in the middle of
    // an animation callback (see https://bugs.webkit.org/show_bug.cgi?id=22052)
    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* transition = it->second.get();
        transition->clearRenderer();
    }

    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        KeyframeAnimation* anim = it->second.get();
        anim->clearRenderer();
    }
    
    // Toss the refs to all animations
    m_transitions.clear();
    m_keyframeAnimations.clear();
}
    
void CompositeAnimationPrivate::updateTransitions(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // If currentStyle is null, we don't do transitions
    if (!currentStyle || !targetStyle->transitions())
        return;

    // Check to see if we need to update the active transitions
    for (size_t i = 0; i < targetStyle->transitions()->size(); ++i) {
        const Animation* anim = targetStyle->transitions()->animation(i);
        bool isActiveTransition = anim->duration() || anim->delay() > 0;

        int prop = anim->property();

        if (prop == cAnimateNone)
            continue;

        bool all = prop == cAnimateAll;

        // Handle both the 'all' and single property cases. For the single prop case, we make only one pass
        // through the loop.
        for (int propertyIndex = 0; propertyIndex < AnimationBase::getNumProperties(); ++propertyIndex) {
            if (all) {
                // Get the next property
                prop = AnimationBase::getPropertyAtIndex(propertyIndex);
            }

            // ImplicitAnimations are always hashed by actual properties, never cAnimateAll
            ASSERT(prop > firstCSSProperty && prop < (firstCSSProperty + numCSSProperties));

            // If there is a running animation for this property, the transition is overridden
            // and we have to use the unanimatedStyle from the animation. We do the test
            // against the unanimated style here, but we "override" the transition later.
            const KeyframeAnimation* keyframeAnim = getAnimationForProperty(prop);
            RenderStyle* fromStyle = keyframeAnim ? keyframeAnim->unanimatedStyle() : currentStyle;

            // See if there is a current transition for this prop
            ImplicitAnimation* implAnim = m_transitions.get(prop).get();
            bool equal = true;

            if (implAnim) {
               // This implAnim might not be an already running transition. It might be
               // newly added to the list in a previous iteration. This would happen if
               // you have both an explicit transition-property and 'all' in the same
               // list. In this case, the latter one overrides the earlier one, so we
               // behave as though this is a running animation being replaced.
                if (!isActiveTransition)
                    m_transitions.remove(prop);
                else if (!implAnim->isTargetPropertyEqual(prop, targetStyle)) {
                    m_transitions.remove(prop);
                    equal = false;
                }
            } else {
                // We need to start a transition if it is active and the properties don't match
                equal = !isActiveTransition || AnimationBase::propertiesEqual(prop, fromStyle, targetStyle);
            }

            if (!equal) {
                // Add the new transition
                m_transitions.set(prop, ImplicitAnimation::create(const_cast<Animation*>(anim), prop, renderer, m_compositeAnimation, fromStyle));
            }
            
            // We only need one pass for the single prop case
            if (!all)
                break;
        }
    }
}

void CompositeAnimationPrivate::updateKeyframeAnimations(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // Nothing to do if we don't have any animations, and didn't have any before
    if (m_keyframeAnimations.isEmpty() && !targetStyle->hasAnimations())
        return;

    // Nothing to do if the current and target animations are the same
    if (currentStyle && currentStyle->hasAnimations() && targetStyle->hasAnimations() && *(currentStyle->animations()) == *(targetStyle->animations()))
        return;
        
    // Mark all existing animations as no longer active
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it)
        it->second->setIndex(-1);
    
    // Now mark any still active animations as active and add any new animations
    if (targetStyle->animations()) {
        int numAnims = targetStyle->animations()->size();
        for (int i = 0; i < numAnims; ++i) {
            const Animation* anim = targetStyle->animations()->animation(i);
            AtomicString animationName(anim->name());

            if (!anim->isValidAnimation())
                continue;
            
            // See if there is a current animation for this name
            RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(animationName.impl());
                
            if (keyframeAnim) {
                // There is one so it is still active

                // Animations match, but play states may differ. update if needed
                keyframeAnim->updatePlayState(anim->playState() == AnimPlayStatePlaying);
                            
                // Set the saved animation to this new one, just in case the play state has changed
                keyframeAnim->setAnimation(anim);
                keyframeAnim->setIndex(i);
            } else if ((anim->duration() || anim->delay()) && anim->iterationCount()) {
                keyframeAnim = KeyframeAnimation::create(const_cast<Animation*>(anim), renderer, i, m_compositeAnimation, currentStyle ? currentStyle : targetStyle);
                m_keyframeAnimations.set(keyframeAnim->name().impl(), keyframeAnim);
            }
        }
    }

    // Make a list of animations to be removed
    Vector<AtomicStringImpl*> animsToBeRemoved;
    kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* keyframeAnim = it->second.get();
        if (keyframeAnim->index() < 0)
            animsToBeRemoved.append(keyframeAnim->name().impl());
    }
    
    // Now remove the animations from the list
    for (size_t j = 0; j < animsToBeRemoved.size(); ++j)
        m_keyframeAnimations.remove(animsToBeRemoved[j]);
}

PassRefPtr<RenderStyle> CompositeAnimationPrivate::animate(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    RefPtr<RenderStyle> resultStyle;

    // Update animations first so we can see if any transitions are overridden
    updateKeyframeAnimations(renderer, currentStyle, targetStyle);

    // We don't do any transitions if we don't have a currentStyle (on startup)
    updateTransitions(renderer, currentStyle, targetStyle);

    if (currentStyle) {
        // Now that we have transition objects ready, let them know about the new goal state.  We want them
        // to fill in a RenderStyle*& only if needed.
        CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
            if (ImplicitAnimation* anim = it->second.get())
                anim->animate(m_compositeAnimation, renderer, currentStyle, targetStyle, resultStyle);
        }
    }

    // Now that we have animation objects ready, let them know about the new goal state.  We want them
    // to fill in a RenderStyle*& only if needed.
    if (targetStyle->hasAnimations()) {
        for (size_t i = 0; i < targetStyle->animations()->size(); ++i) {
            const Animation* anim = targetStyle->animations()->animation(i);

            if (anim->isValidAnimation()) {
                AtomicString animationName(anim->name());
                RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(animationName.impl());
                if (keyframeAnim)
                    keyframeAnim->animate(m_compositeAnimation, renderer, currentStyle, targetStyle, resultStyle);
            }
        }
    }

    cleanupFinishedAnimations(renderer);

    return resultStyle ? resultStyle.release() : targetStyle;
}

// "animating" means that something is running that requires the timer to keep firing
void CompositeAnimationPrivate::setAnimating(bool animating)
{
    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* transition = it->second.get();
        transition->setAnimating(animating);
    }

    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        KeyframeAnimation* anim = it->second.get();
        anim->setAnimating(animating);
    }
}

bool CompositeAnimationPrivate::isAnimating() const
{
    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* transition = it->second.get();
        if (transition && !transition->paused() && transition->isAnimating() && transition->active())
            return true;
    }

    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        KeyframeAnimation* anim = it->second.get();
        if (anim && !anim->paused() && anim->isAnimating() && anim->active())
            return true;
    }

    return false;
}

const KeyframeAnimation* CompositeAnimationPrivate::getAnimationForProperty(int property) const
{
    const KeyframeAnimation* retval = 0;
    
    // We want to send back the last animation with the property if there are multiples.
    // So we need to iterate through all animations
    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        const KeyframeAnimation* anim = it->second.get();
        if (anim->hasAnimationForProperty(property))
            retval = anim;
    }
    
    return retval;
}

void CompositeAnimationPrivate::cleanupFinishedAnimations(RenderObject* renderer)
{
    if (isSuspended())
        return;

    // Make a list of transitions to be deleted
    Vector<int> finishedTransitions;
    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();

    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (!anim)
            continue;
        if (anim->postActive())
            finishedTransitions.append(anim->animatingProperty());
    }

    // Delete them
    size_t finishedTransitionCount = finishedTransitions.size();
    for (size_t i = 0; i < finishedTransitionCount; ++i)
        m_transitions.remove(finishedTransitions[i]);

    // Make a list of animations to be deleted
    Vector<AtomicStringImpl*> finishedAnimations;
    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();

    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        KeyframeAnimation* anim = it->second.get();
        if (!anim)
            continue;
        if (anim->postActive())
            finishedAnimations.append(anim->name().impl());
    }

    // Delete them
    size_t finishedAnimationCount = finishedAnimations.size();
    for (size_t i = 0; i < finishedAnimationCount; ++i)
        m_keyframeAnimations.remove(finishedAnimations[i]);
}

void CompositeAnimationPrivate::setAnimationStartTime(double t)
{
    // Set start time on all animations waiting for it
    AnimationNameMap::const_iterator end = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != end; ++it) {
        KeyframeAnimation* anim = it->second.get();
        if (anim && anim->waitingForStartTime())
            anim->updateStateMachine(AnimationBase::AnimationStateInputStartTimeSet, t);
    }
}

void CompositeAnimationPrivate::setTransitionStartTime(int property, double t)
{
    // Set the start time for given property transition
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->waitingForStartTime() && anim->animatingProperty() == property)
            anim->updateStateMachine(AnimationBase::AnimationStateInputStartTimeSet, t);
    }
}

void CompositeAnimationPrivate::suspendAnimations()
{
    if (m_isSuspended)
        return;

    m_isSuspended = true;

    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        if (KeyframeAnimation* anim = it->second.get())
            anim->updatePlayState(false);
    }

    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->hasStyle())
            anim->updatePlayState(false);
    }
}

void CompositeAnimationPrivate::resumeAnimations()
{
    if (!m_isSuspended)
        return;

    m_isSuspended = false;

    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        KeyframeAnimation* anim = it->second.get();
        if (anim && anim->playStatePlaying())
            anim->updatePlayState(true);
    }

    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->hasStyle())
            anim->updatePlayState(true);
    }
}

void CompositeAnimationPrivate::overrideImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->animatingProperty() == property)
            anim->setOverridden(true);
    }
}

void CompositeAnimationPrivate::resumeOverriddenImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->animatingProperty() == property)
            anim->setOverridden(false);
    }
}

static inline bool compareAnimationIndices(RefPtr<KeyframeAnimation> a, const RefPtr<KeyframeAnimation> b)
{
    return a->index() < b->index();
}

void CompositeAnimationPrivate::styleAvailable()
{
    if (m_numStyleAvailableWaiters == 0)
        return;

    // We have to go through animations in the order in which they appear in
    // the style, because order matters for additivity.
    Vector<RefPtr<KeyframeAnimation> > animations(m_keyframeAnimations.size());
    copyValuesToVector(m_keyframeAnimations, animations);

    if (animations.size() > 1)
        std::stable_sort(animations.begin(), animations.end(), compareAnimationIndices);

    for (size_t i = 0; i < animations.size(); ++i) {
        KeyframeAnimation* anim = animations[i].get();
        if (anim && anim->waitingForStyleAvailable())
            anim->updateStateMachine(AnimationBase::AnimationStateInputStyleAvailable, -1);
    }

    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->waitingForStyleAvailable())
            anim->updateStateMachine(AnimationBase::AnimationStateInputStyleAvailable, -1);
    }
}

bool CompositeAnimationPrivate::isAnimatingProperty(int property, bool isRunningNow) const
{
    AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
        KeyframeAnimation* anim = it->second.get();
        if (anim && anim->isAnimatingProperty(property, isRunningNow))
            return true;
    }

    CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
        ImplicitAnimation* anim = it->second.get();
        if (anim && anim->isAnimatingProperty(property, isRunningNow))
            return true;
    }
    return false;
}

void CompositeAnimationPrivate::setWaitingForStyleAvailable(bool waiting)
{
    if (waiting)
        m_numStyleAvailableWaiters++;
    else
        m_numStyleAvailableWaiters--;
    m_animationController->setWaitingForStyleAvailable(waiting);
}

bool CompositeAnimationPrivate::pauseAnimationAtTime(const AtomicString& name, double t)
{
    if (!name)
        return false;

    RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(name.impl());
    if (!keyframeAnim || !keyframeAnim->running())
        return false;

    int count = keyframeAnim->m_animation->iterationCount();
    if ((t >= 0.0) && (!count || (t <= count * keyframeAnim->duration()))) {
        keyframeAnim->pauseAtTime(t);
        return true;
    }

    return false;
}

bool CompositeAnimationPrivate::pauseTransitionAtTime(int property, double t)
{
    if ((property < firstCSSProperty) || (property >= firstCSSProperty + numCSSProperties))
        return false;

    ImplicitAnimation* implAnim = m_transitions.get(property).get();
    if (!implAnim || !implAnim->running())
        return false;

    if ((t >= 0.0) && (t <= implAnim->duration())) {
        implAnim->pauseAtTime(t);
        return true;
    }

    return false;
}

CompositeAnimation::CompositeAnimation(AnimationController* animationController)
    : m_data(new CompositeAnimationPrivate(animationController, this))
{
}

CompositeAnimation::~CompositeAnimation()
{
    delete m_data;
}

PassRefPtr<RenderStyle> CompositeAnimation::animate(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    return m_data->animate(renderer, currentStyle, targetStyle);
}

bool CompositeAnimation::isAnimating() const
{
    return m_data->isAnimating();
}

void CompositeAnimation::setWaitingForStyleAvailable(bool b)
{
    m_data->setWaitingForStyleAvailable(b);
}

void CompositeAnimation::suspendAnimations()
{
    m_data->suspendAnimations();
}

void CompositeAnimation::resumeAnimations()
{
    m_data->resumeAnimations();
}

bool CompositeAnimation::isSuspended() const
{
    return m_data->isSuspended();
}

void CompositeAnimation::styleAvailable()
{
    m_data->styleAvailable();
}

void CompositeAnimation::setAnimating(bool b)
{
    m_data->setAnimating(b);
}

bool CompositeAnimation::isAnimatingProperty(int property, bool isRunningNow) const
{
    return m_data->isAnimatingProperty(property, isRunningNow);
}

void CompositeAnimation::setAnimationStartTime(double t)
{
    m_data->setAnimationStartTime(t);
}

void CompositeAnimation::setTransitionStartTime(int property, double t)
{
    m_data->setTransitionStartTime(property, t);
}

void CompositeAnimation::overrideImplicitAnimations(int property)
{
    m_data->overrideImplicitAnimations(property);
}

void CompositeAnimation::resumeOverriddenImplicitAnimations(int property)
{
    m_data->resumeOverriddenImplicitAnimations(property);
}

bool CompositeAnimation::pauseAnimationAtTime(const AtomicString& name, double t)
{
    return m_data->pauseAnimationAtTime(name, t);
}

bool CompositeAnimation::pauseTransitionAtTime(int property, double t)
{
    return m_data->pauseTransitionAtTime(property, t);
}

} // namespace WebCore
