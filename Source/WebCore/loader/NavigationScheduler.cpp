/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "NavigationScheduler.h"

#include "BackForwardController.h"
#include "CommonVM.h"
#include "DOMWindow.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "FormState.h"
#include "FormSubmission.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderStateMachine.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HistoryItem.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "NavigationDisabler.h"
#include "Page.h"
#include "PolicyChecker.h"
#include "ScriptController.h"
#include "UserGestureIndicator.h"
#include <wtf/Ref.h>

namespace WebCore {

unsigned NavigationDisabler::s_globalNavigationDisableCount = 0;

class ScheduledNavigation {
    WTF_MAKE_NONCOPYABLE(ScheduledNavigation); WTF_MAKE_FAST_ALLOCATED;
public:
    ScheduledNavigation(double delay, LockHistory lockHistory, LockBackForwardList lockBackForwardList, bool wasDuringLoad, bool isLocationChange)
        : m_delay(delay)
        , m_lockHistory(lockHistory)
        , m_lockBackForwardList(lockBackForwardList)
        , m_wasDuringLoad(wasDuringLoad)
        , m_isLocationChange(isLocationChange)
        , m_userGestureToForward(UserGestureIndicator::currentUserGesture())
    {
    }
    ScheduledNavigation(double delay, LockHistory lockHistory, LockBackForwardList lockBackForwardList, bool wasDuringLoad, bool isLocationChange, ShouldOpenExternalURLsPolicy externalURLPolicy)
        : m_delay(delay)
        , m_lockHistory(lockHistory)
        , m_lockBackForwardList(lockBackForwardList)
        , m_wasDuringLoad(wasDuringLoad)
        , m_isLocationChange(isLocationChange)
        , m_userGestureToForward(UserGestureIndicator::currentUserGesture())
        , m_shouldOpenExternalURLsPolicy(externalURLPolicy)
    {
        if (auto* frame = lexicalFrameFromCommonVM()) {
            if (frame->isMainFrame())
                m_initiatedByMainFrame = InitiatedByMainFrame::Yes;
        }
    }
    virtual ~ScheduledNavigation() = default;

    virtual void fire(Frame&) = 0;

    virtual bool shouldStartTimer(Frame&) { return true; }
    virtual void didStartTimer(Frame&, Timer&) { }
    virtual void didStopTimer(Frame&, NewLoadInProgress) { }

    double delay() const { return m_delay; }
    LockHistory lockHistory() const { return m_lockHistory; }
    LockBackForwardList lockBackForwardList() const { return m_lockBackForwardList; }
    bool wasDuringLoad() const { return m_wasDuringLoad; }
    bool isLocationChange() const { return m_isLocationChange; }
    UserGestureToken* userGestureToForward() const { return m_userGestureToForward.get(); }

protected:
    void clearUserGesture() { m_userGestureToForward = nullptr; }
    ShouldOpenExternalURLsPolicy shouldOpenExternalURLs() const { return m_shouldOpenExternalURLsPolicy; }
    InitiatedByMainFrame initiatedByMainFrame() const { return m_initiatedByMainFrame; };

private:
    double m_delay;
    LockHistory m_lockHistory;
    LockBackForwardList m_lockBackForwardList;
    bool m_wasDuringLoad;
    bool m_isLocationChange;
    RefPtr<UserGestureToken> m_userGestureToForward;
    ShouldOpenExternalURLsPolicy m_shouldOpenExternalURLsPolicy { ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    InitiatedByMainFrame m_initiatedByMainFrame { InitiatedByMainFrame::Unknown };
};

class ScheduledURLNavigation : public ScheduledNavigation {
protected:
    ScheduledURLNavigation(Document& initiatingDocument, double delay, SecurityOrigin* securityOrigin, const URL& url, const String& referrer, LockHistory lockHistory, LockBackForwardList lockBackForwardList, bool duringLoad, bool isLocationChange)
        : ScheduledNavigation(delay, lockHistory, lockBackForwardList, duringLoad, isLocationChange, initiatingDocument.shouldOpenExternalURLsPolicyToPropagate())
        , m_initiatingDocument { makeRef(initiatingDocument) }
        , m_securityOrigin{ makeRefPtr(securityOrigin) }
        , m_url { url }
        , m_referrer { referrer }
    {
    }

    void didStartTimer(Frame& frame, Timer& timer) override
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;

