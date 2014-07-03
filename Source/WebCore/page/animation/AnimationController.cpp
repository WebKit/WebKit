/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "AnimationController.h"

#include "AnimationBase.h"
#include "AnimationControllerPrivate.h"
#include "CSSParser.h"
#include "CSSPropertyAnimation.h"
#include "CompositeAnimation.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "Logging.h"
#include "PseudoElement.h"
#include "RenderView.h"
#include "TransitionEvent.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const double cAnimationTimerDelay = 0.025;
static const double cBeginAnimationUpdateTimeNotSet = -1;

AnimationControllerPrivate::AnimationControllerPrivate(Frame& frame)
    : m_animationTimer(this, &AnimationControllerPrivate::animationTimerFired)
    , m_updateStyleIfNeededDispatcher(this, &AnimationControllerPrivate::updateStyleIfNeededDispatcherFired)
    , m_frame(frame)
    , m_beginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet)
    , m_animationsWaitingForStyle()
    , m_animationsWaitingForStartTimeResponse()
    , m_waitingForAsyncStartNotification(false)
    , m_isSuspended(false)
    , m_allowsNewAnimationsWhileSuspended(false)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
}

CompositeAnimation& AnimationControllerPrivate::ensureCompositeAnimation(RenderElement* renderer)
{
    auto result = m_compositeAnimations.add(renderer, nullptr);
    if (result.isNewEntry)
        result.iterator->value = CompositeAnimation::create(this);
    return *result.iterator->value;
}

bool AnimationControllerPrivate::clear(RenderElement* renderer)
{
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setNeedsStyleRecalc() when suspended).
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.take(renderer);
    if (!animation)
        return false;
    animation->clearRenderer();
    return animation->isSuspended();
}

double AnimationControllerPrivate::updateAnimations(SetChanged callSetChanged/* = DoNotCallSetChanged*/)
{
    double timeToNextService = -1;
    bool calledSetChanged = false;

    auto end = m_compositeAnimations.end();
    for (auto it = m_compositeAnimations.begin(); it != end; ++it) {
        CompositeAnimation& animation = *it->value;
        if (!animation.isSuspended() && animation.hasAnimations()) {
            double t = animation.timeToNextService();
            if (t != -1 && (t < timeToNextService || timeToNextService == -1))
                timeToNextService = t;
            if (!timeToNextService) {
                if (callSetChanged != CallSetChanged)
                    break;
                Element* element = it->key->element();
                ASSERT(element);
                ASSERT(!element->document().inPageCache());
                element->setNeedsStyleRecalc(SyntheticStyleChange);
                calledSetChanged = true;
            }
        }
    }

    if (calledSetChanged)
        m_frame.document()->updateStyleIfNeeded();

    return timeToNextService;
}

void AnimationControllerPrivate::updateAnimationTimerForRenderer(RenderElement* renderer)
{
    double timeToNextService = 0;

    const CompositeAnimation* compositeAnimation = m_compositeAnimations.get(renderer);
    if (!compositeAnimation->isSuspended() && compositeAnimation->hasAnimations())
        timeToNextService = compositeAnimation->timeToNextService();

    if (m_animationTimer.isActive() && (m_animationTimer.repeatInterval() || m_animationTimer.nextFireInterval() <= timeToNextService))
        return;

    m_animationTimer.startOneShot(timeToNextService);
}

void AnimationControllerPrivate::updateAnimationTimer(SetChanged callSetChanged/* = DoNotCallSetChanged*/)
{
    double timeToNextService = updateAnimations(callSetChanged);

    LOG(Animations, "updateAnimationTimer: timeToNextService is %.2f", timeToNextService);

    // If we want service immediately, we start a repeating timer to reduce the overhead of starting
    if (!timeToNextService) {
        if (!m_animationTimer.isActive() || m_animationTimer.repeatInterval() == 0)
            m_animationTimer.startRepeating(cAnimationTimerDelay);
        return;
    }

    // If we don't need service, we want to make sure the timer is no longer running
    if (timeToNextService < 0) {
        if (m_animationTimer.isActive())
            m_animationTimer.stop();
        return;
    }

    // Otherwise, we want to start a one-shot timer so we get here again
    m_animationTimer.startOneShot(timeToNextService);
}

