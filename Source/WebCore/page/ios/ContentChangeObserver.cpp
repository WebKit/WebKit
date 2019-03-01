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
    setObservedContentChange(WKContentIndeterminateChange);
    addObservedDOMTimer(timer);
    LOG_WITH_STREAM(ContentObservation, stream << "didInstallDOMTimer: register this timer: (" << &timer << ") and observe when it fires.");
}

void ContentChangeObserver::didRemoveDOMTimer(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    removeObservedDOMTimer(timer);
    LOG_WITH_STREAM(ContentObservation, stream << "removeDOMTimer: remove registered timer (" << &timer << ")");
    if (countOfObservedDOMTimers())
        return;
    m_page.chrome().client().observedContentChange(m_page.mainFrame());
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

    removeObservedDOMTimer(timer);
    stopObservingContentChanges();
    auto observedContentChange = this->observedContentChange();
    auto hasPendingStyleRecalc = WebCore::hasPendingStyleRecalc(m_page);
    // Check if the timer callback triggered either a sync or async style update.
    auto hasDeterminedState = observedContentChange == WKContentVisibilityChange || (!countOfObservedDOMTimers() && observedContentChange == WKContentNoChange && !hasPendingStyleRecalc);  
    if (hasDeterminedState) {
        LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: (" << &timer << ") in determined state.");
        m_page.chrome().client().observedContentChange(m_page.mainFrame());
        return;
    }
    if (hasPendingStyleRecalc) {
        // An async style recalc has been scheduled. Let's observe it.
        LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: (" << &timer << ") wait until next style recalc fires.");
        setShouldObserveStyleRecalc(true);
    }
}

void ContentChangeObserver::startObservingStyleRecalc()
{
    if (!shouldObserveStyleRecalc())
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
    auto hasDeterminedState = observedContentChange() == WKContentVisibilityChange || !countOfObservedDOMTimers();
    if (!hasDeterminedState) {
        LOG(ContentObservation, "stopObservingStyleRecalc: can't decided it yet.");
        return;
    }
    LOG(ContentObservation, "stopObservingStyleRecalc: notify the pending synthetic click handler.");
    m_page.chrome().client().observedContentChange(m_page.mainFrame());
}

void ContentChangeObserver::clearTimersAndReportContentChange()
{
    if (!countOfObservedDOMTimers())
        return;
    LOG(ContentObservation, "clearTimersAndReportContentChange: remove registered timers and report content change.");
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

void ContentChangeObserver::didContentVisibilityChange()
{
    setObservedContentChange(WKContentVisibilityChange);
}

void ContentChangeObserver::startObservingContentChanges()
{
    ASSERT(!hasPendingStyleRecalc(m_page));
    startObservingDOMTimerScheduling();
    resetObservedContentChange();
    clearObservedDOMTimers();
    m_isObservingContentChanges = true;
}

void ContentChangeObserver::stopObservingContentChanges()
{
    stopObservingDOMTimerScheduling();
    m_isObservingContentChanges = false;
}

WKContentChange ContentChangeObserver::observedContentChange()
{
    return WKObservedContentChange();
}

void ContentChangeObserver::resetObservedContentChange()
{
    WKSetObservedContentChange(WKContentNoChange);
}

void ContentChangeObserver::setObservedContentChange(WKContentChange change)
{
    if (observedContentChange() == WKContentVisibilityChange)
        return;
    WKSetObservedContentChange(change);
}

void ContentChangeObserver::addObservedDOMTimer(const DOMTimer& timer)
{
    ASSERT(isObservingDOMTimerScheduling());
    if (observedContentChange() == WKContentVisibilityChange)
        return;
    m_DOMTimerList.add(&timer);
}

void ContentChangeObserver::removeObservedDOMTimer(const DOMTimer& timer)
{
    m_DOMTimerList.remove(&timer);
    // Force reset the content change flag when the last observed content modifier is removed. We should not be in an indeterminate state anymore.
    if (!countOfObservedDOMTimers() && observedContentChange() == WKContentIndeterminateChange)
        resetObservedContentChange();
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
        m_contentChangeObserver->didContentVisibilityChange();
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