        UserGestureIndicator gestureIndicator(userGestureToForward());
        frame.loader().clientRedirected(m_url, delay(), WallTime::now() + timer.nextFireInterval(), lockBackForwardList());
    }

    void didStopTimer(Frame& frame, NewLoadInProgress newLoadInProgress) override
    {
        if (!m_haveToldClient)
            return;

        // Do not set a UserGestureIndicator because
        // clientRedirectCancelledOrFinished() is also called from many places
        // inside FrameLoader, where the gesture state is not set and is in
        // fact unavailable. We need to be consistent with them, otherwise the
        // gesture state will sometimes be set and sometimes not within
        // dispatchDidCancelClientRedirect().
        frame.loader().clientRedirectCancelledOrFinished(newLoadInProgress);
    }

    Document& initiatingDocument() { return m_initiatingDocument.get(); }
    SecurityOrigin* securityOrigin() const { return m_securityOrigin.get(); }
    const URL& url() const { return m_url; }
    String referrer() const { return m_referrer; }

private:
    Ref<Document> m_initiatingDocument;
    RefPtr<SecurityOrigin> m_securityOrigin;
    URL m_url;
    String m_referrer;
    bool m_haveToldClient { false };
};

class ScheduledRedirect : public ScheduledURLNavigation {
public:
    ScheduledRedirect(Document& initiatingDocument, double delay, SecurityOrigin* securityOrigin, const URL& url, LockHistory lockHistory, LockBackForwardList lockBackForwardList)
        : ScheduledURLNavigation(initiatingDocument, delay, securityOrigin, url, String(), lockHistory, lockBackForwardList, false, false)
    {
        clearUserGesture();
    }

    bool shouldStartTimer(Frame& frame) override
    {
        return frame.loader().allAncestorsAreComplete();
    }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator { userGestureToForward() };

        bool refresh = equalIgnoringFragmentIdentifier(frame.document()->url(), url());
        ResourceRequest resourceRequest { url(), referrer(), refresh ? ResourceRequestCachePolicy::ReloadIgnoringCacheData : ResourceRequestCachePolicy::UseProtocolCachePolicy };
        if (initiatedByMainFrame() == InitiatedByMainFrame::Yes)
            resourceRequest.setRequester(ResourceRequest::Requester::Main);
        FrameLoadRequest frameLoadRequest { initiatingDocument(), *securityOrigin(), resourceRequest, "_self", lockHistory(), lockBackForwardList(), MaybeSendReferrer, AllowNavigationToInvalidURL::No, NewFrameOpenerPolicy::Allow, shouldOpenExternalURLs(), initiatedByMainFrame() };

        frame.loader().changeLocation(WTFMove(frameLoadRequest));
    }
};

class ScheduledLocationChange : public ScheduledURLNavigation {
public:
    ScheduledLocationChange(Document& initiatingDocument, SecurityOrigin* securityOrigin, const URL& url, const String& referrer, LockHistory lockHistory, LockBackForwardList lockBackForwardList, bool duringLoad)
        : ScheduledURLNavigation(initiatingDocument, 0.0, securityOrigin, url, referrer, lockHistory, lockBackForwardList, duringLoad, true) { }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator { userGestureToForward() };

        ResourceRequest resourceRequest { url(), referrer(), ResourceRequestCachePolicy::UseProtocolCachePolicy };
        FrameLoadRequest frameLoadRequest { initiatingDocument(), *securityOrigin(), resourceRequest, "_self", lockHistory(), lockBackForwardList(), MaybeSendReferrer, AllowNavigationToInvalidURL::No, NewFrameOpenerPolicy::Allow, shouldOpenExternalURLs(), initiatedByMainFrame() };

        frame.loader().changeLocation(WTFMove(frameLoadRequest));
    }
};

class ScheduledRefresh : public ScheduledURLNavigation {
public:
    ScheduledRefresh(Document& initiatingDocument, SecurityOrigin* securityOrigin, const URL& url, const String& referrer)
        : ScheduledURLNavigation(initiatingDocument, 0.0, securityOrigin, url, referrer, LockHistory::Yes, LockBackForwardList::Yes, false, true)
    {
    }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator { userGestureToForward() };

        ResourceRequest resourceRequest { url(), referrer(), ResourceRequestCachePolicy::ReloadIgnoringCacheData };
        FrameLoadRequest frameLoadRequest { initiatingDocument(), *securityOrigin(), resourceRequest, "_self", lockHistory(), lockBackForwardList(), MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, shouldOpenExternalURLs(), initiatedByMainFrame() };

