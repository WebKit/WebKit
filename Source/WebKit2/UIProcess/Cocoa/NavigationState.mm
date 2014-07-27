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
#import "APIString.h"
#import "APIURL.h"
#import "AuthenticationDecisionListener.h"
#import "CompletionHandlerCallChecker.h"
#import "NavigationActionData.h"
#import "PageLoadState.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WKNSURLProtectionSpace.h"
#import "WKNSURLRequest.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDataInternal.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKNavigationInternal.h"
#import "WKNavigationResponseInternal.h"
#import "WKReloadFrameErrorRecoveryAttempter.h"
#import "WKWebViewInternal.h"
#import "WebCredential.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKErrorRecoveryAttempting.h"
#import "_WKFrameHandleInternal.h"
#import <WebCore/AuthenticationMac.h>
#import <wtf/NeverDestroyed.h>

#if USE(QUICK_LOOK)
#import "QuickLookDocumentData.h"
#endif

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

    m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError = [delegate respondsToSelector:@selector(_webView:navigation:didFailProvisionalLoadInSubframe:withError:)];
    m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad = [delegate respondsToSelector:@selector(_webView:navigationDidFinishDocumentLoad:)];
    m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation = [delegate respondsToSelector:@selector(_webView:navigation:didSameDocumentNavigation:)];
    m_navigationDelegateMethods.webViewRenderingProgressDidChange = [delegate respondsToSelector:@selector(_webView:renderingProgressDidChange:)];
    m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler = [delegate respondsToSelector:@selector(webView:didReceiveAuthenticationChallenge:completionHandler:)];
    m_navigationDelegateMethods.webViewCanAuthenticateAgainstProtectionSpace = [delegate respondsToSelector:@selector(_webView:canAuthenticateAgainstProtectionSpace:)];
    m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallenge = [delegate respondsToSelector:@selector(_webView:didReceiveAuthenticationChallenge:)];
    m_navigationDelegateMethods.webViewWebProcessDidCrash = [delegate respondsToSelector:@selector(_webViewWebProcessDidCrash:)];
    m_navigationDelegateMethods.webCryptoMasterKeyForWebView = [delegate respondsToSelector:@selector(_webCryptoMasterKeyForWebView:)];
    m_navigationDelegateMethods.webViewDidBeginNavigationGesture = [delegate respondsToSelector:@selector(_webViewDidBeginNavigationGesture:)];
    m_navigationDelegateMethods.webViewWillEndNavigationGestureWithNavigationToBackForwardListItem = [delegate respondsToSelector:@selector(_webViewWillEndNavigationGesture:withNavigationToBackForwardListItem:)];
    m_navigationDelegateMethods.webViewDidEndNavigationGestureWithNavigationToBackForwardListItem = [delegate respondsToSelector:@selector(_webViewDidEndNavigationGesture:withNavigationToBackForwardListItem:)];
    m_navigationDelegateMethods.webViewWillSnapshotBackForwardListItem = [delegate respondsToSelector:@selector(_webView:willSnapshotBackForwardListItem:)];
#if USE(QUICK_LOOK)
    m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame = [delegate respondsToSelector:@selector(_webView:didStartLoadForQuickLookDocumentInMainFrameWithFileName:uti:)];
    m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame = [delegate respondsToSelector:@selector(_webView:didFinishLoadForQuickLookDocumentInMainFrame:)];
#endif
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
    navigation->_request = request;

    m_navigations.set(navigationID, navigation);

    return navigation;
}

RetainPtr<WKNavigation> NavigationState::createBackForwardNavigation(uint64_t navigationID, const WebBackForwardListItem& item)
{
    ASSERT(!m_navigations.contains(navigationID));

    auto navigation = adoptNS([[WKNavigation alloc] init]);

    m_navigations.set(navigationID, navigation);

    return navigation;
}

RetainPtr<WKNavigation> NavigationState::createReloadNavigation(uint64_t navigationID)
{
    ASSERT(!m_navigations.contains(navigationID));

    auto navigation = adoptNS([[WKNavigation alloc] init]);

    m_navigations.set(navigationID, navigation);

    return navigation;
}

