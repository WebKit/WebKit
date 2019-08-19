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
#include "CSSAnimationController.h"

#include "AnimationBase.h"
#include "AnimationEvent.h"
#include "CSSAnimationControllerPrivate.h"
#include "CSSPropertyAnimation.h"
#include "CSSPropertyParser.h"
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

namespace WebCore {

// Allow a little more than 60fps to make sure we can at least hit that frame rate.
static const Seconds animationTimerDelay { 15_ms };
// Allow a little more than 30fps to make sure we can at least hit that frame rate.
static const Seconds animationTimerThrottledDelay { 30_ms };

class AnimationPrivateUpdateBlock {
public:
    AnimationPrivateUpdateBlock(CSSAnimationControllerPrivate& animationController)
        : m_animationController(animationController)
    {
        m_animationController.beginAnimationUpdate();
    }
    
    ~AnimationPrivateUpdateBlock()
    {
        m_animationController.endAnimationUpdate();
    }
    
    CSSAnimationControllerPrivate& m_animationController;
};

CSSAnimationControllerPrivate::CSSAnimationControllerPrivate(Frame& frame)
    : m_animationTimer(*this, &CSSAnimationControllerPrivate::animationTimerFired)
    , m_updateStyleIfNeededDispatcher(*this, &CSSAnimationControllerPrivate::updateStyleIfNeededDispatcherFired)
    , m_frame(frame)
    , m_beginAnimationUpdateCount(0)
    , m_waitingForAsyncStartNotification(false)
    , m_allowsNewAnimationsWhileSuspended(false)
{
}

CSSAnimationControllerPrivate::~CSSAnimationControllerPrivate()
{
    // We need to explicitly clear the composite animations here because the
    // destructor of CompositeAnimation will call members of this class back.
    m_compositeAnimations.clear();
}

CompositeAnimation& CSSAnimationControllerPrivate::ensureCompositeAnimation(Element& element)
{
    element.setHasCSSAnimation();

    auto result = m_compositeAnimations.ensure(&element, [&] {
        return CompositeAnimation::create(*this);
    });

    if (animationsAreSuspendedForDocument(&element.document()))
        result.iterator->value->suspendAnimations();

    return *result.iterator->value;
}

bool CSSAnimationControllerPrivate::clear(Element& element)
{
    ASSERT(element.hasCSSAnimation() == m_compositeAnimations.contains(&element));

    if (!element.hasCSSAnimation())
        return false;
    element.clearHasCSSAnimation();

    auto it = m_compositeAnimations.find(&element);
    if (it == m_compositeAnimations.end())
        return false;

    LOG(Animations, "CSSAnimationControllerPrivate %p clear: %p", this, &element);

    m_eventsToDispatch.removeAllMatching([&] (const EventToDispatch& info) {
        return info.element.ptr() == &element;
    });

    m_elementChangesToDispatch.removeAllMatching([&](auto& currentElement) {
        return currentElement.ptr() == &element;
    });
    
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a invalidateStyleForSubtree() when suspended).
    // FIXME: The code below does the opposite of what the comment above says regarding suspended state.
    auto& animation = *it->value;
    bool result = animation.isSuspended();
    animation.clearElement();

    m_compositeAnimations.remove(it);

    return result;
}

Optional<Seconds> CSSAnimationControllerPrivate::updateAnimations(SetChanged callSetChanged/* = DoNotCallSetChanged*/)
{
    AnimationPrivateUpdateBlock updateBlock(*this);
    Optional<Seconds> timeToNextService;
    bool calledSetChanged = false;

    for (auto& compositeAnimation : m_compositeAnimations) {
        CompositeAnimation& animation = *compositeAnimation.value;
        if (!animation.isSuspended() && animation.hasAnimations()) {
            Optional<Seconds> t = animation.timeToNextService();
            if (t && (!timeToNextService || t.value() < timeToNextService.value()))
                timeToNextService = t.value();
            if (timeToNextService && timeToNextService.value() == 0_s) {
                if (callSetChanged != CallSetChanged)
                    break;
                
                Element& element = *compositeAnimation.key;
                ASSERT(element.document().pageCacheState() == Document::NotInPageCache);
                element.invalidateStyle();
                calledSetChanged = true;
            }
        }
    }

    if (calledSetChanged)
        m_frame.document()->updateStyleIfNeeded();

    return timeToNextService;
}

