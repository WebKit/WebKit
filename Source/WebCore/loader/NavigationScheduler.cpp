/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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
#include "CommonAtomStrings.h"
#include "CommonVM.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "FormState.h"
#include "FormSubmission.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderStateMachine.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HistoryItem.h"
#include "InspectorInstrumentation.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "Navigation.h"
#include "NavigationDisabler.h"
#include "Page.h"
#include "PolicyChecker.h"
#include "ScriptController.h"
#include "URLKeepingBlobAlive.h"
#include "UserGestureIndicator.h"
#include <wtf/Ref.h>

namespace WebCore {

unsigned NavigationDisabler::s_globalNavigationDisableCount = 0;

class ScheduledNavigation {
    WTF_MAKE_NONCOPYABLE(ScheduledNavigation); WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Loader);
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
    virtual bool targetIsCurrentFrame() const { return true; }

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
        , m_initiatingDocument { initiatingDocument }
        , m_securityOrigin { securityOrigin }
        , m_url { url, initiatingDocument.topOrigin().data() }
        , m_referrer { referrer }
    {
    }

    void didStartTimer(Frame& frame, Timer& timer) override
    {
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;

        UserGestureIndicator gestureIndicator(userGestureToForward());
        Ref protectedFrame { frame };

        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            return;
        localFrame->protectedLoader()->clientRedirected(URL(m_url), delay(), WallTime::now() + timer.nextFireInterval(), lockBackForwardList());
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
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame))
            localFrame->protectedLoader()->clientRedirectCancelledOrFinished(newLoadInProgress);
    }

    Document& initiatingDocument() { return m_initiatingDocument.get(); }
    Ref<Document> protectedInitiatingDocument() { return m_initiatingDocument; }
    SecurityOrigin* securityOrigin() const { return m_securityOrigin.get(); }
    RefPtr<SecurityOrigin> protectedSecurityOrigin() const { return m_securityOrigin; }
    const URL& url() const { return m_url; }
    String referrer() const { return m_referrer; }

private:
    Ref<Document> m_initiatingDocument;
    RefPtr<SecurityOrigin> m_securityOrigin;
    URLKeepingBlobAlive m_url;
    String m_referrer;
    bool m_haveToldClient { false };
};

class ScheduledRedirect : public ScheduledURLNavigation {
public:
    ScheduledRedirect(Document& initiatingDocument, double delay, SecurityOrigin* securityOrigin, const URL& url, LockHistory lockHistory, LockBackForwardList lockBackForwardList, IsMetaRefresh isMetaRefresh)
        : ScheduledURLNavigation(initiatingDocument, delay, securityOrigin, url, String(), lockHistory, lockBackForwardList, false, false)
        , m_isMetaRefresh(isMetaRefresh)
    {
        clearUserGesture();
    }

    bool shouldStartTimer(Frame& frame) override
    {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        return localFrame && localFrame->protectedLoader()->allAncestorsAreComplete();
    }

    void fire(Frame& frame) override
    {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            return;

        if (m_isMetaRefresh == IsMetaRefresh::Yes) {
            if (RefPtr document = localFrame->document(); document && document->isSandboxed(SandboxFlag::AutomaticFeatures)) {
                document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Unable to do meta refresh due to sandboxing"_s);
                return;
            }
        }

        UserGestureIndicator gestureIndicator { userGestureToForward() };

        bool refresh = equalIgnoringFragmentIdentifier(localFrame->document()->url(), url());
        ResourceRequest resourceRequest { url(), referrer(), refresh ? ResourceRequestCachePolicy::ReloadIgnoringCacheData : ResourceRequestCachePolicy::UseProtocolCachePolicy };
        if (initiatedByMainFrame() == InitiatedByMainFrame::Yes)
            resourceRequest.setRequester(ResourceRequestRequester::Main);
        FrameLoadRequest frameLoadRequest { protectedInitiatingDocument(), *protectedSecurityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame() };
        frameLoadRequest.setLockHistory(lockHistory());
        frameLoadRequest.setLockBackForwardList(lockBackForwardList());
        frameLoadRequest.disableNavigationToInvalidURL();
        frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLs());

        localFrame->protectedLoader()->changeLocation(WTFMove(frameLoadRequest));
    }

private:
    IsMetaRefresh m_isMetaRefresh;
};

