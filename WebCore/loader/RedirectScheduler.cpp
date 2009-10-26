/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "RedirectScheduler.h"

#include "DocumentLoader.h"
#include "Event.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "HTMLFormElement.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

struct ScheduledRedirection {
    enum Type { redirection, locationChange, historyNavigation, formSubmission };

    const Type type;
    const double delay;
    const String url;
    const String referrer;
    const FrameLoadRequest frameRequest;
    const RefPtr<Event> event;
    const RefPtr<FormState> formState;
    const int historySteps;
    const bool lockHistory;
    const bool lockBackForwardList;
    const bool wasUserGesture;
    const bool wasRefresh;
    const bool wasDuringLoad;
    bool toldClient;

    ScheduledRedirection(double delay, const String& url, bool lockHistory, bool lockBackForwardList, bool wasUserGesture, bool refresh)
        : type(redirection)
        , delay(delay)
        , url(url)
        , historySteps(0)
        , lockHistory(lockHistory)
        , lockBackForwardList(lockBackForwardList)
        , wasUserGesture(wasUserGesture)
        , wasRefresh(refresh)
        , wasDuringLoad(false)
        , toldClient(false)
    {
        ASSERT(!url.isEmpty());
    }

    ScheduledRedirection(const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool wasUserGesture, bool refresh, bool duringLoad)
        : type(locationChange)
        , delay(0)
        , url(url)
        , referrer(referrer)
        , historySteps(0)
        , lockHistory(lockHistory)
        , lockBackForwardList(lockBackForwardList)
        , wasUserGesture(wasUserGesture)
        , wasRefresh(refresh)
        , wasDuringLoad(duringLoad)
        , toldClient(false)
    {
        ASSERT(!url.isEmpty());
    }

    explicit ScheduledRedirection(int historyNavigationSteps)
        : type(historyNavigation)
        , delay(0)
        , historySteps(historyNavigationSteps)
        , lockHistory(false)
        , lockBackForwardList(false)
        , wasUserGesture(false)
        , wasRefresh(false)
        , wasDuringLoad(false)
        , toldClient(false)
    {
    }

    ScheduledRedirection(const FrameLoadRequest& frameRequest,
            bool lockHistory, bool lockBackForwardList, PassRefPtr<Event> event, PassRefPtr<FormState> formState,
            bool duringLoad)
        : type(formSubmission)
        , delay(0)
        , frameRequest(frameRequest)
        , event(event)
        , formState(formState)
        , historySteps(0)
        , lockHistory(lockHistory)
        , lockBackForwardList(lockBackForwardList)
        , wasUserGesture(false)
        , wasRefresh(false)
        , wasDuringLoad(duringLoad)
        , toldClient(false)
    {
        ASSERT(!frameRequest.isEmpty());
        ASSERT(this->formState);
    }
};

RedirectScheduler::RedirectScheduler(Frame* frame)
    : m_frame(frame)
    , m_timer(this, &RedirectScheduler::timerFired)
{
}

RedirectScheduler::~RedirectScheduler()
{
}

bool RedirectScheduler::redirectScheduledDuringLoad()
{
    return m_scheduledRedirection && m_scheduledRedirection->wasDuringLoad;
}

void RedirectScheduler::clear()
{
    m_timer.stop();
    m_scheduledRedirection.clear();
}

void RedirectScheduler::scheduleRedirect(double delay, const String& url)
{
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
        
    if (!m_frame->page())
        return;

    if (url.isEmpty())
        return;

    // We want a new history item if the refresh timeout is > 1 second.
    if (!m_scheduledRedirection || delay <= m_scheduledRedirection->delay)
        schedule(new ScheduledRedirection(delay, url, true, delay <= 1, false, false));
}

bool RedirectScheduler::mustLockBackForwardList(Frame* targetFrame)
{
    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    
    for (Frame* ancestor = targetFrame->tree()->parent(); ancestor; ancestor = ancestor->tree()->parent()) {
        Document* document = ancestor->document();
        if (!ancestor->loader()->isComplete() || document && document->processingLoadEvent())
            return true;
    }
    return false;
}

void RedirectScheduler::scheduleLocationChange(const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool wasUserGesture)
{
    if (!m_frame->page())
        return;

    if (url.isEmpty())
        return;

    lockBackForwardList = lockBackForwardList || mustLockBackForwardList(m_frame);

    FrameLoader* loader = m_frame->loader();
    
    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change.
    KURL parsedURL(ParsedURLString, url);
    if (parsedURL.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(loader->url(), parsedURL)) {
        loader->changeLocation(loader->completeURL(url), referrer, lockHistory, lockBackForwardList, wasUserGesture);
        return;
    }

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !loader->committedFirstRealDocumentLoad();

    schedule(new ScheduledRedirection(url, referrer, lockHistory, lockBackForwardList, wasUserGesture, false, duringLoad));
}

void RedirectScheduler::scheduleFormSubmission(const FrameLoadRequest& frameRequest,
    bool lockHistory, PassRefPtr<Event> event, PassRefPtr<FormState> formState)
{
    ASSERT(m_frame->page());
    ASSERT(!frameRequest.isEmpty());

    // FIXME: Do we need special handling for form submissions where the URL is the same
    // as the current one except for the fragment part? See scheduleLocationChange above.

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !m_frame->loader()->committedFirstRealDocumentLoad();

    schedule(new ScheduledRedirection(frameRequest, lockHistory, mustLockBackForwardList(m_frame), event, formState, duringLoad));
}

