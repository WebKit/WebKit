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
#include "Logging.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "Settings.h"

namespace WebCore {

ContentChangeObserver::ContentChangeObserver(Document& document)
    : m_document(document)
    , m_contentObservationTimer([this] { completeDurationBasedContentObservation(); })
{
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

void ContentChangeObserver::didInstallDOMTimer(const DOMTimer& timer, Seconds timeout, bool singleShot)
{
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    if (m_document.activeDOMObjectsAreSuspended())
        return;
    if (timeout > 250_ms || !singleShot)
        return;
    if (!isObservingDOMTimerScheduling())
        return;
    if (hasVisibleChangeState())
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

void ContentChangeObserver::domTimerExecuteDidStart(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "startObservingDOMTimerExecute: start observing (" << &timer << ") timer callback.");

    m_domTimerIsBeingExecuted = true;
}

void ContentChangeObserver::domTimerExecuteDidFinish(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: stop observing (" << &timer << ") timer callback.");

    m_domTimerIsBeingExecuted = false;
    unregisterDOMTimer(timer);
    setShouldObserveNextStyleRecalc(m_document.hasPendingStyleRecalc());
    adjustObservedState(Event::EndedDOMTimerExecution);
}

void ContentChangeObserver::styleRecalcDidStart()
{
    if (!isObservingStyleRecalc())
        return;
    if (hasVisibleChangeState())
        return;
    LOG(ContentObservation, "startObservingStyleRecalc: start observing style recalc.");

    m_styleRecalcIsBeingExecuted = true;
}

void ContentChangeObserver::styleRecalcDidFinish()
{
    if (!isObservingStyleRecalc())
        return;
    LOG(ContentObservation, "stopObservingStyleRecalc: stop observing style recalc");

    m_styleRecalcIsBeingExecuted = false;
    setShouldObserveNextStyleRecalc(false);
    adjustObservedState(Event::StyleRecalcFinished);
}

void ContentChangeObserver::cancelPendingActivities()
{
    clearObservedDOMTimers();
    m_contentObservationTimer.stop();
}

void ContentChangeObserver::didSuspendActiveDOMObjects()
{
    LOG(ContentObservation, "didSuspendActiveDOMObjects");
    cancelPendingActivities();
}

void ContentChangeObserver::willDetachPage()
{
    LOG(ContentObservation, "willDetachPage");
    cancelPendingActivities();
}

void ContentChangeObserver::contentVisibilityDidChange()
{
    LOG(ContentObservation, "contentVisibilityDidChange: visible content change did happen.");
    adjustObservedState(Event::ContentVisibilityChanged);
}

void ContentChangeObserver::touchEventDidStart(PlatformEvent::Type eventType)
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    if (eventType != PlatformEvent::Type::TouchStart)
        return;
    LOG(ContentObservation, "touchEventDidStart: touch start event started.");
    m_touchEventIsBeingDispatched = true;
    adjustObservedState(Event::StartedTouchStartEventDispatching);
#else
    UNUSED_PARAM(eventType);
#endif
}

void ContentChangeObserver::touchEventDidFinish()
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchEventIsBeingDispatched)
        return;
    ASSERT(m_document.settings().contentChangeObserverEnabled());
    LOG(ContentObservation, "touchEventDidFinish: touch start event finished.");
    m_touchEventIsBeingDispatched = false;
    adjustObservedState(Event::EndedTouchStartEventDispatching);
#endif
}

void ContentChangeObserver::mouseMovedDidStart()
{
    if (!m_document.settings().contentChangeObserverEnabled())
        return;
    LOG(ContentObservation, "mouseMovedDidStart: mouseMoved started.");
    m_mouseMovedEventIsBeingDispatched = true;
    adjustObservedState(Event::StartedMouseMovedEventDispatching);
}

void ContentChangeObserver::mouseMovedDidFinish()
{
    if (!m_mouseMovedEventIsBeingDispatched)
        return;
    ASSERT(m_document.settings().contentChangeObserverEnabled());
    LOG(ContentObservation, "mouseMovedDidFinish: mouseMoved finished.");
    adjustObservedState(Event::EndedMouseMovedEventDispatching);
    m_mouseMovedEventIsBeingDispatched = false;
}

WKContentChange ContentChangeObserver::observedContentChange() const
{
    return WKObservedContentChange();
}

void ContentChangeObserver::setShouldObserveNextStyleRecalc(bool shouldObserve)
{
    if (shouldObserve)
        LOG(ContentObservation, "Wait until next style recalc fires.");
    m_isObservingStyleRecalc = shouldObserve;
}