        frame.loader().changeLocation(WTFMove(frameLoadRequest));
    }
};

class ScheduledHistoryNavigation : public ScheduledNavigation {
public:
    explicit ScheduledHistoryNavigation(int historySteps)
        : ScheduledNavigation(0, LockHistory::No, LockBackForwardList::No, false, true)
        , m_historySteps(historySteps)
    {
    }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator(userGestureToForward());

        if (!m_historySteps) {
            // Special case for go(0) from a frame -> reload only the frame
            // To follow Firefox and IE's behavior, history reload can only navigate the self frame.
            frame.loader().urlSelected(frame.document()->url(), "_self", 0, lockHistory(), lockBackForwardList(), MaybeSendReferrer, shouldOpenExternalURLs());
            return;
        }
        
        // go(i!=0) from a frame navigates into the history of the frame only,
        // in both IE and NS (but not in Mozilla). We can't easily do that.
        frame.page()->backForward().goBackOrForward(m_historySteps);
    }

private:
    int m_historySteps;
};

class ScheduledFormSubmission : public ScheduledNavigation {
public:
    ScheduledFormSubmission(Ref<FormSubmission>&& submission, LockBackForwardList lockBackForwardList, bool duringLoad)
        : ScheduledNavigation(0, submission->lockHistory(), lockBackForwardList, duringLoad, true, submission->state().sourceDocument().shouldOpenExternalURLsPolicyToPropagate())
        , m_submission(WTFMove(submission))
    {
    }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator(userGestureToForward());

        // The submitForm function will find a target frame before using the redirection timer.
        // Now that the timer has fired, we need to repeat the security check which normally is done when
        // selecting a target, in case conditions have changed. Other code paths avoid this by targeting
        // without leaving a time window. If we fail the check just silently drop the form submission.
        auto& requestingDocument = m_submission->state().sourceDocument();
        if (!requestingDocument.canNavigate(&frame))
            return;
        FrameLoadRequest frameLoadRequest { requestingDocument, requestingDocument.securityOrigin(), { }, { }, lockHistory(), lockBackForwardList(), MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, shouldOpenExternalURLs(), initiatedByMainFrame() };
        m_submission->populateFrameLoadRequest(frameLoadRequest);
        frame.loader().loadFrameRequest(WTFMove(frameLoadRequest), m_submission->event(), m_submission->takeState());
    }

    void didStartTimer(Frame& frame, Timer& timer) override
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;

        UserGestureIndicator gestureIndicator(userGestureToForward());
        frame.loader().clientRedirected(m_submission->requestURL(), delay(), WallTime::now() + timer.nextFireInterval(), lockBackForwardList());
    }

    void didStopTimer(Frame& frame, NewLoadInProgress newLoadInProgress) override
    {
        if (!m_haveToldClient)
            return;

        // Do not set a UserGestureIndicator because
        // clientRedirectCancelledOrFinished() is also called from many places
        // inside FrameLoader, where the gesture state is not set and is in
        // fact unavailable. We need to be consistent with them, otherwise the
        // gesture state will sometimes be set and sometimes not within
        // dispatchDidCancelClientRedirect().
        frame.loader().clientRedirectCancelledOrFinished(newLoadInProgress);
    }

private:
    Ref<FormSubmission> m_submission;
    bool m_haveToldClient { false };
};

class ScheduledPageBlock final : public ScheduledNavigation {
public:
    ScheduledPageBlock(Document& originDocument)
        : ScheduledNavigation(0, LockHistory::Yes, LockBackForwardList::Yes, false, false)
        , m_originDocument(originDocument)
    {
    }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator { userGestureToForward() };

        ResourceResponse replacementResponse { m_originDocument.url(), "text/plain"_s, 0, "UTF-8"_s };
        SubstituteData replacementData { SharedBuffer::create(), m_originDocument.url(), replacementResponse, SubstituteData::SessionHistoryVisibility::Hidden };

        ResourceRequest resourceRequest { m_originDocument.url(), emptyString(), ResourceRequestCachePolicy::ReloadIgnoringCacheData };
        FrameLoadRequest frameLoadRequest { m_originDocument, m_originDocument.securityOrigin(), resourceRequest, { }, lockHistory(), lockBackForwardList(), MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, shouldOpenExternalURLs(), initiatedByMainFrame() };
        frameLoadRequest.setSubstituteData(replacementData);
        frame.loader().load(WTFMove(frameLoadRequest));
    }

