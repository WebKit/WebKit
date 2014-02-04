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

#import "NavigationActionData.h"
#import "PageLoadState.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDelegate.h"
#import "WKNavigationInternal.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"

namespace WebKit {

NavigationState::NavigationState(WKWebView *webView)
    : m_webView(webView)
    , m_navigationDelegateMethods()
{
    ASSERT(m_webView->_page);

    m_webView->_page->pageLoadState().addObserver(*this);
}

NavigationState::~NavigationState()
{
    m_webView->_page->pageLoadState().removeObserver(*this);
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
    m_navigationDelegateMethods.webViewDidFinishLoadingNavigation = [delegate respondsToSelector:@selector(webView:didFinishLoadingNavigation:)];
    m_navigationDelegateMethods.webViewDidFailNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailNavigation:withError:)];
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

NavigationState::PolicyClient::PolicyClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::PolicyClient::~PolicyClient()
{
}

static WKNavigationType toWKNavigationType(WebCore::NavigationType navigationType)
{
    switch (navigationType) {
    case WebCore::NavigationTypeLinkClicked:
        return WKNavigationTypeLinkActivated;
    case WebCore::NavigationTypeFormSubmitted:
        return WKNavigationTypeFormSubmitted;
    case WebCore::NavigationTypeBackForward:
        return WKNavigationTypeBackForward;
    case WebCore::NavigationTypeReload:
        return WKNavigationTypeReload;
    case WebCore::NavigationTypeFormResubmitted:
        return WKNavigationTypeFormResubmitted;
    case WebCore::NavigationTypeOther:
        return WKNavigationTypeOther;
    }

    ASSERT_NOT_REACHED();
    return WKNavigationTypeOther;
}

void NavigationState::PolicyClient::decidePolicyForNavigationAction(WebPageProxy*, WebFrameProxy* destinationFrame, const NavigationActionData& navigationActionData, WebFrameProxy* sourceFrame, const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceRequest&, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
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

    [navigationAction setNavigationType:toWKNavigationType(navigationActionData.navigationType)];

    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationAction:navigationAction.get() decisionHandler:[listener](WKNavigationPolicyDecision policyDecision) {
        switch (policyDecision) {
        case WKNavigationPolicyDecisionAllow:
            listener->use();
            break;

        case WKNavigationPolicyDecisionCancel:
            listener->ignore();
            break;

        case WKNavigationPolicyDecisionDownload:
            listener->download();
            break;
        }
    }];
}

void NavigationState::PolicyClient::decidePolicyForNewWindowAction(WebPageProxy* webPageProxy, WebFrameProxy* sourceFrame, const NavigationActionData& navigationActionData, const WebCore::ResourceRequest& request, const WTF::String& frameName, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
{
    decidePolicyForNavigationAction(webPageProxy, nullptr, navigationActionData, sourceFrame, request, request, std::move(listener), userData);
}

void NavigationState::PolicyClient::decidePolicyForResponse(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceResponse&, const WebCore::ResourceRequest& resourceRequest, bool canShowMIMEType, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
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
    WKNavigationResponse *navigationResponse = nil;

    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationResponse:navigationResponse decisionHandler:[listener](WKNavigationResponsePolicyDecision policyDecision) {
        switch (policyDecision) {
        case WKNavigationResponsePolicyDecisionAllow:
            listener->use();
            break;

        case WKNavigationResponsePolicyDecisionCancel:
            listener->ignore();
            break;

        case WKNavigationResponsePolicyDecisionBecomeDownload:
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

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadingNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for navigations originating from the web process.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didFinishLoadingNavigation:navigation];
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
}

void NavigationState::didChangeActiveURL()
{
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
}

void NavigationState::didChangeEstimatedProgress()
{
}

} // namespace WebKit

#endif