RetainPtr<WKNavigation> NavigationState::createLoadDataNavigation(uint64_t navigationID)
{
    ASSERT(!m_navigations.contains(navigationID));

    auto navigation = adoptNS([[WKNavigation alloc] init]);

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

void NavigationState::navigationGestureDidBegin()
{
    if (!m_navigationDelegateMethods.webViewDidBeginNavigationGesture)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidBeginNavigationGesture:m_webView];
}

void NavigationState::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewWillEndNavigationGestureWithNavigationToBackForwardListItem)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewWillEndNavigationGesture:m_webView withNavigationToBackForwardListItem:willNavigate ? wrapper(item) : nil];
}

void NavigationState::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewDidEndNavigationGestureWithNavigationToBackForwardListItem)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidEndNavigationGesture:m_webView withNavigationToBackForwardListItem:willNavigate ? wrapper(item) : nil];
}

void NavigationState::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewWillSnapshotBackForwardListItem)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_webView willSnapshotBackForwardListItem:wrapper(item)];
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
    RetainPtr<NSURLRequest> nsURLRequest = adoptNS(wrapper(*API::URLRequest::create(request).leakRef()));

    if (listener->navigationID())
        m_navigationState.createLoadRequestNavigation(listener->navigationID(), nsURLRequest.get());

    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler) {
        if (!destinationFrame) {
            listener->use();
            return;
        }

        if ([NSURLConnection canHandleRequest:nsURLRequest.get()]) {
            listener->use();
            return;
        }

#if PLATFORM(MAC)
        // A file URL shouldn't fall through to here, but if it did,
        // it would be a security risk to open it.
        if (![[nsURLRequest URL] isFileURL])
            [[NSWorkspace sharedWorkspace] openURL:[nsURLRequest URL]];
#endif
        listener->ignore();
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto navigationAction = adoptNS([[WKNavigationAction alloc] _initWithNavigationActionData:navigationActionData]);

    if (destinationFrame)
        [navigationAction setTargetFrame:adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*destinationFrame]).get()];

    if (sourceFrame) {
        if (sourceFrame == destinationFrame)
            [navigationAction setSourceFrame:[navigationAction targetFrame]];
        else
            [navigationAction setSourceFrame:adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*sourceFrame]).get()];
    }

    [navigationAction setRequest:nsURLRequest.get()];
    [navigationAction _setOriginalURL:originalRequest.url()];

    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:decidePolicyForNavigationAction:decisionHandler:));
    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationAction:navigationAction.get() decisionHandler:[listener, checker](WKNavigationActionPolicy actionPolicy) {
        checker->didCallCompletionHandler();

        switch (actionPolicy) {
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
    decidePolicyForNavigationAction(webPageProxy, nullptr, navigationActionData, sourceFrame, request, request, WTF::move(listener), userData);
}

void NavigationState::PolicyClient::decidePolicyForResponse(WebPageProxy*, WebFrameProxy* frame, const WebCore::ResourceResponse& resourceResponse, const WebCore::ResourceRequest& resourceRequest, bool canShowMIMEType, RefPtr<WebFramePolicyListenerProxy> listener, API::Object* userData)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler) {
        NSURL *url = resourceResponse.nsURLResponse().URL;
        if ([url isFileURL]) {
            BOOL isDirectory = NO;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDirectory];

            if (exists && !isDirectory && canShowMIMEType)
                listener->use();
            else
                listener->ignore();
            return;
        }

        if (canShowMIMEType)
            listener->use();
        else
            listener->ignore();
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto navigationResponse = adoptNS([[WKNavigationResponse alloc] init]);

    navigationResponse->_frame = adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*frame]);
    navigationResponse->_request = resourceRequest.nsURLRequest(WebCore::DoNotUpdateHTTPBody);
    [navigationResponse setResponse:resourceResponse.nsURLResponse()];
    [navigationResponse setCanShowMIMEType:canShowMIMEType];

    RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:decidePolicyForNavigationResponse:decisionHandler:));
    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationResponse:navigationResponse.get() decisionHandler:[listener, checker](WKNavigationResponsePolicy responsePolicy) {
        checker->didCallCompletionHandler();

        switch (responsePolicy) {
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

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
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

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didReceiveServerRedirectForProvisionalNavigation:navigation];
}

