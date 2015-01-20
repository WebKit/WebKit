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

#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import "APILoaderClient.h"
#import "APIPolicyClient.h"
#import "PageLoadState.h"
#import "ProcessThrottler.h"
#import "WeakObjCPtr.h"

@class WKWebView;
@protocol WKHistoryDelegatePrivate;
@protocol WKNavigationDelegate;

namespace API {
class Navigation;
}

namespace WebKit {

struct WebNavigationDataStore;

class NavigationState final : private PageLoadState::Observer {
public:
    explicit NavigationState(WKWebView *);
    ~NavigationState();

    static NavigationState& fromWebPage(WebPageProxy&);

    std::unique_ptr<API::PolicyClient> createPolicyClient();
    std::unique_ptr<API::LoaderClient> createLoaderClient();

    RetainPtr<id <WKNavigationDelegate> > navigationDelegate();
    void setNavigationDelegate(id <WKNavigationDelegate>);

    RetainPtr<id <WKHistoryDelegatePrivate> > historyDelegate();
    void setHistoryDelegate(id <WKHistoryDelegatePrivate>);

    // Called by the page client.
    void navigationGestureDidBegin();
    void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&);
    void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&);
    void willRecordNavigationSnapshot(WebBackForwardListItem&);

private:
    class PolicyClient final : public API::PolicyClient {
    public:
        explicit PolicyClient(NavigationState&);
        ~PolicyClient();

    private:
        // API::PolicyClient
        virtual void decidePolicyForNavigationAction(WebPageProxy&, WebFrameProxy* destinationFrame, const NavigationActionData&, WebFrameProxy* sourceFrame, const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceRequest&, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;
        virtual void decidePolicyForNewWindowAction(WebPageProxy&, WebFrameProxy& sourceFrame, const NavigationActionData&, const WebCore::ResourceRequest&, const WTF::String& frameName, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;
        virtual void decidePolicyForResponse(WebPageProxy&, WebFrameProxy&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, Ref<WebFramePolicyListenerProxy>&&, API::Object* userData) override;

        NavigationState& m_navigationState;
    };

    class LoaderClient final : public API::LoaderClient {
    public:
        explicit LoaderClient(NavigationState&);
        ~LoaderClient();

    private:
        virtual void didStartProvisionalLoadForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, API::Object*) override;
        virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, API::Object*) override;
        virtual void didFailProvisionalLoadWithErrorForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        virtual void didCommitLoadForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, API::Object*) override;
        virtual void didFinishDocumentLoadForFrame(WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) override;
        virtual void didFinishLoadForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, API::Object*) override;
        virtual void didFailLoadWithErrorForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) override;
        virtual void didSameDocumentNavigationForFrame(WebPageProxy&, WebFrameProxy&, API::Navigation*, SameDocumentNavigationType, API::Object*) override;
        virtual void didLayout(WebPageProxy&, WebCore::LayoutMilestones, API::Object*) override;
        virtual bool canAuthenticateAgainstProtectionSpaceInFrame(WebPageProxy&, WebKit::WebFrameProxy&, WebKit::WebProtectionSpace*) override;
        virtual void didReceiveAuthenticationChallengeInFrame(WebPageProxy&, WebKit::WebFrameProxy&, WebKit::AuthenticationChallengeProxy*) override;
        virtual void processDidCrash(WebPageProxy&) override;
        virtual PassRefPtr<API::Data> webCryptoMasterKey(WebPageProxy&) override;

#if USE(QUICK_LOOK)
        virtual void didStartLoadForQuickLookDocumentInMainFrame(const WTF::String& fileName, const WTF::String& uti) override;
        virtual void didFinishLoadForQuickLookDocumentInMainFrame(const WebKit::QuickLookDocumentData&) override;
#endif

        virtual void didNavigateWithNavigationData(WebPageProxy&, const WebNavigationDataStore&, WebFrameProxy&) override;
        virtual void didPerformClientRedirect(WebPageProxy&, const WTF::String&, const WTF::String&, WebFrameProxy&) override;
        virtual void didPerformServerRedirect(WebPageProxy&, const WTF::String&, const WTF::String&, WebFrameProxy&) override;
        virtual void didUpdateHistoryTitle(WebPageProxy&, const WTF::String&, const WTF::String&, WebFrameProxy&) override;

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
        bool webViewCanAuthenticateAgainstProtectionSpace : 1;
        bool webViewDidReceiveAuthenticationChallenge : 1;
        bool webViewWebProcessDidCrash : 1;
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