void AnimationControllerPrivate::updateStyleIfNeededDispatcherFired(Timer<AnimationControllerPrivate>&)
{
    fireEventsAndUpdateStyle();
}

void AnimationControllerPrivate::fireEventsAndUpdateStyle()
{
    // Protect the frame from getting destroyed in the event handler
    Ref<Frame> protector(m_frame);

    bool updateStyle = !m_eventsToDispatch.isEmpty() || !m_elementChangesToDispatch.isEmpty();

    // fire all the events
    Vector<EventToDispatch> eventsToDispatch = WTF::move(m_eventsToDispatch);
    Vector<EventToDispatch>::const_iterator eventsToDispatchEnd = eventsToDispatch.end();
    for (Vector<EventToDispatch>::const_iterator it = eventsToDispatch.begin(); it != eventsToDispatchEnd; ++it) {
        Element* element = it->element.get();
        if (it->eventType == eventNames().transitionendEvent)
            element->dispatchEvent(TransitionEvent::create(it->eventType, it->name, it->elapsedTime, PseudoElement::pseudoElementNameForEvents(element->pseudoId())));
        else
            element->dispatchEvent(WebKitAnimationEvent::create(it->eventType, it->name, it->elapsedTime));
    }

    for (unsigned i = 0, size = m_elementChangesToDispatch.size(); i < size; ++i)
        m_elementChangesToDispatch[i]->setNeedsStyleRecalc(SyntheticStyleChange);

    m_elementChangesToDispatch.clear();

    if (updateStyle)
        m_frame.document()->updateStyleIfNeeded();
}

void AnimationControllerPrivate::startUpdateStyleIfNeededDispatcher()
{
    if (!m_updateStyleIfNeededDispatcher.isActive())
        m_updateStyleIfNeededDispatcher.startOneShot(0);
}

void AnimationControllerPrivate::addEventToDispatch(PassRefPtr<Element> element, const AtomicString& eventType, const String& name, double elapsedTime)
{
    m_eventsToDispatch.grow(m_eventsToDispatch.size()+1);
    EventToDispatch& event = m_eventsToDispatch[m_eventsToDispatch.size()-1];
    event.element = element;
    event.eventType = eventType;
    event.name = name;
    event.elapsedTime = elapsedTime;
    
    startUpdateStyleIfNeededDispatcher();
}

void AnimationControllerPrivate::addElementChangeToDispatch(PassRef<Element> element)
{
    m_elementChangesToDispatch.append(WTF::move(element));
    ASSERT(!m_elementChangesToDispatch.last()->document().inPageCache());
    startUpdateStyleIfNeededDispatcher();
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void AnimationControllerPrivate::animationFrameCallbackFired()
{
    double timeToNextService = updateAnimations(CallSetChanged);

    if (timeToNextService >= 0)
        m_frame.document()->view()->scheduleAnimation();
}
#endif

void AnimationControllerPrivate::animationTimerFired(Timer<AnimationControllerPrivate>&)
{
    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. accelerated animations)
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateStyleIfNeeded.  It will then call back to us with new information.
    updateAnimationTimer(CallSetChanged);

    // Fire events right away, to avoid a flash of unanimated style after an animation completes, and before
    // the 'end' event fires.
    fireEventsAndUpdateStyle();
}

bool AnimationControllerPrivate::isRunningAnimationOnRenderer(RenderElement* renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    const CompositeAnimation* animation = m_compositeAnimations.get(renderer);
    return animation && animation->isAnimatingProperty(property, false, runningState);
}

bool AnimationControllerPrivate::isRunningAcceleratedAnimationOnRenderer(RenderElement* renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    const CompositeAnimation* animation = m_compositeAnimations.get(renderer);
    return animation && animation->isAnimatingProperty(property, true, runningState);
}

void AnimationControllerPrivate::suspendAnimations()
{
    if (isSuspended())
        return;

    suspendAnimationsForDocument(m_frame.document());

    // Traverse subframes
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->animation().suspendAnimations();

    m_isSuspended = true;
}

