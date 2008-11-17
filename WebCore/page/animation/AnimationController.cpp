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
#include "AnimationController.h"
#include "CompositeAnimation.h"
#include "CSSParser.h"
#include "Frame.h"
#include "Timer.h"

namespace WebCore {

static const double cAnimationTimerDelay = 0.025;

class AnimationControllerPrivate {
public:
    AnimationControllerPrivate(Frame*);
    ~AnimationControllerPrivate();

    CompositeAnimation* accessCompositeAnimation(RenderObject*);
    bool clear(RenderObject*);

    void animationTimerFired(Timer<AnimationControllerPrivate>*);
    void updateAnimationTimer();

    void updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*);
    void startUpdateRenderingDispatcher();

    bool hasAnimations() const { return !m_compositeAnimations.isEmpty(); }

    void suspendAnimations(Document*);
    void resumeAnimations(Document*);

    void styleAvailable();

    bool isAnimatingPropertyOnRenderer(RenderObject*, int property, bool isRunningNow) const;

    bool pauseAnimationAtTime(RenderObject*, const String& name, double t);
    bool pauseTransitionAtTime(RenderObject*, const String& property, double t);

private:
    typedef HashMap<RenderObject*, CompositeAnimation*> RenderObjectAnimationMap;

    RenderObjectAnimationMap m_compositeAnimations;
    Timer<AnimationControllerPrivate> m_animationTimer;
    Timer<AnimationControllerPrivate> m_updateRenderingDispatcher;
    Frame* m_frame;
};

AnimationControllerPrivate::AnimationControllerPrivate(Frame* frame)
    : m_animationTimer(this, &AnimationControllerPrivate::animationTimerFired)
    , m_updateRenderingDispatcher(this, &AnimationControllerPrivate::updateRenderingDispatcherFired)
    , m_frame(frame)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
    deleteAllValues(m_compositeAnimations);
}

CompositeAnimation* AnimationControllerPrivate::accessCompositeAnimation(RenderObject* renderer)
{
    CompositeAnimation* animation = m_compositeAnimations.get(renderer);
    if (!animation) {
        animation = new CompositeAnimation(m_frame->animation());
        m_compositeAnimations.set(renderer, animation);
    }
    return animation;
}

bool AnimationControllerPrivate::clear(RenderObject* renderer)
{
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setChanged() when suspended).
    CompositeAnimation* animation = m_compositeAnimations.take(renderer);
    if (!animation)
        return false;
    animation->resetTransitions(renderer);
    bool wasSuspended = animation->isSuspended();
    delete animation;
    return !wasSuspended;
}

void AnimationControllerPrivate::styleAvailable()
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it)
        it->second->styleAvailable();
}

void AnimationControllerPrivate::updateAnimationTimer()
{
    bool isAnimating = false;

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second;
        if (!compAnim->isSuspended() && compAnim->isAnimating()) {
            isAnimating = true;
            break;
        }
    }
    
    if (isAnimating) {
        if (!m_animationTimer.isActive())
            m_animationTimer.startRepeating(cAnimationTimerDelay);
    } else if (m_animationTimer.isActive())
        m_animationTimer.stop();
}

void AnimationControllerPrivate::updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*)
{
    if (m_frame && m_frame->document())
        m_frame->document()->updateRendering();
}

void AnimationControllerPrivate::startUpdateRenderingDispatcher()
{
    if (!m_updateRenderingDispatcher.isActive())
        m_updateRenderingDispatcher.startOneShot(0);
}

void AnimationControllerPrivate::animationTimerFired(Timer<AnimationControllerPrivate>* timer)
{
    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateRendering.  It will then call back to us with new information.
    bool isAnimating = false;
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second;
        if (!compAnim->isSuspended() && compAnim->isAnimating()) {
            isAnimating = true;
            compAnim->setAnimating(false);

            Node* node = it->first->element();
            ASSERT(!node || (node->document() && !node->document()->inPageCache()));
            node->setChanged(AnimationStyleChange);
        }
    }

    m_frame->document()->updateRendering();

    updateAnimationTimer();
}

bool AnimationControllerPrivate::isAnimatingPropertyOnRenderer(RenderObject* renderer, int property, bool isRunningNow) const
{
    CompositeAnimation* animation = m_compositeAnimations.get(renderer);
    if (!animation)
        return false;

    return animation->isAnimatingProperty(property, isRunningNow);
}

void AnimationControllerPrivate::suspendAnimations(Document* document)
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        CompositeAnimation* compAnim = it->second;
        if (renderer->document() == document)
            compAnim->suspendAnimations();
    }
    
    updateAnimationTimer();
}

