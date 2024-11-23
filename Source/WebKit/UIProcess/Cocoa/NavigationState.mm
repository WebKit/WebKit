/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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
#import "WKDownloadInternal.h"
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
#import "_WKPageLoadTimingInternal.h"
#import "_WKRenderingProgressEventsInternal.h"
#import "_WKSameDocumentNavigationTypeInternal.h"
#import <JavaScriptCore/ConsoleTypes.h>
#import <WebCore/AuthenticationMac.h>
#import <WebCore/ContentRuleListResults.h>
#import <WebCore/Credential.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/SerializedCryptoKeyWrap.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Scope.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/WeakHashMap.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

#if ENABLE(WK_WEB_EXTENSIONS)
#import "WKWebViewConfigurationPrivate.h"
#import "WebExtensionController.h"
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#import <pal/ios/ManagedConfigurationSoftLink.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#endif

#if HAVE(APP_LINKS)
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#endif

#if HAVE(MARKETPLACE_KIT)
#import "WebKitSwiftSoftLink.h"

SOFT_LINK_CLASS_FOR_HEADER(WebKit, WKMarketplaceKit)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKMarketplaceKit)

@interface WKMarketplaceKit : NSObject
+ (void)requestAppInstallationWithTopOrigin:(NSURL *)topOrigin url:(NSURL *)url completionHandler:(void (^)(NSError *))completionHandler;
@end
#endif

namespace WebKit {
using namespace WebCore;

static WeakHashMap<WebPageProxy, WeakPtr<NavigationState>>& navigationStates()
{
    static NeverDestroyed<WeakHashMap<WebPageProxy, WeakPtr<NavigationState>>> navigationStates;

    return navigationStates;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(NavigationState);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(NavigationState, HistoryClient);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(NavigationState, NavigationClient);

NavigationState::NavigationState(WKWebView *webView)
    : m_webView(webView)
    , m_navigationDelegateMethods()
    , m_historyDelegateMethods()
#if USE(RUNNINGBOARD)
    , m_releaseNetworkActivityTimer(RunLoop::current(), this, &NavigationState::releaseNetworkActivityAfterLoadCompletion)
#endif
{
    RefPtr page = webView->_page;
    ASSERT(page);
    ASSERT(!navigationStates().contains(*page));

    navigationStates().add(*page, *this);
    page->pageLoadState().addObserver(*this);
}

NavigationState::~NavigationState()
{
    if (auto webView = this->webView()) {
        RefPtr page = webView->_page;
        ASSERT(navigationStates().get(*page).get() == this);

        navigationStates().remove(*page);
        page->pageLoadState().removeObserver(*this);
    }
}

void NavigationState::ref() const
{
    [m_webView.get() retain];
}

void NavigationState::deref() const
{
    [m_webView.get() release];
}

NavigationState* NavigationState::fromWebPage(WebPageProxy& webPageProxy)
{
    ASSERT(navigationStates().contains(webPageProxy));

    return navigationStates().get(webPageProxy).get();
}

UniqueRef<API::NavigationClient> NavigationState::createNavigationClient()
{
    return makeUniqueRef<NavigationClient>(*this);
}
    
UniqueRef<API::HistoryClient> NavigationState::createHistoryClient()
{
    return makeUniqueRef<HistoryClient>(*this);
}

RetainPtr<id<WKNavigationDelegate>> NavigationState::navigationDelegate() const
{
    return m_navigationDelegate.get();
}

void NavigationState::setNavigationDelegate(id<WKNavigationDelegate> delegate)
{
    m_navigationDelegate = delegate;

    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationAction:preferences:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler = [delegate respondsToSelector:@selector(_webView:decidePolicyForNavigationAction:preferences:userInfo:decisionHandler:)];
    m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler = [delegate respondsToSelector:@selector(webView:decidePolicyForNavigationResponse:decisionHandler:)];

    m_navigationDelegateMethods.webViewDidStartProvisionalNavigation = [delegate respondsToSelector:@selector(webView:didStartProvisionalNavigation:)];
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
    m_navigationDelegateMethods.webViewDidFailLoadDueToNetworkConnectionIntegrityWithURL = [delegate respondsToSelector:@selector(_webView:didFailLoadDueToNetworkConnectionIntegrityWithURL:)];
    m_navigationDelegateMethods.webViewDidChangeLookalikeCharactersFromURLToURL = [delegate respondsToSelector:@selector(_webView:didChangeLookalikeCharactersFromURL:toURL:)];
    m_navigationDelegateMethods.webViewDidPromptForStorageAccessForSubFrameDomainForQuirk = [delegate respondsToSelector:@selector(_webView:didPromptForStorageAccess:forSubFrameDomain:forQuirk:)];
    
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
    m_navigationDelegateMethods.webViewDidNegotiateModernTLS = [delegate respondsToSelector:@selector(_webView:didNegotiateModernTLSForURL:)];
    m_navigationDelegateMethods.webViewWebContentProcessDidTerminate = [delegate respondsToSelector:@selector(webViewWebContentProcessDidTerminate:)];
    m_navigationDelegateMethods.webViewWebContentProcessDidTerminateWithReason = [delegate respondsToSelector:@selector(_webView:webContentProcessDidTerminateWithReason:)];
    m_navigationDelegateMethods.webViewWebProcessDidCrash = [delegate respondsToSelector:@selector(_webViewWebProcessDidCrash:)];
    m_navigationDelegateMethods.webViewWebProcessDidBecomeResponsive = [delegate respondsToSelector:@selector(_webViewWebProcessDidBecomeResponsive:)];
    m_navigationDelegateMethods.webViewWebProcessDidBecomeUnresponsive = [delegate respondsToSelector:@selector(_webViewWebProcessDidBecomeUnresponsive:)];
    m_navigationDelegateMethods.webCryptoMasterKeyForWebView = [delegate respondsToSelector:@selector(_webCryptoMasterKeyForWebView:)];
    m_navigationDelegateMethods.webCryptoMasterKeyForWebViewCompletionHandler = [delegate respondsToSelector:@selector(_webCryptoMasterKeyForWebView:completionHandler:)];
    m_navigationDelegateMethods.navigationActionDidBecomeDownload = [delegate respondsToSelector:@selector(webView:navigationAction:didBecomeDownload:)];
    m_navigationDelegateMethods.navigationResponseDidBecomeDownload = [delegate respondsToSelector:@selector(webView:navigationResponse:didBecomeDownload:)];
    m_navigationDelegateMethods.contextMenuDidCreateDownload = [delegate respondsToSelector:@selector(_webView:contextMenuDidCreateDownload:)];
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
#endif
#if PLATFORM(IOS_FAMILY)
    m_navigationDelegateMethods.webViewDidRequestPasswordForQuickLookDocument = [delegate respondsToSelector:@selector(_webViewDidRequestPasswordForQuickLookDocument:)];
    m_navigationDelegateMethods.webViewDidStopRequestingPasswordForQuickLookDocument = [delegate respondsToSelector:@selector(_webViewDidStopRequestingPasswordForQuickLookDocument:)];
#endif
#if PLATFORM(MAC)
    m_navigationDelegateMethods.webViewBackForwardListItemAddedRemoved = [delegate respondsToSelector:@selector(_webView:backForwardListItemAdded:removed:)];
#endif
    m_navigationDelegateMethods.webViewWillGoToBackForwardListItemInBackForwardCache = [delegate respondsToSelector:@selector(_webView:willGoToBackForwardListItem:inPageCache:)];
#if HAVE(APP_SSO)
    m_navigationDelegateMethods.webViewDecidePolicyForSOAuthorizationLoadWithCurrentPolicyForExtensionCompletionHandler = [delegate respondsToSelector:@selector(_webView:decidePolicyForSOAuthorizationLoadWithCurrentPolicy:forExtension:completionHandler:)];
#endif
    m_navigationDelegateMethods.webViewDidGeneratePageLoadTiming = [delegate respondsToSelector:@selector(_webView:didGeneratePageLoadTiming:)];
}

RetainPtr<id<WKHistoryDelegatePrivate>> NavigationState::historyDelegate() const
{
    return m_historyDelegate.get();
}

void NavigationState::setHistoryDelegate(id<WKHistoryDelegatePrivate> historyDelegate)
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

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidBeginNavigationGesture:webView().get()];
}

