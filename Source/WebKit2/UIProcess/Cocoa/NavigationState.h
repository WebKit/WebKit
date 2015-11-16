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

namespace WebKit {

struct SecurityOriginData;
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

    void didFirstPaint();

private:
    class NavigationClient final : public API::NavigationClient {
    public:
        explicit NavigationClient(NavigationState&);
        ~NavigationClient();

    private:
        virtual void didStartProvisionalNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        virtual void didReceiveServerRedirectForProvisionalNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        virtual void didFailProvisionalNavigationWithError(WebPageProxy&, WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        virtual void didFailProvisionalLoadInSubframeWithError(WebPageProxy&, WebFrameProxy&, const SecurityOriginData&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        virtual void didCommitNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        virtual void didFinishDocumentLoad(WebPageProxy&, API::Navigation*, API::Object*) override;
        virtual void didFinishNavigation(WebPageProxy&, API::Navigation*, API::Object*) override;
        virtual void didFailNavigationWithError(WebPageProxy&, WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        virtual void didSameDocumentNavigation(WebPageProxy&, API::Navigation*, SameDocumentNavigationType, API::Object*) override;

        virtual void renderingProgressDidChange(WebPageProxy&, WebCore::LayoutMilestones, API::Object*) override;

        virtual bool canAuthenticateAgainstProtectionSpace(WebPageProxy&, WebProtectionSpace*) override;
        virtual void didReceiveAuthenticationChallenge(WebPageProxy&, AuthenticationChallengeProxy*) override;
        virtual void processDidCrash(WebPageProxy&) override;
        virtual void processDidBecomeResponsive(WebPageProxy&) override;
        virtual void processDidBecomeUnresponsive(WebPageProxy&) override;

        virtual PassRefPtr<API::Data> webCryptoMasterKey(WebPageProxy&) override;

#if USE(QUICK_LOOK)
        virtual void didStartLoadForQuickLookDocumentInMainFrame(const WTF::String& fileName, const WTF::String& uti) override;
        virtual void didFinishLoadForQuickLookDocumentInMainFrame(const QuickLookDocumentData&) override;
#endif

        virtual void decidePolicyForNavigationAction(WebPageProxy&, API::NavigationAction&, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;
        virtual void decidePolicyForNavigationResponse(WebPageProxy&, API::NavigationResponse&, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;

        NavigationState& m_navigationState;
    };
    
    class HistoryClient final : public API::HistoryClient {
    public:
        explicit HistoryClient(NavigationState&);
        ~HistoryClient();
        
    private:
        virtual void didNavigateWithNavigationData(WebPageProxy&, const WebNavigationDataStore&) override;
        virtual void didPerformClientRedirect(WebPageProxy&, const WTF::String&, const WTF::String&) override;
        virtual void didPerformServerRedirect(WebPageProxy&, const WTF::String&, const WTF::String&) override;
        virtual void didUpdateHistoryTitle(WebPageProxy&, const WTF::String&, const WTF::String&) override;
        
        NavigationState& m_navigationState;
    };

    // PageLoadState::Observer
    virtual void willChangeIsLoading() override;
    virtual void didChangeIsLoading() override;
    virtual void willChangeTitle() override;
    virtual void didChangeTitle() override;
    virtual void willChangeActiveURL() override;
    virtual void didChangeActiveURL() override;
    virtual void willChangeHasOnlySecureContent() override;
    virtual void didChangeHasOnlySecureContent() override;
    virtual void willChangeEstimatedProgress() override;
    virtual void didChangeEstimatedProgress() override;
    virtual void willChangeCanGoBack() override;
    virtual void didChangeCanGoBack() override;
    virtual void willChangeCanGoForward() override;
    virtual void didChangeCanGoForward() override;
    virtual void willChangeNetworkRequestsInProgress() override;
    virtual void didChangeNetworkRequestsInProgress() override;
    virtual void willChangeCertificateInfo() override;
    virtual void didChangeCertificateInfo() override;

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
