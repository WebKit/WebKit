/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "BackForwardList.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "FormState.h"
#include "FormSubmission.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderStateMachine.h"
#include "HistoryItem.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "UserGestureIndicator.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

class ScheduledNavigation : public Noncopyable {
public:
    ScheduledNavigation(double delay, bool lockHistory, bool lockBackForwardList, bool wasDuringLoad, bool isLocationChange)
        : m_delay(delay)
        , m_lockHistory(lockHistory)
        , m_lockBackForwardList(lockBackForwardList)
        , m_wasDuringLoad(wasDuringLoad)
        , m_isLocationChange(isLocationChange)
    {
    }
    virtual ~ScheduledNavigation() { }

    virtual void fire(Frame*) = 0;

    virtual bool shouldStartTimer(Frame*) { return true; }
    virtual void didStartTimer(Frame*, Timer<RedirectScheduler>*) { }
    virtual void didStopTimer(Frame*, bool /* newLoadInProgress */) { }

    double delay() const { return m_delay; }
    bool lockHistory() const { return m_lockHistory; }
    bool lockBackForwardList() const { return m_lockBackForwardList; }
    bool wasDuringLoad() const { return m_wasDuringLoad; }
    bool isLocationChange() const { return m_isLocationChange; }

private:
    double m_delay;
    bool m_lockHistory;
    bool m_lockBackForwardList;
    bool m_wasDuringLoad;
    bool m_isLocationChange;
};

class ScheduledURLNavigation : public ScheduledNavigation {
public:
    ScheduledURLNavigation(double delay, const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool wasUserGesture, bool duringLoad, bool isLocationChange)
        : ScheduledNavigation(delay, lockHistory, lockBackForwardList, duringLoad, isLocationChange)
        , m_url(url)
        , m_referrer(referrer)
        , m_wasUserGesture(wasUserGesture)
        , m_haveToldClient(false)
    {
    }

    virtual void fire(Frame* frame)
    {
        frame->loader()->changeLocation(KURL(ParsedURLString, m_url), m_referrer, lockHistory(), lockBackForwardList(), m_wasUserGesture, false);
    }

    virtual void didStartTimer(Frame* frame, Timer<RedirectScheduler>* timer)
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;
        frame->loader()->clientRedirected(KURL(ParsedURLString, m_url), delay(), currentTime() + timer->nextFireInterval(), lockBackForwardList());
    }

    virtual void didStopTimer(Frame* frame, bool newLoadInProgress)
    {
        if (!m_haveToldClient)
            return;
        frame->loader()->clientRedirectCancelledOrFinished(newLoadInProgress);
    }

    String url() const { return m_url; }
    String referrer() const { return m_referrer; }
    bool wasUserGesture() const { return m_wasUserGesture; }

private:
    String m_url;
    String m_referrer;
    bool m_wasUserGesture;
    bool m_haveToldClient;
};

class ScheduledRedirect : public ScheduledURLNavigation {
public:
    ScheduledRedirect(double delay, const String& url, bool lockHistory, bool lockBackForwardList, bool wasUserGesture)
        : ScheduledURLNavigation(delay, url, String(), lockHistory, lockBackForwardList, wasUserGesture, false, false) { }

    virtual bool shouldStartTimer(Frame* frame) { return frame->loader()->allAncestorsAreComplete(); }
};

class ScheduledLocationChange : public ScheduledURLNavigation {
public:
    ScheduledLocationChange(const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool wasUserGesture, bool duringLoad)
        : ScheduledURLNavigation(0.0, url, referrer, lockHistory, lockBackForwardList, wasUserGesture, duringLoad, true) { }
};

class ScheduledRefresh : public ScheduledURLNavigation {
public:
    ScheduledRefresh(const String& url, const String& referrer, bool wasUserGesture)
        : ScheduledURLNavigation(0.0, url, referrer, true, true, wasUserGesture, false, true) { }

    virtual void fire(Frame* frame)
    {
        frame->loader()->changeLocation(KURL(ParsedURLString, url()), referrer(), lockHistory(), lockBackForwardList(), wasUserGesture(), true);
    }
};