void NavigationState::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewWillEndNavigationGestureWithNavigationToBackForwardListItem)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewWillEndNavigationGesture:webView().get() withNavigationToBackForwardListItem:willNavigate ? wrapper(item) : nil];
}

void NavigationState::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewDidEndNavigationGestureWithNavigationToBackForwardListItem)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidEndNavigationGesture:webView().get() withNavigationToBackForwardListItem:willNavigate ? wrapper(item) : nil];
}

void NavigationState::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (!m_navigationDelegateMethods.webViewWillSnapshotBackForwardListItem)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:webView().get() willSnapshotBackForwardListItem:wrapper(item)];
}

void NavigationState::navigationGestureSnapshotWasRemoved()
{
    if (!m_navigationDelegateMethods.webViewNavigationGestureSnapshotWasRemoved)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidRemoveNavigationGestureSnapshot:webView().get()];
}

#if PLATFORM(IOS_FAMILY)
void NavigationState::didRequestPasswordForQuickLookDocument()
{
    if (!m_navigationDelegateMethods.webViewDidRequestPasswordForQuickLookDocument)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidRequestPasswordForQuickLookDocument:webView().get()];
}

void NavigationState::didStopRequestingPasswordForQuickLookDocument()
{
    if (!m_navigationDelegateMethods.webViewDidStopRequestingPasswordForQuickLookDocument)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidStopRequestingPasswordForQuickLookDocument:webView().get()];
}
#endif

void NavigationState::didFirstPaint()
{
    if (!m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = this->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:webView().get() renderingProgressDidChange:_WKRenderingProgressEventFirstPaint];
}

NavigationState::NavigationClient::NavigationClient(NavigationState& navigationState)
    : m_navigationState(navigationState)
{
}

NavigationState::NavigationClient::~NavigationClient()
{
}

#if PLATFORM(MAC)
bool NavigationState::NavigationClient::didChangeBackForwardList(WebPageProxy&, WebBackForwardListItem* added, const Vector<Ref<WebBackForwardListItem>>& removed)
{
    if (!m_navigationState)
        return false;

    if (!m_navigationState->m_navigationDelegateMethods.webViewBackForwardListItemAddedRemoved)
        return false;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return false;

    RetainPtr<NSArray<WKBackForwardListItem *>> removedItems;
    if (!removed.isEmpty()) {
        removedItems = createNSArray(removed, [] (auto& removedItem) {
            return wrapper(removedItem.get());
        });
    }
    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() backForwardListItemAdded:wrapper(added) removed:removedItems.get()];
    return true;
}
#endif

bool NavigationState::NavigationClient::willGoToBackForwardListItem(WebPageProxy&, WebBackForwardListItem& item, bool inBackForwardCache)
{
    if (!m_navigationState)
        return false;

    if (!m_navigationState->m_navigationDelegateMethods.webViewWillGoToBackForwardListItemInBackForwardCache)
        return false;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return false;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() willGoToBackForwardListItem:wrapper(item) inPageCache:inBackForwardCache];
    return true;
}

