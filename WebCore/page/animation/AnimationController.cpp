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
#include "EventNames.h"
#include "Frame.h"
#include "Timer.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const double cAnimationTimerDelay = 0.025;
static const double cBeginAnimationUpdateTimeNotSet = -1;

class AnimationControllerPrivate {
public:
    AnimationControllerPrivate(Frame*);
    ~AnimationControllerPrivate();

    PassRefPtr<CompositeAnimation> accessCompositeAnimation(RenderObject*);
    bool clear(RenderObject*);

    void animationTimerFired(Timer<AnimationControllerPrivate>*);
    void updateAnimationTimer(bool callSetChanged = false);

    void updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*);
    void startUpdateRenderingDispatcher();
    void addEventToDispatch(PassRefPtr<Element> element, const AtomicString& eventType, const String& name, double elapsedTime);
    void addNodeChangeToDispatch(PassRefPtr<Node>);

    bool hasAnimations() const { return !m_compositeAnimations.isEmpty(); }

    void suspendAnimations(Document*);
    void resumeAnimations(Document*);

    void styleAvailable();

    bool isAnimatingPropertyOnRenderer(RenderObject*, int property, bool isRunningNow) const;

    bool pauseAnimationAtTime(RenderObject*, const String& name, double t);
    bool pauseTransitionAtTime(RenderObject*, const String& property, double t);
    unsigned numberOfActiveAnimations() const;

    double beginAnimationUpdateTime()
    {
        if (m_beginAnimationUpdateTime == cBeginAnimationUpdateTimeNotSet)
            m_beginAnimationUpdateTime = currentTime();
        return m_beginAnimationUpdateTime;
    }
    
    void setBeginAnimationUpdateTime(double t) { m_beginAnimationUpdateTime = t; }
    
private:
    typedef HashMap<RenderObject*, RefPtr<CompositeAnimation> > RenderObjectAnimationMap;

    RenderObjectAnimationMap m_compositeAnimations;
    Timer<AnimationControllerPrivate> m_animationTimer;
    Timer<AnimationControllerPrivate> m_updateRenderingDispatcher;
    Frame* m_frame;
    
    class EventToDispatch {
    public:
        RefPtr<Element> element;
        AtomicString eventType;
        String name;
        double elapsedTime;
    };
    
    Vector<EventToDispatch> m_eventsToDispatch;
    Vector<RefPtr<Node> > m_nodeChangesToDispatch;
    
    double m_beginAnimationUpdateTime;
};

AnimationControllerPrivate::AnimationControllerPrivate(Frame* frame)
    : m_animationTimer(this, &AnimationControllerPrivate::animationTimerFired)
    , m_updateRenderingDispatcher(this, &AnimationControllerPrivate::updateRenderingDispatcherFired)
    , m_frame(frame)
    , m_beginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
}

PassRefPtr<CompositeAnimation> AnimationControllerPrivate::accessCompositeAnimation(RenderObject* renderer)
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation) {
        animation = CompositeAnimation::create(m_frame->animation());
        m_compositeAnimations.set(renderer, animation);
    }
    return animation;
}

bool AnimationControllerPrivate::clear(RenderObject* renderer)
{
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setChanged() when suspended).
    PassRefPtr<CompositeAnimation> animation = m_compositeAnimations.take(renderer);
    if (!animation)
        return false;
    animation->clearRenderer();
    return animation->isSuspended();
}

void AnimationControllerPrivate::styleAvailable()
{
    // styleAvailable() can call event handlers which would ultimately delete a CompositeAnimation
    // from the m_compositeAnimations table. So we can't iterate it directly. We will instead build
    // a list of CompositeAnimations which need the styleAvailable() call iterate over that.
    Vector<RefPtr<CompositeAnimation> > list;
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it)
        if (it->second->isWaitingForStyleAvailable())
            list.append(it->second);
    
    Vector<RefPtr<CompositeAnimation> >::const_iterator listEnd = list.end();
    for (Vector<RefPtr<CompositeAnimation> >::const_iterator it = list.begin(); it != listEnd; ++it)
        (*it)->styleAvailable();
}

void AnimationControllerPrivate::updateAnimationTimer(bool callSetChanged/* = false*/)
{
    double needsService = -1;
    bool calledSetChanged = false;

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RefPtr<CompositeAnimation> compAnim = it->second;
        if (!compAnim->isSuspended()) {
            double t = compAnim->willNeedService();
            if (t != -1 && (t < needsService || needsService == -1))
                needsService = t;
            if (needsService == 0) {
                if (callSetChanged) {
                    Node* node = it->first->element();
                    ASSERT(!node || (node->document() && !node->document()->inPageCache()));
                    node->setChanged(AnimationStyleChange);
                    calledSetChanged = true;
                }
                else
                    break;
            }
        }
    }
    
    if (calledSetChanged)
        m_frame->document()->updateRendering();
    
    // If we want service immediately, we start a repeating timer to reduce the overhead of starting
    if (needsService == 0) {
        if (!m_animationTimer.isActive() || m_animationTimer.repeatInterval() == 0)
            m_animationTimer.startRepeating(cAnimationTimerDelay);
        return;
    }
    
    // If we don't need service, we want to make sure the timer is no longer running
    if (needsService < 0) {
        if (m_animationTimer.isActive())
            m_animationTimer.stop();
        return;
    }
    
    // Otherwise, we want to start a one-shot timer so we get here again
    if (m_animationTimer.isActive())
        m_animationTimer.stop();
    m_animationTimer.startOneShot(needsService);
}