class ScheduledLocationChange : public ScheduledURLNavigation {
public:
    ScheduledLocationChange(Document& initiatingDocument, SecurityOrigin* securityOrigin, const URL& url, const String& referrer, LockHistory lockHistory, LockBackForwardList lockBackForwardList, bool duringLoad, NavigationHistoryBehavior navigationHandling, CompletionHandler<void(bool)>&& completionHandler)
        : ScheduledURLNavigation(initiatingDocument, 0.0, securityOrigin, url, referrer, lockHistory, lockBackForwardList, duringLoad, true)
        , m_completionHandler(WTFMove(completionHandler))
        , m_navigationHistoryBehavior(navigationHandling)
    {
    }

    ~ScheduledLocationChange()
    {
        if (m_completionHandler)
            m_completionHandler(false);
    }

    void fire(Frame& frame) override
    {
        UserGestureIndicator gestureIndicator { userGestureToForward() };

        ResourceRequest resourceRequest { url(), referrer(), ResourceRequestCachePolicy::UseProtocolCachePolicy };
        FrameLoadRequest frameLoadRequest { protectedInitiatingDocument(), *protectedSecurityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame() };
        frameLoadRequest.setLockHistory(lockHistory());
        frameLoadRequest.setLockBackForwardList(lockBackForwardList());
        frameLoadRequest.disableNavigationToInvalidURL();
        frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLs());
        frameLoadRequest.setNavigationHistoryBehavior(m_navigationHistoryBehavior);

        auto completionHandler = std::exchange(m_completionHandler, nullptr);
        frame.changeLocation(WTFMove(frameLoadRequest));
        completionHandler(true);
    }

private:
    CompletionHandler<void(bool)> m_completionHandler;
    NavigationHistoryBehavior m_navigationHistoryBehavior;
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
        FrameLoadRequest frameLoadRequest { protectedInitiatingDocument(), *protectedSecurityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame() };
        frameLoadRequest.setLockHistory(lockHistory());
        frameLoadRequest.setLockBackForwardList(lockBackForwardList());
        frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLs());

        frame.changeLocation(WTFMove(frameLoadRequest));
    }
};

class ScheduledHistoryNavigation : public ScheduledNavigation {
public:
    explicit ScheduledHistoryNavigation(Ref<HistoryItem>&& historyItem)
        : ScheduledNavigation(0, LockHistory::No, LockBackForwardList::No, false, true)
        , m_historyItem(WTFMove(historyItem))
    {
    }

    void fire(Frame& frame) override
    {
        // If the destination HistoryItem is no longer in the back/forward list, then we don't proceed.
        RefPtr page { frame.page() };
        if (!page || !page->checkedBackForward()->containsItem(m_historyItem))
            return;

        UserGestureIndicator gestureIndicator(userGestureToForward());

        if (page->checkedBackForward()->currentItem() && page->checkedBackForward()->currentItem()->identifier() == m_historyItem->identifier()) {
            // Special case for go(0) from a frame -> reload only the frame
            // To follow Firefox and IE's behavior, history reload can only navigate the self frame.
            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame))
                localFrame->protectedLoader()->changeLocation(localFrame->document()->url(), selfTargetFrameName(), 0, ReferrerPolicy::EmptyString, shouldOpenExternalURLs(), std::nullopt, nullAtom(), std::nullopt, NavigationHistoryBehavior::Reload);
            return;
        }
        
        // go(i!=0) from a frame navigates into the history of the frame only,
        // in both IE and NS (but not in Mozilla). We can't easily do that.
        page->goToItem(page->mainFrame(), m_historyItem, FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No);
    }

private:
    Ref<HistoryItem> m_historyItem;
};

// This matches ScheduledHistoryNavigation, but instead of having a HistoryItem provided, it finds
// the HistoryItem corresponding to the provided Navigation API key:
// https://html.spec.whatwg.org/multipage/browsing-the-web.html#she-navigation-api-key
class ScheduledHistoryNavigationByKey : public ScheduledNavigation {
public:
    explicit ScheduledHistoryNavigationByKey(const String& key, CompletionHandler<void(ScheduleHistoryNavigationResult)>&& completionHandler)
        : ScheduledNavigation(0, LockHistory::No, LockBackForwardList::No, false, true)
        , m_key(key)
        , m_completionHandler(WTFMove(completionHandler))
    {
    }

    ~ScheduledHistoryNavigationByKey()
    {
        if (m_completionHandler)
            m_completionHandler(ScheduleHistoryNavigationResult::Aborted);
    }

