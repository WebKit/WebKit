/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentChangeObserver.h"

#if PLATFORM(IOS_FAMILY)
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMTimer.h"
#include "Document.h"
#include "FullscreenManager.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "Logging.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderDescendantIterator.h"
#include "Settings.h"

namespace WebCore {

static const Seconds maximumDelayForTimers { 400_ms };
static const Seconds maximumDelayForTransitions { 300_ms };

#if ENABLE(FULLSCREEN_API)
static bool isHiddenBehindFullscreenElement(const Node& descendantCandidate)
{
    // Fullscreen status is propagated on the ancestor document chain all the way to the top document.
    auto& document = descendantCandidate.document();
    auto* topMostFullScreenElement = document.topDocument().fullscreenManager().fullscreenElement();
    if (!topMostFullScreenElement)
        return false;

    // If the document where the node lives does not have an active fullscreen element, it is a sibling/nephew document -> not a descendant.
    auto* fullscreenElement = document.fullscreenManager().fullscreenElement();
    if (!fullscreenElement)
        return true;
    return !descendantCandidate.isDescendantOf(*fullscreenElement);
}
#endif

bool ContentChangeObserver::isVisuallyHidden(const Node& node)
{
    if (!node.renderStyle())
        return true;

    auto& style = *node.renderStyle();
    if (style.display() == DisplayType::None)
        return true;

    if (style.visibility() == Visibility::Hidden)
        return true;

    if (!style.opacity())
        return true;

    auto width = style.logicalWidth();
    auto height = style.logicalHeight();
    if ((width.isFixed() && !width.value()) || (height.isFixed() && !height.value()))
        return true;

    auto top = style.logicalTop();
    auto left = style.logicalLeft();
    // FIXME: This is trying to check if the element is outside of the viewport. This is incorrect for many reasons.
    if (left.isFixed() && width.isFixed() && -left.value() >= width.value())
        return true;
    if (top.isFixed() && height.isFixed() && -top.value() >= height.value())
        return true;

    // It's a common technique used to position content offscreen.
    if (style.hasOutOfFlowPosition() && left.isFixed() && left.value() <= -999)
        return true;

    // FIXME: Check for other cases like zero height with overflow hidden.
    auto maxHeight = style.maxHeight();
    if (maxHeight.isFixed() && !maxHeight.value())
        return true;

    // Special case opacity, because a descendant with non-zero opacity should still be considered hidden when one of its ancetors has opacity: 0;
    // YouTube.com has this setup with the bottom control bar.
    constexpr static unsigned numberOfAncestorsToCheckForOpacity = 4;
    unsigned i = 0;
    for (auto* parent = node.parentNode(); parent && i < numberOfAncestorsToCheckForOpacity; parent = parent->parentNode(), ++i) {
        if (!parent->renderStyle() || !parent->renderStyle()->opacity())
            return true;
    }

#if ENABLE(FULLSCREEN_API)
    if (isHiddenBehindFullscreenElement(node))
        return true;
#endif
    return false;
}

bool ContentChangeObserver::isConsideredVisible(const Node& node)
{
    if (isVisuallyHidden(node))
        return false;

    auto& style = *node.renderStyle();
    auto width = style.logicalWidth();
    // 1px width or height content is not considered visible.
    if (width.isFixed() && width.value() <= 1)
        return false;

    auto height = style.logicalHeight();
    if (height.isFixed() && height.value() <= 1)
        return false;

    return true;
}

enum class ElementHadRenderer { No, Yes };
static bool isConsideredClickable(const Element& candidateElement, ElementHadRenderer hadRenderer)
{
    auto& element = const_cast<Element&>(candidateElement);
    if (element.isInUserAgentShadowTree())
        return false;

    if (is<HTMLIFrameElement>(element))
        return true;

    if (is<HTMLImageElement>(element)) {
        // This is required to avoid HTMLImageElement's touch callout override logic. See rdar://problem/48937767.
        return element.Element::willRespondToMouseClickEvents();
    }

    bool hasRenderer = element.renderer();
    auto willRespondToMouseClickEvents = element.willRespondToMouseClickEvents();
    if (willRespondToMouseClickEvents || !hasRenderer || hadRenderer == ElementHadRenderer::No)
        return willRespondToMouseClickEvents;

    // In case when the content already had renderers it's not sufficient to check the candidate element only since it might just be the container for the clickable content.  
    for (auto& descendant : descendantsOfType<RenderElement>(*element.renderer())) {
        if (!descendant.element())
            continue;
        if (descendant.element()->willRespondToMouseClickEvents())
            return true;
    }
    return false;
}

ContentChangeObserver::ContentChangeObserver(Document& document)
    : m_document(document)
    , m_contentObservationTimer([this] { completeDurationBasedContentObservation(); })
{
}

static void willNotProceedWithClick(Frame& mainFrame)
{
    for (auto* frame = &mainFrame; frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document())
            document->contentChangeObserver().willNotProceedWithClick();
    }
}

