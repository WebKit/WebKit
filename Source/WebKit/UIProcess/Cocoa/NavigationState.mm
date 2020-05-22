/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#import "APIContentRuleListAction.h"
#import "APIFrameInfo.h"
#import "APINavigationData.h"
#import "APINavigationResponse.h"
#import "APIPageConfiguration.h"
#import "APIString.h"
#import "APIURL.h"
#import "AuthenticationChallengeDisposition.h"
#import "AuthenticationChallengeDispositionCocoa.h"
#import "AuthenticationDecisionListener.h"
#import "CompletionHandlerCallChecker.h"
#import "Logging.h"
#import "NavigationActionData.h"
#import "PageLoadState.h"
#import "SOAuthorizationCoordinator.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKFrameInfoInternal.h"
#import "WKHistoryDelegatePrivate.h"
#import "WKNSDictionary.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WKNSURLRequest.h"
#import "WKNavigationActionInternal.h"
#import "WKNavigationDataInternal.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKNavigationInternal.h"
#import "WKNavigationResponseInternal.h"
#import "WKReloadFrameErrorRecoveryAttempter.h"
#import "WKWebViewInternal.h"
#import "WKWebpagePreferencesInternal.h"
#import "WebCredential.h"
#import "WebFrameProxy.h"
#import "WebNavigationState.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WebProtectionSpace.h"
#import "WebsiteDataStore.h"
#import "_WKContentRuleListActionInternal.h"
#import "_WKErrorRecoveryAttempting.h"
#import "_WKFrameHandleInternal.h"
#import "_WKRenderingProgressEventsInternal.h"
#import "_WKSameDocumentNavigationTypeInternal.h"
#import "_WKWebsitePoliciesInternal.h"
#import <WebCore/AuthenticationMac.h>
#import <WebCore/ContentRuleListResults.h>
#import <WebCore/Credential.h>
#import <WebCore/SSLKeyGenerator.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/SerializedCryptoKeyWrap.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#import <pal/ios/ManagedConfigurationSoftLink.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#endif

#if HAVE(APP_LINKS)
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#endif

namespace WebKit {
using namespace WebCore;

static HashMap<WebPageProxy*, NavigationState*>& navigationStates()
{
    static NeverDestroyed<HashMap<WebPageProxy*, NavigationState*>> navigationStates;

    return navigationStates;
}

NavigationState::NavigationState(WKWebView *webView)
    : m_webView(webView)
    , m_navigationDelegateMethods()
    , m_historyDelegateMethods()
#if PLATFORM(IOS_FAMILY)
    , m_releaseNetworkActivityTimer(RunLoop::current(), this, &NavigationState::releaseNetworkActivityAfterLoadCompletion)
#endif
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

UniqueRef<API::NavigationClient> NavigationState::createNavigationClient()
{
    return makeUniqueRef<NavigationClient>(*this);
}
    
UniqueRef<API::HistoryClient> NavigationState::createHistoryClient()
{
    return makeUniqueRef<HistoryClient>(*this);
}

RetainPtr<id <WKNavigationDelegate> > NavigationState::navigationDelegate()
{
    return m_navigationDelegate.get();
}

void NavigationState::setNavigationDelegate(id <WKNavigationDelegate> delegate)
{
    m_navigationDelegate = delegate;

    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:preferences:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandlerWebsitePolicies = [delegate respondsToSelector:@selector(_webView:decidePolicyForNavigationAction:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionUserInfoDecisionHandlerWebsitePolicies = [delegate respondsToSelector:@selector(_webView:decidePolicyForNavigationAction:userInfo:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler = [delegate respondsToSelector:@selector(_webView:decidePolicyForNavigationAction:preferences:userInfo:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationResponse:decisionHandler:)];

    m_navigationDelegateMethods.webViewDidStartProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didStartProvisionalNavigation:)];
    m_navigationDelegateMethods.webViewDidStartProvisionalNavigationUserInfo = [delegate respondsToSelector:@selector(_webView:didStartProvisionalNavigation:userInfo:)];
    m_navigationDelegateMethods.webViewDidStartProvisionalLoadWithRequestInFrame = [delegate respondsToSelector:@selector(_webView:didStartProvisionalLoadWithRequest:inFrame:)];
    m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didReceiveServerRedirectForProvisionalNavigation:)];
    m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)];
    m_navigationDelegateMethods.webViewDidFailProvisionalLoadWithRequestInFrameWithError = [delegate respondsToSelector:@selector(_webView:didFailProvisionalLoadWithRequest:inFrame:withError:)];
    m_navigationDelegateMethods.webViewDidCommitNavigation = [delegate respondsToSelector:@selector(webView:didCommitNavigation:)];
    m_navigationDelegateMethods.webViewDidCommitLoadWithRequestInFrame = [delegate respondsToSelector:@selector(_webView:didCommitLoadWithRequest:inFrame:)];
    m_navigationDelegateMethods.webViewDidFinishNavigation = [delegate respondsToSelector:@selector(webView:didFinishNavigation:)];
    m_navigationDelegateMethods.webViewDidFinishLoadWithRequestInFrame = [delegate respondsToSelector:@selector(_webView:didFinishLoadWithRequest:inFrame:)];
    m_navigationDelegateMethods.webViewDidFailNavigationWithError = [delegate respondsToSelector:@selector(webView:didFailNavigation:withError:)];
    m_navigationDelegateMethods.webViewDidFailNavigationWithErrorUserInfo = [delegate respondsToSelector:@selector(_webView:didFailNavigation:withError:userInfo:)];
    m_navigationDelegateMethods.webViewDidFailLoadWithRequestInFrameWithError = [delegate respondsToSelector:@selector(_webView:didFailLoadWithRequest:inFrame:withError:)];

    m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError = [delegate respondsToSelector:@selector(_webView:navigation:didFailProvisionalLoadInSubframe:withError:)];
    m_navigationDelegateMethods.webViewWillPerformClientRedirect = [delegate respondsToSelector:@selector(_webView:willPerformClientRedirectToURL:delay:)];
    m_navigationDelegateMethods.webViewDidPerformClientRedirect = [delegate respondsToSelector:@selector(_webView:didPerformClientRedirectFromURL:toURL:)];
    m_navigationDelegateMethods.webViewDidCancelClientRedirect = [delegate respondsToSelector:@selector(_webViewDidCancelClientRedirect:)];
    m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad = [delegate respondsToSelector:@selector(_webView:navigationDidFinishDocumentLoad:)];
    m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation = [delegate respondsToSelector:@selector(_webView:navigation:didSameDocumentNavigation:)];
    m_navigationDelegateMethods.webViewRenderingProgressDidChange = [delegate respondsToSelector:@selector(_webView:renderingProgressDidChange:)];
    m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler = [delegate respondsToSelector:@selector(webView:didReceiveAuthenticationChallenge:completionHandler:)];
    m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowLegacyTLS = [delegate respondsToSelector:@selector(_webView:authenticationChallenge:shouldAllowLegacyTLS:)];
    m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowDeprecatedTLS = [delegate respondsToSelector:@selector(webView:authenticationChallenge:shouldAllowDeprecatedTLS:)];
    m_navigationDelegateMethods.webViewDidNegotiateModernTLS = [delegate respondsToSelector:@selector(_webView:didNegotiateModernTLS:)];
    m_navigationDelegateMethods.webViewWebContentProcessDidTerminate = [delegate respondsToSelector:@selector(webViewWebContentProcessDidTerminate:)];
    m_navigationDelegateMethods.webViewWebContentProcessDidTerminateWithReason = [delegate respondsToSelector:@selector(_webView:webContentProcessDidTerminateWithReason:)];
    m_navigationDelegateMethods.webViewWebProcessDidCrash = [delegate respondsToSelector:@selector(_webViewWebProcessDidCrash:)];
    m_navigationDelegateMethods.webViewWebProcessDidBecomeResponsive = [delegate respondsToSelector:@selector(_webViewWebProcessDidBecomeResponsive:)];
    m_navigationDelegateMethods.webViewWebProcessDidBecomeUnresponsive = [delegate respondsToSelector:@selector(_webViewWebProcessDidBecomeUnresponsive:)];
    m_navigationDelegateMethods.webCryptoMasterKeyForWebView = [delegate respondsToSelector:@selector(_webCryptoMasterKeyForWebView:)];
    m_navigationDelegateMethods.webViewDidBeginNavigationGesture = [delegate respondsToSelector:@selector(_webViewDidBeginNavigationGesture:)];
    m_navigationDelegateMethods.webViewWillEndNavigationGestureWithNavigationToBackForwardListItem = [delegate respondsToSelector:@selector(_webViewWillEndNavigationGesture:withNavigationToBackForwardListItem:)];
    m_navigationDelegateMethods.webViewDidEndNavigationGestureWithNavigationToBackForwardListItem = [delegate respondsToSelector:@selector(_webViewDidEndNavigationGesture:withNavigationToBackForwardListItem:)];
    m_navigationDelegateMethods.webViewWillSnapshotBackForwardListItem = [delegate respondsToSelector:@selector(_webView:willSnapshotBackForwardListItem:)];
    m_navigationDelegateMethods.webViewNavigationGestureSnapshotWasRemoved = [delegate respondsToSelector:@selector(_webViewDidRemoveNavigationGestureSnapshot:)];
    m_navigationDelegateMethods.webViewURLContentRuleListIdentifiersNotifications = [delegate respondsToSelector:@selector(_webView:URL:contentRuleListIdentifiers:notifications:)];
    m_navigationDelegateMethods.webViewContentRuleListWithIdentifierPerformedActionForURL = [delegate respondsToSelector:@selector(_webView:contentRuleListWithIdentifier:performedAction:forURL:)];

