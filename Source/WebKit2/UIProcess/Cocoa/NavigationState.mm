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

#import "config.h"
#import "NavigationState.h"

#if WK_API_ENABLED

#import "APINavigationData.h"
#import "APIURL.h"
#import "APIString.h"
#import "NavigationActionData.h"
#import "PageLoadState.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WKNSURLProtectionSpace.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDataInternal.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKNavigationInternal.h"
#import "WKNavigationResponseInternal.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import <wtf/NeverDestroyed.h>

namespace WebKit {

static HashMap<WebPageProxy*, NavigationState*>& navigationStates()
{
    static NeverDestroyed<HashMap<WebPageProxy*, NavigationState*>> navigationStates;

    return navigationStates;
}

NavigationState::NavigationState(WKWebView *webView)
    : m_webView(webView)
    , m_navigationDelegateMethods()
    , m_historyDelegateMethods()
{
    ASSERT(m_webView->_page);
    ASSERT(!navigationStates().contains(m_webView->_page.get()));

    navigationStates().add(m_webView->_page.get(), this);
    m_webView->_page->pageLoadState().addObserver(*this);
}

NavigationState::~NavigationState()
{
    ASSERT(navigationStates().get(m_webView->_page.get()) == this);

    navigationStates().remove(m_webView->_page.get());
    m_webView->_page->pageLoadState().removeObserver(*this);
}

NavigationState& NavigationState::fromWebPage(WebPageProxy& webPageProxy)
{
    ASSERT(navigationStates().contains(&webPageProxy));

    return *navigationStates().get(&webPageProxy);
}

std::unique_ptr<API::LoaderClient> NavigationState::createLoaderClient()
{
    return std::make_unique<LoaderClient>(*this);
}

std::unique_ptr<API::PolicyClient> NavigationState::createPolicyClient()
{
    return std::make_unique<PolicyClient>(*this);
}

RetainPtr<id <WKNavigationDelegate> > NavigationState::navigationDelegate()
{
    return m_navigationDelegate.get();
}

void NavigationState::setNavigationDelegate(id <WKNavigationDelegate> delegate)
{
    m_navigationDelegate = delegate;

    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationResponse:decisionHandler:)];

    m_navigationDelegateMethods.webViewDidStartProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didStartProvisionalNavigation:)];
    m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didReceiveServerRedirectForProvisionalNavigation:)];
    m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)];
    m_navigationDelegateMethods.webViewDidCommitNavigation = [delegate respondsToSelector:@selector(webView:didCommitNavigation:)];
    m_navigationDelegateMethods.webViewDidFinishNavigation = [delegate respondsToSelector:@selector(webView:didFinishNavigation:)];
    m_navigationDelegateMethods.webViewDidFailNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailNavigation:withError:)];

    m_navigationDelegateMethods.webViewRenderingProgressDidChange = [delegate respondsToSelector:@selector(_webView:renderingProgressDidChange:)];
    m_navigationDelegateMethods.webViewCanAuthenticateAgainstProtectionSpace = [delegate respondsToSelector:@selector(_webView:canAuthenticateAgainstProtectionSpace:)];
    m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallenge = [delegate respondsToSelector:@selector(_webView:didReceiveAuthenticationChallenge:)];
    m_navigationDelegateMethods.webViewWebProcessDidCrash = [delegate respondsToSelector:@selector(_webViewWebProcessDidCrash:)];

    // FIXME: Remove this once no clients depend on it being called.
    m_navigationDelegateMethods.webViewDidFinishLoadingNavigation = [delegate respondsToSelector:@selector(webView:didFinishLoadingNavigation:)];
}

RetainPtr<id <WKHistoryDelegatePrivate> > NavigationState::historyDelegate()
{
    return m_historyDelegate.get();
}

void NavigationState::setHistoryDelegate(id <WKHistoryDelegatePrivate> historyDelegate)
{
    m_historyDelegate = historyDelegate;

    m_historyDelegateMethods.webViewDidNavigateWithNavigationData = [historyDelegate respondsToSelector:@selector(_webView:didNavigateWithNavigationData:)];
    m_historyDelegateMethods.webViewDidPerformClientRedirectFromURLToURL = [historyDelegate respondsToSelector:@selector(_webView:didPerformClientRedirectFromURL:toURL:)];
    m_historyDelegateMethods.webViewDidPerformServerRedirectFromURLToURL = [historyDelegate respondsToSelector:@selector(_webView:didPerformServerRedirectFromURL:toURL:)];
    m_historyDelegateMethods.webViewDidUpdateHistoryTitleForURL = [historyDelegate respondsToSelector:@selector(_webView:didUpdateHistoryTitle:forURL:)];
}