class ScheduledHistoryNavigation : public ScheduledNavigation {
public:
    explicit ScheduledHistoryNavigation(int historySteps) : ScheduledNavigation(0, false, false, false, true), m_historySteps(historySteps) { }

    virtual void fire(Frame* frame)
    {
        FrameLoader* loader = frame->loader();
        if (!m_historySteps) {
            // Special case for go(0) from a frame -> reload only the frame
            loader->urlSelected(loader->url(), "", 0, lockHistory(), lockBackForwardList(), false, SendReferrer);
            return;
        }
        // go(i!=0) from a frame navigates into the history of the frame only,
        // in both IE and NS (but not in Mozilla). We can't easily do that.
        frame->page()->goBackOrForward(m_historySteps);
    }

private:
    int m_historySteps;
};

class ScheduledFormSubmission : public ScheduledNavigation {
public:
    ScheduledFormSubmission(PassRefPtr<FormSubmission> submission, bool lockBackForwardList, bool duringLoad, bool wasUserGesture)
        : ScheduledNavigation(0, submission->lockHistory(), lockBackForwardList, duringLoad, true)
        , m_submission(submission)
        , m_haveToldClient(false)
        , m_wasUserGesture(wasUserGesture)
    {
        ASSERT(m_submission->state());
    }

    virtual void fire(Frame* frame)
    {
        UserGestureIndicator gestureIndicator(m_wasUserGesture ? DefinitelyProcessingUserGesture : DefinitelyNotProcessingUserGesture);

        // The submitForm function will find a target frame before using the redirection timer.
        // Now that the timer has fired, we need to repeat the security check which normally is done when
        // selecting a target, in case conditions have changed. Other code paths avoid this by targeting
        // without leaving a time window. If we fail the check just silently drop the form submission.
        if (!m_submission->state()->sourceFrame()->loader()->shouldAllowNavigation(frame))
            return;
        FrameLoadRequest frameRequest;
        m_submission->populateFrameLoadRequest(frameRequest);
        frame->loader()->loadFrameRequest(frameRequest, lockHistory(), lockBackForwardList(), m_submission->event(), m_submission->state(), SendReferrer);
    }
    
    virtual void didStartTimer(Frame* frame, Timer<RedirectScheduler>* timer)
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;
        frame->loader()->clientRedirected(m_submission->requestURL(), delay(), currentTime() + timer->nextFireInterval(), lockBackForwardList());
    }

    virtual void didStopTimer(Frame* frame, bool newLoadInProgress)
    {
        if (!m_haveToldClient)
            return;
        frame->loader()->clientRedirectCancelledOrFinished(newLoadInProgress);
    }

private:
    RefPtr<FormSubmission> m_submission;
    bool m_haveToldClient;
    bool m_wasUserGesture;
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
    return m_redirect && m_redirect->wasDuringLoad();
}

bool RedirectScheduler::locationChangePending()
{
    return m_redirect && m_redirect->isLocationChange();
}

void RedirectScheduler::clear()
{
    m_timer.stop();
    m_redirect.clear();
}

void RedirectScheduler::scheduleRedirect(double delay, const String& url)
{
    if (!m_frame->page())
        return;
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
    if (url.isEmpty())
        return;

    // We want a new back/forward list item if the refresh timeout is > 1 second.
    if (!m_redirect || delay <= m_redirect->delay())
        schedule(adoptPtr(new ScheduledRedirect(delay, url, true, delay <= 1, false)));
}