#if USE(QUICK_LOOK)
    m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame = [delegate respondsToSelector:@selector(_webView:didStartLoadForQuickLookDocumentInMainFrameWithFileName:uti:)];
    m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame = [delegate respondsToSelector:@selector(_webView:didFinishLoadForQuickLookDocumentInMainFrame:)];
    m_navigationDelegateMethods.webViewDidRequestPasswordForQuickLookDocument = [delegate respondsToSelector:@selector(_webViewDidRequestPasswordForQuickLookDocument:)];
#endif
#if PLATFORM(MAC)
    m_navigationDelegateMethods.webViewWebGLLoadPolicyForURL = [delegate respondsToSelector:@selector(_webView:webGLLoadPolicyForURL:decisionHandler:)];
    m_navigationDelegateMethods.webViewResolveWebGLLoadPolicyForURL = [delegate respondsToSelector:@selector(_webView:resolveWebGLLoadPolicyForURL:decisionHandler:)];
    m_navigationDelegateMethods.webViewDidFailToInitializePlugInWithInfo = [delegate respondsToSelector:@selector(_webView:didFailToInitializePlugInWithInfo:)];
    m_navigationDelegateMethods.webViewDidBlockInsecurePluginVersionWithInfo = [delegate respondsToSelector:@selector(_webView:didBlockInsecurePluginVersionWithInfo:)];
    m_navigationDelegateMethods.webViewBackForwardListItemAddedRemoved = [delegate respondsToSelector:@selector(_webView:backForwardListItemAdded:removed:)];
    m_navigationDelegateMethods.webViewDecidePolicyForPluginLoadWithCurrentPolicyPluginInfoCompletionHandler = [delegate respondsToSelector:@selector(_webView:decidePolicyForPluginLoadWithCurrentPolicy:pluginInfo:completionHandler:)];
#endif
    m_navigationDelegateMethods.webViewWillGoToBackForwardListItemInBackForwardCache = [delegate respondsToSelector:@selector(_webView:willGoToBackForwardListItem:inPageCache:)];
#if HAVE(APP_SSO)
    m_navigationDelegateMethods.webViewDecidePolicyForSOAuthorizationLoadWithCurrentPolicyForExtensionCompletionHandler = [delegate respondsToSelector:@selector(_webView:decidePolicyForSOAuthorizationLoadWithCurrentPolicy:forExtension:completionHandler:)];
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

void NavigationState::navigationGestureSnapshotWasRemoved()
{
    if (!m_navigationDelegateMethods.webViewNavigationGestureSnapshotWasRemoved)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidRemoveNavigationGestureSnapshot:m_webView];
}

#if USE(QUICK_LOOK)
void NavigationState::didRequestPasswordForQuickLookDocument()
{
    if (!m_navigationDelegateMethods.webViewDidRequestPasswordForQuickLookDocument)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidRequestPasswordForQuickLookDocument:m_webView];
}
#endif