bool ContentChangeObserver::hasDeterminateState() const
{
    if (hasVisibleChangeState())
        return true;
    return observedContentChange() == WKContentNoChange && !hasPendingActivity();
}

#if !ASSERT_DISABLED
bool ContentChangeObserver::isNotifyContentChangeAllowed() const
{
    return m_document.settings().contentChangeObserverEnabled() && !m_mouseMovedEventIsBeingDispatched;
}
#endif

void ContentChangeObserver::adjustObservedState(Event event)
{
    auto notifyContentChangeIfNeeded = [&] {
        if (!hasDeterminateState()) {
            LOG(ContentObservation, "notifyContentChangeIfNeeded: not in a determined state yet.");
            return;
        }
        LOG_WITH_STREAM(ContentObservation, stream << "notifyContentChangeIfNeeded: sending observedContentChange ->" << observedContentChange());
        ASSERT(isNotifyContentChangeAllowed());
        ASSERT(m_document.page());
        ASSERT(m_document.frame());
        m_document.page()->chrome().client().observedContentChange(*m_document.frame());
    };

    switch (event) {
    case Event::StartedTouchStartEventDispatching:
        setHasNoChangeState();
        clearObservedDOMTimers();
        m_isMouseMovedPrecededByTouch = true;
        setShouldObserveDOMTimerScheduling(true);
        break;
    case Event::StartedMouseMovedEventDispatching:
        ASSERT(!m_document.hasPendingStyleRecalc());
        if (!m_isMouseMovedPrecededByTouch) {
            setHasNoChangeState();
            clearObservedDOMTimers();
        }
        setShouldObserveDOMTimerScheduling(true);
        m_isMouseMovedPrecededByTouch = false;
        break;
    case Event::EndedTouchStartEventDispatching:
    case Event::EndedMouseMovedEventDispatching:
        setShouldObserveDOMTimerScheduling(false);
        break;
    case Event::InstalledDOMTimer:
    case Event::StartedFixedObservationTimeWindow:
        // Expecting a timer fire. Promote to an indeterminate state.
        ASSERT(!hasVisibleChangeState());
        setHasIndeterminateState();
        break;
    case Event::RemovedDOMTimer:
    case Event::StyleRecalcFinished:
    case Event::EndedDOMTimerExecution:
    case Event::EndedFixedObservationTimeWindow:
        // Demote to "no change" when there's no pending activity anymore.
        if (observedContentChange() == WKContentIndeterminateChange && !hasPendingActivity())
            setHasNoChangeState();
        notifyContentChangeIfNeeded();
        break;
    case Event::ContentVisibilityChanged:
        setHasVisibleChangeState();
        break;
    }
}

static bool isConsideredHidden(const Element& element)
{
    if (!element.renderStyle())
        return true;

    auto& style = *element.renderStyle();
    if (style.display() == DisplayType::None)
        return true;

    if (style.visibility() == Visibility::Hidden)
        return true;

    auto width = style.width();
    auto height = style.height();
    if ((width.isFixed() && !width.value()) || (height.isFixed() && !height.value()))
        return true;

    auto top = style.top();
    auto left = style.left();
    // FIXME: This is trying to check if the element is outside of the viewport. This is incorrect for many reasons.
    if (left.isFixed() && width.isFixed() && -left.value() >= width.value())
        return true;

    if (top.isFixed() && height.isFixed() && -top.value() >= height.value())
        return true;

    // FIXME: Check for other cases like zero height with overflow hidden.
    auto maxHeight = style.maxHeight();
    if (maxHeight.isFixed() && !maxHeight.value())
        return true;

    return false;
}

ContentChangeObserver::StyleChangeScope::StyleChangeScope(Document& document, const Element& element)
    : m_contentChangeObserver(document.contentChangeObserver())
    , m_element(element)
{
    auto qualifiesForVisibilityCheck = [&] {
        if (m_element.isInUserAgentShadowTree())
            return false;
        if (!const_cast<Element&>(m_element).willRespondToMouseClickEvents())
            return false;
        return true;
    };

    auto needsObserving = m_contentChangeObserver.isObservingContentChanges() && !m_contentChangeObserver.hasVisibleChangeState() && qualifiesForVisibilityCheck();
    if (needsObserving)
        m_wasHidden = isConsideredHidden(m_element);
}

ContentChangeObserver::StyleChangeScope::~StyleChangeScope()
{
    if (!m_wasHidden || isConsideredHidden(m_element))
        return;

    m_contentChangeObserver.contentVisibilityDidChange();
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