private:
    Document& m_originDocument;
};

NavigationScheduler::NavigationScheduler(Frame& frame)
    : m_frame(frame)
    , m_timer(*this, &NavigationScheduler::timerFired)
{
}

NavigationScheduler::~NavigationScheduler() = default;

bool NavigationScheduler::redirectScheduledDuringLoad()
{
    return m_redirect && m_redirect->wasDuringLoad();
}

bool NavigationScheduler::locationChangePending()
{
    return m_redirect && m_redirect->isLocationChange();
}

void NavigationScheduler::clear()
{
    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
    m_timer.stop();
    m_redirect = nullptr;
}

inline bool NavigationScheduler::shouldScheduleNavigation() const
{
    return m_frame.page();
}

inline bool NavigationScheduler::shouldScheduleNavigation(const URL& url) const
{
    if (!shouldScheduleNavigation())
        return false;
    if (WTF::protocolIsJavaScript(url))
        return true;
    return NavigationDisabler::isNavigationAllowed(m_frame);
}

void NavigationScheduler::scheduleRedirect(Document& initiatingDocument, double delay, const URL& url)
{
    if (!shouldScheduleNavigation(url))
        return;
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
    if (url.isEmpty())
        return;

    // We want a new back/forward list item if the refresh timeout is > 1 second.
    if (!m_redirect || delay <= m_redirect->delay()) {
        auto lockBackForwardList = delay <= 1 ? LockBackForwardList::Yes : LockBackForwardList::No;
        schedule(std::make_unique<ScheduledRedirect>(initiatingDocument, delay, &m_frame.document()->securityOrigin(), url, LockHistory::Yes, lockBackForwardList));
    }
}

LockBackForwardList NavigationScheduler::mustLockBackForwardList(Frame& targetFrame)
{
    // Non-user navigation before the page has finished firing onload should not create a new back/forward item.
    // See https://webkit.org/b/42861 for the original motivation for this.    
    if (!UserGestureIndicator::processingUserGesture() && targetFrame.loader().documentLoader() && !targetFrame.loader().documentLoader()->wasOnloadDispatched())
        return LockBackForwardList::Yes;
    
    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    for (Frame* ancestor = targetFrame.tree().parent(); ancestor; ancestor = ancestor->tree().parent()) {
        Document* document = ancestor->document();
        if (!ancestor->loader().isComplete() || (document && document->processingLoadEvent()))
            return LockBackForwardList::Yes;
    }
    return LockBackForwardList::No;
}

void NavigationScheduler::scheduleLocationChange(Document& initiatingDocument, SecurityOrigin& securityOrigin, const URL& url, const String& referrer, LockHistory lockHistory, LockBackForwardList lockBackForwardList)
{
    if (!shouldScheduleNavigation(url))
        return;

    if (lockBackForwardList == LockBackForwardList::No)
        lockBackForwardList = mustLockBackForwardList(m_frame);

    FrameLoader& loader = m_frame.loader();

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change.
    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(m_frame.document()->url(), url)) {
        ResourceRequest resourceRequest { m_frame.document()->completeURL(url), referrer, ResourceRequestCachePolicy::UseProtocolCachePolicy };
        auto* frame = lexicalFrameFromCommonVM();
        auto initiatedByMainFrame = frame && frame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;
        
        FrameLoadRequest frameLoadRequest { initiatingDocument, securityOrigin, resourceRequest, "_self"_s, lockHistory, lockBackForwardList, MaybeSendReferrer, AllowNavigationToInvalidURL::No, NewFrameOpenerPolicy::Allow, initiatingDocument.shouldOpenExternalURLsPolicyToPropagate(), initiatedByMainFrame };
        loader.changeLocation(WTFMove(frameLoadRequest));
        return;
    }

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !loader.stateMachine().committedFirstRealDocumentLoad();

    schedule(std::make_unique<ScheduledLocationChange>(initiatingDocument, &securityOrigin, url, referrer, lockHistory, lockBackForwardList, duringLoad));
}