void CSSAnimationControllerPrivate::updateAnimationTimerForElement(Element& element)
{
    Optional<Seconds> timeToNextService;

    const CompositeAnimation* compositeAnimation = m_compositeAnimations.get(&element);
    if (!compositeAnimation->isSuspended() && compositeAnimation->hasAnimations())
        timeToNextService = compositeAnimation->timeToNextService();

    if (!timeToNextService)
        return;

    if (m_animationTimer.isActive() && (m_animationTimer.repeatInterval() || m_animationTimer.nextFireInterval() <= timeToNextService.value()))
        return;

    m_animationTimer.startOneShot(timeToNextService.value());
}

void CSSAnimationControllerPrivate::updateAnimationTimer(SetChanged callSetChanged/* = DoNotCallSetChanged*/)
{
    Optional<Seconds> timeToNextService = updateAnimations(callSetChanged);

    LOG(Animations, "updateAnimationTimer: timeToNextService is %.2f", timeToNextService.valueOr(Seconds { -1 }).value());

    // If we don't need service, we want to make sure the timer is no longer running
    if (!timeToNextService) {
        if (m_animationTimer.isActive())
            m_animationTimer.stop();
        return;
    }

    // If we want service immediately, we start a repeating timer to reduce the overhead of starting
    if (!timeToNextService.value()) {
        auto* page = m_frame.page();
        bool shouldThrottle = page && page->isLowPowerModeEnabled();
        Seconds delay = shouldThrottle ? animationTimerThrottledDelay : animationTimerDelay;

        if (!m_animationTimer.isActive() || m_animationTimer.repeatInterval() != delay)
            m_animationTimer.startRepeating(delay);
        return;
    }

    // Otherwise, we want to start a one-shot timer so we get here again
    m_animationTimer.startOneShot(timeToNextService.value());
}

void CSSAnimationControllerPrivate::updateStyleIfNeededDispatcherFired()
{
    fireEventsAndUpdateStyle();
}

void CSSAnimationControllerPrivate::fireEventsAndUpdateStyle()
{
    // Protect the frame from getting destroyed in the event handler
    Ref<Frame> protector(m_frame);

    bool updateStyle = !m_eventsToDispatch.isEmpty() || !m_elementChangesToDispatch.isEmpty();

    // fire all the events
    Vector<EventToDispatch> eventsToDispatch = WTFMove(m_eventsToDispatch);
    for (auto& event : eventsToDispatch) {
        Element& element = event.element;
        if (event.eventType == eventNames().transitionendEvent)
            element.dispatchEvent(TransitionEvent::create(event.eventType, event.name, event.elapsedTime, PseudoElement::pseudoElementNameForEvents(element.pseudoId())));
        else
            element.dispatchEvent(AnimationEvent::create(event.eventType, event.name, event.elapsedTime));
    }

    for (auto& change : m_elementChangesToDispatch)
        change->invalidateStyle();

    m_elementChangesToDispatch.clear();

    if (updateStyle)
        m_frame.document()->updateStyleIfNeeded();
}

void CSSAnimationControllerPrivate::startUpdateStyleIfNeededDispatcher()
{
    if (!m_updateStyleIfNeededDispatcher.isActive())
        m_updateStyleIfNeededDispatcher.startOneShot(0_s);
}

void CSSAnimationControllerPrivate::addEventToDispatch(Element& element, const AtomString& eventType, const String& name, double elapsedTime)
{
    m_eventsToDispatch.append({ element, eventType, name, elapsedTime });
    startUpdateStyleIfNeededDispatcher();
}

void CSSAnimationControllerPrivate::addElementChangeToDispatch(Element& element)
{
    m_elementChangesToDispatch.append(element);
    ASSERT(m_elementChangesToDispatch.last()->document().pageCacheState() == Document::NotInPageCache);
    startUpdateStyleIfNeededDispatcher();
}