void AnimationControllerPrivate::updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*)
{
    // fire all the events
    Vector<EventToDispatch>::const_iterator eventsToDispatchEnd = m_eventsToDispatch.end();
    for (Vector<EventToDispatch>::const_iterator it = m_eventsToDispatch.begin(); it != eventsToDispatchEnd; ++it) {
        if (it->eventType == eventNames().webkitTransitionEndEvent)
            it->element->dispatchWebKitTransitionEvent(it->eventType,it->name, it->elapsedTime);
        else
            it->element->dispatchWebKitAnimationEvent(it->eventType,it->name, it->elapsedTime);
    }
    
    m_eventsToDispatch.clear();
    
    // call setChanged on all the elements
    Vector<RefPtr<Node> >::const_iterator nodeChangesToDispatchEnd = m_nodeChangesToDispatch.end();
    for (Vector<RefPtr<Node> >::const_iterator it = m_nodeChangesToDispatch.begin(); it != nodeChangesToDispatchEnd; ++it)
        (*it)->setChanged(AnimationStyleChange);
    
    m_nodeChangesToDispatch.clear();
    
    if (m_frame && m_frame->document())
        m_frame->document()->updateRendering();
}

void AnimationControllerPrivate::startUpdateRenderingDispatcher()
{
    if (!m_updateRenderingDispatcher.isActive())
        m_updateRenderingDispatcher.startOneShot(0);
}

void AnimationControllerPrivate::addEventToDispatch(PassRefPtr<Element> element, const AtomicString& eventType, const String& name, double elapsedTime)
{
    m_eventsToDispatch.grow(m_eventsToDispatch.size()+1);
    EventToDispatch& event = m_eventsToDispatch[m_eventsToDispatch.size()-1];
    event.element = element;
    event.eventType = eventType;
    event.name = name;
    event.elapsedTime = elapsedTime;
    
    startUpdateRenderingDispatcher();
}

void AnimationControllerPrivate::addNodeChangeToDispatch(PassRefPtr<Node> node)
{
    m_nodeChangesToDispatch.append(node);
    startUpdateRenderingDispatcher();
}

void AnimationControllerPrivate::animationTimerFired(Timer<AnimationControllerPrivate>*)
{
    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. hardware animations)
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateRendering.  It will then call back to us with new information.
    updateAnimationTimer(true);
}

bool AnimationControllerPrivate::isAnimatingPropertyOnRenderer(RenderObject* renderer, int property, bool isRunningNow) const
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation)
        return false;

    return animation->isAnimatingProperty(property, isRunningNow);
}

void AnimationControllerPrivate::suspendAnimations(Document* document)
{
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        RefPtr<CompositeAnimation> compAnim = it->second;
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
        RefPtr<CompositeAnimation> compAnim = it->second;
        if (renderer->document() == document)
            compAnim->resumeAnimations();
    }
    
    updateAnimationTimer();
}

bool AnimationControllerPrivate::pauseAnimationAtTime(RenderObject* renderer, const String& name, double t)
{
    if (!renderer)
        return false;

    RefPtr<CompositeAnimation> compAnim = accessCompositeAnimation(renderer);
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

    RefPtr<CompositeAnimation> compAnim = accessCompositeAnimation(renderer);
    if (!compAnim)
        return false;

    if (compAnim->pauseTransitionAtTime(cssPropertyID(property), t)) {
        renderer->node()->setChanged(AnimationStyleChange);
        return true;
    }

    return false;
}

unsigned AnimationControllerPrivate::numberOfActiveAnimations() const
{
    unsigned count = 0;
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second.get();
        count += compAnim->numberOfActiveAnimations();
    }
    
    return count;
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

    RefPtr<CompositeAnimation> rendererAnimations = m_data->accessCompositeAnimation(renderer);
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
    RefPtr<CompositeAnimation> rendererAnimations = m_data->accessCompositeAnimation(renderer);
    rendererAnimations->setAnimationStartTime(t);
}

void AnimationController::setTransitionStartTime(RenderObject* renderer, int property, double t)
{
    RefPtr<CompositeAnimation> rendererAnimations = m_data->accessCompositeAnimation(renderer);
    rendererAnimations->setTransitionStartTime(property, t);
}

bool AnimationController::pauseAnimationAtTime(RenderObject* renderer, const String& name, double t)
{
    return m_data->pauseAnimationAtTime(renderer, name, t);
}

unsigned AnimationController::numberOfActiveAnimations() const
{
    return m_data->numberOfActiveAnimations();
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

void AnimationController::addEventToDispatch(PassRefPtr<Element> element, const AtomicString& eventType, const String& name, double elapsedTime)
{
    m_data->addEventToDispatch(element, eventType, name, elapsedTime);
}

void AnimationController::addNodeChangeToDispatch(PassRefPtr<Node> node)
{
    ASSERT(!node || (node->document() && !node->document()->inPageCache()));
    if (node)
        m_data->addNodeChangeToDispatch(node);
}

void AnimationController::styleAvailable()
{
    if (!m_numStyleAvailableWaiters)
        return;

    m_data->styleAvailable();
}

double AnimationController::beginAnimationUpdateTime()
{
    return m_data->beginAnimationUpdateTime();
}

void AnimationController::beginAnimationUpdate()
{
    m_data->setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
}

void AnimationController::endAnimationUpdate()
{
}

} // namespace WebCore