static RetainPtr<NSError> createErrorWithRecoveryAttempter(WKWebView *webView, WebFrameProxy& webFrameProxy, NSError *originalError)
{
    RefPtr<API::FrameHandle> frameHandle = API::FrameHandle::create(webFrameProxy.frameID());

    auto recoveryAttempter = adoptNS([[WKReloadFrameErrorRecoveryAttempter alloc] initWithWebView:webView frameHandle:wrapper(*frameHandle) urlString:originalError.userInfo[NSURLErrorFailingURLStringErrorKey]]);

    auto userInfo = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:recoveryAttempter.get(), _WKRecoveryAttempterErrorKey, nil]);

    if (NSDictionary *originalUserInfo = originalError.userInfo)
        [userInfo addEntriesFromDictionary:originalUserInfo];

    return adoptNS([[NSError alloc] initWithDomain:originalError.domain code:originalError.code userInfo:userInfo.get()]);
}

void NavigationState::LoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, const WebCore::ResourceError& error, API::Object*)
{
    if (!webFrameProxy->isMainFrame()) {
        if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError)
            return;

        auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
        auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, *webFrameProxy, error);
        // FIXME: Get the main frame's current navigation.
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView navigation:nil didFailProvisionalLoadInSubframe:adoptNS([[WKFrameInfo alloc] initWithWebFrameProxy:*webFrameProxy]).get() withError:errorWithRecoveryAttempter.get()];

        return;
    }

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    RetainPtr<WKNavigation> navigation;
    if (navigationID)
        navigation = m_navigationState.m_navigations.take(navigationID);

    // FIXME: Set the error on the navigation object.

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, *webFrameProxy, error);
    [navigationDelegate webView:m_navigationState.m_webView didFailProvisionalNavigation:navigation.get() withError:errorWithRecoveryAttempter.get()];
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

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [navigationDelegate webView:m_navigationState.m_webView didCommitNavigation:navigation];
}

void NavigationState::LoaderClient::didFinishDocumentLoadForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView navigationDidFinishDocumentLoad:navigation];
}

void NavigationState::LoaderClient::didFinishLoadForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

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

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, *webFrameProxy, error);
    [navigationDelegate webView:m_navigationState.m_webView didFailNavigation:navigation withError:errorWithRecoveryAttempter.get()];
}

static _WKSameDocumentNavigationType toWKSameDocumentNavigationType(SameDocumentNavigationType navigationType)
{
    switch (navigationType) {
    case SameDocumentNavigationAnchorNavigation:
        return _WKSameDocumentNavigationTypeAnchorNavigation;
    case SameDocumentNavigationSessionStatePush:
        return _WKSameDocumentNavigationTypeSessionStatePush;
    case SameDocumentNavigationSessionStateReplace:
        return _WKSameDocumentNavigationTypeSessionStateReplace;
    case SameDocumentNavigationSessionStatePop:
        return _WKSameDocumentNavigationTypeSessionStatePop;
    }

    ASSERT_NOT_REACHED();
    return _WKSameDocumentNavigationTypeAnchorNavigation;
}

void NavigationState::LoaderClient::didSameDocumentNavigationForFrame(WebPageProxy*, WebFrameProxy* webFrameProxy, uint64_t navigationID, SameDocumentNavigationType navigationType, API::Object*)
{
    if (!webFrameProxy->isMainFrame())
        return;

    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the page cache.
    WKNavigation *navigation = nil;
    if (navigationID)
        navigation = m_navigationState.m_navigations.get(navigationID).get();

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView navigation:navigation didSameDocumentNavigation:toWKSameDocumentNavigationType(navigationType)];
}

void NavigationState::LoaderClient::didDestroyNavigation(WebPageProxy*, uint64_t navigationID)
{
    m_navigationState.m_navigations.remove(navigationID);
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
    if (m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler)
        return protectionSpace->authenticationScheme() != WebCore::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested;

    if (!m_navigationState.m_navigationDelegateMethods.webViewCanAuthenticateAgainstProtectionSpace)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    return [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView canAuthenticateAgainstProtectionSpace:wrapper(*protectionSpace)];
}