void RedirectScheduler::scheduleRefresh(bool wasUserGesture)
{
    if (!m_frame->page())
        return;
    
    const KURL& url = m_frame->loader()->url();

    if (url.isEmpty())
        return;

    schedule(new ScheduledRedirection(url.string(), m_frame->loader()->outgoingReferrer(), true, true, wasUserGesture, true, false));
}

bool RedirectScheduler::locationChangePending()
{
    if (!m_scheduledRedirection)
        return false;

    switch (m_scheduledRedirection->type) {
        case ScheduledRedirection::redirection:
            return false;
        case ScheduledRedirection::historyNavigation:
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::formSubmission:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void RedirectScheduler::scheduleHistoryNavigation(int steps)
{
    if (!m_frame->page())
        return;

    // Invalid history navigations (such as history.forward() during a new load) have the side effect of cancelling any scheduled
    // redirects. We also avoid the possibility of cancelling the current load by avoiding the scheduled redirection altogether.
    if (!m_frame->page()->canGoBackOrForward(steps)) { 
        cancel(); 
        return; 
    } 

    schedule(new ScheduledRedirection(steps));
}

void RedirectScheduler::timerFired(Timer<RedirectScheduler>*)
{
    ASSERT(m_frame->page());

    if (m_frame->page()->defersLoading())
        return;

    OwnPtr<ScheduledRedirection> redirection(m_scheduledRedirection.release());
    FrameLoader* loader = m_frame->loader();

    switch (redirection->type) {
        case ScheduledRedirection::redirection:
        case ScheduledRedirection::locationChange:
            loader->changeLocation(KURL(ParsedURLString, redirection->url), redirection->referrer,
                redirection->lockHistory, redirection->lockBackForwardList, redirection->wasUserGesture, redirection->wasRefresh);
            return;
        case ScheduledRedirection::historyNavigation:
            if (redirection->historySteps == 0) {
                // Special case for go(0) from a frame -> reload only the frame
                loader->urlSelected(loader->url(), "", 0, redirection->lockHistory, redirection->lockBackForwardList, redirection->wasUserGesture, SendReferrer);
                return;
            }
            // go(i!=0) from a frame navigates into the history of the frame only,
            // in both IE and NS (but not in Mozilla). We can't easily do that.
            m_frame->page()->goBackOrForward(redirection->historySteps);
            return;
        case ScheduledRedirection::formSubmission:
            // The submitForm function will find a target frame before using the redirection timer.
            // Now that the timer has fired, we need to repeat the security check which normally is done when
            // selecting a target, in case conditions have changed. Other code paths avoid this by targeting
            // without leaving a time window. If we fail the check just silently drop the form submission.
            if (!redirection->formState->sourceFrame()->loader()->shouldAllowNavigation(m_frame))
                return;
            loader->loadFrameRequest(redirection->frameRequest, redirection->lockHistory, redirection->lockBackForwardList,
                redirection->event, redirection->formState, SendReferrer);
            return;
    }

    ASSERT_NOT_REACHED();
}

void RedirectScheduler::schedule(PassOwnPtr<ScheduledRedirection> redirection)
{
    ASSERT(m_frame->page());
    FrameLoader* loader = m_frame->loader();

    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (redirection->wasDuringLoad) {
        if (DocumentLoader* provisionalDocumentLoader = loader->provisionalDocumentLoader())
            provisionalDocumentLoader->stopLoading();
        loader->stopLoading(UnloadEventPolicyUnloadAndPageHide);   
    }

    cancel();
    m_scheduledRedirection = redirection;
    if (!loader->isComplete() && m_scheduledRedirection->type != ScheduledRedirection::redirection)
        loader->completed();
    startTimer();
}

void RedirectScheduler::startTimer()
{
    if (!m_scheduledRedirection)
        return;

    ASSERT(m_frame->page());
    
    FrameLoader* loader = m_frame->loader();

    if (m_timer.isActive())
        return;

    if (m_scheduledRedirection->type == ScheduledRedirection::redirection && !loader->allAncestorsAreComplete())
        return;

    m_timer.startOneShot(m_scheduledRedirection->delay);

    switch (m_scheduledRedirection->type) {
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::redirection:
            if (m_scheduledRedirection->toldClient)
                return;
            m_scheduledRedirection->toldClient = true;
            loader->clientRedirected(KURL(ParsedURLString, m_scheduledRedirection->url),
                m_scheduledRedirection->delay,
                currentTime() + m_timer.nextFireInterval(),
                m_scheduledRedirection->lockBackForwardList);
            return;
        case ScheduledRedirection::formSubmission:
            // FIXME: It would make sense to report form submissions as client redirects too.
            // But we didn't do that in the past when form submission used a separate delay
            // mechanism, so doing it will be a behavior change.
            return;
        case ScheduledRedirection::historyNavigation:
            // Don't report history navigations.
            return;
    }
    ASSERT_NOT_REACHED();
}

void RedirectScheduler::cancel(bool newLoadInProgress)
{
    m_timer.stop();

    OwnPtr<ScheduledRedirection> redirection(m_scheduledRedirection.release());
    if (redirection && redirection->toldClient)
        m_frame->loader()->clientRedirectCancelledOrFinished(newLoadInProgress);
}

} // namespace WebCore