void NavigationState::didFirstPaint()
{
    if (!m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_webView renderingProgressDidChange:_WKRenderingProgressEventFirstPaint];
}

NavigationState::NavigationClient::NavigationClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::NavigationClient::~NavigationClient()
{
}

#if PLATFORM(MAC)
bool NavigationState::NavigationClient::didFailToInitializePlugIn(WebPageProxy&, API::Dictionary& info)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFailToInitializePlugInWithInfo)
        return false;
    
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;
    
    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didFailToInitializePlugInWithInfo:wrapper(info)];
    return true;
}

bool NavigationState::NavigationClient::didBlockInsecurePluginVersion(WebPageProxy&, API::Dictionary& info)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidBlockInsecurePluginVersionWithInfo)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didBlockInsecurePluginVersionWithInfo:wrapper(info)];
    return true;
}

static WebKit::PluginModuleLoadPolicy pluginModuleLoadPolicy(_WKPluginModuleLoadPolicy policy)
{
    switch (policy) {
    case _WKPluginModuleLoadPolicyLoadNormally:
        return WebKit::PluginModuleLoadNormally;
    case _WKPluginModuleLoadPolicyLoadUnsandboxed:
        return WebKit::PluginModuleLoadUnsandboxed;
    case _WKPluginModuleLoadPolicyBlockedForSecurity:
        return WebKit::PluginModuleBlockedForSecurity;
    case _WKPluginModuleLoadPolicyBlockedForCompatibility:
        return WebKit::PluginModuleBlockedForCompatibility;
    }
    ASSERT_NOT_REACHED();
    return WebKit::PluginModuleLoadNormally;
}

static _WKPluginModuleLoadPolicy wkPluginModuleLoadPolicy(WebKit::PluginModuleLoadPolicy policy)
{
    switch (policy) {
    case WebKit::PluginModuleLoadNormally:
        return _WKPluginModuleLoadPolicyLoadNormally;
    case WebKit::PluginModuleLoadUnsandboxed:
        return _WKPluginModuleLoadPolicyLoadUnsandboxed;
    case WebKit::PluginModuleBlockedForSecurity:
        return _WKPluginModuleLoadPolicyBlockedForSecurity;
    case WebKit::PluginModuleBlockedForCompatibility:
        return _WKPluginModuleLoadPolicyBlockedForCompatibility;
    }
    ASSERT_NOT_REACHED();
    return _WKPluginModuleLoadPolicyLoadNormally;
}

void NavigationState::NavigationClient::decidePolicyForPluginLoad(WebKit::WebPageProxy&, WebKit::PluginModuleLoadPolicy currentPluginLoadPolicy, API::Dictionary& pluginInformation, CompletionHandler<void(WebKit::PluginModuleLoadPolicy, const String&)>&& completionHandler)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForPluginLoadWithCurrentPolicyPluginInfoCompletionHandler) {
        completionHandler(currentPluginLoadPolicy, { });
        return;
    }
    
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate) {
        completionHandler(currentPluginLoadPolicy, { });
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:decidePolicyForPluginLoadWithCurrentPolicy:pluginInfo:completionHandler:));
    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView decidePolicyForPluginLoadWithCurrentPolicy:wkPluginModuleLoadPolicy(currentPluginLoadPolicy) pluginInfo:wrapper(pluginInformation) completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKPluginModuleLoadPolicy policy, NSString *unavailabilityDescription) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(pluginModuleLoadPolicy(policy), unavailabilityDescription);
    }).get()];
}

inline WebCore::WebGLLoadPolicy toWebCoreWebGLLoadPolicy(_WKWebGLLoadPolicy policy)
{
    switch (policy) {
    case _WKWebGLLoadPolicyAllowCreation:
        return WebCore::WebGLLoadPolicy::WebGLAllowCreation;
    case _WKWebGLLoadPolicyBlockCreation:
        return WebCore::WebGLLoadPolicy::WebGLBlockCreation;
    case _WKWebGLLoadPolicyPendingCreation:
        return WebCore::WebGLLoadPolicy::WebGLPendingCreation;
    }
    
    ASSERT_NOT_REACHED();
    return WebCore::WebGLLoadPolicy::WebGLAllowCreation;
}

void NavigationState::NavigationClient::webGLLoadPolicy(WebPageProxy&, const URL& url, CompletionHandler<void(WebCore::WebGLLoadPolicy)>&& completionHandler) const
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebGLLoadPolicyForURL) {
        completionHandler(WebGLLoadPolicy::WebGLAllowCreation);
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:webGLLoadPolicyForURL:decisionHandler:));
    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView webGLLoadPolicyForURL:(NSURL *)url decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKWebGLLoadPolicy policy) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(toWebCoreWebGLLoadPolicy(policy));
    }).get()];
}

void NavigationState::NavigationClient::resolveWebGLLoadPolicy(WebPageProxy&, const URL& url, CompletionHandler<void(WebCore::WebGLLoadPolicy)>&& completionHandler) const
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewResolveWebGLLoadPolicyForURL) {
        completionHandler(WebGLLoadPolicy::WebGLAllowCreation);
        return;
    }
    
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:resolveWebGLLoadPolicyForURL:decisionHandler:));
    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView resolveWebGLLoadPolicyForURL:(NSURL *)url decisionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKWebGLLoadPolicy policy) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(toWebCoreWebGLLoadPolicy(policy));
    }).get()];
}

bool NavigationState::NavigationClient::didChangeBackForwardList(WebPageProxy&, WebBackForwardListItem* added, const Vector<Ref<WebBackForwardListItem>>& removed)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewBackForwardListItemAddedRemoved)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    RetainPtr<NSArray<WKBackForwardListItem *>> removedItems;
    if (!removed.isEmpty()) {
        removedItems = createNSArray(removed, [] (auto& removedItem) {
            return wrapper(removedItem.get());
        });
    }
    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView backForwardListItemAdded:wrapper(added) removed:removedItems.get()];
    return true;
}
#endif

bool NavigationState::NavigationClient::willGoToBackForwardListItem(WebPageProxy&, WebBackForwardListItem& item, bool inBackForwardCache)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWillGoToBackForwardListItemInBackForwardCache)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView willGoToBackForwardListItem:wrapper(item) inPageCache:inBackForwardCache];
    return true;
}