void CSSAnimationControllerPrivate::animationFrameCallbackFired()
{
    Optional<Seconds> timeToNextService = updateAnimations(CallSetChanged);

    if (timeToNextService)
        m_frame.document()->view()->scheduleAnimation();
}

void CSSAnimationControllerPrivate::animationTimerFired()
{
    // We need to keep the frame alive, since it owns us.
    Ref<Frame> protector(m_frame);

    // The animation timer might fire before the layout timer, in
    // which case we might create some animations with incorrect
    // values if we don't layout first.
    if (m_requiresLayout) {
        if (auto* frameView = m_frame.document()->view()) {
            if (frameView->needsLayout())
                frameView->forceLayout();
        }
        m_requiresLayout = false;
    }

    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. accelerated animations)
    AnimationPrivateUpdateBlock updateBlock(*this);

    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateStyleIfNeeded. It will then call back to us with new information.
    updateAnimationTimer(CallSetChanged);

    // Fire events right away, to avoid a flash of unanimated style after an animation completes, and before
    // the 'end' event fires.
    fireEventsAndUpdateStyle();
}

bool CSSAnimationControllerPrivate::isRunningAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property) const
{
    if (!renderer.element())
        return false;
    auto* animation = m_compositeAnimations.get(renderer.element());
    if (!animation)
        return false;
    return animation->isAnimatingProperty(property, false);
}

bool CSSAnimationControllerPrivate::isRunningAcceleratedAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property) const
{
    if (!renderer.element())
        return false;
    auto* animation = m_compositeAnimations.get(renderer.element());
    if (!animation)
        return false;
    return animation->isAnimatingProperty(property, true);
}

void CSSAnimationControllerPrivate::updateThrottlingState()
{
    updateAnimationTimer();

    for (auto* childFrame = m_frame.tree().firstChild(); childFrame; childFrame = childFrame->tree().nextSibling())
        childFrame->animation().updateThrottlingState();
}

Seconds CSSAnimationControllerPrivate::animationInterval() const
{
    if (!m_animationTimer.isActive())
        return Seconds { INFINITY };

    return Seconds { m_animationTimer.repeatInterval() };
}

void CSSAnimationControllerPrivate::suspendAnimations()
{
    if (isSuspended())
        return;

    suspendAnimationsForDocument(m_frame.document());

    // Traverse subframes
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->animation().suspendAnimations();

    m_isSuspended = true;
}

void CSSAnimationControllerPrivate::resumeAnimations()
{
    if (!isSuspended())
        return;

    resumeAnimationsForDocument(m_frame.document());

    // Traverse subframes
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->animation().resumeAnimations();

    m_isSuspended = false;
}

bool CSSAnimationControllerPrivate::animationsAreSuspendedForDocument(Document* document)
{
    return isSuspended() || m_suspendedDocuments.contains(document);
}

void CSSAnimationControllerPrivate::detachFromDocument(Document* document)
{
    m_suspendedDocuments.remove(document);
}

void CSSAnimationControllerPrivate::suspendAnimationsForDocument(Document* document)
{
    if (animationsAreSuspendedForDocument(document))
        return;

    m_suspendedDocuments.add(document);

    AnimationPrivateUpdateBlock updateBlock(*this);

    for (auto& animation : m_compositeAnimations) {
        if (&animation.key->document() == document)
            animation.value->suspendAnimations();
    }

    updateAnimationTimer();
}

void CSSAnimationControllerPrivate::resumeAnimationsForDocument(Document* document)
{
    if (!animationsAreSuspendedForDocument(document))
        return;

    detachFromDocument(document);

    AnimationPrivateUpdateBlock updateBlock(*this);

    for (auto& animation : m_compositeAnimations) {
        if (&animation.key->document() == document)
            animation.value->resumeAnimations();
    }

    updateAnimationTimer();
}

void CSSAnimationControllerPrivate::startAnimationsIfNotSuspended(Document* document)
{
    if (!animationsAreSuspendedForDocument(document) || allowsNewAnimationsWhileSuspended())
        resumeAnimationsForDocument(document);
}