static void trySOAuthorization(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Function<void(bool)>&& completionHandler)
{
#if HAVE(APP_SSO)
    if (!navigationAction->shouldPerformSOAuthorization()) {
        completionHandler(false);
        return;
    }
    page.websiteDataStore().soAuthorizationCoordinator(page).tryAuthorize(WTFMove(navigationAction), page, WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

#if HAVE(MARKETPLACE_KIT)

static bool isMarketplaceKitURL(const URL& url)
{
    return url.protocolIs("app-distribution"_s) || url.protocolIs("marketplace-kit"_s);
}

static void interceptMarketplaceKitNavigation(Ref<API::NavigationAction>&& action, WebPageProxy& page)
{
    std::optional<FrameIdentifier> sourceFrameID;
    if (auto sourceFrame = action->sourceFrame())
        sourceFrameID = sourceFrame->handle()->frameID();

    auto addConsoleError = [sourceFrameID, url = action->request().url(), weakPage = WeakPtr { page }](const String& error) {
        if (!sourceFrameID || !weakPage)
            return;

        weakPage->addConsoleMessage(*sourceFrameID, MessageSource::Network, MessageLevel::Error, makeString("Can't handle MarketplaceKit link "_s, url.string(), " due to error: "_s, error));
    };

    if (!action->shouldOpenExternalSchemes() || !action->isProcessingUserGesture() || action->isRedirect() || action->data().requesterTopOrigin.isNull()) {
        RELEASE_LOG_ERROR(Loading, "NavigationState: can't handle MarketplaceKit navigation with shouldOpenExternalSchemes: %d, isProcessingUserGesture: %d, isRedirect: %d, requesterTopOriginIsNull: %d", action->shouldOpenExternalSchemes(), action->isProcessingUserGesture(), action->isRedirect(), action->data().requesterTopOrigin.isNull());

        if (!action->isProcessingUserGesture())
            addConsoleError("must be activated via a user gesture"_s);
        else if (action->isRedirect())
            addConsoleError("must be a direct link without a redirect"_s);

        return;
    }

    RetainPtr<NSURL> requesterTopOriginURL = static_cast<NSURL *>(action->data().requesterTopOrigin.toURL());
    RetainPtr<NSURL> url = static_cast<NSURL *>(action->request().url());

    if (!requesterTopOriginURL || !url) {
        RELEASE_LOG_ERROR(Loading, "NavigationState: can't handle MarketplaceKit navigation with requesterTopOriginURL: %d url: %d", static_cast<bool>(requesterTopOriginURL), static_cast<bool>(url));
        return;
    }

    if (![getWKMarketplaceKitClass() respondsToSelector:@selector(requestAppInstallationWithTopOrigin:url:completionHandler:)])
        return;

    [getWKMarketplaceKitClass() requestAppInstallationWithTopOrigin:requesterTopOriginURL.get() url:url.get() completionHandler:makeBlockPtr([addConsoleError = WTFMove(addConsoleError)](NSError *error) mutable {
        if (error)
            addConsoleError(error.description);
    }).get()];
}

#endif // HAVE(MARKETPLACE_KIT)

static void tryInterceptNavigation(Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, WTF::Function<void(bool)>&& completionHandler)
{
#if HAVE(MARKETPLACE_KIT)
    if (isMarketplaceKitURL(navigationAction->request().url())) {
        interceptMarketplaceKitNavigation(WTFMove(navigationAction), page);
        return completionHandler(true /* interceptedNavigation */);
    }
#endif

#if HAVE(APP_LINKS)
    if (navigationAction->shouldOpenAppLinks()) {
        auto url = navigationAction->request().url();

        NSURL *referrerURL = nil;
        auto request = navigationAction->request();
        request.setExistingHTTPReferrerToOriginString();
        auto referrerString = request.httpReferrer();
        if (!referrerString.isEmpty())
            referrerURL = (NSURL *)URL { referrerString };

        auto* localCompletionHandler = new WTF::Function<void (bool)>([navigationAction = WTFMove(navigationAction), weakPage = WeakPtr { page }, completionHandler = WTFMove(completionHandler)] (bool success) mutable {
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

        RetainPtr<_LSOpenConfiguration> configuration = adoptNS([[_LSOpenConfiguration alloc] init]);
        configuration.get().referrerURL = referrerURL;

        [LSAppLink openWithURL:url configuration:configuration.get() completionHandler:[localCompletionHandler](BOOL success, NSError *) {
            RunLoop::main().dispatch([localCompletionHandler, success] {
                (*localCompletionHandler)(success);
                delete localCompletionHandler;
            });
        }];
        return;
    }
#endif

    trySOAuthorization(WTFMove(navigationAction), page, WTFMove(completionHandler));
}

#if ENABLE(WK_WEB_EXTENSIONS)
static bool isUnsupportedWebExtensionNavigation(API::NavigationAction& navigationAction, WebPageProxy& page)
{
    bool subframeNavigation = navigationAction.targetFrame() && !navigationAction.targetFrame()->isMainFrame();
    if (subframeNavigation)
        return false;

    auto *requiredBaseURL = page.cocoaView().get()._requiredWebExtensionBaseURL;
    if (!requiredBaseURL || navigationAction.shouldPerformDownload())
        return false;

    if (RefPtr extensionController = page.webExtensionController()) {
        auto extensionContext = extensionController->extensionContext(navigationAction.originalURL());
        if (!extensionContext || !extensionContext->isURLForThisExtension(requiredBaseURL))
            return true;
    }

    return false;
}
#endif // ENABLE(WK_WEB_EXTENSIONS)

void NavigationState::NavigationClient::decidePolicyForNavigationAction(WebPageProxy& webPageProxy, Ref<API::NavigationAction>&& navigationAction, Ref<WebFramePolicyListenerProxy>&& listener)
{
    bool subframeNavigation = navigationAction->targetFrame() && !navigationAction->targetFrame()->isMainFrame();

    RefPtr<API::WebsitePolicies> defaultWebsitePolicies = webPageProxy.configuration().defaultWebsitePolicies().copy();

    if (!m_navigationState || (!m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionDecisionHandler
        && !m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler
        && !m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler)) {
        auto completionHandler = [webPage = Ref { webPageProxy }, listener = WTFMove(listener), navigationAction, defaultWebsitePolicies] (bool interceptedNavigation) {
            if (interceptedNavigation) {
                listener->ignore(WasNavigationIntercepted::Yes);
                return;
            }

#if ENABLE(WK_WEB_EXTENSIONS)
            if (isUnsupportedWebExtensionNavigation(navigationAction, webPage)) {
                RELEASE_LOG_DEBUG(Extensions, "Ignoring unsupported web extension navigation");
                listener->ignore();
                return;
            }

            if (RefPtr extensionController = webPage->webExtensionController())
                extensionController->updateWebsitePoliciesForNavigation(*defaultWebsitePolicies, navigationAction);
#endif

            if (!navigationAction->targetFrame()) {
                listener->use(defaultWebsitePolicies.get());
                return;
            }

            auto nsURLRequest = wrapper(API::URLRequest::create(navigationAction->request()));
            if ([NSURLConnection canHandleRequest:nsURLRequest.get()]
                || webPage->urlSchemeHandlerForScheme(nsURLRequest.get().URL.scheme)
                || [nsURLRequest.get().URL.scheme isEqualToString:@"blob"]) {
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

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    bool delegateHasWebpagePreferences = m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler
        || m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler;

    auto selectorForCompletionHandlerChecker = ([&] () -> SEL {
        if (delegateHasWebpagePreferences)
            return m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler ? @selector(webView:decidePolicyForNavigationAction:preferences:decisionHandler:) : @selector(_webView:decidePolicyForNavigationAction:preferences:userInfo:decisionHandler:);
        return @selector(webView:decidePolicyForNavigationAction:decisionHandler:);
    })();

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), selectorForCompletionHandlerChecker);
    auto decisionHandlerWithPreferencesOrPolicies = [localListener = WTFMove(listener), navigationAction, checker = WTFMove(checker), webPageProxy = Ref { webPageProxy }, subframeNavigation, defaultWebsitePolicies] (WKNavigationActionPolicy actionPolicy, WKWebpagePreferences *preferences) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();

        RefPtr<API::WebsitePolicies> apiWebsitePolicies = preferences ?  preferences->_websitePolicies.get() : defaultWebsitePolicies;

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

        ensureOnMainRunLoop([navigationAction = WTFMove(navigationAction), webPageProxy = WTFMove(webPageProxy), actionPolicy, localListener = WTFMove(localListener), apiWebsitePolicies = WTFMove(apiWebsitePolicies)] () mutable {
#if ENABLE(WK_WEB_EXTENSIONS)
            if (actionPolicy != WKNavigationActionPolicyCancel && isUnsupportedWebExtensionNavigation(navigationAction, webPageProxy)) {
                RELEASE_LOG_DEBUG(Extensions, "Ignoring unsupported web extension navigation");
                localListener->ignore();
                return;
            }

            if (RefPtr extensionController = webPageProxy->webExtensionController())
                extensionController->updateWebsitePoliciesForNavigation(*apiWebsitePolicies, navigationAction);
#endif

            switch (actionPolicy) {
            case WKNavigationActionPolicyAllow:
            case _WKNavigationActionPolicyAllowInNewProcess:
                tryInterceptNavigation(WTFMove(navigationAction), webPageProxy, [actionPolicy, localListener = WTFMove(localListener), websitePolicies = WTFMove(apiWebsitePolicies)](bool interceptedNavigation) mutable {
                    if (interceptedNavigation) {
                        localListener->ignore(WasNavigationIntercepted::Yes);
                        return;
                    }

                    localListener->use(websitePolicies.get(), actionPolicy == _WKNavigationActionPolicyAllowInNewProcess ? ProcessSwapRequestedByClient::Yes : ProcessSwapRequestedByClient::No);
                });
                break;

            case WKNavigationActionPolicyCancel:
                localListener->ignore();
                break;

            case WKNavigationActionPolicyDownload:
                localListener->download();
                break;

            case _WKNavigationActionPolicyAllowWithoutTryingAppLink:
#if HAVE(MARKETPLACE_KIT)
                if (isMarketplaceKitURL(navigationAction->request().url())) {
                    interceptMarketplaceKitNavigation(WTFMove(navigationAction), webPageProxy);
                    localListener->ignore();
                    return;
                }
#endif

                trySOAuthorization(WTFMove(navigationAction), webPageProxy, [localListener = WTFMove(localListener), websitePolicies = WTFMove(apiWebsitePolicies)] (bool optimizedLoad) {
                    if (optimizedLoad) {
                        localListener->ignore(WasNavigationIntercepted::Yes);
                        return;
                    }

                    localListener->use(websitePolicies.get());
                });
                break;
            }
        });
    };

    if (delegateHasWebpagePreferences) {
        if (m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler)
            [navigationDelegate webView:m_navigationState->webView().get() decidePolicyForNavigationAction:wrapper(navigationAction) preferences:wrapper(defaultWebsitePolicies) decisionHandler:makeBlockPtr(WTFMove(decisionHandlerWithPreferencesOrPolicies)).get()];
        else
            [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() decidePolicyForNavigationAction:wrapper(navigationAction) preferences:wrapper(defaultWebsitePolicies) userInfo:nil decisionHandler:makeBlockPtr(WTFMove(decisionHandlerWithPreferencesOrPolicies)).get()];
    } else {
        auto decisionHandler = [decisionHandlerWithPreferencesOrPolicies = WTFMove(decisionHandlerWithPreferencesOrPolicies)] (WKNavigationActionPolicy actionPolicy) mutable {
            decisionHandlerWithPreferencesOrPolicies(actionPolicy, nil);
        };
        [navigationDelegate webView:m_navigationState->webView().get() decidePolicyForNavigationAction:wrapper(navigationAction) decisionHandler:makeBlockPtr(WTFMove(decisionHandler)).get()];
    }
}

#if ENABLE(CONTENT_EXTENSIONS)
void NavigationState::NavigationClient::contentRuleListNotification(WebPageProxy& page, URL&& url, ContentRuleListResults&& results)
{
    if (!m_navigationState)
        return;

#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr extensionController = page.webExtensionController())
        extensionController->handleContentRuleListNotification(page.identifier(), url, results);
#endif

    if (!m_navigationState->m_navigationDelegateMethods.webViewURLContentRuleListIdentifiersNotifications
        && !m_navigationState->m_navigationDelegateMethods.webViewContentRuleListWithIdentifierPerformedActionForURL)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
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

    if (notifications && m_navigationState->m_navigationDelegateMethods.webViewURLContentRuleListIdentifiersNotifications)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() URL:url contentRuleListIdentifiers:identifiers.get() notifications:notifications.get()];

    if (m_navigationState->m_navigationDelegateMethods.webViewContentRuleListWithIdentifierPerformedActionForURL) {
        for (auto&& pair : WTFMove(results.results))
            [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() contentRuleListWithIdentifier:pair.first performedAction:wrapper(API::ContentRuleListAction::create(WTFMove(pair.second)).get()) forURL:url];
    }
}
#endif
    
void NavigationState::NavigationClient::decidePolicyForNavigationResponse(WebPageProxy& page, Ref<API::NavigationResponse>&& navigationResponse, Ref<WebFramePolicyListenerProxy>&& listener)
{
    if (!m_navigationState || !m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationResponseDecisionHandler) {
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

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:decidePolicyForNavigationResponse:decisionHandler:));
    [navigationDelegate webView:m_navigationState->webView().get() decidePolicyForNavigationResponse:wrapper(navigationResponse) decisionHandler:makeBlockPtr([localListener = WTFMove(listener), checker = WTFMove(checker)](WKNavigationResponsePolicy responsePolicy) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        ensureOnMainRunLoop([responsePolicy, localListener = WTFMove(localListener)] {
            switch (responsePolicy) {
            case WKNavigationResponsePolicyAllow:
                localListener->use();
                break;

            case WKNavigationResponsePolicyCancel:
                localListener->ignore();
                break;

            case WKNavigationResponsePolicyDownload:
                localListener->download();
                break;
            }
        });
    }).get()];
}

void NavigationState::NavigationClient::didStartProvisionalNavigation(WebPageProxy&, const WebCore::ResourceRequest& request, API::Navigation* navigation, API::Object* userInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState->m_navigationDelegateMethods.webViewDidStartProvisionalNavigation)
        [navigationDelegate webView:m_navigationState->webView().get() didStartProvisionalNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::didStartProvisionalLoadForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, FrameInfoData&& frameInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    if (m_navigationState->m_navigationDelegateMethods.webViewDidStartProvisionalLoadWithRequestInFrame)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didStartProvisionalLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get()];
}