static void trySOAuthorization(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Function<void(bool)>&& completionHandler)
{
#if HAVE(APP_SSO)
    if (!navigationAction->shouldPerformSOAuthorization()) {
        completionHandler(false);
        return;
    }
    page.websiteDataStore().soAuthorizationCoordinator().tryAuthorize(WTFMove(navigationAction), page, WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

static void tryInterceptNavigation(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, WTF::Function<void(bool)>&& completionHandler)
{
#if HAVE(APP_LINKS)
    if (navigationAction->shouldOpenAppLinks()) {
        auto url = navigationAction->request().url();
        auto* localCompletionHandler = new WTF::Function<void (bool)>([navigationAction = WTFMove(navigationAction), weakPage = makeWeakPtr(page), completionHandler = WTFMove(completionHandler)] (bool success) mutable {
            ASSERT(RunLoop::isMain());
            if (!success && weakPage) {
                trySOAuthorization(WTFMove(navigationAction), *weakPage, WTFMove(completionHandler));
                return;
            }
#if PLATFORM(IOS_FAMILY)
            if (success && weakPage)
                weakPage->willOpenAppLink();
#endif
            completionHandler(success);
        });
        [LSAppLink openWithURL:url completionHandler:[localCompletionHandler](BOOL success, NSError *) {
            dispatch_async(dispatch_get_main_queue(), [localCompletionHandler, success] {
                (*localCompletionHandler)(success);
                delete localCompletionHandler;
            });
        }];
        return;
    }
#endif

    trySOAuthorization(WTFMove(navigationAction), page, WTFMove(completionHandler));
}

void NavigationState::NavigationClient::decidePolicyForNavigationAction(WebPageProxy& webPageProxy, Ref<API::NavigationAction>&& navigationAction, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userInfo)
{
    bool subframeNavigation = navigationAction->targetFrame() && !navigationAction->targetFrame()->isMainFrame();

    RefPtr<API::WebsitePolicies> defaultWebsitePolicies;
    if (auto* policies = webPageProxy.configuration().defaultWebsitePolicies())
        defaultWebsitePolicies = policies->copy();

    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler
        && !m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler
        && !m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler
        && !m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandlerWebsitePolicies
        && !m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionUserInfoDecisionHandlerWebsitePolicies) {
        auto completionHandler = [webPage = makeRef(webPageProxy), listener = WTFMove(listener), navigationAction = navigationAction.copyRef(), defaultWebsitePolicies] (bool interceptedNavigation) {
            if (interceptedNavigation) {
                listener->ignore();
                return;
            }

            if (!navigationAction->targetFrame()) {
                listener->use(defaultWebsitePolicies.get());
                return;
            }

            NSURLRequest *nsURLRequest = wrapper(API::URLRequest::create(navigationAction->request()));
            if ([NSURLConnection canHandleRequest:nsURLRequest] || webPage->urlSchemeHandlerForScheme([nsURLRequest URL].scheme)) {
                if (navigationAction->shouldPerformDownload())
                    listener->download();
                else
                    listener->use(defaultWebsitePolicies.get());
                return;
            }

#if PLATFORM(MAC)
            // A file URL shouldn't fall through to here, but if it did,
            // it would be a security risk to open it.
            if (![[nsURLRequest URL] isFileURL])
                [[NSWorkspace sharedWorkspace] openURL:[nsURLRequest URL]];
#endif
            listener->ignore();
        };
        tryInterceptNavigation(WTFMove(navigationAction), webPageProxy, WTFMove(completionHandler));
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    bool delegateHasWebpagePreferences = m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler
        || m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler;
    bool delegateHasWebsitePolicies = m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandlerWebsitePolicies || m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionUserInfoDecisionHandlerWebsitePolicies;

    auto selectorForCompletionHandlerChecker = ([&] () -> SEL {
        if (delegateHasWebpagePreferences)
            return m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler ? @selector(webView:decidePolicyForNavigationAction:preferences:decisionHandler:) : @selector(_webView:decidePolicyForNavigationAction:preferences:userInfo:decisionHandler:);
        if (delegateHasWebsitePolicies)
            return @selector(_webView:decidePolicyForNavigationAction:decisionHandler:);
        return @selector(webView:decidePolicyForNavigationAction:decisionHandler:);
    })();

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), selectorForCompletionHandlerChecker);
    auto decisionHandlerWithPreferencesOrPolicies = [localListener = WTFMove(listener), navigationAction = navigationAction.copyRef(), checker = WTFMove(checker), webPageProxy = makeRef(webPageProxy), subframeNavigation, defaultWebsitePolicies] (WKNavigationActionPolicy actionPolicy, id policiesOrPreferences) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        RefPtr<API::WebsitePolicies> apiWebsitePolicies;
        if ([policiesOrPreferences isKindOfClass:WKWebpagePreferences.class])
            apiWebsitePolicies = ((WKWebpagePreferences *)policiesOrPreferences)->_websitePolicies.get();
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        else if ([policiesOrPreferences isKindOfClass:_WKWebsitePolicies.class])
            apiWebsitePolicies = [policiesOrPreferences webpagePreferences]->_websitePolicies.get();
        else if (policiesOrPreferences)
            [NSException raise:NSInvalidArgumentException format:@"Expected policies of class %@, but got %@", NSStringFromClass(_WKWebsitePolicies.class), [policiesOrPreferences class]];
ALLOW_DEPRECATED_DECLARATIONS_END
        else
            apiWebsitePolicies = defaultWebsitePolicies;

        if (apiWebsitePolicies) {
            if (apiWebsitePolicies->websiteDataStore() && subframeNavigation)
                [NSException raise:NSInvalidArgumentException format:@"WKWebpagePreferences._websiteDataStore must be nil for subframe navigations."];
            if (apiWebsitePolicies->userContentController() && subframeNavigation)
                [NSException raise:NSInvalidArgumentException format:@"WKWebpagePreferences._userContentController must be nil for subframe navigations."];
            if (!apiWebsitePolicies->customUserAgent().isNull() && subframeNavigation)
                [NSException raise:NSInvalidArgumentException format:@"WKWebpagePreferences._customUserAgent must be nil for subframe navigations."];
            if (!apiWebsitePolicies->customNavigatorPlatform().isNull() && subframeNavigation)
                [NSException raise:NSInvalidArgumentException format:@"WKWebpagePreferences._customNavigatorPlatform must be nil for subframe navigations."];
        }

        switch (actionPolicy) {
        case WKNavigationActionPolicyAllow:
        case _WKNavigationActionPolicyAllowInNewProcess:
            tryInterceptNavigation(WTFMove(navigationAction), webPageProxy, [actionPolicy, localListener = WTFMove(localListener), websitePolicies = WTFMove(apiWebsitePolicies)](bool interceptedNavigation) mutable {
                if (interceptedNavigation) {
                    localListener->ignore();
                    return;
                }

                localListener->use(websitePolicies.get(), actionPolicy == _WKNavigationActionPolicyAllowInNewProcess ? ProcessSwapRequestedByClient::Yes : ProcessSwapRequestedByClient::No);
            });
        
            break;

        case WKNavigationActionPolicyCancel:
            localListener->ignore();
            break;

        case _WKNavigationActionPolicyDownload:
            localListener->download();
            break;
        case _WKNavigationActionPolicyAllowWithoutTryingAppLink:
            trySOAuthorization(WTFMove(navigationAction), webPageProxy, [localListener = WTFMove(localListener), websitePolicies = WTFMove(apiWebsitePolicies)] (bool optimizedLoad) {
                if (optimizedLoad) {
                    localListener->ignore();
                    return;
                }

                localListener->use(websitePolicies.get());
            });
            break;
        }
    };

    if (delegateHasWebpagePreferences) {
        if (m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler)
            [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationAction:wrapper(navigationAction) preferences:wrapper(defaultWebsitePolicies) decisionHandler:makeBlockPtr(WTFMove(decisionHandlerWithPreferencesOrPolicies)).get()];
        else
            [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView decidePolicyForNavigationAction:wrapper(navigationAction) preferences:wrapper(defaultWebsitePolicies) userInfo:userInfo ? static_cast<id<NSSecureCoding>>(userInfo->wrapper()) : nil decisionHandler:makeBlockPtr(WTFMove(decisionHandlerWithPreferencesOrPolicies)).get()];
    } else if (delegateHasWebsitePolicies) {
        auto decisionHandler = makeBlockPtr(WTFMove(decisionHandlerWithPreferencesOrPolicies));
        if (m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionUserInfoDecisionHandlerWebsitePolicies) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView decidePolicyForNavigationAction:wrapper(navigationAction) userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil decisionHandler:decisionHandler.get()];
            ALLOW_DEPRECATED_DECLARATIONS_END
        } else {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView decidePolicyForNavigationAction:wrapper(navigationAction) decisionHandler:decisionHandler.get()];
            ALLOW_DEPRECATED_DECLARATIONS_END
        }
    } else {
        auto decisionHandler = [decisionHandlerWithPreferencesOrPolicies = WTFMove(decisionHandlerWithPreferencesOrPolicies)] (WKNavigationActionPolicy actionPolicy) mutable {
            decisionHandlerWithPreferencesOrPolicies(actionPolicy, nil);
        };
        [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationAction:wrapper(navigationAction) decisionHandler:makeBlockPtr(WTFMove(decisionHandler)).get()];
    }
}