void NavigationScheduler::scheduleFormSubmission(Ref<FormSubmission>&& submission)
{
    ASSERT(m_frame.page());

    // FIXME: Do we need special handling for form submissions where the URL is the same
    // as the current one except for the fragment part? See scheduleLocationChange above.

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !m_frame.loader().stateMachine().committedFirstRealDocumentLoad();

    // If this is a child frame and the form submission was triggered by a script, lock the back/forward list
    // to match IE and Opera.
    // See https://bugs.webkit.org/show_bug.cgi?id=32383 for the original motivation for this.
    LockBackForwardList lockBackForwardList = mustLockBackForwardList(m_frame);
    if (lockBackForwardList == LockBackForwardList::No
        && (submission->state().formSubmissionTrigger() == SubmittedByJavaScript && m_frame.tree().parent() && !UserGestureIndicator::processingUserGesture())) {
        lockBackForwardList = LockBackForwardList::Yes;
    }
    
    schedule(std::make_unique<ScheduledFormSubmission>(WTFMove(submission), lockBackForwardList, duringLoad));
}

void NavigationScheduler::scheduleRefresh(Document& initiatingDocument)
{
    if (!shouldScheduleNavigation())
        return;
    const URL& url = m_frame.document()->url();
    if (url.isEmpty())
        return;

    schedule(std::make_unique<ScheduledRefresh>(initiatingDocument, &m_frame.document()->securityOrigin(), url, m_frame.loader().outgoingReferrer()));
}

void NavigationScheduler::scheduleHistoryNavigation(int steps)
{
    LOG(History, "NavigationScheduler %p scheduleHistoryNavigation(%d) - shouldSchedule %d", this, steps, shouldScheduleNavigation());
    if (!shouldScheduleNavigation())
        return;

    // Invalid history navigations (such as history.forward() during a new load) have the side effect of cancelling any scheduled
    // redirects. We also avoid the possibility of cancelling the current load by avoiding the scheduled redirection altogether.
    BackForwardController& backForward = m_frame.page()->backForward();
    if ((steps > 0 && static_cast<unsigned>(steps) > backForward.forwardCount())
        || (steps < 0 && static_cast<unsigned>(-steps) > backForward.backCount())) {
        cancel();
        return;
    }

    // In all other cases, schedule the history traversal to occur asynchronously.
    schedule(std::make_unique<ScheduledHistoryNavigation>(steps));
}

void NavigationScheduler::schedulePageBlock(Document& originDocument)
{
    if (shouldScheduleNavigation())
        schedule(std::make_unique<ScheduledPageBlock>(originDocument));
}

void NavigationScheduler::timerFired()
{
    if (!m_frame.page())
        return;
    if (m_frame.page()->defersLoading()) {
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
        return;
    }

    Ref<Frame> protect(m_frame);

    std::unique_ptr<ScheduledNavigation> redirect = WTFMove(m_redirect);
    LOG(History, "NavigationScheduler %p timerFired - firing redirect %p", this, redirect.get());

    redirect->fire(m_frame);
    InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
}

void NavigationScheduler::schedule(std::unique_ptr<ScheduledNavigation> redirect)
{
    ASSERT(m_frame.page());

    Ref<Frame> protect(m_frame);

    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (redirect->wasDuringLoad()) {
        if (DocumentLoader* provisionalDocumentLoader = m_frame.loader().provisionalDocumentLoader())
            provisionalDocumentLoader->stopLoading();
        m_frame.loader().stopLoading(UnloadEventPolicyUnloadAndPageHide);
    }

    cancel();
    m_redirect = WTFMove(redirect);

    if (!m_frame.loader().isComplete() && m_redirect->isLocationChange())
        m_frame.loader().completed();

    if (!m_frame.page())
        return;

    startTimer();
}

void NavigationScheduler::startTimer()
{
    if (!m_redirect)
        return;

    ASSERT(m_frame.page());
    if (m_timer.isActive())
        return;
    if (!m_redirect->shouldStartTimer(m_frame))
        return;

    Seconds delay = 1_s * m_redirect->delay();
    m_timer.startOneShot(delay);
    InspectorInstrumentation::frameScheduledNavigation(m_frame, delay);
    m_redirect->didStartTimer(m_frame, m_timer); // m_redirect may be null on return (e.g. the client canceled the load)
}

void NavigationScheduler::cancel(NewLoadInProgress newLoadInProgress)
{
    LOG(History, "NavigationScheduler %p cancel(newLoadInProgress=%d)", this, newLoadInProgress == NewLoadInProgress::Yes);

    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
    m_timer.stop();

    if (auto redirect = WTFMove(m_redirect))
        redirect->didStopTimer(m_frame, newLoadInProgress);
}

} // namespace WebCore