void NavigationState::NavigationClient::didReceiveServerRedirectForProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidReceiveServerRedirectForProvisionalNavigation)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.

    [navigationDelegate webView:m_navigationState->webView().get() didReceiveServerRedirectForProvisionalNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::willPerformClientRedirect(WebPageProxy& page, const WTF::String& urlString, double delay)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewWillPerformClientRedirect)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    URL url { urlString };

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() willPerformClientRedirectToURL:url delay:delay];
}

void NavigationState::NavigationClient::didPerformClientRedirect(WebPageProxy& page, const WTF::String& sourceURLString, const WTF::String& destinationURLString)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidPerformClientRedirect)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    URL sourceURL { sourceURLString };
    URL destinationURL { destinationURLString };

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didPerformClientRedirectFromURL:sourceURL toURL:destinationURL];
}

void NavigationState::NavigationClient::didCancelClientRedirect(WebPageProxy& page)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidCancelClientRedirect)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewDidCancelClientRedirect:m_navigationState->webView().get()];
}

static RetainPtr<NSError> createErrorWithRecoveryAttempter(WKWebView *webView, const FrameInfoData& frameInfo, NSError *originalError, const URL& url)
{
    auto frameHandle = API::FrameHandle::create(frameInfo.frameID);
    auto recoveryAttempter = adoptNS([[WKReloadFrameErrorRecoveryAttempter alloc] initWithWebView:webView frameHandle:wrapper(frameHandle.get()) urlString:url.string()]);

    auto userInfo = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys:recoveryAttempter.get(), _WKRecoveryAttempterErrorKey, nil]);

    if (NSDictionary *originalUserInfo = originalError.userInfo)
        [userInfo addEntriesFromDictionary:originalUserInfo];

    return adoptNS([[NSError alloc] initWithDomain:originalError.domain code:originalError.code userInfo:userInfo.get()]);
}