void NavigationState::NavigationClient::contentRuleListNotification(WebPageProxy&, URL&& url, ContentRuleListResults&& results)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewURLContentRuleListIdentifiersNotifications
        && !m_navigationState.m_navigationDelegateMethods.webViewContentRuleListWithIdentifierPerformedActionForURL)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    RetainPtr<NSMutableArray<NSString *>> identifiers;
    RetainPtr<NSMutableArray<NSString *>> notifications;

    for (const auto& pair : results.results) {
        const String& listIdentifier = pair.first;
        const auto& result = pair.second;
        for (const String& notification : result.notifications) {
            if (!identifiers)
                identifiers = adoptNS([NSMutableArray new]);
            if (!notifications)
                notifications = adoptNS([NSMutableArray new]);
            [identifiers addObject:listIdentifier];
            [notifications addObject:notification];
        }
    }

    if (notifications && m_navigationState.m_navigationDelegateMethods.webViewURLContentRuleListIdentifiersNotifications)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView URL:url contentRuleListIdentifiers:identifiers.get() notifications:notifications.get()];

    if (m_navigationState.m_navigationDelegateMethods.webViewContentRuleListWithIdentifierPerformedActionForURL) {
        for (auto&& pair : WTFMove(results.results))
            [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView contentRuleListWithIdentifier:pair.first performedAction:wrapper(API::ContentRuleListAction::create(WTFMove(pair.second)).get()) forURL:url];
    }
}
    
void NavigationState::NavigationClient::decidePolicyForNavigationResponse(WebPageProxy& page, Ref<API::NavigationResponse>&& navigationResponse, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* userData)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler) {
        NSURL *url = navigationResponse->response().nsURLResponse().URL;
        if ([url isFileURL]) {
            BOOL isDirectory = NO;
            BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDirectory];

            if (exists && !isDirectory && navigationResponse->canShowMIMEType())
                listener->use();
            else
                listener->ignore();
            return;
        }

        if (navigationResponse->canShowMIMEType())
            listener->use();
        else
            listener->ignore();
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:decidePolicyForNavigationResponse:decisionHandler:));
    [navigationDelegate webView:m_navigationState.m_webView decidePolicyForNavigationResponse:wrapper(navigationResponse) decisionHandler:makeBlockPtr([localListener = WTFMove(listener), checker = WTFMove(checker)](WKNavigationResponsePolicy responsePolicy) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        switch (responsePolicy) {
        case WKNavigationResponsePolicyAllow:
            localListener->use();
            break;

        case WKNavigationResponsePolicyCancel:
            localListener->ignore();
            break;

        case _WKNavigationResponsePolicyBecomeDownload:
            localListener->download();
            break;
        }
    }).get()];
}

void NavigationState::NavigationClient::didStartProvisionalNavigation(WebPageProxy&, const WebCore::ResourceRequest& request, API::Navigation* navigation, API::Object* userInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState.m_navigationDelegateMethods.webViewDidStartProvisionalNavigationUserInfo)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didStartProvisionalNavigation:wrapper(navigation) userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
    else if (m_navigationState.m_navigationDelegateMethods.webViewDidStartProvisionalNavigation)
        [navigationDelegate webView:m_navigationState.m_webView didStartProvisionalNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::didStartProvisionalLoadForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, FrameInfoData&& frameInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    if (m_navigationState.m_navigationDelegateMethods.webViewDidStartProvisionalLoadWithRequestInFrame)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate.get() _webView:m_navigationState.m_webView didStartProvisionalLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page))];
}