void NavigationState::LoaderClient::didReceiveAuthenticationChallengeInFrame(WebPageProxy*, WebFrameProxy*, AuthenticationChallengeProxy* authenticationChallenge)
{
#if !defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    if (m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler) {
        ASSERT(authenticationChallenge->protectionSpace()->authenticationScheme() != WebCore::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested);
        auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
        if (!navigationDelegate) {
            authenticationChallenge->listener()->performDefaultHandling();
            return;
        }

        RefPtr<AuthenticationChallengeProxy> challenge = authenticationChallenge;
        RefPtr<CompletionHandlerCallChecker> checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:didReceiveAuthenticationChallenge:completionHandler:));
        [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) webView:m_navigationState.m_webView didReceiveAuthenticationChallenge:wrapper(*authenticationChallenge)
            completionHandler:[challenge, checker](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
                checker->didCallCompletionHandler();

                switch (disposition) {
                case NSURLSessionAuthChallengeUseCredential: {
                    RefPtr<WebCredential> webCredential;
                    if (credential)
                        webCredential = WebCredential::create(WebCore::core(credential));

                    challenge->listener()->useCredential(webCredential.get());
                    break;
                }

                case NSURLSessionAuthChallengePerformDefaultHandling:
                    challenge->listener()->performDefaultHandling();
                    break;

                case NSURLSessionAuthChallengeCancelAuthenticationChallenge:
                    challenge->listener()->cancel();
                    break;

                case NSURLSessionAuthChallengeRejectProtectionSpace:
                    challenge->listener()->rejectProtectionSpaceAndContinue();
                    break;

                default:
                    [NSException raise:NSInvalidArgumentException format:@"Invalid NSURLSessionAuthChallengeDisposition (%ld)", (long)disposition];
                }
            }
        ];
        return;
    }
#endif

    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallenge)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didReceiveAuthenticationChallenge:wrapper(*authenticationChallenge)];
}

void NavigationState::LoaderClient::processDidCrash(WebKit::WebPageProxy*)
{
    m_navigationState.m_navigations.clear();

    if (!m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidCrash)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidCrash:m_navigationState.m_webView];
}

PassRefPtr<API::Data> NavigationState::LoaderClient::webCryptoMasterKey(WebKit::WebPageProxy&)
{
    if (!m_navigationState.m_navigationDelegateMethods.webCryptoMasterKeyForWebView)
        return nullptr;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return nullptr;

    RetainPtr<NSData> data = [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webCryptoMasterKeyForWebView:m_navigationState.m_webView];

    return API::Data::createWithoutCopying((const unsigned char*)[data bytes], [data length], [] (unsigned char*, const void* data) {
        [(NSData *)data release];
    }, data.leakRef());
}

#if USE(QUICK_LOOK)
void NavigationState::LoaderClient::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:fileName uti:uti];
}

void NavigationState::LoaderClient::didFinishLoadForQuickLookDocumentInMainFrame(const WebKit::QuickLookDocumentData& data)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didFinishLoadForQuickLookDocumentInMainFrame:(NSData *)data.decodedData()];
}
#endif

void NavigationState::willChangeIsLoading()
{
    [m_webView willChangeValueForKey:@"loading"];
}

void NavigationState::didChangeIsLoading()
{
#if PLATFORM(IOS)
    if (m_webView->_page->pageLoadState().isLoading())
        m_activityToken = std::make_unique<ProcessThrottler::BackgroundActivityToken>(m_webView->_page->process().throttler());
    else
        m_activityToken = nullptr;
#endif

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

void NavigationState::willChangeCanGoBack()
{
    [m_webView willChangeValueForKey:@"canGoBack"];
}

void NavigationState::didChangeCanGoBack()
{
    [m_webView didChangeValueForKey:@"canGoBack"];
}

void NavigationState::willChangeCanGoForward()
{
    [m_webView willChangeValueForKey:@"canGoForward"];
}

void NavigationState::didChangeCanGoForward()
{
    [m_webView didChangeValueForKey:@"canGoForward"];
}

void NavigationState::willChangeNetworkRequestsInProgress()
{
    [m_webView willChangeValueForKey:@"_networkRequestsInProgress"];
}

void NavigationState::didChangeNetworkRequestsInProgress()
{
    [m_webView didChangeValueForKey:@"_networkRequestsInProgress"];
}

} // namespace WebKit

#endif
