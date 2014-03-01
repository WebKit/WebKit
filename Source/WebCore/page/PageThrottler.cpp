/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageThrottler.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageActivityAssertionToken.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

static const double kThrottleHysteresisSeconds = 2.0;

PageThrottler::PageThrottler(Page& page, ViewState::Flags viewState)
    : m_page(page)
    , m_viewState(viewState)
    , m_throttleState(PageNotThrottledState)
    , m_throttleHysteresisTimer(this, &PageThrottler::throttleHysteresisTimerFired)
    , m_weakPtrFactory(this)
    , m_activityCount(0)
    , m_visuallyNonIdle("Page is not visually idle.")
    , m_pageActivity("Page is active.")
{
    m_pageActivity.increment();

    setIsVisuallyIdle(viewState & ViewState::IsVisuallyIdle);
}

PageThrottler::~PageThrottler()
{
    m_page.setTimerThrottlingEnabled(false);

    if (m_throttleState != PageThrottledState)
        m_pageActivity.decrement();
}

void PageThrottler::hiddenPageDOMTimerThrottlingStateChanged()
{
    m_page.setTimerThrottlingEnabled(m_throttleState != PageNotThrottledState);
}

std::unique_ptr<PageActivityAssertionToken> PageThrottler::mediaActivityToken()
{
    return std::make_unique<PageActivityAssertionToken>(*this);
}

std::unique_ptr<PageActivityAssertionToken> PageThrottler::pageLoadActivityToken()
{
    return std::make_unique<PageActivityAssertionToken>(*this);
}

void PageThrottler::throttlePage()
{
    m_throttleState = PageThrottledState;

    m_pageActivity.decrement();

    m_page.setTimerThrottlingEnabled(true);
}

void PageThrottler::unthrottlePage()
{
    PageThrottleState oldState = m_throttleState;
    m_throttleState = PageNotThrottledState;

    if (oldState == PageNotThrottledState)
        return;

    if (oldState == PageThrottledState)
        m_pageActivity.increment();
    
    m_page.setTimerThrottlingEnabled(false);
}

void PageThrottler::setViewState(ViewState::Flags viewState)
{
    ViewState::Flags changed = m_viewState ^ viewState;
    m_viewState = viewState;

    if (changed & ViewState::IsVisible)
        setIsVisible(viewState & ViewState::IsVisible);
    if (changed & ViewState::IsVisuallyIdle)
        setIsVisuallyIdle(viewState & ViewState::IsVisuallyIdle);
}

void PageThrottler::setIsVisible(bool isVisible)
{
    if (isVisible)
        m_page.setTimerThrottlingEnabled(false);
    else if (m_throttleState != PageNotThrottledState)
        m_page.setTimerThrottlingEnabled(true);
}

void PageThrottler::setIsVisuallyIdle(bool isVisuallyIdle)
{
    if (isVisuallyIdle) {
        m_throttleState = PageWaitingToThrottleState;
        startThrottleHysteresisTimer();
        m_visuallyNonIdle.stop();
    } else {
        unthrottlePage();
        stopThrottleHysteresisTimer();
        m_visuallyNonIdle.start();
    }
}

void PageThrottler::stopThrottleHysteresisTimer()
{
    m_throttleHysteresisTimer.stop();
}

void PageThrottler::reportInterestingEvent()
{
    if (m_throttleState == PageNotThrottledState)
        return;
    if (m_throttleState == PageThrottledState)
        unthrottlePage();
    m_throttleState = PageWaitingToThrottleState;
    startThrottleHysteresisTimer();
}

void PageThrottler::startThrottleHysteresisTimer()
{
    if (m_throttleHysteresisTimer.isActive())
        m_throttleHysteresisTimer.stop();
    if (!m_activityCount)
        m_throttleHysteresisTimer.startOneShot(kThrottleHysteresisSeconds);
}

void PageThrottler::throttleHysteresisTimerFired(Timer<PageThrottler>&)
{
    ASSERT(!m_activityCount);
    throttlePage();
}

void PageThrottler::incrementActivityCount()
{
    // If we've already got events that block throttling we can return early
    if (m_activityCount++)
        return;

    if (m_throttleState == PageNotThrottledState)
        return;

    if (m_throttleState == PageThrottledState)
        unthrottlePage();

    m_throttleState = PageWaitingToThrottleState;
    stopThrottleHysteresisTimer();
}

void PageThrottler::decrementActivityCount()
{
    if (--m_activityCount)
        return;

    if (m_throttleState == PageNotThrottledState)
        return;

    ASSERT(m_throttleState == PageWaitingToThrottleState);
    startThrottleHysteresisTimer();
}

}
