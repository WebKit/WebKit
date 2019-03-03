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
#include "Logging.h"
#include "NodeRenderStyle.h"
#include "Page.h"

namespace WebCore {

static bool hasPendingStyleRecalc(const Page& page)
{
    for (auto* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document()) {
            if (document->hasPendingStyleRecalc())
                return true;
        }
    }
    return false;
}

ContentChangeObserver::ContentChangeObserver(Page& page)
    : m_page(page)
{
}

void ContentChangeObserver::didInstallDOMTimer(const DOMTimer& timer, Seconds timeout, bool singleShot)
{
    if (!m_page.mainFrame().document())
        return;
    if (m_page.mainFrame().document()->activeDOMObjectsAreSuspended())
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

void ContentChangeObserver::startObservingDOMTimerExecute(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "startObservingDOMTimerExecute: start observing (" << &timer << ") timer callback.");

    m_isObservingContentChanges = true;
}

void ContentChangeObserver::stopObservingDOMTimerExecute(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: stop observing (" << &timer << ") timer callback.");

    m_isObservingContentChanges = false;
    unregisterDOMTimer(timer);
    setShouldObserveStyleRecalc(WebCore::hasPendingStyleRecalc(m_page));
    notifyContentChangeIfNeeded();
}

void ContentChangeObserver::startObservingStyleRecalc()
{
    if (!shouldObserveStyleRecalc())
        return;
    if (hasVisibleChangeState())
        return;
    LOG(ContentObservation, "startObservingStyleRecalc: start observing style recalc.");

    m_isObservingContentChanges = true;
}

void ContentChangeObserver::stopObservingStyleRecalc()
{
    if (!shouldObserveStyleRecalc())
        return;
    LOG(ContentObservation, "stopObservingStyleRecalc: stop observing style recalc");

    setShouldObserveStyleRecalc(false);
    adjustObservedState(Event::StyleRecalcFinished);
    notifyContentChangeIfNeeded();
}

void ContentChangeObserver::clearTimersAndReportContentChange()
{
    if (!hasObservedDOMTimer())
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "clearTimersAndReportContentChange: remove registered timers and report content change." << observedContentChange());

    clearObservedDOMTimers();
    m_page.chrome().client().observedContentChange(m_page.mainFrame());
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

void ContentChangeObserver::startObservingContentChanges()
{
    ASSERT(!hasPendingStyleRecalc(m_page));
    clearObservedDOMTimers();
    startObservingDOMTimerScheduling();
    m_isObservingContentChanges = true;
    adjustObservedState(Event::ContentObservationStarted);
}

void ContentChangeObserver::stopObservingContentChanges()
{
    stopObservingDOMTimerScheduling();
    m_isObservingContentChanges = false;
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

void ContentChangeObserver::setShouldObserveStyleRecalc(bool shouldObserve)
{
    if (shouldObserve)
        LOG(ContentObservation, "Wait until next style recalc fires.");
    m_shouldObserveStyleRecalc = shouldObserve;
}

bool ContentChangeObserver::hasDeterminateState() const
{
    if (hasVisibleChangeState())
        return true;
    return observedContentChange() == WKContentNoChange && !hasObservedDOMTimer() && !hasPendingStyleRecalc(m_page);
}

void ContentChangeObserver::adjustObservedState(Event event)
{
    switch (event) {
    case Event::ContentObservationStarted:
        setHasNoChangeState();
        break;
    case Event::InstalledDOMTimer:
        // Expecting a timer fire. Promote to an indeterminate state.
        ASSERT(!hasVisibleChangeState());
        setHasIndeterminateState();
        break;
    case Event::RemovedDOMTimer:
    case Event::StyleRecalcFinished:
        // Demote to "no change" when there's no pending activity anymore.
        if (observedContentChange() == WKContentIndeterminateChange && !hasObservedDOMTimer() && !hasPendingStyleRecalc(m_page))
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
    m_page.chrome().client().observedContentChange(m_page.mainFrame());
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

ContentChangeObserver::StyleChangeScope::StyleChangeScope(Page* page, const Element& element)
    : m_contentChangeObserver(page ? &page->contentChangeObserver() : nullptr)
    , m_element(element)
    , m_needsObserving(m_contentChangeObserver && m_contentChangeObserver->isObservingContentChanges() && m_contentChangeObserver->observedContentChange() != WKContentVisibilityChange)
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
        m_contentChangeObserver->contentVisibilityDidChange();
}

ContentChangeObserver::StyleRecalcScope::StyleRecalcScope(Page* page)
    : m_contentChangeObserver(page ? &page->contentChangeObserver() : nullptr)
{
    if (m_contentChangeObserver)
        m_contentChangeObserver->startObservingStyleRecalc();
}

ContentChangeObserver::StyleRecalcScope::~StyleRecalcScope()
{
    if (m_contentChangeObserver)
        m_contentChangeObserver->stopObservingStyleRecalc();
}

ContentChangeObserver::DOMTimerScope::DOMTimerScope(Page* page, const DOMTimer& domTimer)
    : m_contentChangeObserver(page ? &page->contentChangeObserver() : nullptr)
    , m_domTimer(domTimer)
{
    if (m_contentChangeObserver)
        m_contentChangeObserver->startObservingDOMTimerExecute(m_domTimer);
}

ContentChangeObserver::DOMTimerScope::~DOMTimerScope()
{
    if (m_contentChangeObserver)
        m_contentChangeObserver->stopObservingDOMTimerExecute(m_domTimer);
}

}

#endif // PLATFORM(IOS_FAMILY)