void NavigationState::NavigationClient::didReceiveServerRedirectForProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.

    [navigationDelegate webView:m_navigationState.m_webView didReceiveServerRedirectForProvisionalNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::willPerformClientRedirect(WebPageProxy& page, const WTF::String& urlString, double delay)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWillPerformClientRedirect)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    URL url(URL(), urlString);

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState.m_webView willPerformClientRedirectToURL:url delay:delay];
}

void NavigationState::NavigationClient::didPerformClientRedirect(WebPageProxy& page, const WTF::String& sourceURLString, const WTF::String& destinationURLString)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidPerformClientRedirect)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    URL sourceURL(URL(), sourceURLString);
    URL destinationURL(URL(), destinationURLString);

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState.m_webView didPerformClientRedirectFromURL:sourceURL toURL:destinationURL];
}

void NavigationState::NavigationClient::didCancelClientRedirect(WebPageProxy& page)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidCancelClientRedirect)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidCancelClientRedirect:m_navigationState.m_webView];
}

static RetainPtr<NSError> createErrorWithRecoveryAttempter(WKWebView *webView, const FrameInfoData& frameInfo, NSError *originalError)
{
    auto frameHandle = API::FrameHandle::create(frameInfo.frameID ? *frameInfo.frameID : FrameIdentifier { });

    auto recoveryAttempter = adoptNS([[WKReloadFrameErrorRecoveryAttempter alloc] initWithWebView:webView frameHandle:wrapper(frameHandle.get()) urlString:originalError.userInfo[NSURLErrorFailingURLStringErrorKey]]);

    auto userInfo = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:recoveryAttempter.get(), _WKRecoveryAttempterErrorKey, nil]);

    if (NSDictionary *originalUserInfo = originalError.userInfo)
        [userInfo addEntriesFromDictionary:originalUserInfo];

    return adoptNS([[NSError alloc] initWithDomain:originalError.domain code:originalError.code userInfo:userInfo.get()]);
}

void NavigationState::NavigationClient::didFailProvisionalNavigationWithError(WebPageProxy& page, FrameInfoData&& frameInfo, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object*)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, frameInfo, error);

    if (frameInfo.isMainFrame) {
        // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
        if (m_navigationState.m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError)
            [navigationDelegate webView:m_navigationState.m_webView didFailProvisionalNavigation:wrapper(navigation) withError:errorWithRecoveryAttempter.get()];
    } else {
        if (m_navigationState.m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError)
            [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView navigation:nil didFailProvisionalLoadInSubframe:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) withError:errorWithRecoveryAttempter.get()];
    }
}

void NavigationState::NavigationClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, const WebCore::ResourceError& error, FrameInfoData&& frameInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, frameInfo, error);

    if (m_navigationState.m_navigationDelegateMethods.webViewDidFailProvisionalLoadWithRequestInFrameWithError)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didFailProvisionalLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didCommitNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState.m_navigationDelegateMethods.webViewDidCommitNavigation)
        [navigationDelegate webView:m_navigationState.m_webView didCommitNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::didCommitLoadForFrame(WebKit::WebPageProxy& page, WebCore::ResourceRequest&& request, FrameInfoData&& frameInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    if (m_navigationState.m_navigationDelegateMethods.webViewDidCommitLoadWithRequestInFrame)
        [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didCommitLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page))];
}

void NavigationState::NavigationClient::didFinishDocumentLoad(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView navigationDidFinishDocumentLoad:wrapper(navigation)];
}

void NavigationState::NavigationClient::didFinishNavigation(WebPageProxy&, API::Navigation* navigation, API::Object*)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState.m_navigationDelegateMethods.webViewDidFinishNavigation)
        [navigationDelegate webView:m_navigationState.m_webView didFinishNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::didFinishLoadForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, FrameInfoData&& frameInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    if (m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadWithRequestInFrame)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didFinishLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page))];
}

void NavigationState::NavigationClient::didFailNavigationWithError(WebPageProxy& page, const FrameInfoData& frameInfo, API::Navigation* navigation, const WebCore::ResourceError& error, API::Object* userInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, frameInfo, error);

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState.m_navigationDelegateMethods.webViewDidFailNavigationWithErrorUserInfo)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didFailNavigation:wrapper(navigation) withError:errorWithRecoveryAttempter.get() userInfo:userInfo ? static_cast<id <NSSecureCoding>>(userInfo->wrapper()) : nil];
    else if (m_navigationState.m_navigationDelegateMethods.webViewDidFailNavigationWithError)
        [navigationDelegate webView:m_navigationState.m_webView didFailNavigation:wrapper(navigation) withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didFailLoadWithErrorForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, const WebCore::ResourceError& error, FrameInfoData&& frameInfo)
{
    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState.m_webView, frameInfo, error);

    if (m_navigationState.m_navigationDelegateMethods.webViewDidFailLoadWithRequestInFrameWithError)
        [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView didFailLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)) withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didSameDocumentNavigation(WebPageProxy&, API::Navigation* navigation, SameDocumentNavigationType navigationType, API::Object*)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigationID is not zero here, but it's currently zero for some navigations through the back/forward cache.

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView navigation:wrapper(navigation) didSameDocumentNavigation:toWKSameDocumentNavigationType(navigationType)];
}

void NavigationState::NavigationClient::renderingProgressDidChange(WebPageProxy&, OptionSet<WebCore::LayoutMilestone> layoutMilestones)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView renderingProgressDidChange:renderingProgressEvents(layoutMilestones)];
}

bool NavigationState::NavigationClient::shouldBypassContentModeSafeguards() const
{
    return m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler
        || m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler;
}

void NavigationState::NavigationClient::didReceiveAuthenticationChallenge(WebPageProxy&, AuthenticationChallengeProxy& authenticationChallenge)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler)
        return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::PerformDefaultHandling);

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:didReceiveAuthenticationChallenge:completionHandler:));
    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) webView:m_navigationState.m_webView didReceiveAuthenticationChallenge:wrapper(authenticationChallenge) completionHandler:makeBlockPtr([challenge = makeRef(authenticationChallenge), checker = WTFMove(checker)](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        challenge->listener().completeChallenge(WebKit::toAuthenticationChallengeDisposition(disposition), Credential(credential));
    }).get()];
}

