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

namespace WebCore {

ContentChangeObserver::ContentChangeObserver(Document& document)
    : m_document(document)
    , m_contentObservationTimer([this] { stopDurationBasedContentObservation(); })
{
}

void ContentChangeObserver::startContentObservationForDuration(Seconds duration)
{
    if (hasVisibleChangeState())
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "startContentObservationForDuration: start observing the content for " << duration.milliseconds() << "ms");
    adjustObservedState(Event::StartedFixedObservationTimeWindow);
    m_contentObservationTimer.startOneShot(duration);
}

void ContentChangeObserver::stopDurationBasedContentObservation()
{
    LOG_WITH_STREAM(ContentObservation, stream << "stopDurationBasedContentObservation: stop duration based content observing ");
    adjustObservedState(Event::EndedFixedObservationTimeWindow);
    notifyContentChangeIfNeeded();
}

void ContentChangeObserver::didInstallDOMTimer(const DOMTimer& timer, Seconds timeout, bool singleShot)
{
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
}

void ContentChangeObserver::didRemoveDOMTimer(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "removeDOMTimer: remove registered timer (" << &timer << ")");

    unregisterDOMTimer(timer);
    notifyContentChangeIfNeeded();
}

void ContentChangeObserver::domTimerExecuteDidStart(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "startObservingDOMTimerExecute: start observing (" << &timer << ") timer callback.");

    m_domTimerisBeingExecuted = true;
}

void ContentChangeObserver::domTimerExecuteDidFinish(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: stop observing (" << &timer << ") timer callback.");

    m_domTimerisBeingExecuted = false;
    unregisterDOMTimer(timer);
    setShouldObserveNextStyleRecalc(m_document.hasPendingStyleRecalc());
    notifyContentChangeIfNeeded();
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
    notifyContentChangeIfNeeded();
}

void ContentChangeObserver::clearTimersAndReportContentChange()
{
    if (!hasObservedDOMTimer())
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "clearTimersAndReportContentChange: remove registered timers and report content change." << observedContentChange());

    clearObservedDOMTimers();
    ASSERT(m_document.page());
    ASSERT(m_document.frame());
    m_document.page()->chrome().client().observedContentChange(*m_document.frame());
}

void ContentChangeObserver::didSuspendActiveDOMObjects()
{
    clearTimersAndReportContentChange();
}

void ContentChangeObserver::willDetachPage()
{
    clearTimersAndReportContentChange();
}

void ContentChangeObserver::contentVisibilityDidChange()
{
    LOG(ContentObservation, "contentVisibilityDidChange: visible content change did happen.");
    adjustObservedState(Event::ContentVisibilityChanged);
}

void ContentChangeObserver::mouseMovedDidStart()
{
    ASSERT(!m_document.hasPendingStyleRecalc());
    clearObservedDOMTimers();
    setShouldObserveDOMTimerScheduling(true);
    adjustObservedState(Event::ContentObservationStarted);
}

void ContentChangeObserver::mouseMovedDidFinish()
{
    setShouldObserveDOMTimerScheduling(false);
}

WKContentChange ContentChangeObserver::observedContentChange() const
{
    return WKObservedContentChange();
}

void ContentChangeObserver::registerDOMTimer(const DOMTimer& timer)
{
    m_DOMTimerList.add(&timer);
    adjustObservedState(Event::InstalledDOMTimer);
}

void ContentChangeObserver::unregisterDOMTimer(const DOMTimer& timer)
{
    m_DOMTimerList.remove(&timer);
    adjustObservedState(Event::RemovedDOMTimer);
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

void ContentChangeObserver::adjustObservedState(Event event)
{
    switch (event) {
    case Event::ContentObservationStarted:
        setHasNoChangeState();
        break;
    case Event::InstalledDOMTimer:
    case Event::StartedFixedObservationTimeWindow:
        // Expecting a timer fire. Promote to an indeterminate state.
        ASSERT(!hasVisibleChangeState());
        setHasIndeterminateState();
        break;
    case Event::RemovedDOMTimer:
    case Event::StyleRecalcFinished:
    case Event::EndedFixedObservationTimeWindow:
        // Demote to "no change" when there's no pending activity anymore.
        if (observedContentChange() == WKContentIndeterminateChange && !hasPendingActivity())
            setHasNoChangeState();
        break;
    case Event::ContentVisibilityChanged:
        setHasVisibleChangeState();
        break;
    }
}

void ContentChangeObserver::notifyContentChangeIfNeeded()
{
    if (!hasDeterminateState()) {
        LOG(ContentObservation, "notifyContentChangeIfNeeded: not in a determined state yet.");
        return;
    }
    LOG_WITH_STREAM(ContentObservation, stream << "notifyContentChangeIfNeeded: sending observedContentChange ->" << observedContentChange());
    ASSERT(m_document.page());
    ASSERT(m_document.frame());
    m_document.page()->chrome().client().observedContentChange(*m_document.frame());
}


static Visibility elementImplicitVisibility(const Element& element)
{
    auto* renderer = element.renderer();
    if (!renderer)
        return Visibility::Visible;

    auto& style = renderer->style();

    auto width = style.width();
    auto height = style.height();
    if ((width.isFixed() && width.value() <= 0) || (height.isFixed() && height.value() <= 0))
        return Visibility::Hidden;

    auto top = style.top();
    auto left = style.left();
    if (left.isFixed() && width.isFixed() && -left.value() >= width.value())
        return Visibility::Hidden;

    if (top.isFixed() && height.isFixed() && -top.value() >= height.value())
        return Visibility::Hidden;
    return Visibility::Visible;
}

ContentChangeObserver::StyleChangeScope::StyleChangeScope(Document& document, const Element& element)
    : m_contentChangeObserver(document.contentChangeObserver())
    , m_element(element)
    , m_needsObserving(m_contentChangeObserver.isObservingContentChanges() && !m_contentChangeObserver.hasVisibleChangeState())
{
    if (m_needsObserving) {
        m_previousDisplay = element.renderStyle() ? element.renderStyle()->display() : DisplayType::None;
        m_previousVisibility = element.renderStyle() ? element.renderStyle()->visibility() : Visibility::Hidden;
        m_previousImplicitVisibility = elementImplicitVisibility(element);
    }
}

ContentChangeObserver::StyleChangeScope::~StyleChangeScope()
{
    if (!m_needsObserving)
        return;

    auto* style = m_element.renderStyle();
    auto qualifiesForVisibilityCheck = [&] {
        if (!style)
            return false;
        if (m_element.isInUserAgentShadowTree())
            return false;
        if (!const_cast<Element&>(m_element).willRespondToMouseClickEvents())
            return false;
        return true;
    };

    if (!qualifiesForVisibilityCheck())
        return;

    if ((m_previousDisplay == DisplayType::None && style->display() != DisplayType::None)
        || (m_previousVisibility == Visibility::Hidden && style->visibility() != Visibility::Hidden)
        || (m_previousImplicitVisibility == Visibility::Hidden && elementImplicitVisibility(m_element) == Visibility::Visible))
        m_contentChangeObserver.contentVisibilityDidChange();
}

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