void AnimationControllerPrivate::resumeAnimations()
{
    if (!isSuspended())
        return;

    resumeAnimationsForDocument(m_frame.document());

    // Traverse subframes
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->animation().resumeAnimations();

    m_isSuspended = false;
}

void AnimationControllerPrivate::suspendAnimationsForDocument(Document* document)
{
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    for (auto it = m_compositeAnimations.begin(), end = m_compositeAnimations.end(); it != end; ++it) {
        if (&it->key->document() == document)
            it->value->suspendAnimations();
    }

    updateAnimationTimer();
}

void AnimationControllerPrivate::resumeAnimationsForDocument(Document* document)
{
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    for (auto it = m_compositeAnimations.begin(), end = m_compositeAnimations.end(); it != end; ++it) {
        if (&it->key->document() == document)
            it->value->resumeAnimations();
    }

    updateAnimationTimer();
}

void AnimationControllerPrivate::startAnimationsIfNotSuspended(Document* document)
{
    if (!isSuspended() || allowsNewAnimationsWhileSuspended())
        resumeAnimationsForDocument(document);
}

void AnimationControllerPrivate::setAllowsNewAnimationsWhileSuspended(bool allowed)
{
    m_allowsNewAnimationsWhileSuspended = allowed;
}

bool AnimationControllerPrivate::pauseAnimationAtTime(RenderElement* renderer, const AtomicString& name, double t)
{
    if (!renderer)
        return false;

    CompositeAnimation& compositeAnimation = ensureCompositeAnimation(renderer);
    if (compositeAnimation.pauseAnimationAtTime(name, t)) {
        renderer->element()->setNeedsStyleRecalc(SyntheticStyleChange);
        startUpdateStyleIfNeededDispatcher();
        return true;
    }

    return false;
}

bool AnimationControllerPrivate::pauseTransitionAtTime(RenderElement* renderer, const String& property, double t)
{
    if (!renderer)
        return false;

    CompositeAnimation& compositeAnimation = ensureCompositeAnimation(renderer);
    if (compositeAnimation.pauseTransitionAtTime(cssPropertyID(property), t)) {
        renderer->element()->setNeedsStyleRecalc(SyntheticStyleChange);
        startUpdateStyleIfNeededDispatcher();
        return true;
    }

    return false;
}

double AnimationControllerPrivate::beginAnimationUpdateTime()
{
    if (m_beginAnimationUpdateTime == cBeginAnimationUpdateTimeNotSet)
        m_beginAnimationUpdateTime = monotonicallyIncreasingTime();
    return m_beginAnimationUpdateTime;
}

void AnimationControllerPrivate::endAnimationUpdate()
{
    styleAvailable();
    if (!m_waitingForAsyncStartNotification)
        startTimeResponse(beginAnimationUpdateTime());
}

void AnimationControllerPrivate::receivedStartTimeResponse(double time)
{
    m_waitingForAsyncStartNotification = false;
    startTimeResponse(time);
}

PassRefPtr<RenderStyle> AnimationControllerPrivate::getAnimatedStyleForRenderer(RenderElement* renderer)
{
    if (!renderer)
        return 0;

    const CompositeAnimation* rendererAnimations = m_compositeAnimations.get(renderer);
    if (!rendererAnimations)
        return &renderer->style();
    
    RefPtr<RenderStyle> animatingStyle = rendererAnimations->getAnimatedStyle();
    if (!animatingStyle)
        animatingStyle = &renderer->style();
    
    return animatingStyle.release();
}

unsigned AnimationControllerPrivate::numberOfActiveAnimations(Document* document) const
{
    unsigned count = 0;
    
    for (auto it = m_compositeAnimations.begin(), end = m_compositeAnimations.end(); it != end; ++it) {
        if (&it->key->document() == document)
            count += it->value->numberOfActiveAnimations();
    }

    return count;
}

void AnimationControllerPrivate::addToAnimationsWaitingForStyle(AnimationBase* animation)
{
    // Make sure this animation is not in the start time waiters
    m_animationsWaitingForStartTimeResponse.remove(animation);

    m_animationsWaitingForStyle.add(animation);
}