void ContentChangeObserver::didCancelPotentialTap(Frame& mainFrame)
{
    LOG(ContentObservation, "didCancelPotentialTap: cancel ongoing content change observing.");
    WebCore::willNotProceedWithClick(mainFrame);
}

void ContentChangeObserver::didRecognizeLongPress(Frame& mainFrame)
{
    LOG(ContentObservation, "didRecognizeLongPress: cancel ongoing content change observing.");
    WebCore::willNotProceedWithClick(mainFrame);
}

void ContentChangeObserver::didPreventDefaultForEvent(Frame& mainFrame)
{
    LOG(ContentObservation, "didPreventDefaultForEvent: cancel ongoing content change observing.");
    WebCore::willNotProceedWithClick(mainFrame);
}

void ContentChangeObserver::startContentObservationForDuration(Seconds duration)
{
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    ASSERT(!hasVisibleChangeState());
    LOG_WITH_STREAM(ContentObservation, stream << "startContentObservationForDuration: start observing the content for " << duration.milliseconds() << "ms");
    adjustObservedState(Event::StartedFixedObservationTimeWindow);
    m_contentObservationTimer.startOneShot(duration);
}

void ContentChangeObserver::completeDurationBasedContentObservation()
{
    LOG_WITH_STREAM(ContentObservation, stream << "completeDurationBasedContentObservation: complete duration based content observing ");
    adjustObservedState(Event::EndedFixedObservationTimeWindow);
}

void ContentChangeObserver::didAddTransition(const Element& element, const Animation& transition)
{
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    if (hasVisibleChangeState())
        return;
    if (!isObservingContentChanges())
        return;
    if (!isObservingTransitions())
        return;
    if (!transition.isDurationSet() || !transition.isPropertySet())
        return;
    if (!isObservedPropertyForTransition(transition.property()))
        return;
    auto transitionEnd = Seconds { transition.duration() + std::max<double>(0, transition.isDelaySet() ? transition.delay() : 0) };
    if (transitionEnd > maximumDelayForTransitions)
        return;
    if (!isVisuallyHidden(element))
        return;
    // In case of multiple transitions, the first tranistion wins (and it has to produce a visible content change in order to show up as hover).
    if (m_elementsWithTransition.contains(&element))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "didAddTransition: transition created on " << &element << " (" << transitionEnd.milliseconds() << "ms).");

    m_elementsWithTransition.add(&element);
    adjustObservedState(Event::AddedTransition);
}

void ContentChangeObserver::didFinishTransition(const Element& element, CSSPropertyID propertyID)
{
    if (!isObservedPropertyForTransition(propertyID))
        return;
    if (!m_elementsWithTransition.take(&element))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "didFinishTransition: transition finished (" << &element << ").");

    // isConsideredClickable may trigger style update through Node::computeEditability. Let's adjust the state in the next runloop.
    callOnMainThread([weakThis = makeWeakPtr(*this), targetElement = makeWeakPtr(element)] {
        if (!weakThis || !targetElement)
            return;
        if (isVisuallyHidden(*targetElement)) {
            weakThis->adjustObservedState(Event::EndedTransitionButFinalStyleIsNotDefiniteYet);
            return;
        }
        if (isConsideredClickable(*targetElement, ElementHadRenderer::Yes))
            weakThis->elementDidBecomeVisible(*targetElement);
        weakThis->adjustObservedState(Event::CompletedTransition);
    });
}