void NavigationState::NavigationClient::didFailProvisionalNavigationWithError(WebPageProxy& page, FrameInfoData&& frameInfo, API::Navigation* navigation, const URL& url, const WebCore::ResourceError& error, API::Object*)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState->webView().get(), frameInfo, error, url);

    if (frameInfo.isMainFrame) {
        // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
        if (m_navigationState->m_navigationDelegateMethods.webViewDidFailProvisionalNavigationWithError)
            [navigationDelegate webView:m_navigationState->webView().get() didFailProvisionalNavigation:wrapper(navigation) withError:errorWithRecoveryAttempter.get()];
    } else {
        if (m_navigationState->m_navigationDelegateMethods.webViewNavigationDidFailProvisionalLoadInSubframeWithError)
            [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() navigation:nil didFailProvisionalLoadInSubframe:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() withError:errorWithRecoveryAttempter.get()];
    }
}

void NavigationState::NavigationClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, const WebCore::ResourceError& error, FrameInfoData&& frameInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState->webView().get(), frameInfo, error, request.url());

    if (m_navigationState->m_navigationDelegateMethods.webViewDidFailProvisionalLoadWithRequestInFrameWithError)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didFailProvisionalLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didCommitNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState->m_navigationDelegateMethods.webViewDidCommitNavigation)
        [navigationDelegate webView:m_navigationState->webView().get() didCommitNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::didCommitLoadForFrame(WebKit::WebPageProxy& page, WebCore::ResourceRequest&& request, FrameInfoData&& frameInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    if (m_navigationState->m_navigationDelegateMethods.webViewDidCommitLoadWithRequestInFrame)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didCommitLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get()];
}

void NavigationState::NavigationClient::didFinishDocumentLoad(WebPageProxy& page, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewNavigationDidFinishDocumentLoad)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() navigationDidFinishDocumentLoad:wrapper(navigation)];
}

void NavigationState::NavigationClient::didFinishNavigation(WebPageProxy&, API::Navigation* navigation, API::Object*)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState->m_navigationDelegateMethods.webViewDidFinishNavigation)
        [navigationDelegate webView:m_navigationState->webView().get() didFinishNavigation:wrapper(navigation)];
}

void NavigationState::NavigationClient::didFinishLoadForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, FrameInfoData&& frameInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    if (m_navigationState->m_navigationDelegateMethods.webViewDidFinishLoadWithRequestInFrame)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didFinishLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get()];
}

void NavigationState::NavigationClient::didBlockLoadToKnownTracker(WebPageProxy& page, const URL& url)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    if (m_navigationState->m_navigationDelegateMethods.webViewDidFailLoadDueToNetworkConnectionIntegrityWithURL)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didFailLoadDueToNetworkConnectionIntegrityWithURL:url];
}

void NavigationState::NavigationClient::didApplyLinkDecorationFiltering(WebPageProxy& page, const URL& originalURL, const URL& adjustedURL)
{
    if (!m_navigationState)
        return;
    
    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    if (m_navigationState->m_navigationDelegateMethods.webViewDidChangeLookalikeCharactersFromURLToURL)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didChangeLookalikeCharactersFromURL:originalURL toURL:adjustedURL];
}

void NavigationState::NavigationClient::didPromptForStorageAccess(WebPageProxy&, const String& topFrameDomain, const String& subFrameDomain, bool hasQuirk)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    if (m_navigationState->m_navigationDelegateMethods.webViewDidPromptForStorageAccessForSubFrameDomainForQuirk)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didPromptForStorageAccess:topFrameDomain forSubFrameDomain:subFrameDomain forQuirk:hasQuirk];
}