    void fire(Frame& frame) override
    {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        RefPtr page { frame.page() };
        if (!page || !localFrame) {
            m_completionHandler(ScheduleHistoryNavigationResult::Aborted);
            return;
        }

        auto entry = localFrame->window()->navigation().findEntryByKey(m_key);
        if (!entry) {
            m_completionHandler(ScheduleHistoryNavigationResult::Aborted);
            return;
        }
        Ref historyItem = entry.value()->associatedHistoryItem();

        UserGestureIndicator gestureIndicator(userGestureToForward());

        if (page->backForward().currentItem() && page->backForward().currentItem()->identifier() == historyItem->identifier()) {
            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame))
                localFrame->protectedLoader()->changeLocation(localFrame->document()->url(), selfTargetFrameName(), 0, ReferrerPolicy::EmptyString, shouldOpenExternalURLs(), std::nullopt, nullAtom(), std::nullopt, NavigationHistoryBehavior::Reload);
            return;
        }

        auto completionHandler = std::exchange(m_completionHandler, nullptr);
        page->goToItem(page->mainFrame(), historyItem, FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No);
        completionHandler(ScheduleHistoryNavigationResult::Completed);
    }

private:
    String m_key;
    CompletionHandler<void(ScheduleHistoryNavigationResult)> m_completionHandler;
};

class ScheduledFormSubmission final : public ScheduledNavigation {
public:
    ScheduledFormSubmission(Ref<FormSubmission>&& submission, LockBackForwardList lockBackForwardList, bool duringLoad)
        : ScheduledNavigation(0, submission->lockHistory(), lockBackForwardList, duringLoad, true, submission->state().sourceDocument().shouldOpenExternalURLsPolicyToPropagate())
        , m_submission(WTFMove(submission))
    {
        Ref requestingDocument = m_submission->state().sourceDocument();
        if (!requestingDocument->loadEventFinished() && !UserGestureIndicator::processingUserGesture())
            m_navigationHistoryBehavior = NavigationHistoryBehavior::Replace;
    }

    void fire(Frame& frame) final
    {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            return;
        Ref submission = m_submission;
        if (submission->wasCancelled())
            return;

        UserGestureIndicator gestureIndicator(userGestureToForward());

        // The submitForm function will find a target frame before using the redirection timer.
        // Now that the timer has fired, we need to repeat the security check which normally is done when
        // selecting a target, in case conditions have changed. Other code paths avoid this by targeting
        // without leaving a time window. If we fail the check just silently drop the form submission.
        Ref requestingDocument = submission->state().sourceDocument();
        if (!requestingDocument->canNavigate(localFrame.get()))
            return;
        FrameLoadRequest frameLoadRequest { requestingDocument.copyRef(), requestingDocument->protectedSecurityOrigin(), { }, { }, initiatedByMainFrame() };
        frameLoadRequest.setLockHistory(lockHistory());
        frameLoadRequest.setLockBackForwardList(lockBackForwardList());
        frameLoadRequest.setReferrerPolicy(submission->referrerPolicy());
        frameLoadRequest.setNewFrameOpenerPolicy(submission->newFrameOpenerPolicy());
        frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLs());
        frameLoadRequest.disableShouldReplaceDocumentIfJavaScriptURL();
        submission->populateFrameLoadRequest(frameLoadRequest);
        auto navigationHistoryBehavior = m_navigationHistoryBehavior;
        if (localFrame->document() != requestingDocument.ptr())
            navigationHistoryBehavior = NavigationHistoryBehavior::Push;
        frameLoadRequest.setNavigationHistoryBehavior(navigationHistoryBehavior);
        localFrame->protectedLoader()->loadFrameRequest(WTFMove(frameLoadRequest), submission->protectedEvent().get(), submission->takeState());
    }

    void didStartTimer(Frame& frame, Timer& timer) final
    {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            return;
        if (m_haveToldClient)
            return;
        m_haveToldClient = true;

        UserGestureIndicator gestureIndicator(userGestureToForward());
        localFrame->protectedLoader()->clientRedirected(m_submission->requestURL(), delay(), WallTime::now() + timer.nextFireInterval(), lockBackForwardList());
    }

    void didStopTimer(Frame& frame, NewLoadInProgress newLoadInProgress) final
    {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            return;
        if (!m_haveToldClient)
            return;

        // Do not set a UserGestureIndicator because
        // clientRedirectCancelledOrFinished() is also called from many places
        // inside FrameLoader, where the gesture state is not set and is in
        // fact unavailable. We need to be consistent with them, otherwise the
        // gesture state will sometimes be set and sometimes not within
        // dispatchDidCancelClientRedirect().
        localFrame->protectedLoader()->clientRedirectCancelledOrFinished(newLoadInProgress);
    }

    bool targetIsCurrentFrame() const final
    {
        // For form submissions, we normally resolve the target frame before scheduling the submission on the
        // NavigationScheduler. However, if the target is _blank, we schedule the submission on the submitter's
        // frame and only create the new frame when actually starting the navigation.
        return !isBlankTargetFrameName(m_submission->target());
    }