void CSSAnimationControllerPrivate::setAllowsNewAnimationsWhileSuspended(bool allowed)
{
    m_allowsNewAnimationsWhileSuspended = allowed;
}

bool CSSAnimationControllerPrivate::pauseAnimationAtTime(Element& element, const AtomString& name, double t)
{
    CompositeAnimation& compositeAnimation = ensureCompositeAnimation(element);
    if (compositeAnimation.pauseAnimationAtTime(name, t)) {
        element.invalidateStyle();
        startUpdateStyleIfNeededDispatcher();
        return true;
    }

    return false;
}

bool CSSAnimationControllerPrivate::pauseTransitionAtTime(Element& element, const String& property, double t)
{
    CompositeAnimation& compositeAnimation = ensureCompositeAnimation(element);
    if (compositeAnimation.pauseTransitionAtTime(cssPropertyID(property), t)) {
        element.invalidateStyle();
        startUpdateStyleIfNeededDispatcher();
        return true;
    }

    return false;
}

MonotonicTime CSSAnimationControllerPrivate::beginAnimationUpdateTime()
{
    ASSERT(m_beginAnimationUpdateCount);
    if (!m_beginAnimationUpdateTime)
        m_beginAnimationUpdateTime = MonotonicTime::now();

    return m_beginAnimationUpdateTime.value();
}

void CSSAnimationControllerPrivate::beginAnimationUpdate()
{
    if (!m_beginAnimationUpdateCount)
        m_beginAnimationUpdateTime = WTF::nullopt;
    ++m_beginAnimationUpdateCount;
}

void CSSAnimationControllerPrivate::endAnimationUpdate()
{
    ASSERT(m_beginAnimationUpdateCount > 0);
    if (m_beginAnimationUpdateCount == 1) {
        styleAvailable();
        if (!m_waitingForAsyncStartNotification)
            startTimeResponse(beginAnimationUpdateTime());
    }
    --m_beginAnimationUpdateCount;
}

void CSSAnimationControllerPrivate::receivedStartTimeResponse(MonotonicTime time)
{
    LOG(Animations, "CSSAnimationControllerPrivate %p receivedStartTimeResponse %f", this, time.secondsSinceEpoch().seconds());

    m_waitingForAsyncStartNotification = false;
    startTimeResponse(time);
}

std::unique_ptr<RenderStyle> CSSAnimationControllerPrivate::animatedStyleForElement(Element& element)
{
    auto* animation = m_compositeAnimations.get(&element);
    if (!animation)
        return nullptr;

    AnimationPrivateUpdateBlock animationUpdateBlock(*this);

    auto animatingStyle = animation->getAnimatedStyle();
    if (!animatingStyle)
        return nullptr;
    
    return animatingStyle;
}

bool CSSAnimationControllerPrivate::computeExtentOfAnimation(Element& element, LayoutRect& bounds) const
{
    auto* animation = m_compositeAnimations.get(&element);
    if (!animation)
        return true;

    if (!animation->isAnimatingProperty(CSSPropertyTransform, false))
        return true;

    return animation->computeExtentOfTransformAnimation(bounds);
}

unsigned CSSAnimationControllerPrivate::numberOfActiveAnimations(Document* document) const
{
    unsigned count = 0;
    
    for (auto& animation : m_compositeAnimations) {
        if (&animation.key->document() == document)
            count += animation.value->numberOfActiveAnimations();
    }

    return count;
}

void CSSAnimationControllerPrivate::addToAnimationsWaitingForStyle(AnimationBase& animation)
{
    m_animationsWaitingForStartTimeResponse.remove(&animation);
    m_animationsWaitingForStyle.add(&animation);
}

void CSSAnimationControllerPrivate::removeFromAnimationsWaitingForStyle(AnimationBase& animationToRemove)
{
    m_animationsWaitingForStyle.remove(&animationToRemove);
}

void CSSAnimationControllerPrivate::styleAvailable()
{
    // Go through list of waiters and send them on their way
    for (const auto& waitingAnimation : m_animationsWaitingForStyle)
        waitingAnimation->styleAvailable();

    m_animationsWaitingForStyle.clear();
}