RetainPtr<WKNavigation> NavigationState::createLoadRequestNavigation(uint64_t navigationID, NSURLRequest *request)
{
    ASSERT(!m_navigations.contains(navigationID));

    RetainPtr<WKNavigation> navigation = adoptNS([[WKNavigation alloc] init]);
    [navigation setRequest:request];

    // FIXME: We need to remove the navigation when we're done with it!
    m_navigations.set(navigationID, navigation);

    return navigation;
}

void NavigationState::didNavigateWithNavigationData(const WebKit::WebNavigationDataStore& navigationDataStore)
{
    if (!m_historyDelegateMethods.webViewDidNavigateWithNavigationData)
        return;

    auto historyDelegate = m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_webView didNavigateWithNavigationData:wrapper(*API::NavigationData::create(navigationDataStore))];
}

void NavigationState::didPerformClientRedirect(const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_historyDelegateMethods.webViewDidPerformClientRedirectFromURLToURL)
        return;

    auto historyDelegate = m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_webView didPerformClientRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::didPerformServerRedirect(const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_historyDelegateMethods.webViewDidPerformServerRedirectFromURLToURL)
        return;

    auto historyDelegate = m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_webView didPerformServerRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::didUpdateHistoryTitle(const WTF::String& title, const WTF::String& url)
{
    if (!m_historyDelegateMethods.webViewDidUpdateHistoryTitleForURL)
        return;

    auto historyDelegate = m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_webView didUpdateHistoryTitle:title forURL:[NSURL _web_URLWithWTFString:url]];
}

NavigationState::PolicyClient::PolicyClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::PolicyClient::~PolicyClient()
{
}

void NavigationState::PolicyClient::decidePolicyForNavigationAction(WebPageProxy*, WebFrameProxy* destinationFrame, const NavigationActionData& navigationActionData, WebFrameProxy* sourceFrame, const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceRequest& request, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler) {
        // FIXME: <rdar://problem/15949822> Figure out what the "default delegate behavior" should be here.
        listener->use();
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: Set up the navigation action object.
    auto navigationAction = adoptNS([[WKNavigationAction alloc] init]);

    if (destinationFrame)
        [navigationAction setTargetFrame:adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*destinationFrame]).get()];

    if (sourceFrame) {
        if (sourceFrame == destinationFrame)
            [navigationAction setSourceFrame:[navigationAction targetFrame]];
        else
            [navigationAction setTargetFrame:adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*sourceFrame]).get()];
    }

    [navigationAction setNavigationType:toWKNavigationType(navigationActionData.navigationType)];
    [navigationAction setRequest:request.nsURLRequest(WebCore::DoNotUpdateHTTPBody)];
    [navigationAction _setOriginalURL:originalRequest.url()];
    [navigationAction _setUserInitiated:navigationActionData.isProcessingUserGesture];

    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationAction:navigationAction.get() decisionHandler:[listener](WKNavigationPolicyDecision policyDecision) {
        switch (policyDecision) {
        case WKNavigationActionPolicyAllow:
            listener->use();
            break;

        case WKNavigationActionPolicyCancel:
            listener->ignore();
            break;

// FIXME: Once we have a new enough compiler everywhere we don't need to ignore -Wswitch.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
        case _WKNavigationActionPolicyDownload:
#pragma clang diagnostic pop
            listener->download();
            break;
        }
    }];
}

void NavigationState::PolicyClient::decidePolicyForNewWindowAction(WebPageProxy* webPageProxy, WebFrameProxy* sourceFrame, const NavigationActionData& navigationActionData, const WebCore::ResourceRequest& request, const WTF::String& frameName, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
{
    decidePolicyForNavigationAction(webPageProxy, nullptr, navigationActionData, sourceFrame, request, request, std::move(listener), userData);
}

void NavigationState::PolicyClient::decidePolicyForResponse(WebPageProxy*, WebFrameProxy* frame, const WebCore::ResourceResponse& resourceResponse, const WebCore::ResourceRequest& resourceRequest, bool canShowMIMEType, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler) {
        // FIXME: <rdar://problem/15949822> Figure out what the "default delegate behavior" should be here.
        listener->use();
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: Set up the navigation response object.
    auto navigationResponse = adoptNS([[WKNavigationResponse alloc] init]);

    [navigationResponse setFrame:adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*frame]).get()];
    [navigationResponse setResponse:resourceResponse.nsURLResponse()];
    [navigationResponse setCanShowMIMEType:canShowMIMEType];

    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationResponse:navigationResponse.get() decisionHandler:[listener](WKNavigationResponsePolicyDecision policyDecision) {
        switch (policyDecision) {
        case WKNavigationResponsePolicyAllow:
            listener->use();
            break;

        case WKNavigationResponsePolicyCancel:
            listener->ignore();
            break;

// FIXME: Once we have a new enough compiler everywhere we don't need to ignore -Wswitch.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
        case _WKNavigationResponsePolicyBecomeDownload:
#pragma clang diagnostic pop
            listener->download();
            break;
        }
    }];
}