static bool systemAllowsLegacyTLSFor(WebPageProxy& page)
{
    bool enableLegacyTLS = page.websiteDataStore().configuration().legacyTLSEnabled();
    if (id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitEnableLegacyTLS"])
        enableLegacyTLS = [value boolValue];
    if (!enableLegacyTLS) {
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
        enableLegacyTLS = [[PAL::getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:@"allowDeprecatedWebKitTLS"] == MCRestrictedBoolExplicitYes;
#elif PLATFORM(MAC)
        enableLegacyTLS = CFPreferencesGetAppBooleanValue(CFSTR("allowDeprecatedWebKitTLS"), CFSTR("com.apple.applicationaccess"), nullptr);
#endif
    }
    return enableLegacyTLS;
}

void NavigationState::NavigationClient::shouldAllowLegacyTLS(WebPageProxy& page, AuthenticationChallengeProxy& authenticationChallenge, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowLegacyTLS
        && !m_navigationState.m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowDeprecatedTLS)
        return completionHandler(systemAllowsLegacyTLSFor(page));

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return completionHandler(systemAllowsLegacyTLSFor(page));

    if (m_navigationState.m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowDeprecatedTLS) {
        auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:authenticationChallenge:shouldAllowDeprecatedTLS:));
        [navigationDelegate.get() webView:m_navigationState.m_webView authenticationChallenge:wrapper(authenticationChallenge) shouldAllowDeprecatedTLS:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)](BOOL shouldAllow) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(shouldAllow);
        }).get()];
        return;
    }
    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:authenticationChallenge:shouldAllowLegacyTLS:));
    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView authenticationChallenge:wrapper(authenticationChallenge) shouldAllowLegacyTLS:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)](BOOL shouldAllow) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(shouldAllow);
    }).get()];
}

void NavigationState::NavigationClient::didNegotiateModernTLS(const WebCore::AuthenticationChallenge& challenge)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidNegotiateModernTLS)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didNegotiateModernTLS:mac(challenge)];
}

static _WKProcessTerminationReason wkProcessTerminationReason(ProcessTerminationReason reason)
{
    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
        return _WKProcessTerminationReasonExceededMemoryLimit;
    case ProcessTerminationReason::ExceededCPULimit:
        return _WKProcessTerminationReasonExceededCPULimit;
    case ProcessTerminationReason::NavigationSwap:
        // We probably shouldn't bother coming up with a new API type for process-swapping.
        // "Requested by client" seems like the best match for existing types.
        FALLTHROUGH;
    case ProcessTerminationReason::RequestedByClient:
        return _WKProcessTerminationReasonRequestedByClient;
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::Crash:
        return _WKProcessTerminationReasonCrash;
    }
    ASSERT_NOT_REACHED();
    return _WKProcessTerminationReasonCrash;
}

bool NavigationState::NavigationClient::processDidTerminate(WebPageProxy& page, ProcessTerminationReason reason)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebContentProcessDidTerminate
        && !m_navigationState.m_navigationDelegateMethods.webViewWebContentProcessDidTerminateWithReason
        && !m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidCrash)
        return false;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return false;

    if (m_navigationState.m_navigationDelegateMethods.webViewWebContentProcessDidTerminateWithReason) {
        [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView webContentProcessDidTerminateWithReason:wkProcessTerminationReason(reason)];
        return true;
    }

    // We prefer webViewWebContentProcessDidTerminate: over _webViewWebProcessDidCrash:.
    if (m_navigationState.m_navigationDelegateMethods.webViewWebContentProcessDidTerminate) {
        [navigationDelegate webViewWebContentProcessDidTerminate:m_navigationState.m_webView];
        return true;
    }

    ASSERT(m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidCrash);
    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidCrash:m_navigationState.m_webView];
    return true;
}

void NavigationState::NavigationClient::processDidBecomeResponsive(WebPageProxy& page)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidBecomeResponsive)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidBecomeResponsive:m_navigationState.m_webView];
}

void NavigationState::NavigationClient::processDidBecomeUnresponsive(WebPageProxy& page)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewWebProcessDidBecomeUnresponsive)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webViewWebProcessDidBecomeUnresponsive:m_navigationState.m_webView];
}

RefPtr<API::Data> NavigationState::NavigationClient::webCryptoMasterKey(WebPageProxy&)
{
    if (!m_navigationState.m_navigationDelegateMethods.webCryptoMasterKeyForWebView) {
        Vector<uint8_t> masterKey;
        if (!getDefaultWebCryptoMasterKey(masterKey))
            return nullptr;

        return API::Data::create(masterKey.data(), masterKey.size());
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return nullptr;

    NSData *data = [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webCryptoMasterKeyForWebView:m_navigationState.m_webView];
    return API::Data::createWithoutCopying(data);
}

RefPtr<API::String> NavigationState::NavigationClient::signedPublicKeyAndChallengeString(WebPageProxy& page, unsigned keySizeIndex, const RefPtr<API::String>& challengeString, const URL& url)
{
    // WebKitTestRunner uses C API. Hence, no SPI is provided to override the following function.
    return API::String::create(WebCore::signedPublicKeyAndChallengeString(keySizeIndex, challengeString->string(), url));
}

#if USE(QUICK_LOOK)
void NavigationState::NavigationClient::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didStartLoadForQuickLookDocumentInMainFrameWithFileName:fileName uti:uti];
}

void NavigationState::NavigationClient::didFinishLoadForQuickLookDocumentInMainFrame(const SharedBuffer& buffer)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate)
        return;

    [static_cast<id <WKNavigationDelegatePrivate>>(navigationDelegate.get()) _webView:m_navigationState.m_webView didFinishLoadForQuickLookDocumentInMainFrame:buffer.createNSData().get()];
}
#endif

#if HAVE(APP_SSO)
static SOAuthorizationLoadPolicy soAuthorizationLoadPolicy(_WKSOAuthorizationLoadPolicy policy)
{
    switch (policy) {
    case _WKSOAuthorizationLoadPolicyAllow:
        return SOAuthorizationLoadPolicy::Allow;
    case _WKSOAuthorizationLoadPolicyIgnore:
        return SOAuthorizationLoadPolicy::Ignore;
    }
    ASSERT_NOT_REACHED();
    return SOAuthorizationLoadPolicy::Allow;
}