void CSSAnimationControllerPrivate::addToAnimationsWaitingForStartTimeResponse(AnimationBase& animation, bool willGetResponse)
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

    m_animationsWaitingForStartTimeResponse.add(&animation);
}

void CSSAnimationControllerPrivate::removeFromAnimationsWaitingForStartTimeResponse(AnimationBase& animationToRemove)
{
    m_animationsWaitingForStartTimeResponse.remove(&animationToRemove);
    if (m_animationsWaitingForStartTimeResponse.isEmpty())
        m_waitingForAsyncStartNotification = false;
}

void CSSAnimationControllerPrivate::startTimeResponse(MonotonicTime time)
{
    // Go through list of waiters and send them on their way

    for (const auto& animation : m_animationsWaitingForStartTimeResponse)
        animation->onAnimationStartResponse(time);
    
    m_animationsWaitingForStartTimeResponse.clear();
    m_waitingForAsyncStartNotification = false;
}

void CSSAnimationControllerPrivate::animationWillBeRemoved(AnimationBase& animation)
{
    LOG(Animations, "CSSAnimationControllerPrivate %p animationWillBeRemoved: %p", this, &animation);

    removeFromAnimationsWaitingForStyle(animation);
    removeFromAnimationsWaitingForStartTimeResponse(animation);

    bool anyAnimationsWaitingForAsyncStart = false;
    for (auto& animation : m_animationsWaitingForStartTimeResponse) {
        if (animation->waitingForStartTime() && animation->isAccelerated()) {
            anyAnimationsWaitingForAsyncStart = true;
            break;
        }
    }

    if (!anyAnimationsWaitingForAsyncStart)
        m_waitingForAsyncStartNotification = false;
}

CSSAnimationController::CSSAnimationController(Frame& frame)
    : m_data(makeUnique<CSSAnimationControllerPrivate>(frame))
{
}

CSSAnimationController::~CSSAnimationController() = default;

void CSSAnimationController::cancelAnimations(Element& element)
{
    if (!m_data->clear(element))
        return;

    if (element.document().renderTreeBeingDestroyed())
        return;
    ASSERT(element.document().pageCacheState() == Document::NotInPageCache);
    element.invalidateStyle();
}

AnimationUpdate CSSAnimationController::updateAnimations(Element& element, const RenderStyle& newStyle, const RenderStyle* oldStyle)
{
    bool hasOrHadAnimations = (oldStyle && oldStyle->hasAnimationsOrTransitions()) || newStyle.hasAnimationsOrTransitions();
    if (!hasOrHadAnimations)
        return { };

    if (element.document().pageCacheState() != Document::NotInPageCache)
        return { };

    // Don't run transitions when printing.
    if (element.document().renderView()->printing())
        return { };

    // Fetch our current set of implicit animations from a hashtable. We then compare them
    // against the animations in the style and make sure we're in sync. If destination values
    // have changed, we reset the animation. We then do a blend to get new values and we return
    // a new style.

    CompositeAnimation& compositeAnimation = m_data->ensureCompositeAnimation(element);
    auto update = compositeAnimation.animate(element, oldStyle, newStyle);

    auto* renderer = element.renderer();
    if ((renderer && renderer->parent()) || newStyle.animations() || (oldStyle && oldStyle->animations())) {
        auto& frameView = *element.document().view();
        if (compositeAnimation.hasAnimationThatDependsOnLayout())
            m_data->setRequiresLayout();
        m_data->updateAnimationTimerForElement(element);
        frameView.scheduleAnimation();
    }

    return update;
}

std::unique_ptr<RenderStyle> CSSAnimationController::animatedStyleForRenderer(RenderElement& renderer)
{
    std::unique_ptr<RenderStyle> result;

    if (renderer.style().hasAnimationsOrTransitions() && renderer.element())
        result = m_data->animatedStyleForElement(*renderer.element());

    if (!result)
        result = RenderStyle::clonePtr(renderer.style());

    return result;
}

bool CSSAnimationController::computeExtentOfAnimation(RenderElement& renderer, LayoutRect& bounds) const
{
    if (!renderer.element())
        return true;
    if (!renderer.style().hasAnimationsOrTransitions())
        return true;

    return m_data->computeExtentOfAnimation(*renderer.element(), bounds);
}