void AnimationControllerPrivate::removeFromAnimationsWaitingForStyle(AnimationBase* animationToRemove)
{
    m_animationsWaitingForStyle.remove(animationToRemove);
}

void AnimationControllerPrivate::styleAvailable()
{
    // Go through list of waiters and send them on their way
    for (const auto& waitingAnimation : m_animationsWaitingForStyle)
        waitingAnimation->styleAvailable();

    m_animationsWaitingForStyle.clear();
}

void AnimationControllerPrivate::addToAnimationsWaitingForStartTimeResponse(AnimationBase* animation, bool willGetResponse)
{
    // If willGetResponse is true, it means this animation is actually waiting for a response
    // (which will come in as a call to notifyAnimationStarted()).
    // In that case we don't need to add it to this list. We just set a waitingForAResponse flag 
    // which says we are waiting for the response. If willGetResponse is false, this animation 
    // is not waiting for a response for itself, but rather for a notifyXXXStarted() call for 
    // another animation to which it will sync.
    //
    // When endAnimationUpdate() is called we check to see if the waitingForAResponse flag is
    // true. If so, we just return and will do our work when the first notifyXXXStarted() call
    // comes in. If it is false, we will not be getting a notifyXXXStarted() call, so we will
    // do our work right away. In both cases we call the onAnimationStartResponse() method
    // on each animation. In the first case we send in the time we got from notifyXXXStarted().
    // In the second case, we just pass in the beginAnimationUpdateTime().
    //
    // This will synchronize all software and accelerated animations started in the same 
    // updateStyleIfNeeded cycle.
    //
    
    if (willGetResponse)
        m_waitingForAsyncStartNotification = true;
    
    m_animationsWaitingForStartTimeResponse.add(animation);
}

void AnimationControllerPrivate::removeFromAnimationsWaitingForStartTimeResponse(AnimationBase* animationToRemove)
{
    m_animationsWaitingForStartTimeResponse.remove(animationToRemove);
    
    if (m_animationsWaitingForStartTimeResponse.isEmpty())
        m_waitingForAsyncStartNotification = false;
}

void AnimationControllerPrivate::startTimeResponse(double time)
{
    // Go through list of waiters and send them on their way

    for (const auto& animation : m_animationsWaitingForStartTimeResponse)
        animation->onAnimationStartResponse(time);
    
    m_animationsWaitingForStartTimeResponse.clear();
    m_waitingForAsyncStartNotification = false;
}

void AnimationControllerPrivate::animationWillBeRemoved(AnimationBase* animation)
{
    removeFromAnimationsWaitingForStyle(animation);
    removeFromAnimationsWaitingForStartTimeResponse(animation);
}

AnimationController::AnimationController(Frame& frame)
    : m_data(std::make_unique<AnimationControllerPrivate>(frame))
    , m_beginAnimationUpdateCount(0)
{
}

AnimationController::~AnimationController()
{
}

void AnimationController::cancelAnimations(RenderElement* renderer)
{
    if (!m_data->hasAnimations())
        return;

    if (m_data->clear(renderer)) {
        Element* element = renderer->element();
        ASSERT(!element || !element->document().inPageCache());
        if (element)
            element->setNeedsStyleRecalc(SyntheticStyleChange);
    }
}

