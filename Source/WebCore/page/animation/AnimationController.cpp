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
#include "AnimationEvent.h"
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

class AnimationPrivateUpdateBlock {
public:
    AnimationPrivateUpdateBlock(AnimationControllerPrivate& animationController)
        : m_animationController(animationController)
    {
        m_animationController.beginAnimationUpdate();
    }
    
    ~AnimationPrivateUpdateBlock()
    {
        m_animationController.endAnimationUpdate();
    }
    
    AnimationControllerPrivate& m_animationController;
};

AnimationControllerPrivate::AnimationControllerPrivate(Frame& frame)
    : m_animationTimer(*this, &AnimationControllerPrivate::animationTimerFired)
    , m_updateStyleIfNeededDispatcher(*this, &AnimationControllerPrivate::updateStyleIfNeededDispatcherFired)
    , m_frame(frame)
    , m_beginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet)
    , m_beginAnimationUpdateCount(0)
    , m_waitingForAsyncStartNotification(false)
    , m_isSuspended(false)
    , m_allowsNewAnimationsWhileSuspended(false)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
}

CompositeAnimation& AnimationControllerPrivate::ensureCompositeAnimation(RenderElement& renderer)
{
    auto result = m_compositeAnimations.add(&renderer, nullptr);
    if (result.isNewEntry) {
        result.iterator->value = CompositeAnimation::create(*this);
        renderer.setIsCSSAnimating(true);
    }

    return *result.iterator->value;
}

bool AnimationControllerPrivate::clear(RenderElement& renderer)
{
    LOG(Animations, "AnimationControllerPrivate %p clear: %p", this, &renderer);

    ASSERT(renderer.isCSSAnimating());
    ASSERT(m_compositeAnimations.contains(&renderer));

    Element* element = renderer.element();

    m_eventsToDispatch.removeAllMatching([element] (const EventToDispatch& info) {
        return info.element == element;
    });

    m_elementChangesToDispatch.removeAllMatching([element] (const Ref<Element>& currElement) {
        return &currElement.get() == element;
    });
    
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setNeedsStyleRecalc() when suspended).
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.take(&renderer);
    ASSERT(animation);
    renderer.setIsCSSAnimating(false);
    animation->clearRenderer();
    return animation->isSuspended();
}