void ContentChangeObserver::didRemoveTransition(const Element& element, CSSPropertyID propertyID)
{
    if (!isObservedPropertyForTransition(propertyID))
        return;
    if (!m_elementsWithTransition.take(&element))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "didRemoveTransition: transition got interrupted (" << &element << ").");

    adjustObservedState(Event::CanceledTransition);
}

void ContentChangeObserver::didInstallDOMTimer(const DOMTimer& timer, Seconds timeout, bool singleShot)
{
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    if (!isObservingContentChanges())
        return;
    if (!isObservingDOMTimerScheduling())
        return;
    if (hasVisibleChangeState())
        return;
    if (m_document.activeDOMObjectsAreSuspended())
        return;
    if (timeout > maximumDelayForTimers || !singleShot)
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "didInstallDOMTimer: register this timer: (" << &timer << ") and observe when it fires.");

    registerDOMTimer(timer);
    adjustObservedState(Event::InstalledDOMTimer);
}

void ContentChangeObserver::didRemoveDOMTimer(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "removeDOMTimer: remove registered timer (" << &timer << ")");

    unregisterDOMTimer(timer);
    adjustObservedState(Event::RemovedDOMTimer);
}

void ContentChangeObserver::willNotProceedWithClick()
{
    LOG(ContentObservation, "willNotProceedWithClick: click will not happen.");
    adjustObservedState(Event::WillNotProceedWithClick);
}

void ContentChangeObserver::domTimerExecuteDidStart(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "startObservingDOMTimerExecute: start observing (" << &timer << ") timer callback.");

    m_observedDomTimerIsBeingExecuted = true;
    adjustObservedState(Event::StartedDOMTimerExecution);
}

void ContentChangeObserver::domTimerExecuteDidFinish(const DOMTimer& timer)
{
    if (!m_observedDomTimerIsBeingExecuted)
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: stop observing (" << &timer << ") timer callback.");

    m_observedDomTimerIsBeingExecuted = false;
    unregisterDOMTimer(timer);
    adjustObservedState(Event::EndedDOMTimerExecution);
}

void ContentChangeObserver::styleRecalcDidStart()
{
    if (!isWaitingForStyleRecalc())
        return;
    LOG(ContentObservation, "startObservingStyleRecalc: start observing style recalc.");

    m_isInObservedStyleRecalc = true;
    adjustObservedState(Event::StartedStyleRecalc);
}

void ContentChangeObserver::styleRecalcDidFinish()
{
    if (!m_isInObservedStyleRecalc)
        return;
    LOG(ContentObservation, "stopObservingStyleRecalc: stop observing style recalc");

    m_isInObservedStyleRecalc = false;
    adjustObservedState(Event::EndedStyleRecalc);
}

void ContentChangeObserver::stopObservingPendingActivities()
{
    setShouldObserveNextStyleRecalc(false);
    setShouldObserveDOMTimerSchedulingAndTransitions(false);
    clearObservedDOMTimers();
    clearObservedTransitions();
}

void ContentChangeObserver::stopContentObservation()
{
    reset();
}

void ContentChangeObserver::reset()
{
    stopObservingPendingActivities();
    setHasNoChangeState();

    setTouchEventIsBeingDispatched(false);
    setIsBetweenTouchEndAndMouseMoved(false);
    setMouseMovedEventIsBeingDispatched(false);

    m_isInObservedStyleRecalc = false;
    m_observedDomTimerIsBeingExecuted = false;

    m_visibilityCandidateList.clear();

    m_contentObservationTimer.stop();
    m_elementsWithDestroyedVisibleRenderer.clear();
    resetHiddenTouchTarget();
}

void ContentChangeObserver::didSuspendActiveDOMObjects()
{
    LOG(ContentObservation, "didSuspendActiveDOMObjects");
    reset();
}

void ContentChangeObserver::willDetachPage()
{
    LOG(ContentObservation, "willDetachPage");
    reset();
}