void NavigationState::NavigationClient::didFailNavigationWithError(WebPageProxy& page, const FrameInfoData& frameInfo, API::Navigation* navigation, const URL& url, const WebCore::ResourceError& error, API::Object* userInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState->webView().get(), frameInfo, error, url);

    // FIXME: We should assert that navigation is not null here, but it's currently null for some navigations through the back/forward cache.
    if (m_navigationState->m_navigationDelegateMethods.webViewDidFailNavigationWithErrorUserInfo)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didFailNavigation:wrapper(navigation) withError:errorWithRecoveryAttempter.get() userInfo:userInfo ? static_cast<id<NSSecureCoding>>(userInfo->wrapper()) : nil];
    else if (m_navigationState->m_navigationDelegateMethods.webViewDidFailNavigationWithError)
        [navigationDelegate webView:m_navigationState->webView().get() didFailNavigation:wrapper(navigation) withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didFailLoadWithErrorForFrame(WebPageProxy& page, WebCore::ResourceRequest&& request, const WebCore::ResourceError& error, FrameInfoData&& frameInfo)
{
    if (!m_navigationState)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    auto errorWithRecoveryAttempter = createErrorWithRecoveryAttempter(m_navigationState->webView().get(), frameInfo, error, request.url());

    if (m_navigationState->m_navigationDelegateMethods.webViewDidFailLoadWithRequestInFrameWithError)
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didFailLoadWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody) inFrame:wrapper(API::FrameInfo::create(WTFMove(frameInfo), &page)).get() withError:errorWithRecoveryAttempter.get()];
}

void NavigationState::NavigationClient::didSameDocumentNavigation(WebPageProxy&, API::Navigation* navigation, SameDocumentNavigationType navigationType, API::Object*)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewNavigationDidSameDocumentNavigation)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() navigation:wrapper(navigation) didSameDocumentNavigation:toWKSameDocumentNavigationType(navigationType)];
}

void NavigationState::NavigationClient::renderingProgressDidChange(WebPageProxy&, OptionSet<WebCore::LayoutMilestone> layoutMilestones)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewRenderingProgressDidChange)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() renderingProgressDidChange:renderingProgressEvents(layoutMilestones)];
}

bool NavigationState::NavigationClient::shouldBypassContentModeSafeguards() const
{
    if (!m_navigationState)
        return false;

    return m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesDecisionHandler
        || m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForNavigationActionWithPreferencesUserInfoDecisionHandler;
}

void NavigationState::NavigationClient::didReceiveAuthenticationChallenge(WebPageProxy&, AuthenticationChallengeProxy& authenticationChallenge)
{
    if (!m_navigationState)
        return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidReceiveAuthenticationChallengeCompletionHandler)
        return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return authenticationChallenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::RejectProtectionSpaceAndContinue);

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:didReceiveAuthenticationChallenge:completionHandler:));
    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) webView:m_navigationState->webView().get() didReceiveAuthenticationChallenge:wrapper(authenticationChallenge) completionHandler:makeBlockPtr([challenge = Ref { authenticationChallenge }, checker = WTFMove(checker)](NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential) {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        challenge->listener().completeChallenge(WebKit::toAuthenticationChallengeDisposition(disposition), Credential(credential));
    }).get()];
}

void NavigationState::NavigationClient::shouldAllowLegacyTLS(WebPageProxy& page, AuthenticationChallengeProxy& authenticationChallenge, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_navigationState)
        return completionHandler(page.websiteDataStore().configuration().legacyTLSEnabled());

    if (!m_navigationState->m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowLegacyTLS
        && !m_navigationState->m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowDeprecatedTLS)
        return completionHandler(page.websiteDataStore().configuration().legacyTLSEnabled());

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return completionHandler(page.websiteDataStore().configuration().legacyTLSEnabled());

    if (m_navigationState->m_navigationDelegateMethods.webViewAuthenticationChallengeShouldAllowDeprecatedTLS) {
        auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(webView:authenticationChallenge:shouldAllowDeprecatedTLS:));
        [navigationDelegate webView:m_navigationState->webView().get() authenticationChallenge:wrapper(authenticationChallenge) shouldAllowDeprecatedTLS:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)](BOOL shouldAllow) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionHandler(shouldAllow);
        }).get()];
        return;
    }
    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:authenticationChallenge:shouldAllowLegacyTLS:));
    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() authenticationChallenge:wrapper(authenticationChallenge) shouldAllowLegacyTLS:makeBlockPtr([checker = WTFMove(checker), completionHandler = WTFMove(completionHandler)](BOOL shouldAllow) mutable {
        if (checker->completionHandlerHasBeenCalled())
            return;
        checker->didCallCompletionHandler();
        completionHandler(shouldAllow);
    }).get()];
}

void NavigationState::NavigationClient::didNegotiateModernTLS(const URL& url)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidNegotiateModernTLS)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didNegotiateModernTLSForURL:url];
}

static _WKProcessTerminationReason wkProcessTerminationReason(ProcessTerminationReason reason)
{
    switch (reason) {
    case ProcessTerminationReason::ExceededMemoryLimit:
        return _WKProcessTerminationReasonExceededMemoryLimit;
    case ProcessTerminationReason::ExceededCPULimit:
        return _WKProcessTerminationReasonExceededCPULimit;
    case ProcessTerminationReason::NavigationSwap:
    case ProcessTerminationReason::IdleExit:
        // We probably shouldn't bother coming up with a new API type for process-swapping.
        // "Requested by client" seems like the best match for existing types.
        FALLTHROUGH;
    case ProcessTerminationReason::RequestedByClient:
        return _WKProcessTerminationReasonRequestedByClient;
    case ProcessTerminationReason::ExceededProcessCountLimit:
    case ProcessTerminationReason::Unresponsive:
    case ProcessTerminationReason::RequestedByNetworkProcess:
    case ProcessTerminationReason::RequestedByGPUProcess:
    case ProcessTerminationReason::RequestedByModelProcess:
    case ProcessTerminationReason::Crash:
        return _WKProcessTerminationReasonCrash;
    case ProcessTerminationReason::GPUProcessCrashedTooManyTimes:
    case ProcessTerminationReason::ModelProcessCrashedTooManyTimes:
        return _WKProcessTerminationReasonExceededSharedProcessCrashLimit;
    }
    ASSERT_NOT_REACHED();
    return _WKProcessTerminationReasonCrash;
}

bool NavigationState::NavigationClient::processDidTerminate(WebPageProxy& page, ProcessTerminationReason reason)
{
    if (!m_navigationState)
        return false;

    if (!m_navigationState->m_navigationDelegateMethods.webViewWebContentProcessDidTerminate
        && !m_navigationState->m_navigationDelegateMethods.webViewWebContentProcessDidTerminateWithReason
        && !m_navigationState->m_navigationDelegateMethods.webViewWebProcessDidCrash)
        return false;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return false;

    if (m_navigationState->m_navigationDelegateMethods.webViewWebContentProcessDidTerminateWithReason) {
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() webContentProcessDidTerminateWithReason:wkProcessTerminationReason(reason)];
        return true;
    }

    // We prefer webViewWebContentProcessDidTerminate: over _webViewWebProcessDidCrash:.
    if (m_navigationState->m_navigationDelegateMethods.webViewWebContentProcessDidTerminate) {
        [navigationDelegate webViewWebContentProcessDidTerminate:m_navigationState->webView().get()];
        return true;
    }

    ASSERT(m_navigationState->m_navigationDelegateMethods.webViewWebProcessDidCrash);
    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewWebProcessDidCrash:m_navigationState->webView().get()];
    return true;
}