double AnimationControllerPrivate::updateAnimations(SetChanged callSetChanged/* = DoNotCallSetChanged*/)
{
    AnimationPrivateUpdateBlock updateBlock(*this);
    double timeToNextService = -1;
    bool calledSetChanged = false;

    for (auto& compositeAnimation : m_compositeAnimations) {
        CompositeAnimation& animation = *compositeAnimation.value;
        if (!animation.isSuspended() && animation.hasAnimations()) {
            double t = animation.timeToNextService();
            if (t != -1 && (t < timeToNextService || timeToNextService == -1))
                timeToNextService = t;
            if (!timeToNextService) {
                if (callSetChanged != CallSetChanged)
                    break;
                Element* element = compositeAnimation.key->element();
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

void AnimationControllerPrivate::updateAnimationTimerForRenderer(RenderElement& renderer)
{
    double timeToNextService = 0;

    const CompositeAnimation* compositeAnimation = m_compositeAnimations.get(&renderer);
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

void AnimationControllerPrivate::updateStyleIfNeededDispatcherFired()
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
    for (auto& event : eventsToDispatch) {
        Element& element = *event.element;
        if (event.eventType == eventNames().transitionendEvent)
            element.dispatchEvent(TransitionEvent::create(event.eventType, event.name, event.elapsedTime, PseudoElement::pseudoElementNameForEvents(element.pseudoId())));
        else
            element.dispatchEvent(AnimationEvent::create(event.eventType, event.name, event.elapsedTime));
    }

    for (auto& change : m_elementChangesToDispatch)
        change->setNeedsStyleRecalc(SyntheticStyleChange);

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

void AnimationControllerPrivate::addElementChangeToDispatch(Ref<Element>&& element)
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

void AnimationControllerPrivate::animationTimerFired()
{
    // We need to keep the frame alive, since it owns us.
    Ref<Frame> protector(m_frame);

    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. accelerated animations)
    AnimationPrivateUpdateBlock updateBlock(*this);

    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateStyleIfNeeded.  It will then call back to us with new information.
    updateAnimationTimer(CallSetChanged);

    // Fire events right away, to avoid a flash of unanimated style after an animation completes, and before
    // the 'end' event fires.
    fireEventsAndUpdateStyle();
}

bool AnimationControllerPrivate::isRunningAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    ASSERT(renderer.isCSSAnimating());
    ASSERT(m_compositeAnimations.contains(&renderer));
    const CompositeAnimation& animation = *m_compositeAnimations.get(&renderer);
    return animation.isAnimatingProperty(property, false, runningState);
}

bool AnimationControllerPrivate::isRunningAcceleratedAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    ASSERT(renderer.isCSSAnimating());
    ASSERT(m_compositeAnimations.contains(&renderer));
    const CompositeAnimation& animation = *m_compositeAnimations.get(&renderer);
    return animation.isAnimatingProperty(property, true, runningState);
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
    AnimationPrivateUpdateBlock updateBlock(*this);

    for (auto& animation : m_compositeAnimations) {
        if (&animation.key->document() == document)
            animation.value->suspendAnimations();
    }

    updateAnimationTimer();
}

void AnimationControllerPrivate::resumeAnimationsForDocument(Document* document)
{
    AnimationPrivateUpdateBlock updateBlock(*this);

    for (auto& animation : m_compositeAnimations) {
        if (&animation.key->document() == document)
            animation.value->resumeAnimations();
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

    CompositeAnimation& compositeAnimation = ensureCompositeAnimation(*renderer);
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

    CompositeAnimation& compositeAnimation = ensureCompositeAnimation(*renderer);
    if (compositeAnimation.pauseTransitionAtTime(cssPropertyID(property), t)) {
        renderer->element()->setNeedsStyleRecalc(SyntheticStyleChange);
        startUpdateStyleIfNeededDispatcher();
        return true;
    }

    return false;
}

double AnimationControllerPrivate::beginAnimationUpdateTime()
{
    ASSERT(m_beginAnimationUpdateCount);
    if (m_beginAnimationUpdateTime == cBeginAnimationUpdateTimeNotSet)
        m_beginAnimationUpdateTime = monotonicallyIncreasingTime();

    return m_beginAnimationUpdateTime;
}

void AnimationControllerPrivate::beginAnimationUpdate()
{
    if (!m_beginAnimationUpdateCount)
        setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    ++m_beginAnimationUpdateCount;
}

void AnimationControllerPrivate::endAnimationUpdate()
{
    ASSERT(m_beginAnimationUpdateCount > 0);
    if (m_beginAnimationUpdateCount == 1) {
        styleAvailable();
        if (!m_waitingForAsyncStartNotification)
            startTimeResponse(beginAnimationUpdateTime());
    }
    --m_beginAnimationUpdateCount;
}

void AnimationControllerPrivate::receivedStartTimeResponse(double time)
{
    LOG(Animations, "AnimationControllerPrivate %p receivedStartTimeResponse %f", this, time);

    m_waitingForAsyncStartNotification = false;
    startTimeResponse(time);
}

PassRefPtr<RenderStyle> AnimationControllerPrivate::getAnimatedStyleForRenderer(RenderElement& renderer)
{
    AnimationPrivateUpdateBlock animationUpdateBlock(*this);

    ASSERT(renderer.isCSSAnimating());
    ASSERT(m_compositeAnimations.contains(&renderer));
    const CompositeAnimation& rendererAnimations = *m_compositeAnimations.get(&renderer);
    RefPtr<RenderStyle> animatingStyle = rendererAnimations.getAnimatedStyle();
    if (!animatingStyle)
        animatingStyle = &renderer.style();
    
    return animatingStyle.release();
}

bool AnimationControllerPrivate::computeExtentOfAnimation(RenderElement& renderer, LayoutRect& bounds) const
{
    ASSERT(renderer.isCSSAnimating());
    ASSERT(m_compositeAnimations.contains(&renderer));

    const CompositeAnimation& rendererAnimations = *m_compositeAnimations.get(&renderer);
    if (!rendererAnimations.isAnimatingProperty(CSSPropertyTransform, false, AnimationBase::Running | AnimationBase::Paused))
        return true;

    return rendererAnimations.computeExtentOfTransformAnimation(bounds);
}

unsigned AnimationControllerPrivate::numberOfActiveAnimations(Document* document) const
{
    unsigned count = 0;
    
    for (auto& animation : m_compositeAnimations) {
        if (&animation.key->document() == document)
            count += animation.value->numberOfActiveAnimations();
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
    LOG(Animations, "AnimationControllerPrivate %p animationWillBeRemoved: %p", this, animation);

    removeFromAnimationsWaitingForStyle(animation);
    removeFromAnimationsWaitingForStartTimeResponse(animation);
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    removeFromAnimationsDependentOnScroll(animation);
#endif

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

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
void AnimationControllerPrivate::addToAnimationsDependentOnScroll(AnimationBase* animation)
{
    m_animationsDependentOnScroll.add(animation);
}

void AnimationControllerPrivate::removeFromAnimationsDependentOnScroll(AnimationBase* animation)
{
    m_animationsDependentOnScroll.remove(animation);
}

void AnimationControllerPrivate::scrollWasUpdated()
{
    auto* view = m_frame.view();
    if (!view || !wantsScrollUpdates())
        return;

    m_scrollPosition = view->scrollPositionForFixedPosition().y().toFloat();

    // FIXME: This is updating all the animations, rather than just the ones
    // that are dependent on scroll. We to go from our AnimationBase to its CompositeAnimation
    // so we can execute code similar to updateAnimations.
    // https://bugs.webkit.org/show_bug.cgi?id=144170
    updateAnimations(CallSetChanged);
}
#endif

AnimationController::AnimationController(Frame& frame)
    : m_data(std::make_unique<AnimationControllerPrivate>(frame))
{
}

AnimationController::~AnimationController()
{
}

void AnimationController::cancelAnimations(RenderElement& renderer)
{
    if (!renderer.isCSSAnimating())
        return;

    if (!m_data->clear(renderer))
        return;

    Element* element = renderer.element();
    ASSERT(!element || !element->document().inPageCache());
    if (element)
        element->setNeedsStyleRecalc(SyntheticStyleChange);
}

bool AnimationController::updateAnimations(RenderElement& renderer, RenderStyle& newStyle, Ref<RenderStyle>& animatedStyle)
{
    RenderStyle* oldStyle = renderer.hasInitializedStyle() ? &renderer.style() : nullptr;
    if ((!oldStyle || (!oldStyle->animations() && !oldStyle->transitions())) && (!newStyle.animations() && !newStyle.transitions()))
        return false;

    if (renderer.document().inPageCache())
        return false;

    // Don't run transitions when printing.
    if (renderer.view().printing())
        return false;

    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.

    // We don't support anonymous pseudo elements like :first-line or :first-letter.
    ASSERT(renderer.element());

    CompositeAnimation& rendererAnimations = m_data->ensureCompositeAnimation(renderer);
    bool animationStateChanged = rendererAnimations.animate(renderer, oldStyle, newStyle, animatedStyle);

    if (renderer.parent() || newStyle.animations() || (oldStyle && oldStyle->animations())) {
        m_data->updateAnimationTimerForRenderer(renderer);
#if ENABLE(REQUEST_ANIMATION_FRAME)
        renderer.view().frameView().scheduleAnimation();
#endif
    }

    if (animatedStyle.ptr() != &newStyle) {
        // If the animations/transitions change opacity or transform, we need to update
        // the style to impose the stacking rules. Note that this is also
        // done in StyleResolver::adjustRenderStyle().
        if (animatedStyle.get().hasAutoZIndex() && (animatedStyle.get().opacity() < 1.0f || animatedStyle.get().hasTransform()))
            animatedStyle.get().setZIndex(0);
    }
    return animationStateChanged;
}

PassRefPtr<RenderStyle> AnimationController::getAnimatedStyleForRenderer(RenderElement& renderer)
{
    if (!renderer.isCSSAnimating())
        return &renderer.style();
    return m_data->getAnimatedStyleForRenderer(renderer);
}

bool AnimationController::computeExtentOfAnimation(RenderElement& renderer, LayoutRect& bounds) const
{
    if (!renderer.isCSSAnimating())
        return true;

    return m_data->computeExtentOfAnimation(renderer, bounds);
}

void AnimationController::notifyAnimationStarted(RenderElement& renderer, double startTime)
{
    LOG(Animations, "AnimationController %p notifyAnimationStarted on renderer %p, time=%f", this, &renderer, startTime);
    UNUSED_PARAM(renderer);

    AnimationUpdateBlock animationUpdateBlock(this);
    m_data->receivedStartTimeResponse(startTime);
}

bool AnimationController::pauseAnimationAtTime(RenderElement* renderer, const AtomicString& name, double t)
{
    AnimationUpdateBlock animationUpdateBlock(this);
    return m_data->pauseAnimationAtTime(renderer, name, t);
}

unsigned AnimationController::numberOfActiveAnimations(Document* document) const
{
    return m_data->numberOfActiveAnimations(document);
}

bool AnimationController::pauseTransitionAtTime(RenderElement* renderer, const String& property, double t)
{
    AnimationUpdateBlock animationUpdateBlock(this);
    return m_data->pauseTransitionAtTime(renderer, property, t);
}

bool AnimationController::isRunningAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    return renderer.isCSSAnimating() && m_data->isRunningAnimationOnRenderer(renderer, property, runningState);
}

bool AnimationController::isRunningAcceleratedAnimationOnRenderer(RenderElement& renderer, CSSPropertyID property, AnimationBase::RunningState runningState) const
{
    return renderer.isCSSAnimating() && m_data->isRunningAcceleratedAnimationOnRenderer(renderer, property, runningState);
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
    AnimationUpdateBlock animationUpdateBlock(this);
    m_data->resumeAnimationsForDocument(document);
}

void AnimationController::startAnimationsIfNotSuspended(Document* document)
{
    LOG(Animations, "animations may start for document %p", document);

    AnimationUpdateBlock animationUpdateBlock(this);
    m_data->startAnimationsIfNotSuspended(document);
}

void AnimationController::beginAnimationUpdate()
{
    m_data->beginAnimationUpdate();
}

void AnimationController::endAnimationUpdate()
{
    m_data->endAnimationUpdate();
}

bool AnimationController::supportsAcceleratedAnimationOfProperty(CSSPropertyID property)
{
    return CSSPropertyAnimation::animationOfPropertyIsAccelerated(property);
}

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
bool AnimationController::wantsScrollUpdates() const
{
    return m_data->wantsScrollUpdates();
}

void AnimationController::scrollWasUpdated()
{
    m_data->scrollWasUpdated();
}
#endif

} // namespace WebCore
