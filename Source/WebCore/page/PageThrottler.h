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

#ifndef PageThrottler_h
#define PageThrottler_h

#include "Timer.h"

#include "CountedUserActivity.h"
#include "ViewState.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Page;
class PageActivityAssertionToken;

class PageThrottler {
public:
    PageThrottler(Page&, ViewState::Flags);
    ~PageThrottler();

    void setViewState(ViewState::Flags);

    void didReceiveUserInput() { reportInterestingEvent(); }
    void pluginDidEvaluate() { reportInterestingEvent(); }
    std::unique_ptr<PageActivityAssertionToken> mediaActivityToken();
    std::unique_ptr<PageActivityAssertionToken> pageLoadActivityToken();

    void hiddenPageDOMTimerThrottlingStateChanged();

private:
    enum PageThrottleState {
        PageNotThrottledState,
        PageWaitingToThrottleState,
        PageThrottledState
    };

    friend class PageActivityAssertionToken;
    WeakPtr<PageThrottler> weakPtr() { return m_weakPtrFactory.createWeakPtr(); }
    void incrementActivityCount();
    void decrementActivityCount();

    void reportInterestingEvent();

    void startThrottleHysteresisTimer();
    void stopThrottleHysteresisTimer();
    void throttleHysteresisTimerFired(Timer<PageThrottler>&);

    void throttlePage();
    void unthrottlePage();

    void setIsVisuallyIdle(bool);
    void setIsVisible(bool);

    Page& m_page;
    ViewState::Flags m_viewState;
    PageThrottleState m_throttleState;
    Timer<PageThrottler> m_throttleHysteresisTimer;
    WeakPtrFactory<PageThrottler> m_weakPtrFactory;
    size_t m_activityCount;
    UserActivity m_visuallyNonIdle;
    CountedUserActivity m_pageActivity;
};

}
#endif