PassRef<RenderStyle> AnimationController::updateAnimations(RenderElement& renderer, PassRef<RenderStyle> newStyle)
{
    // Don't do anything if we're in the cache
    if (renderer.document().inPageCache())
        return newStyle;

    RenderStyle* oldStyle = renderer.hasInitializedStyle() ? &renderer.style() : nullptr;

    if ((!oldStyle || (!oldStyle->animations() && !oldStyle->transitions())) && (!newStyle.get().animations() && !newStyle.get().transitions()))
        return newStyle;

    // Don't run transitions when printing.
    if (renderer.view().printing())
        return newStyle;

    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.

    // We don't support anonymous pseudo elements like :first-line or :first-letter.
    ASSERT(renderer.element());

    Ref<RenderStyle> newStyleBeforeAnimation(WTF::move(newStyle));

    CompositeAnimation& rendererAnimations = m_data->ensureCompositeAnimation(&renderer);
    auto blendedStyle = rendererAnimations.animate(renderer, oldStyle, newStyleBeforeAnimation.get());

    if (renderer.parent() || newStyleBeforeAnimation->animations() || (oldStyle && oldStyle->animations())) {
        m_data->updateAnimationTimerForRenderer(&renderer);
#if ENABLE(REQUEST_ANIMATION_FRAME)
        renderer.view().frameView().scheduleAnimation();
#endif
    }

    if (&blendedStyle.get() != &newStyleBeforeAnimation.get()) {
        // If the animations/transitions change opacity or transform, we need to update
        // the style to impose the stacking rules. Note that this is also
        // done in StyleResolver::adjustRenderStyle().
        if (blendedStyle.get().hasAutoZIndex() && (blendedStyle.get().opacity() < 1.0f || blendedStyle.get().hasTransform()))
            blendedStyle.get().setZIndex(0);
    }
    return blendedStyle;
}

PassRefPtr<RenderStyle> AnimationController::getAnimatedStyleForRenderer(RenderElement* renderer)
{
    return m_data->getAnimatedStyleForRenderer(renderer);
}

void AnimationController::notifyAnimationStarted(RenderElement*, double startTime)
{
    m_data->receivedStartTimeResponse(startTime);
}

bool AnimationController::pauseAnimationAtTime(RenderElement* renderer, const AtomicString& name, double t)
{
    return m_data->pauseAnimationAtTime(renderer, name, t);
}

unsigned AnimationController::numberOfActiveAnimations(Document* document) const
{
    return m_data->numberOfActiveAnimations(document);
}

bool AnimationController::pauseTransitionAtTime(RenderElement* renderer, const String& property, double t)
{
    return m_data->pauseTransitionAtTime(renderer, property, t);
}

bool AnimationController::isRunningAnimationOnRenderer(RenderElement* renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    return m_data->isRunningAnimationOnRenderer(renderer, property, runningState);
}

bool AnimationController::isRunningAcceleratedAnimationOnRenderer(RenderElement* renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    return m_data->isRunningAcceleratedAnimationOnRenderer(renderer, property, runningState);
}

bool AnimationController::isSuspended() const
{
    return m_data->isSuspended();
}

void AnimationController::suspendAnimations()
{
    LOG(Animations, "controller is suspending animations");
    m_data->suspendAnimations();
}

void AnimationController::resumeAnimations()
{
    LOG(Animations, "controller is resuming animations");
    m_data->resumeAnimations();
}

bool AnimationController::allowsNewAnimationsWhileSuspended() const
{
    return m_data->allowsNewAnimationsWhileSuspended();
}

void AnimationController::setAllowsNewAnimationsWhileSuspended(bool allowed)
{
    m_data->setAllowsNewAnimationsWhileSuspended(allowed);
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void AnimationController::serviceAnimations()
{
    m_data->animationFrameCallbackFired();
}
#endif

void AnimationController::suspendAnimationsForDocument(Document* document)
{
    LOG(Animations, "suspending animations for document %p", document);
    m_data->suspendAnimationsForDocument(document);
}

void AnimationController::resumeAnimationsForDocument(Document* document)
{
    LOG(Animations, "resuming animations for document %p", document);
    m_data->resumeAnimationsForDocument(document);
}

void AnimationController::startAnimationsIfNotSuspended(Document* document)
{
    LOG(Animations, "animations may start for document %p", document);
    m_data->startAnimationsIfNotSuspended(document);
}

void AnimationController::beginAnimationUpdate()
{
    if (!m_beginAnimationUpdateCount)
        m_data->setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    ++m_beginAnimationUpdateCount;
}

void AnimationController::endAnimationUpdate()
{
    ASSERT(m_beginAnimationUpdateCount > 0);
    --m_beginAnimationUpdateCount;
    if (!m_beginAnimationUpdateCount)
        m_data->endAnimationUpdate();
}

bool AnimationController::supportsAcceleratedAnimationOfProperty(CSSPropertyID property)
{
    return CSSPropertyAnimation::animationOfPropertyIsAccelerated(property);
}

} // namespace WebCore
