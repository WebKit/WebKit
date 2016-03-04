/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef NavigationState_h
#define NavigationState_h

#import "WKFoundation.h"

#if WK_API_ENABLED

#import "APIHistoryClient.h"
#import "APINavigationClient.h"
#import "PageLoadState.h"
#import "ProcessThrottler.h"
#import "WeakObjCPtr.h"
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>

@class WKWebView;
@protocol WKHistoryDelegatePrivate;
@protocol WKNavigationDelegate;

namespace API {
class Navigation;
}

namespace WebCore {
struct SecurityOriginData;
}

namespace WebKit {

struct WebNavigationDataStore;

class NavigationState final : private PageLoadState::Observer {
public:
    explicit NavigationState(WKWebView *);
    ~NavigationState();

    static NavigationState& fromWebPage(WebPageProxy&);

    std::unique_ptr<API::NavigationClient> createNavigationClient();
    std::unique_ptr<API::HistoryClient> createHistoryClient();

    RetainPtr<id <WKNavigationDelegate> > navigationDelegate();
    void setNavigationDelegate(id <WKNavigationDelegate>);

    RetainPtr<id <WKHistoryDelegatePrivate> > historyDelegate();
    void setHistoryDelegate(id <WKHistoryDelegatePrivate>);

    // Called by the page client.
    void navigationGestureDidBegin();
    void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&);
    void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&);
    void willRecordNavigationSnapshot(WebBackForwardListItem&);
    void navigationGestureSnapshotWasRemoved();

    void didFirstPaint();

private:
    class NavigationClient final : public API::NavigationClient {
    public:
        explicit NavigationClient(NavigationState&);
        ~NavigationClient();

    private:
        void didStartProvisionalNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        void didReceiveServerRedirectForProvisionalNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        void didFailProvisionalNavigationWithError(WebPageProxy&, WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        void didFailProvisionalLoadInSubframeWithError(WebPageProxy&, WebFrameProxy&, const WebCore::SecurityOriginData&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        void didCommitNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        void didFinishDocumentLoad(WebPageProxy&, API::Navigation*, API::Object*) override;
        void didFinishNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        void didFailNavigationWithError(WebPageProxy&, WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        void didSameDocumentNavigation(WebPageProxy&, API::Navigation*, SameDocumentNavigationType, API::Object*) override;

        void renderingProgressDidChange(WebPageProxy&, WebCore::LayoutMilestones) override;

        bool canAuthenticateAgainstProtectionSpace(WebPageProxy&, WebProtectionSpace*) override;
        void didReceiveAuthenticationChallenge(WebPageProxy&, AuthenticationChallengeProxy*) override;
        void processDidCrash(WebPageProxy&) override;
        void processDidBecomeResponsive(WebPageProxy&) override;
        void processDidBecomeUnresponsive(WebPageProxy&) override;

        RefPtr<API::Data> webCryptoMasterKey(WebPageProxy&) override;

#if USE(QUICK_LOOK)
        void didStartLoadForQuickLookDocumentInMainFrame(const WTF::String& fileName, const WTF::String& uti) override;
        void didFinishLoadForQuickLookDocumentInMainFrame(const QuickLookDocumentData&) override;
#endif

        void decidePolicyForNavigationAction(WebPageProxy&, API::NavigationAction&, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;
        void decidePolicyForNavigationResponse(WebPageProxy&, API::NavigationResponse&, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;

        NavigationState& m_navigationState;
    };
    
    class HistoryClient final : public API::HistoryClient {
    public:
        explicit HistoryClient(NavigationState&);
        ~HistoryClient();
        
    private:
        void didNavigateWithNavigationData(WebPageProxy&, const WebNavigationDataStore&) override;
        void didPerformClientRedirect(WebPageProxy&, const WTF::String&, const WTF::String&) override;
        void didPerformServerRedirect(WebPageProxy&, const WTF::String&, const WTF::String&) override;
        void didUpdateHistoryTitle(WebPageProxy&, const WTF::String&, const WTF::String&) override;
        
        NavigationState& m_navigationState;
    };

    // PageLoadState::Observer
    void willChangeIsLoading() override;
    void didChangeIsLoading() override;
    void willChangeTitle() override;
    void didChangeTitle() override;
    void willChangeActiveURL() override;
    void didChangeActiveURL() override;
    void willChangeHasOnlySecureContent() override;
    void didChangeHasOnlySecureContent() override;
    void willChangeEstimatedProgress() override;
    void didChangeEstimatedProgress() override;
    void willChangeCanGoBack() override;
    void didChangeCanGoBack() override;
    void willChangeCanGoForward() override;
    void didChangeCanGoForward() override;
    void willChangeNetworkRequestsInProgress() override;
    void didChangeNetworkRequestsInProgress() override;
    void willChangeCertificateInfo() override;
    void didChangeCertificateInfo() override;
    void willChangeWebProcessIsResponsive() override;
    void didChangeWebProcessIsResponsive() override;

    WKWebView *m_webView;
    WeakObjCPtr<id <WKNavigationDelegate> > m_navigationDelegate;

    struct {
        bool webViewDecidePolicyForNavigationActionDecisionHandler : 1;
        bool webViewDecidePolicyForNavigationResponseDecisionHandler : 1;

        bool webViewDidStartProvisionalNavigation : 1;
        bool webViewDidReceiveServerRedirectForProvisionalNavigation : 1;
        bool webViewDidFailProvisionalNavigationWithError : 1;
        bool webViewNavigationDidFailProvisionalLoadInSubframeWithError : 1;
        bool webViewDidCommitNavigation : 1;
        bool webViewNavigationDidFinishDocumentLoad : 1;
        bool webViewDidFinishNavigation : 1;
        bool webViewDidFailNavigationWithError : 1;
        bool webViewNavigationDidSameDocumentNavigation : 1;

        bool webViewRenderingProgressDidChange : 1;
        bool webViewDidReceiveAuthenticationChallengeCompletionHandler : 1;
        bool webViewWebContentProcessDidTerminate : 1;
        bool webViewCanAuthenticateAgainstProtectionSpace : 1;
        bool webViewDidReceiveAuthenticationChallenge : 1;
        bool webViewWebProcessDidCrash : 1;
        bool webViewWebProcessDidBecomeResponsive : 1;
        bool webViewWebProcessDidBecomeUnresponsive : 1;
        bool webCryptoMasterKeyForWebView : 1;
        bool webViewDidBeginNavigationGesture : 1;
        bool webViewWillEndNavigationGestureWithNavigationToBackForwardListItem : 1;
        bool webViewDidEndNavigationGestureWithNavigationToBackForwardListItem : 1;
        bool webViewWillSnapshotBackForwardListItem : 1;
        bool webViewNavigationGestureSnapshotWasRemoved : 1;
#if USE(QUICK_LOOK)
        bool webViewDidStartLoadForQuickLookDocumentInMainFrame : 1;
        bool webViewDidFinishLoadForQuickLookDocumentInMainFrame : 1;
#endif
    } m_navigationDelegateMethods;

    WeakObjCPtr<id <WKHistoryDelegatePrivate> > m_historyDelegate;
    struct {
        bool webViewDidNavigateWithNavigationData : 1;
        bool webViewDidPerformClientRedirectFromURLToURL : 1;
        bool webViewDidPerformServerRedirectFromURLToURL : 1;
        bool webViewDidUpdateHistoryTitleForURL : 1;
    } m_historyDelegateMethods;

#if PLATFORM(IOS)
    ProcessThrottler::BackgroundActivityToken m_activityToken;
#endif
};

} // namespace WebKit

#endif

#endif // NavigationState_h