void ContentChangeObserver::rendererWillBeDestroyed(const Element& element)
{ 
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    if (!isObservingContentChanges())
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "rendererWillBeDestroyed element: " << &element);

    if (!isVisuallyHidden(element))
        m_elementsWithDestroyedVisibleRenderer.add(&element);
    elementDidBecomeHidden(element);
}

void ContentChangeObserver::elementDidBecomeVisible(const Element& element)
{
    LOG_WITH_STREAM(ContentObservation, stream << "elementDidBecomeVisible: element went from hidden to visible: " << &element);
    m_visibilityCandidateList.add(element);
    adjustObservedState(Event::ElementDidBecomeVisible);
}

void ContentChangeObserver::elementDidBecomeHidden(const Element& element)
{
    LOG_WITH_STREAM(ContentObservation, stream << "elementDidBecomeHidden: element went from visible to hidden: " << &element);
    // Candidate element is no longer visible.
    if (!m_visibilityCandidateList.remove(element))
        return;
    ASSERT(hasVisibleChangeState());
    if (m_visibilityCandidateList.computesEmpty())
        setHasIndeterminateState();
}

void ContentChangeObserver::touchEventDidStart(PlatformEvent::Type eventType)
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_document.settings().contentChangeObserverEnabled() || m_document.quirks().shouldDisableContentChangeObserverTouchEventAdjustment())
        return;
    if (eventType != PlatformEvent::Type::TouchStart)
        return;
    LOG(ContentObservation, "touchEventDidStart: touch start event started.");
    setTouchEventIsBeingDispatched(true);
    adjustObservedState(Event::StartedTouchStartEventDispatching);
#else
    UNUSED_PARAM(eventType);
#endif
}

void ContentChangeObserver::touchEventDidFinish()
{
#if ENABLE(TOUCH_EVENTS)
    if (!isTouchEventBeingDispatched())
        return;
    ASSERT(m_document.settings().contentChangeObserverEnabled());
    LOG(ContentObservation, "touchEventDidFinish: touch start event finished.");
    setTouchEventIsBeingDispatched(false);
    adjustObservedState(Event::EndedTouchStartEventDispatching);
#endif
}

void ContentChangeObserver::mouseMovedDidStart()
{
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    LOG(ContentObservation, "mouseMovedDidStart: mouseMoved started.");
    setMouseMovedEventIsBeingDispatched(true);
    adjustObservedState(Event::StartedMouseMovedEventDispatching);
}

void ContentChangeObserver::mouseMovedDidFinish()
{
    if (!isMouseMovedEventBeingDispatched())
        return;
    ASSERT(m_document.settings().contentChangeObserverEnabled());
    LOG(ContentObservation, "mouseMovedDidFinish: mouseMoved finished.");
    adjustObservedState(Event::EndedMouseMovedEventDispatching);
    setMouseMovedEventIsBeingDispatched(false);
}

void ContentChangeObserver::setShouldObserveNextStyleRecalc(bool shouldObserve)
{
    if (shouldObserve)
        LOG(ContentObservation, "Wait until next style recalc fires.");
    m_isWaitingForStyleRecalc = shouldObserve;
}