bool RedirectScheduler::mustLockBackForwardList(Frame* targetFrame, bool wasUserGesture)
{
    // Non-user navigation before the page has finished firing onload should not create a new back/forward item.
    // See https://webkit.org/b/42861 for the original motivation for this.    
    if (!wasUserGesture && targetFrame->loader()->documentLoader() && !targetFrame->loader()->documentLoader()->wasOnloadHandled())
        return true;
    
    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    for (Frame* ancestor = targetFrame->tree()->parent(); ancestor; ancestor = ancestor->tree()->parent()) {
        Document* document = ancestor->document();
        if (!ancestor->loader()->isComplete() || (document && document->processingLoadEvent()))
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

    lockBackForwardList = lockBackForwardList || mustLockBackForwardList(m_frame, wasUserGesture);

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
    bool duringLoad = !loader->stateMachine()->committedFirstRealDocumentLoad();

    schedule(adoptPtr(new ScheduledLocationChange(url, referrer, lockHistory, lockBackForwardList, wasUserGesture, duringLoad)));
}

void RedirectScheduler::scheduleFormSubmission(PassRefPtr<FormSubmission> submission)
{
    ASSERT(m_frame->page());

    // FIXME: Do we need special handling for form submissions where the URL is the same
    // as the current one except for the fragment part? See scheduleLocationChange above.

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !m_frame->loader()->stateMachine()->committedFirstRealDocumentLoad();

    // If this is a child frame and the form submission was triggered by a script, lock the back/forward list
    // to match IE and Opera.
    // See https://bugs.webkit.org/show_bug.cgi?id=32383 for the original motivation for this.
    bool isUserGesture = m_frame->loader()->isProcessingUserGesture();
    bool lockBackForwardList = mustLockBackForwardList(m_frame, isUserGesture) || (submission->state()->formSubmissionTrigger() == SubmittedByJavaScript && m_frame->tree()->parent());

    schedule(adoptPtr(new ScheduledFormSubmission(submission, lockBackForwardList, duringLoad, isUserGesture)));
}

void RedirectScheduler::scheduleRefresh(bool wasUserGesture)
{
    if (!m_frame->page())
        return;
    const KURL& url = m_frame->loader()->url();
    if (url.isEmpty())
        return;

    schedule(adoptPtr(new ScheduledRefresh(url.string(), m_frame->loader()->outgoingReferrer(), wasUserGesture)));
}

void RedirectScheduler::scheduleHistoryNavigation(int steps)
{
    if (!m_frame->page())
        return;

    // Invalid history navigations (such as history.forward() during a new load) have the side effect of cancelling any scheduled
    // redirects. We also avoid the possibility of cancelling the current load by avoiding the scheduled redirection altogether.
    HistoryItem* specifiedEntry = m_frame->page()->backForwardList()->itemAtIndex(steps);
    if (!specifiedEntry) {
        cancel();
        return;
    }
    
    // In all other cases, schedule the history traversal to occur asynchronously.
    schedule(adoptPtr(new ScheduledHistoryNavigation(steps)));
}

void RedirectScheduler::timerFired(Timer<RedirectScheduler>*)
{
    if (!m_frame->page())
        return;
    if (m_frame->page()->defersLoading())
        return;

    OwnPtr<ScheduledNavigation> redirect(m_redirect.release());
    redirect->fire(m_frame);
}

void RedirectScheduler::schedule(PassOwnPtr<ScheduledNavigation> redirect)
{
    ASSERT(m_frame->page());

    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (redirect->wasDuringLoad()) {
        if (DocumentLoader* provisionalDocumentLoader = m_frame->loader()->provisionalDocumentLoader())
            provisionalDocumentLoader->stopLoading();
        m_frame->loader()->stopLoading(UnloadEventPolicyUnloadAndPageHide);   
    }

    cancel();
    m_redirect = redirect;

    if (!m_frame->loader()->isComplete() && m_redirect->isLocationChange())
        m_frame->loader()->completed();

    startTimer();
}

void RedirectScheduler::startTimer()
{
    if (!m_redirect)
        return;

    ASSERT(m_frame->page());
    if (m_timer.isActive())
        return;
    if (!m_redirect->shouldStartTimer(m_frame))
        return;

    m_timer.startOneShot(m_redirect->delay());
    m_redirect->didStartTimer(m_frame, &m_timer);
}

void RedirectScheduler::cancel(bool newLoadInProgress)
{
    m_timer.stop();

    OwnPtr<ScheduledNavigation> redirect(m_redirect.release());
    if (redirect)
        redirect->didStopTimer(m_frame, newLoadInProgress);
}

} // namespace WebCore