static _WKSOAuthorizationLoadPolicy wkSOAuthorizationLoadPolicy(SOAuthorizationLoadPolicy policy)
{
    switch (policy) {
    case SOAuthorizationLoadPolicy::Allow:
        return _WKSOAuthorizationLoadPolicyAllow;
    case SOAuthorizationLoadPolicy::Ignore:
        return _WKSOAuthorizationLoadPolicyIgnore;
    }
    ASSERT_NOT_REACHED();
    return _WKSOAuthorizationLoadPolicyAllow;
}

void NavigationState::NavigationClient::decidePolicyForSOAuthorizationLoad(WebPageProxy&, SOAuthorizationLoadPolicy currentSOAuthorizationLoadPolicy, const String& extension, CompletionHandler<void(SOAuthorizationLoadPolicy)>&& completionHandler)
{
    if (!m_navigationState.m_navigationDelegateMethods.webViewDecidePolicyForSOAuthorizationLoadWithCurrentPolicyForExtensionCompletionHandler) {
        completionHandler(currentSOAuthorizationLoadPolicy);
        return;
    }

    auto navigationDelegate = m_navigationState.m_navigationDelegate.get();
    if (!navigationDelegate) {
        completionHandler(currentSOAuthorizationLoadPolicy);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:decidePolicyForSOAuthorizationLoadWithCurrentPolicy:forExtension:completionHandler:));
    [(id <WKNavigationDelegatePrivate>)navigationDelegate _webView:m_navigationState.m_webView decidePolicyForSOAuthorizationLoadWithCurrentPolicy:wkSOAuthorizationLoadPolicy(currentSOAuthorizationLoadPolicy) forExtension:extension completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKSOAuthorizationLoadPolicy policy) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(soAuthorizationLoadPolicy(policy));
    }).get()];
}
#endif

// HistoryDelegatePrivate support
    
NavigationState::HistoryClient::HistoryClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::HistoryClient::~HistoryClient()
{
}

void NavigationState::HistoryClient::didNavigateWithNavigationData(WebPageProxy&, const WebNavigationDataStore& navigationDataStore)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidNavigateWithNavigationData)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didNavigateWithNavigationData:wrapper(API::NavigationData::create(navigationDataStore))];
}

void NavigationState::HistoryClient::didPerformClientRedirect(WebPageProxy&, const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidPerformClientRedirectFromURLToURL)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didPerformClientRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::HistoryClient::didPerformServerRedirect(WebPageProxy&, const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidPerformServerRedirectFromURLToURL)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didPerformServerRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::HistoryClient::didUpdateHistoryTitle(WebPageProxy&, const WTF::String& title, const WTF::String& url)
{
    if (!m_navigationState.m_historyDelegateMethods.webViewDidUpdateHistoryTitleForURL)
        return;

    auto historyDelegate = m_navigationState.m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState.m_webView didUpdateHistoryTitle:title forURL:[NSURL _web_URLWithWTFString:url]];
}

void NavigationState::willChangeIsLoading()
{
    [m_webView willChangeValueForKey:@"loading"];
}

#if PLATFORM(IOS_FAMILY)
void NavigationState::releaseNetworkActivity(NetworkActivityReleaseReason reason)
{
    if (!m_networkActivity)
        return;

    switch (reason) {
    case NetworkActivityReleaseReason::LoadCompleted:
        RELEASE_LOG_IF(m_webView->_page->isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p NavigationState is releasing background process assertion because a page load completed", this);
        break;
    case NetworkActivityReleaseReason::ScreenLocked:
        RELEASE_LOG_IF(m_webView->_page->isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p NavigationState is releasing background process assertion because the screen was locked", this);
        break;
    }
    m_networkActivity = nullptr;
    m_releaseNetworkActivityTimer.stop();
}
#endif

void NavigationState::didChangeIsLoading()
{
#if PLATFORM(IOS_FAMILY)
    if (m_webView->_page->pageLoadState().isLoading()) {
        // We do not start a network activity if a load starts after the screen has been locked.
        if ([UIApp isSuspendedUnderLock])
            return;

        if (m_releaseNetworkActivityTimer.isActive()) {
            RELEASE_LOG_IF(m_webView->_page->isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NavigationState keeps its process network assertion because a new page load started", this);
            m_releaseNetworkActivityTimer.stop();
        }
        if (!m_networkActivity) {
            RELEASE_LOG_IF(m_webView->_page->isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NavigationState is taking a process network assertion because a page load started", this);
            m_networkActivity = m_webView->_page->process().throttler().backgroundActivity("Page Load"_s).moveToUniquePtr();
        }
    } else if (m_networkActivity) {
        // The application is visible so we delay releasing the background activity for 3 seconds to give it a chance to start another navigation
        // before suspending the WebContent process <rdar://problem/27910964>.
        RELEASE_LOG_IF(m_webView->_page->isAlwaysOnLoggingAllowed(), ProcessSuspension, "%p - NavigationState will release its process network assertion soon because the page load completed", this);
        m_releaseNetworkActivityTimer.startOneShot(3_s);
    }
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

void NavigationState::willChangeNegotiatedLegacyTLS()
{
    [m_webView willChangeValueForKey:@"_negotiatedLegacyTLS"];
}

void NavigationState::didChangeNegotiatedLegacyTLS()
{
    [m_webView didChangeValueForKey:@"_negotiatedLegacyTLS"];
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

void NavigationState::willChangeCertificateInfo()
{
    [m_webView willChangeValueForKey:@"serverTrust"];
    [m_webView willChangeValueForKey:@"certificateChain"];
}

void NavigationState::didChangeCertificateInfo()
{
    [m_webView didChangeValueForKey:@"certificateChain"];
    [m_webView didChangeValueForKey:@"serverTrust"];
}

void NavigationState::willChangeWebProcessIsResponsive()
{
    [m_webView willChangeValueForKey:@"_webProcessIsResponsive"];
}

void NavigationState::didChangeWebProcessIsResponsive()
{
    [m_webView didChangeValueForKey:@"_webProcessIsResponsive"];
}

void NavigationState::didSwapWebProcesses()
{
#if PLATFORM(IOS_FAMILY)
    // Transfer our background assertion from the old process to the new one.
    if (m_networkActivity)
        m_networkActivity = m_webView->_page->process().throttler().backgroundActivity("Page Load"_s).moveToUniquePtr();
#endif
}

} // namespace WebKit