private:
    Ref<FormSubmission> m_submission;
    bool m_haveToldClient { false };
    NavigationHistoryBehavior m_navigationHistoryBehavior { NavigationHistoryBehavior::Push };
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
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            return;
        UserGestureIndicator gestureIndicator { userGestureToForward() };

        Ref originDocument = m_originDocument.get();
        ResourceResponse replacementResponse { originDocument->url(), textPlainContentTypeAtom(), 0, "UTF-8"_s };
        SubstituteData replacementData { SharedBuffer::create(), originDocument->url(), replacementResponse, SubstituteData::SessionHistoryVisibility::Hidden };

        ResourceRequest resourceRequest { originDocument->url(), emptyString(), ResourceRequestCachePolicy::ReloadIgnoringCacheData };
        if (RefPtr documentLoader = originDocument->loader())
            resourceRequest.setIsAppInitiated(documentLoader->lastNavigationWasAppInitiated());
        FrameLoadRequest frameLoadRequest { originDocument.copyRef(), originDocument->protectedSecurityOrigin(), WTFMove(resourceRequest), { }, initiatedByMainFrame() };
        frameLoadRequest.setLockHistory(lockHistory());
        frameLoadRequest.setLockBackForwardList(lockBackForwardList());
        frameLoadRequest.setSubstituteData(replacementData);
        frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLs());
        localFrame->protectedLoader()->load(WTFMove(frameLoadRequest));
    }

private:
    WeakRef<Document, WeakPtrImplWithEventTargetData> m_originDocument;
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
    return m_redirect && m_redirect->isLocationChange() && m_redirect->targetIsCurrentFrame();
}

Ref<Frame> NavigationScheduler::protectedFrame() const
{
    return m_frame.get();
}

void NavigationScheduler::clear()
{
    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(protectedFrame());
    m_timer.stop();
    m_redirect = nullptr;
}

inline bool NavigationScheduler::shouldScheduleNavigation() const
{
    return m_frame->page();
}

inline bool NavigationScheduler::shouldScheduleNavigation(const URL& url) const
{
    if (!shouldScheduleNavigation())
        return false;
    if (url.protocolIsJavaScript())
        return true;
    return NavigationDisabler::isNavigationAllowed(protectedFrame());
}

void NavigationScheduler::scheduleRedirect(Document& initiatingDocument, double delay, const URL& url, IsMetaRefresh isMetaRefresh)
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
        schedule(makeUnique<ScheduledRedirect>(initiatingDocument, delay, downcast<LocalFrame>(m_frame.get()).document()->protectedSecurityOrigin().ptr(), url, LockHistory::Yes, lockBackForwardList, isMetaRefresh));
    }
}

LockBackForwardList NavigationScheduler::mustLockBackForwardList(Frame& targetFrame)
{
    // Non-user navigation before the page has finished firing onload should not create a new back/forward item.
    // See https://webkit.org/b/42861 for the original motivation for this.

    RefPtr localTargetFrame = dynamicDowncast<LocalFrame>(targetFrame);
    if (!UserGestureIndicator::processingUserGesture()
        && localTargetFrame
        && localTargetFrame->loader().documentLoader()
        && !localTargetFrame->loader().documentLoader()->wasOnloadDispatched())
        return LockBackForwardList::Yes;
    
    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    for (auto* ancestor = targetFrame.tree().parent(); ancestor; ancestor = ancestor->tree().parent()) {
        RefPtr localAncestor = dynamicDowncast<LocalFrame>(ancestor);
        if (!localAncestor)
            continue;
        RefPtr document = localAncestor->document();
        if (!localAncestor->loader().isComplete() || (document && document->processingLoadEvent()))
            return LockBackForwardList::Yes;
    }
    return LockBackForwardList::No;
}