void CSSAnimationController::notifyAnimationStarted(RenderElement& renderer, MonotonicTime startTime)
{
    LOG(Animations, "CSSAnimationController %p notifyAnimationStarted on renderer %p, time=%f", this, &renderer, startTime.secondsSinceEpoch().seconds());
    UNUSED_PARAM(renderer);

    AnimationUpdateBlock animationUpdateBlock(this);
    m_data->receivedStartTimeResponse(startTime);
}

bool CSSAnimationController::pauseAnimationAtTime(Element& element, const AtomString& name, double t)
{
    AnimationUpdateBlock animationUpdateBlock(this);
    return m_data->pauseAnimationAtTime(element, name, t);
}

unsigned CSSAnimationController::numberOfActiveAnimations(Document* document) const
{
    return m_data->numberOfActiveAnimations(document);
}

bool CSSAnimationController::pauseTransitionAtTime(Element& element, const String& property, double t)
{
    AnimationUpdateBlock animationUpdateBlock(this);
    return m_data->pauseTransitionAtTime(element, property, t);
}

bool CSSAnimationController::isRunningAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property) const
{
    if (!renderer.style().hasAnimationsOrTransitions())
        return false;
    return m_data->isRunningAnimationOnRenderer(renderer, property);
}

bool CSSAnimationController::isRunningAcceleratedAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property) const
{
    if (!renderer.style().hasAnimationsOrTransitions())
        return false;
    return m_data->isRunningAcceleratedAnimationOnRenderer(renderer, property);
}

bool CSSAnimationController::isSuspended() const
{
    return m_data->isSuspended();
}

void CSSAnimationController::suspendAnimations()
{
    LOG(Animations, "controller is suspending animations");
    m_data->suspendAnimations();
}

void CSSAnimationController::resumeAnimations()
{
    LOG(Animations, "controller is resuming animations");
    m_data->resumeAnimations();
}

bool CSSAnimationController::allowsNewAnimationsWhileSuspended() const
{
    return m_data->allowsNewAnimationsWhileSuspended();
}

void CSSAnimationController::setAllowsNewAnimationsWhileSuspended(bool allowed)
{
    m_data->setAllowsNewAnimationsWhileSuspended(allowed);
}

void CSSAnimationController::serviceAnimations()
{
    m_data->animationFrameCallbackFired();
}

void CSSAnimationController::updateThrottlingState()
{
    m_data->updateThrottlingState();
}

Seconds CSSAnimationController::animationInterval() const
{
    return m_data->animationInterval();
}

bool CSSAnimationController::animationsAreSuspendedForDocument(Document* document)
{
    return m_data->animationsAreSuspendedForDocument(document);
}

void CSSAnimationController::detachFromDocument(Document* document)
{
    return m_data->detachFromDocument(document);
}

void CSSAnimationController::suspendAnimationsForDocument(Document* document)
{
    LOG(Animations, "suspending animations for document %p", document);
    m_data->suspendAnimationsForDocument(document);
}

void CSSAnimationController::resumeAnimationsForDocument(Document* document)
{
    LOG(Animations, "resuming animations for document %p", document);
    AnimationUpdateBlock animationUpdateBlock(this);
    m_data->resumeAnimationsForDocument(document);
}

void CSSAnimationController::startAnimationsIfNotSuspended(Document* document)
{
    LOG(Animations, "animations may start for document %p", document);

    AnimationUpdateBlock animationUpdateBlock(this);
    m_data->startAnimationsIfNotSuspended(document);
}

void CSSAnimationController::beginAnimationUpdate()
{
    m_data->beginAnimationUpdate();
}

void CSSAnimationController::endAnimationUpdate()
{
    m_data->endAnimationUpdate();
}

bool CSSAnimationController::supportsAcceleratedAnimationOfProperty(CSSPropertyID property)
{
    return CSSPropertyAnimation::animationOfPropertyIsAccelerated(property);
}

bool CSSAnimationController::hasAnimations() const
{
    return m_data->hasAnimations();
}

} // namespace WebCore