NavigationState::LoaderClient::LoaderClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::LoaderClient::~LoaderClient()
{
}

void NavigationState::LoaderClient::didStartProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidStartProvisionalNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didStartProvisionalNavigation:navigation];
}

void NavigationState::LoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy* webFrameProxy, uint64_t navigationID, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didReceiveServerRedirectForProvisionalNavigation:navigation];
}

void NavigationState::LoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, const WebCore::ResourceError& error, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    RetainPtr<WKNavigation> navigation;
    if (navigationID)
        navigation = m_navigationState.m_navigations.take(navigationID);

    // FIXME: Set the error on the navigation object.

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [navigationDelegate webView:m_navigationState.m_webView didFailProvisionalNavigation:navigation.get() withError:error];
}

void NavigationState::LoaderClient::didCommitLoadForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidCommitNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didCommitNavigation:navigation];
}

void NavigationState::LoaderClient::didFinishLoadForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishNavigation && !m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadingNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    // FIXME: Remove this once no clients depend on it being called.
    if (m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadingNavigation) {
        [static_cast<id>(navigationDelegate) webView:m_navigationState.m_webView didFinishLoadingNavigation:navigation];
        return;
    }

    [navigationDelegate webView:m_navigationState.m_webView didFinishNavigation:navigation];
}

void NavigationState::LoaderClient::didFailLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, const WebCore::ResourceError& error, API::Object* userData)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFailNavigationWithError)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didFailNavigation:navigation withError:error];
}

static _WKRenderingProgressEvents renderingProgressEvents(WebCore::LayoutMilestones milestones)
{
    _WKRenderingProgressEvents events = 0;

    if (milestones & WebCore::DidFirstLayout)
        events |= _WKRenderingProgressEventFirstLayout;

    if (milestones & WebCore::DidHitRelevantRepaintedObjectsAreaThreshold)
        events |= _WKRenderingProgressEventFirstPaintWithSignificantArea;

    return events;
}

void NavigationState::LoaderClient::didLayout(WebKit::WebPageProxy*, WebCore::LayoutMilestones layoutMilestones, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView renderingProgressDidChange:renderingProgressEvents(layoutMilestones)];
}

bool NavigationState::LoaderClient::canAuthenticateAgainstProtectionSpaceInFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::WebProtectionSpace* protectionSpace)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewCanAuthenticateAgainstProtectionSpace)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    return [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView canAuthenticateAgainstProtectionSpace:wrapper(*protectionSpace)];
}

void NavigationState::LoaderClient::didReceiveAuthenticationChallengeInFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::AuthenticationChallengeProxy* authenticationChallenge)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallenge)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didReceiveAuthenticationChallenge:wrapper(*authenticationChallenge)];
}

void NavigationState::LoaderClient::processDidCrash(WebKit::WebPageProxy*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidCrash)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidCrash:m_navigationState.m_webView];
}

void NavigationState::LoaderClient::didChangeBackForwardList(WebKit::WebPageProxy*, WebKit::WebBackForwardListItem* addedItem, Vector<RefPtr<WebKit::WebBackForwardListItem>> removedItems)
{
    [[NSNotificationCenter defaultCenter] postNotificationName:_WKBackForwardListDidChangeNotification object:wrapper(m_navigationState.m_webView->_page->backForwardList())];
}

void NavigationState::willChangeIsLoading()
{
    [m_webView willChangeValueForKey:@"loading"];
}

void NavigationState::didChangeIsLoading()
{
    [m_webView didChangeValueForKey:@"loading"];
}

void NavigationState::willChangeTitle()
{
    [m_webView willChangeValueForKey:@"title"];
}

void NavigationState::didChangeTitle()
{
    [m_webView didChangeValueForKey:@"title"];
}

void NavigationState::willChangeActiveURL()
{
    [m_webView willChangeValueForKey:@"URL"];
}

void NavigationState::didChangeActiveURL()
{
    [m_webView didChangeValueForKey:@"URL"];
}

void NavigationState::willChangeHasOnlySecureContent()
{
    [m_webView willChangeValueForKey:@"hasOnlySecureContent"];
}

void NavigationState::didChangeHasOnlySecureContent()
{
    [m_webView didChangeValueForKey:@"hasOnlySecureContent"];
}

void NavigationState::willChangeEstimatedProgress()
{
    [m_webView willChangeValueForKey:@"estimatedProgress"];
}

void NavigationState::didChangeEstimatedProgress()
{
    [m_webView didChangeValueForKey:@"estimatedProgress"];
}

} // namespace WebKit

#endif