void NavigationScheduler::scheduleLocationChange(Document& initiatingDocument, SecurityOrigin& securityOrigin, const URL& url, const String& referrer, LockHistory lockHistory, LockBackForwardList lockBackForwardList, NavigationHistoryBehavior historyHandling, CompletionHandler<void(ScheduleLocationChangeResult)>&& completionHandler)
{
    if (!shouldScheduleNavigation(url))
        return completionHandler(ScheduleLocationChangeResult::Stopped);

    if (lockBackForwardList == LockBackForwardList::No)
        lockBackForwardList = mustLockBackForwardList(m_frame);

    RefPtr localFrame = dynamicDowncast<LocalFrame>(m_frame.get());
    RefPtr loader = localFrame ? &localFrame->loader() : nullptr;

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change.
    if (url.hasFragmentIdentifier()
        && localFrame
        && equalIgnoringFragmentIdentifier(localFrame->document()->url(), url)) {
        ResourceRequest resourceRequest { localFrame->protectedDocument()->completeURL(url.string()), referrer, ResourceRequestCachePolicy::UseProtocolCachePolicy };
        auto* frame = lexicalFrameFromCommonVM();
        auto initiatedByMainFrame = frame && frame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;
        
        FrameLoadRequest frameLoadRequest { initiatingDocument, securityOrigin, WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame };
        frameLoadRequest.setLockHistory(lockHistory);
        frameLoadRequest.setLockBackForwardList(lockBackForwardList);
        frameLoadRequest.disableNavigationToInvalidURL();
        frameLoadRequest.setShouldOpenExternalURLsPolicy(initiatingDocument.shouldOpenExternalURLsPolicyToPropagate());
        frameLoadRequest.setNavigationHistoryBehavior(historyHandling);
        if (loader)
            loader->changeLocation(WTFMove(frameLoadRequest));
        return completionHandler(ScheduleLocationChangeResult::Completed);
    }

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = loader && !loader->stateMachine().committedFirstRealDocumentLoad();

    schedule(makeUnique<ScheduledLocationChange>(initiatingDocument, &securityOrigin, url, referrer, lockHistory, lockBackForwardList, duringLoad, historyHandling, [completionHandler = WTFMove(completionHandler)] (bool hasStarted) mutable {
        completionHandler(hasStarted ? ScheduleLocationChangeResult::Started : ScheduleLocationChangeResult::Stopped);
    }));
}

void NavigationScheduler::scheduleFormSubmission(Ref<FormSubmission>&& submission)
{
    ASSERT(m_frame->page());

    // FIXME: Do we need special handling for form submissions where the URL is the same
    // as the current one except for the fragment part? See scheduleLocationChange above.

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !downcast<LocalFrame>(m_frame.get()).loader().stateMachine().committedFirstRealDocumentLoad();

    // If this is a child frame and the form submission was triggered by a script, lock the back/forward list
    // to match IE and Opera.
    // See https://bugs.webkit.org/show_bug.cgi?id=32383 for the original motivation for this.
    LockBackForwardList lockBackForwardList = mustLockBackForwardList(protectedFrame());
    if (lockBackForwardList == LockBackForwardList::No
        && (submission->state().formSubmissionTrigger() == SubmittedByJavaScript && m_frame->tree().parent() && !UserGestureIndicator::processingUserGesture())) {
        lockBackForwardList = LockBackForwardList::Yes;
    }

    bool isJavaScriptURL = submission->requestURL().protocolIsJavaScript();

    auto scheduledFormSubmission = makeUnique<ScheduledFormSubmission>(WTFMove(submission), lockBackForwardList, duringLoad);

    // FIXME: We currently run JavaScript URLs synchronously even though this doesn't appear to match the specification.
    if (isJavaScriptURL) {
        scheduledFormSubmission->fire(downcast<LocalFrame>(protectedFrame()));
        return;
    }
    
    schedule(WTFMove(scheduledFormSubmission));
}

void NavigationScheduler::scheduleRefresh(Document& initiatingDocument)
{
    if (!shouldScheduleNavigation())
        return;
    Ref frame = downcast<LocalFrame>(m_frame.get());
    const URL& url = frame->document()->url();
    if (url.isEmpty())
        return;

    schedule(makeUnique<ScheduledRefresh>(initiatingDocument, frame->document()->protectedSecurityOrigin().ptr(), url, frame->loader().outgoingReferrer()));
}