void NavigationState::NavigationClient::processDidBecomeResponsive(WebPageProxy& page)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewWebProcessDidBecomeResponsive)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewWebProcessDidBecomeResponsive:m_navigationState->webView().get()];
}

void NavigationState::NavigationClient::processDidBecomeUnresponsive(WebPageProxy& page)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewWebProcessDidBecomeUnresponsive)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webViewWebProcessDidBecomeUnresponsive:m_navigationState->webView().get()];
}

void NavigationState::NavigationClient::legacyWebCryptoMasterKey(WebPageProxy&, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& completionHandler)
{
    if (!m_navigationState)
        return completionHandler(std::nullopt);
    if (!(m_navigationState->m_navigationDelegateMethods.webCryptoMasterKeyForWebView || m_navigationState->m_navigationDelegateMethods.webCryptoMasterKeyForWebViewCompletionHandler))
        return completionHandler(WebCore::defaultWebCryptoMasterKey());
    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return completionHandler(std::nullopt);

    if (m_navigationState->m_navigationDelegateMethods.webCryptoMasterKeyForWebView) {
        if (NSData *data = [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webCryptoMasterKeyForWebView:m_navigationState->webView().get()])
            return completionHandler(makeVector(data));
        return completionHandler(std::nullopt);
    }
    if (m_navigationState->m_navigationDelegateMethods.webCryptoMasterKeyForWebViewCompletionHandler) {
        auto checker = WebKit::CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webCryptoMasterKeyForWebView:completionHandler:));
        [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webCryptoMasterKeyForWebView:m_navigationState->webView().get() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)] (NSData *result) mutable {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            if (result)
                return completionHandler(makeVector(result));
            return completionHandler(std::nullopt);
        }).get()];
        return;
    }
    return completionHandler(std::nullopt);
}

void NavigationState::NavigationClient::navigationActionDidBecomeDownload(WebPageProxy&, API::NavigationAction& navigationAction, DownloadProxy& download)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.navigationActionDidBecomeDownload)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [navigationDelegate webView:m_navigationState->webView().get() navigationAction:wrapper(navigationAction) didBecomeDownload:wrapper(download)];
}

void NavigationState::NavigationClient::navigationResponseDidBecomeDownload(WebPageProxy&, API::NavigationResponse& navigationResponse, DownloadProxy& download)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.navigationResponseDidBecomeDownload)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [navigationDelegate webView:m_navigationState->webView().get() navigationResponse:wrapper(navigationResponse) didBecomeDownload:wrapper(download)];
}

void NavigationState::NavigationClient::contextMenuDidCreateDownload(WebPageProxy&, DownloadProxy& download)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.contextMenuDidCreateDownload)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate)  _webView:m_navigationState->webView().get() contextMenuDidCreateDownload:wrapper(download)];
}

#if USE(QUICK_LOOK)
void NavigationState::NavigationClient::didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidStartLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didStartLoadForQuickLookDocumentInMainFrameWithFileName:fileName uti:uti];
}

void NavigationState::NavigationClient::didFinishLoadForQuickLookDocumentInMainFrame(const FragmentedSharedBuffer& buffer)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_navigationDelegateMethods.webViewDidFinishLoadForQuickLookDocumentInMainFrame)
        return;

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate)
        return;

    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() didFinishLoadForQuickLookDocumentInMainFrame:buffer.makeContiguous()->createNSData().get()];
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
    if (!m_navigationState)
        return completionHandler(currentSOAuthorizationLoadPolicy);

    if (!m_navigationState->m_navigationDelegateMethods.webViewDecidePolicyForSOAuthorizationLoadWithCurrentPolicyForExtensionCompletionHandler) {
        completionHandler(currentSOAuthorizationLoadPolicy);
        return;
    }

    auto navigationDelegate = m_navigationState->navigationDelegate();
    if (!navigationDelegate) {
        completionHandler(currentSOAuthorizationLoadPolicy);
        return;
    }

    auto checker = CompletionHandlerCallChecker::create(navigationDelegate.get(), @selector(_webView:decidePolicyForSOAuthorizationLoadWithCurrentPolicy:forExtension:completionHandler:));
    [static_cast<id<WKNavigationDelegatePrivate>>(navigationDelegate) _webView:m_navigationState->webView().get() decidePolicyForSOAuthorizationLoadWithCurrentPolicy:wkSOAuthorizationLoadPolicy(currentSOAuthorizationLoadPolicy) forExtension:extension completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), checker = WTFMove(checker)](_WKSOAuthorizationLoadPolicy policy) mutable {
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
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_historyDelegateMethods.webViewDidNavigateWithNavigationData)
        return;

    auto historyDelegate = m_navigationState->m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState->webView().get() didNavigateWithNavigationData:wrapper(API::NavigationData::create(navigationDataStore)).get()];
}

void NavigationState::HistoryClient::didPerformClientRedirect(WebPageProxy&, const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_historyDelegateMethods.webViewDidPerformClientRedirectFromURLToURL)
        return;

    auto historyDelegate = m_navigationState->m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState->webView().get() didPerformClientRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::HistoryClient::didPerformServerRedirect(WebPageProxy&, const WTF::String& sourceURL, const WTF::String& destinationURL)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_historyDelegateMethods.webViewDidPerformServerRedirectFromURLToURL)
        return;

    auto historyDelegate = m_navigationState->m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState->webView().get() didPerformServerRedirectFromURL:[NSURL _web_URLWithWTFString:sourceURL] toURL:[NSURL _web_URLWithWTFString:destinationURL]];
}

void NavigationState::HistoryClient::didUpdateHistoryTitle(WebPageProxy&, const WTF::String& title, const WTF::String& url)
{
    if (!m_navigationState)
        return;

    if (!m_navigationState->m_historyDelegateMethods.webViewDidUpdateHistoryTitleForURL)
        return;

    auto historyDelegate = m_navigationState->m_historyDelegate.get();
    if (!historyDelegate)
        return;

    [historyDelegate _webView:m_navigationState->webView().get() didUpdateHistoryTitle:title forURL:[NSURL _web_URLWithWTFString:url]];
}