void AnimationControllerPrivate::resumeAnimations(Document* document)
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        CompositeAnimation* compAnim = it->second;
        if (renderer->document() == document)
            compAnim->resumeAnimations();
    }
    
    updateAnimationTimer();
}

bool AnimationControllerPrivate::pauseAnimationAtTime(RenderObject* renderer, const String& name, double t)
{
    if (!renderer)
        return false;

    CompositeAnimation* compAnim = accessCompositeAnimation(renderer);
    if (!compAnim)
        return false;

    if (compAnim->pauseAnimationAtTime(name, t)) {
        renderer->node()->setChanged(AnimationStyleChange);
        return true;
    }

    return false;
}

bool AnimationControllerPrivate::pauseTransitionAtTime(RenderObject* renderer, const String& property, double t)
{
    if (!renderer)
        return false;

    CompositeAnimation* compAnim = accessCompositeAnimation(renderer);
    if (!compAnim)
        return false;

    if (compAnim->pauseTransitionAtTime(cssPropertyID(property), t)) {
        renderer->node()->setChanged(AnimationStyleChange);
        return true;
    }

    return false;
}

AnimationController::AnimationController(Frame* frame)
    : m_data(new AnimationControllerPrivate(frame))
    , m_numStyleAvailableWaiters(0)
{
}

AnimationController::~AnimationController()
{
    delete m_data;
}

void AnimationController::cancelAnimations(RenderObject* renderer)
{
    if (!m_data->hasAnimations())
        return;

    if (m_data->clear(renderer)) {
        Node* node = renderer->element();
        ASSERT(!node || (node->document() && !node->document()->inPageCache()));
        node->setChanged(AnimationStyleChange);
    }
}

PassRefPtr<RenderStyle> AnimationController::updateAnimations(RenderObject* renderer, RenderStyle* newStyle)
{    
    // Don't do anything if we're in the cache
    if (!renderer->document() || renderer->document()->inPageCache())
        return newStyle;

    RenderStyle* oldStyle = renderer->style();

    if ((!oldStyle || (!oldStyle->animations() && !oldStyle->transitions())) && (!newStyle->animations() && !newStyle->transitions()))
        return newStyle;

    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.
    ASSERT(renderer->element()); // FIXME: We do not animate generated content yet.

    CompositeAnimation* rendererAnimations = m_data->accessCompositeAnimation(renderer);
    RefPtr<RenderStyle> blendedStyle = rendererAnimations->animate(renderer, oldStyle, newStyle);

    m_data->updateAnimationTimer();

    if (blendedStyle != newStyle) {
        // If the animations/transitions change opacity or transform, we neeed to update
        // the style to impose the stacking rules. Note that this is also
        // done in CSSStyleSelector::adjustRenderStyle().
        if (blendedStyle->hasAutoZIndex() && (blendedStyle->opacity() < 1.0f || blendedStyle->hasTransform()))
            blendedStyle->setZIndex(0);
    }
    return blendedStyle.release();
}

void AnimationController::setAnimationStartTime(RenderObject* renderer, double t)
{
    CompositeAnimation* rendererAnimations = m_data->accessCompositeAnimation(renderer);
    rendererAnimations->setAnimationStartTime(t);
}

void AnimationController::setTransitionStartTime(RenderObject* renderer, int property, double t)
{
    CompositeAnimation* rendererAnimations = m_data->accessCompositeAnimation(renderer);
    rendererAnimations->setTransitionStartTime(property, t);
}

bool AnimationController::pauseAnimationAtTime(RenderObject* renderer, const String& name, double t)
{
    return m_data->pauseAnimationAtTime(renderer, name, t);
}

bool AnimationController::pauseTransitionAtTime(RenderObject* renderer, const String& property, double t)
{
    return m_data->pauseTransitionAtTime(renderer, property, t);
}

bool AnimationController::isAnimatingPropertyOnRenderer(RenderObject* renderer, int property, bool isRunningNow) const
{
    return m_data->isAnimatingPropertyOnRenderer(renderer, property, isRunningNow);
}

void AnimationController::suspendAnimations(Document* document)
{
    m_data->suspendAnimations(document);
}

void AnimationController::resumeAnimations(Document* document)
{
    m_data->resumeAnimations(document);
}

void AnimationController::startUpdateRenderingDispatcher()
{
    m_data->startUpdateRenderingDispatcher();
}

void AnimationController::styleAvailable()
{
    if (!m_numStyleAvailableWaiters)
        return;

    m_data->styleAvailable();
}

} // namespace WebCore
