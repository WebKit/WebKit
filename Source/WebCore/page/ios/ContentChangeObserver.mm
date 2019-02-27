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

#import "config.h"
#import "ContentChangeObserver.h"

#if PLATFORM(IOS_FAMILY)
#import "Chrome.h"
#import "ChromeClient.h"
#import "DOMTimer.h"
#import "Logging.h"
#import "Page.h"
#import "WKContentObservationInternal.h"

namespace WebCore {

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

void ContentChangeObserver::startObservingDOMTimerExecute(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    LOG_WITH_STREAM(ContentObservation, stream << "startObservingDOMTimerExecute: start observing (" << &timer << ") timer callback.");
    WKStartObservingContentChanges();
    startObservingStyleRecalcScheduling();
}

void ContentChangeObserver::stopObservingDOMTimerExecute(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    removeObservedDOMTimer(timer);
    stopObservingStyleRecalcScheduling();
    stopObservingContentChanges();
    auto observedContentChange = this->observedContentChange();
    // Check if the timer callback triggered either a sync or async style update.
    auto inDeterminedState = observedContentChange == WKContentVisibilityChange || (!countOfObservedDOMTimers() && observedContentChange == WKContentNoChange);  

    LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: stop observing (" << &timer << ") timer callback.");
    if (inDeterminedState) {
        LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: (" << &timer << ") in determined state.");
        m_page.chrome().client().observedContentChange(m_page.mainFrame());
    } else if (observedContentChange == WKContentIndeterminateChange) {
        // An async style recalc has been scheduled. Let's observe it.
        LOG_WITH_STREAM(ContentObservation, stream << "stopObservingDOMTimerExecute: (" << &timer << ") wait until next style recalc fires.");
        setShouldObserveNextStyleRecalc(true);
    }
}

void ContentChangeObserver::didScheduleStyleRecalc()
{
    if (!isObservingStyleRecalcScheduling())
        return;
    LOG(ContentObservation, "didScheduleStyleRecalc: register this style recalc schedule and observe when it fires.");
    setObservedContentChange(WKContentIndeterminateChange);
}

void ContentChangeObserver::startObservingStyleResolve()
{
    if (!shouldObserveNextStyleRecalc())
        return;
    LOG(ContentObservation, "startObservingStyleResolve: start observing style resolve.");
    WKStartObservingContentChanges();
}

void ContentChangeObserver::stopObservingStyleResolve()
{
    if (!shouldObserveNextStyleRecalc())
        return;
    LOG(ContentObservation, "stopObservingStyleResolve: stop observing style resolve");
    setShouldObserveNextStyleRecalc(false);
    auto inDeterminedState = observedContentChange() == WKContentVisibilityChange || !countOfObservedDOMTimers();
    if (!inDeterminedState) {
        LOG(ContentObservation, "stopObservingStyleResolve: can't decided it yet.");
        return;
    }
    LOG(ContentObservation, "stopObservingStyleResolve: notify the pending synthetic click handler.");
    m_page.chrome().client().observedContentChange(m_page.mainFrame());
}

void ContentChangeObserver::removeDOMTimer(const DOMTimer& timer)
{
    if (!containsObservedDOMTimer(timer))
        return;
    removeObservedDOMTimer(timer);
    LOG_WITH_STREAM(ContentObservation, stream << "removeDOMTimer: remove registered timer (" << &timer << ")");
    if (countOfObservedDOMTimers())
        return;
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

void ContentChangeObserver::startObservingContentChanges()
{
    startObservingDOMTimerScheduling();
    WKStartObservingContentChanges();
}

void ContentChangeObserver::stopObservingContentChanges()
{
    stopObservingDOMTimerScheduling();
    WKStopObservingContentChanges();
}

bool ContentChangeObserver::isObservingContentChanges()
{
    return WKObservingContentChanges();
}

void ContentChangeObserver::startObservingDOMTimerScheduling()
{
    WKStartObservingDOMTimerScheduling();
    clearObservedDOMTimers();
}

void ContentChangeObserver::stopObservingDOMTimerScheduling()
{
    WKStopObservingDOMTimerScheduling();
}

bool ContentChangeObserver::isObservingDOMTimerScheduling()
{
    return WKIsObservingDOMTimerScheduling();
}

void ContentChangeObserver::startObservingStyleRecalcScheduling()
{
    m_observingStyleRecalcScheduling = true;
}

void ContentChangeObserver::stopObservingStyleRecalcScheduling()
{
    m_observingStyleRecalcScheduling = false;
}

bool ContentChangeObserver::isObservingStyleRecalcScheduling()
{
    return m_observingStyleRecalcScheduling;
}

void ContentChangeObserver::setShouldObserveNextStyleRecalc(bool observe)
{
    m_observingNextStyleRecalc = observe;
}

bool ContentChangeObserver::shouldObserveNextStyleRecalc()
{
    return m_observingNextStyleRecalc;
}

WKContentChange ContentChangeObserver::observedContentChange()
{
    return WKObservedContentChange();
}

unsigned ContentChangeObserver::countOfObservedDOMTimers()
{
    return m_DOMTimerList.size();
}

void ContentChangeObserver::clearObservedDOMTimers()
{
    m_DOMTimerList.clear();
}

void ContentChangeObserver::setObservedContentChange(WKContentChange change)
{
    WKSetObservedContentChange(change);
}

bool ContentChangeObserver::containsObservedDOMTimer(const DOMTimer& timer)
{
    return m_DOMTimerList.contains(&timer);
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
    // Force reset the content change flag when the last observed content modifier is removed. We should not be in indeterminate state anymore.
    if (!countOfObservedDOMTimers() && observedContentChange() == WKContentIndeterminateChange)
        setObservedContentChange(WKContentNoChange);
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

ContentChangeObserver::StyleChange::StyleChange(const Element& element, ContentChangeObserver& contentChangeObserver)
    : m_element(element)
    , m_contentChangeObserver(contentChangeObserver)
    , m_previousDisplay(element.renderStyle() ? element.renderStyle()->display() : DisplayType::None)
    , m_previousVisibility(element.renderStyle() ? element.renderStyle()->visibility() : Visibility::Hidden)
    , m_previousImplicitVisibility(contentChangeObserver.isObservingContentChanges() && contentChangeObserver.observedContentChange() != WKContentVisibilityChange ? elementImplicitVisibility(element) : Visibility::Visible)
{
}

ContentChangeObserver::StyleChange::~StyleChange()
{
    if (!m_contentChangeObserver.isObservingContentChanges())
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
        m_contentChangeObserver.setObservedContentChange(WKContentVisibilityChange);
}
}

#endif // PLATFORM(IOS_FAMILY)