void NavigationState::willChangeIsLoading()
{
    [webView() willChangeValueForKey:@"loading"];
}

#if USE(RUNNINGBOARD)
void NavigationState::releaseNetworkActivity(NetworkActivityReleaseReason reason)
{
    if (!m_networkActivity)
        return;

    switch (reason) {
    case NetworkActivityReleaseReason::LoadCompleted:
        RELEASE_LOG(ProcessSuspension, "%p NavigationState is releasing background process assertion because a page load completed", this);
        break;
    case NetworkActivityReleaseReason::ScreenLocked:
        RELEASE_LOG(ProcessSuspension, "%p NavigationState is releasing background process assertion because the screen was locked", this);
        break;
    }
    m_networkActivity = nullptr;
    m_releaseNetworkActivityTimer.stop();
}
#endif

void NavigationState::didChangeIsLoading()
{
    auto webView = this->webView();

#if USE(RUNNINGBOARD)
    if (webView && webView->_page->pageLoadState().isLoading()) {
#if PLATFORM(IOS_FAMILY)
        // We do not start a network activity if a load starts after the screen has been locked.
        if (UIApplication.sharedApplication.isSuspendedUnderLock)
            return;
#endif

        if (m_releaseNetworkActivityTimer.isActive()) {
            RELEASE_LOG(ProcessSuspension, "%p - NavigationState keeps its process network assertion because a new page load started", this);
            m_releaseNetworkActivityTimer.stop();
        }
        if (!m_networkActivity) {
            RELEASE_LOG(ProcessSuspension, "%p - NavigationState is taking a process network assertion because a page load started", this);
            m_networkActivity = webView->_page->legacyMainFrameProcess().throttler().backgroundActivity("Page Load"_s);
        }
    } else if (m_networkActivity) {
        // The application is visible so we delay releasing the background activity for 3 seconds to give it a chance to start another navigation
        // before suspending the WebContent process <rdar://problem/27910964>.
        RELEASE_LOG(ProcessSuspension, "%p - NavigationState will release its process network assertion soon because the page load completed", this);
        m_releaseNetworkActivityTimer.startOneShot(3_s);
    }
#endif

    [webView didChangeValueForKey:@"loading"];
}

void NavigationState::willChangeTitle()
{
    [webView() willChangeValueForKey:@"title"];
}

void NavigationState::didChangeTitle()
{
    [webView() didChangeValueForKey:@"title"];
}

void NavigationState::willChangeActiveURL()
{
    [webView() willChangeValueForKey:@"URL"];
}

void NavigationState::didChangeActiveURL()
{
    [webView() didChangeValueForKey:@"URL"];
}

void NavigationState::willChangeHasOnlySecureContent()
{
    [webView() willChangeValueForKey:@"hasOnlySecureContent"];
}

void NavigationState::didChangeHasOnlySecureContent()
{
    [webView() didChangeValueForKey:@"hasOnlySecureContent"];
}

void NavigationState::willChangeNegotiatedLegacyTLS()
{
    [webView() willChangeValueForKey:@"_negotiatedLegacyTLS"];
}

void NavigationState::didChangeNegotiatedLegacyTLS()
{
    [webView() didChangeValueForKey:@"_negotiatedLegacyTLS"];
}

void NavigationState::willChangeWasPrivateRelayed()
{
    [webView() willChangeValueForKey:@"_wasPrivateRelayed"];
}

void NavigationState::didChangeWasPrivateRelayed()
{
    [webView() didChangeValueForKey:@"_wasPrivateRelayed"];
}

void NavigationState::willChangeEstimatedProgress()
{
    [webView() willChangeValueForKey:@"estimatedProgress"];
}

void NavigationState::didChangeEstimatedProgress()
{
    [webView() didChangeValueForKey:@"estimatedProgress"];
}

void NavigationState::willChangeCanGoBack()
{
    [webView() willChangeValueForKey:@"canGoBack"];
}

void NavigationState::didChangeCanGoBack()
{
    [webView() didChangeValueForKey:@"canGoBack"];
}

void NavigationState::willChangeCanGoForward()
{
    [webView() willChangeValueForKey:@"canGoForward"];
}

void NavigationState::didChangeCanGoForward()
{
    [webView() didChangeValueForKey:@"canGoForward"];
}

void NavigationState::willChangeNetworkRequestsInProgress()
{
    [webView() willChangeValueForKey:@"_networkRequestsInProgress"];
}

void NavigationState::didChangeNetworkRequestsInProgress()
{
    [webView() didChangeValueForKey:@"_networkRequestsInProgress"];
}

void NavigationState::willChangeCertificateInfo()
{
    auto webView = this->webView();
    [webView willChangeValueForKey:@"serverTrust"];
    [webView willChangeValueForKey:@"certificateChain"];
}

void NavigationState::didChangeCertificateInfo()
{
    auto webView = this->webView();
    [webView didChangeValueForKey:@"certificateChain"];
    [webView didChangeValueForKey:@"serverTrust"];
}

void NavigationState::willChangeWebProcessIsResponsive()
{
    [webView() willChangeValueForKey:@"_webProcessIsResponsive"];
}

void NavigationState::didChangeWebProcessIsResponsive()
{
    [webView() didChangeValueForKey:@"_webProcessIsResponsive"];
}

void NavigationState::didSwapWebProcesses()
{
#if USE(RUNNINGBOARD)
    // Transfer our background assertion from the old process to the new one.
    auto webView = this->webView();
    if (m_networkActivity && webView)
        m_networkActivity = webView->_page->legacyMainFrameProcess().throttler().backgroundActivity("Page Load"_s);
#endif
}

void NavigationState::didGeneratePageLoadTiming(const WebPageLoadTiming& timing)
{
    if (!m_navigationDelegateMethods.webViewDidGeneratePageLoadTiming)
        return;
    auto delegate = navigationDelegate();
    if (!delegate)
        return;
    RetainPtr wkPageLoadTiming = adoptNS([[_WKPageLoadTiming alloc] _initWithTiming:timing]);
    [static_cast<id<WKNavigationDelegatePrivate>>(delegate) _webView:webView().get() didGeneratePageLoadTiming:wkPageLoadTiming.get()];
}

} // namespace WebKit