void ContentChangeObserver::adjustObservedState(Event event)
{
    auto resetToStartObserving = [&] {
        setHasNoChangeState();
        clearObservedDOMTimers();
        clearObservedTransitions();
        setIsBetweenTouchEndAndMouseMoved(false);
        setShouldObserveNextStyleRecalc(false);
        setShouldObserveDOMTimerSchedulingAndTransitions(false);
        ASSERT(!m_isInObservedStyleRecalc);
        ASSERT(!m_observedDomTimerIsBeingExecuted);
    };

    auto notifyClientIfNeeded = [&] {
        if (isTouchEventBeingDispatched()) {
            LOG(ContentObservation, "notifyClientIfNeeded: Touch event is being dispatched. No need to notify the client.");
            return;
        }
        if (isBetweenTouchEndAndMouseMoved()) {
            LOG(ContentObservation, "notifyClientIfNeeded: Not reached mouseMoved yet. No need to notify the client.");
            return;
        }
        if (isMouseMovedEventBeingDispatched()) {
            LOG(ContentObservation, "notifyClientIfNeeded: in mouseMoved call. No need to notify the client.");
            return;
        }
        if (isObservationTimeWindowActive()) {
            LOG(ContentObservation, "notifyClientIfNeeded: Inside the fixed window observation. No need to notify the client.");
            return;
        }

        // The fixed observation window (which is the final step in content observation) is closed and now we check if are still waiting for timers or animations to finish.
        if (hasPendingActivity()) {
            LOG(ContentObservation, "notifyClientIfNeeded: We are still waiting on some events.");
            return;
        }

        // First demote to "no change" because we've got no pending activity anymore.
        if (observedContentChange() == WKContentIndeterminateChange)
            setHasNoChangeState();

        LOG_WITH_STREAM(ContentObservation, stream << "notifyClientIfNeeded: sending observedContentChange ->" << observedContentChange());
        ASSERT(m_document.page());
        ASSERT(m_document.frame());
        m_document.page()->chrome().client().didFinishContentChangeObserving(*m_document.frame(), observedContentChange());
        stopContentObservation();
    };

    // These user initiated events trigger content observation (touchStart and mouseMove). 
    {
        if (event == Event::StartedTouchStartEventDispatching) {
            resetToStartObserving();
            setShouldObserveDOMTimerSchedulingAndTransitions(true);
            return;
        }
        if (event == Event::EndedTouchStartEventDispatching) {
            setShouldObserveDOMTimerSchedulingAndTransitions(false);
            setIsBetweenTouchEndAndMouseMoved(true);
            return;
        }
        if (event == Event::StartedMouseMovedEventDispatching) {
            ASSERT(!m_document.hasPendingStyleRecalc());
            if (!isBetweenTouchEndAndMouseMoved())
                resetToStartObserving();
            setIsBetweenTouchEndAndMouseMoved(false);
            setShouldObserveDOMTimerSchedulingAndTransitions(!hasVisibleChangeState());
            return;
        }
        if (event == Event::EndedMouseMovedEventDispatching) {
            setShouldObserveDOMTimerSchedulingAndTransitions(false);
            return;
        }
    }
    // Fixed window observation starts soon after mouseMove when we don't have a definite answer to whether we should proceed with hover or click.
    {
        if (event == Event::StartedFixedObservationTimeWindow) {
            ASSERT(!hasVisibleChangeState());
            setHasIndeterminateState();
            return;
        }
        if (event == Event::EndedFixedObservationTimeWindow) {
            notifyClientIfNeeded();
            return;
        }
    }
    // These events (DOM timer, transition and style recalc) could trigger style changes that are candidates to visibility checking.
    {
        if (event == Event::InstalledDOMTimer || event == Event::AddedTransition) {
            ASSERT(!hasVisibleChangeState());
            setHasIndeterminateState();
            return;
        }
        if (event == Event::RemovedDOMTimer || event == Event::CanceledTransition) {
            notifyClientIfNeeded();
            return;
        }
        if (event == Event::StartedDOMTimerExecution) {
            ASSERT(isObservationTimeWindowActive() || observedContentChange() == WKContentIndeterminateChange);
            return;
        }
        if (event == Event::EndedDOMTimerExecution) {
            if (m_document.hasPendingStyleRecalc()) {
                setShouldObserveNextStyleRecalc(true);
                return;
            }
            notifyClientIfNeeded();
            return;
        }
        if (event == Event::EndedTransitionButFinalStyleIsNotDefiniteYet) {
            // onAnimationEnd can be called while in the middle of resolving the document (synchronously) or
            // asynchronously right before the style update is issued. It also means we don't know whether this animation ends up producing visible content yet. 
            if (m_document.inStyleRecalc()) {
                // We need to start observing this style change synchronously.
                m_isInObservedStyleRecalc = true;
                return;
            }
            setShouldObserveNextStyleRecalc(true);
            return;
        }
        if (event == Event::CompletedTransition) {
            if (m_document.inStyleRecalc()) {
                m_isInObservedStyleRecalc = true;
                return;
            }
            notifyClientIfNeeded();
            return;
        }
        if (event == Event::StartedStyleRecalc) {
            setShouldObserveNextStyleRecalc(false);
            ASSERT(isObservationTimeWindowActive() || observedContentChange() == WKContentIndeterminateChange);
            return;
        }
        if (event == Event::EndedStyleRecalc) {
            notifyClientIfNeeded();
            return;
        }
    }
    // Either the page decided to call preventDefault on the touch action or the tap gesture evolved to some other gesture (long press, double tap). 
    if (event == Event::WillNotProceedWithClick) {
        stopContentObservation();
        return;
    }
    // The page produced an visible change on an actionable content.
    if (event == Event::ElementDidBecomeVisible) {
        setHasVisibleChangeState();
        // Stop pending activities. We don't need to observe them anymore.
        stopObservingPendingActivities();
        return;
    }
}