void NavigationScheduler::scheduleHistoryNavigation(int steps)
{
    LOG(History, "NavigationScheduler %p scheduleHistoryNavigation(%d) - shouldSchedule %d", this, steps, shouldScheduleNavigation());
    if (!shouldScheduleNavigation())
        return;

    // Invalid history navigations (such as history.forward() during a new load) have the side effect of cancelling any scheduled
    // redirects. We also avoid the possibility of cancelling the current load by avoiding the scheduled redirection altogether.
    RefPtr page = m_frame->page();
    CheckedRef backForward = page->backForward();
    if ((steps > 0 && static_cast<unsigned>(steps) > backForward->forwardCount())
        || (steps < 0 && static_cast<unsigned>(-steps) > backForward->backCount())) {
        cancel();
        return;
    }

    RefPtr historyItem = backForward->itemAtIndex(steps);
    if (!historyItem) {
        cancel();
        return;
    }

    // In all other cases, schedule the history traversal to occur asynchronously.
    schedule(makeUnique<ScheduledHistoryNavigation>(historyItem.releaseNonNull()));
}

void NavigationScheduler::scheduleHistoryNavigationByKey(const String& key, CompletionHandler<void(ScheduleHistoryNavigationResult)>&& completionHandler)
{
    if (!shouldScheduleNavigation()) {
        completionHandler(ScheduleHistoryNavigationResult::Aborted);
        return;
    }

    schedule(makeUnique<ScheduledHistoryNavigationByKey>(key, WTFMove(completionHandler)));
}

void NavigationScheduler::schedulePageBlock(Document& originDocument)
{
    if (shouldScheduleNavigation())
        schedule(makeUnique<ScheduledPageBlock>(originDocument));
}

void NavigationScheduler::timerFired()
{
    Ref frame = m_frame.get();
    if (!frame->page())
        return;
    if (frame->page()->defersLoading()) {
        InspectorInstrumentation::frameClearedScheduledNavigation(frame);
        return;
    }

    std::unique_ptr<ScheduledNavigation> redirect = std::exchange(m_redirect, nullptr);
    LOG(History, "NavigationScheduler %p timerFired - firing redirect %p", this, redirect.get());

    redirect->fire(frame);
    InspectorInstrumentation::frameClearedScheduledNavigation(frame);
}

void NavigationScheduler::schedule(std::unique_ptr<ScheduledNavigation> redirect)
{
    ASSERT(m_frame->page());

    Ref frame = m_frame.get();
    auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());

    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (redirect->wasDuringLoad()) {
        if (localFrame) {
            if (RefPtr provisionalDocumentLoader = localFrame->loader().provisionalDocumentLoader())
                provisionalDocumentLoader->stopLoading();
            localFrame->protectedLoader()->stopLoading(UnloadEventPolicy::UnloadAndPageHide);
        }
    }

    cancel();
    m_redirect = WTFMove(redirect);

    if (localFrame && !localFrame->loader().isComplete() && m_redirect->isLocationChange())
        localFrame->protectedLoader()->completed();

    if (!m_frame->page())
        return;

    startTimer();
}

void NavigationScheduler::startTimer()
{
    if (!m_redirect)
        return;

    ASSERT(m_frame->page());
    if (m_timer.isActive())
        return;

    Ref frame = m_frame.get();
    if (!m_redirect->shouldStartTimer(frame))
        return;

    Seconds delay = 1_s * m_redirect->delay();
    m_timer.startOneShot(delay);
    InspectorInstrumentation::frameScheduledNavigation(frame, delay);
    m_redirect->didStartTimer(frame, m_timer); // m_redirect may be null on return (e.g. the client canceled the load)
}

void NavigationScheduler::cancel(NewLoadInProgress newLoadInProgress)
{
    LOG(History, "NavigationScheduler %p cancel(newLoadInProgress=%d)", this, newLoadInProgress == NewLoadInProgress::Yes);

    if (m_timer.isActive())
        InspectorInstrumentation::frameClearedScheduledNavigation(protectedFrame());
    m_timer.stop();

    if (auto redirect = std::exchange(m_redirect, nullptr))
        redirect->didStopTimer(protectedFrame(), newLoadInProgress);
}

bool NavigationScheduler::hasQueuedNavigation() const
{
    return m_redirect && !m_redirect->delay();
}

} // namespace WebCore