bool ContentChangeObserver::shouldObserveVisibilityChangeForElement(const Element& element)
{
    return isObservingContentChanges() && !visibleRendererWasDestroyed(element) && !element.document().quirks().shouldIgnoreContentChange(element);
}

ContentChangeObserver::StyleChangeScope::StyleChangeScope(Document& document, const Element& element)
    : m_contentChangeObserver(document.contentChangeObserver())
    , m_element(element)
    , m_hadRenderer(element.renderer())
{
    if (m_contentChangeObserver.shouldObserveVisibilityChangeForElement(element))
        m_wasHidden = isVisuallyHidden(m_element);
}

ContentChangeObserver::StyleChangeScope::~StyleChangeScope()
{
    // Do we track this element?
    if (!m_wasHidden.hasValue())
        return;

    if (!isConsideredClickable(m_element, m_hadRenderer ? ElementHadRenderer::Yes : ElementHadRenderer::No))
        return;

    auto wasVisible = !m_wasHidden.value();
    auto isVisible = isConsideredVisible(m_element);
    if (!wasVisible && isVisible)
        m_contentChangeObserver.elementDidBecomeVisible(m_element);
    else if (wasVisible && !isVisible)
        m_contentChangeObserver.elementDidBecomeHidden(m_element);
}

#if ENABLE(TOUCH_EVENTS)
ContentChangeObserver::TouchEventScope::TouchEventScope(Document& document, PlatformEvent::Type eventType)
    : m_contentChangeObserver(document.contentChangeObserver())
{
    m_contentChangeObserver.touchEventDidStart(eventType);
}

ContentChangeObserver::TouchEventScope::~TouchEventScope()
{
    m_contentChangeObserver.touchEventDidFinish();
}
#endif

ContentChangeObserver::MouseMovedScope::MouseMovedScope(Document& document)
    : m_contentChangeObserver(document.contentChangeObserver())
{
    m_contentChangeObserver.mouseMovedDidStart();
}

ContentChangeObserver::MouseMovedScope::~MouseMovedScope()
{
    m_contentChangeObserver.mouseMovedDidFinish();
    m_contentChangeObserver.resetHiddenTouchTarget();
}

ContentChangeObserver::StyleRecalcScope::StyleRecalcScope(Document& document)
    : m_contentChangeObserver(document.contentChangeObserver())
{
    m_contentChangeObserver.styleRecalcDidStart();
}

ContentChangeObserver::StyleRecalcScope::~StyleRecalcScope()
{
    m_contentChangeObserver.styleRecalcDidFinish();
}

ContentChangeObserver::DOMTimerScope::DOMTimerScope(Document* document, const DOMTimer& domTimer)
    : m_contentChangeObserver(document ? &document->contentChangeObserver() : nullptr)
    , m_domTimer(domTimer)
{
    if (m_contentChangeObserver)
        m_contentChangeObserver->domTimerExecuteDidStart(m_domTimer);
}

ContentChangeObserver::DOMTimerScope::~DOMTimerScope()
{
    if (m_contentChangeObserver)
        m_contentChangeObserver->domTimerExecuteDidFinish(m_domTimer);
}

}

#endif // PLATFORM(IOS_FAMILY)
