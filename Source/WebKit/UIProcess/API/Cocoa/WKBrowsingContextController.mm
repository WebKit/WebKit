/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#import "WKBrowsingContextControllerInternal.h"

#import "APIData.h"
#import "APINavigation.h"
#import "ObjCObjectGraph.h"
#import "PageLoadStateObserver.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKBrowsingContextGroupInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKBrowsingContextLoadDelegatePrivate.h"
#import "WKBrowsingContextPolicyDelegate.h"
#import "WKFrame.h"
#import "WKFramePolicyListener.h"
#import "WKNSArray.h"
#import "WKNSData.h"
#import "WKNSError.h"
#import "WKNSURLAuthenticationChallenge.h"
#import "WKNSURLExtras.h"
#import "WKPagePolicyClientInternal.h"
#import "WKProcessGroupPrivate.h"
#import "WKRetainPtr.h"
#import "WKURLRequestNS.h"
#import "WKURLResponseNS.h"
#import "WKViewInternal.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProtectionSpace.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cf/CFURLExtras.h>

NSString * const WKActionIsMainFrameKey = @"WKActionIsMainFrameKey";
NSString * const WKActionNavigationTypeKey = @"WKActionNavigationTypeKey";
NSString * const WKActionMouseButtonKey = @"WKActionMouseButtonKey";
NSString * const WKActionModifierFlagsKey = @"WKActionModifierFlagsKey";
NSString * const WKActionOriginalURLRequestKey = @"WKActionOriginalURLRequestKey";
NSString * const WKActionURLRequestKey = @"WKActionURLRequestKey";
NSString * const WKActionURLResponseKey = @"WKActionURLResponseKey";
NSString * const WKActionFrameNameKey = @"WKActionFrameNameKey";
NSString * const WKActionOriginatingFrameURLKey = @"WKActionOriginatingFrameURLKey";
NSString * const WKActionCanShowMIMETypeKey = @"WKActionCanShowMIMETypeKey";

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKBrowsingContextController {
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
    RefPtr<WebKit::WebPageProxy> _page;
    std::unique_ptr<WebKit::PageLoadStateObserver> _pageLoadStateObserver;

    WeakObjCPtr<id <WKBrowsingContextLoadDelegate>> _loadDelegate;
    WeakObjCPtr<id <WKBrowsingContextPolicyDelegate>> _policyDelegate;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static HashMap<WebKit::WebPageProxy*, __unsafe_unretained WKBrowsingContextController *>& browsingContextControllerMap()
{
    static NeverDestroyed<HashMap<WebKit::WebPageProxy*, __unsafe_unretained WKBrowsingContextController *>> browsingContextControllerMap;
    return browsingContextControllerMap;
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKBrowsingContextController.class, self))
        return;

    ASSERT(browsingContextControllerMap().get(_page.get()) == self);
    browsingContextControllerMap().remove(_page.get());

    _page->pageLoadState().removeObserver(*_pageLoadStateObserver);

    [super dealloc];
}

#pragma mark Loading

+ (void)registerSchemeForCustomProtocol:(NSString *)scheme
{
    if ([NSThread isMainThread])
        WebKit::WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme);
    else {
        // This cannot be RunLoop::main().dispatch because it is called before the main runloop is initialized.  See rdar://problem/73615999
        WorkQueue::main().dispatch([scheme = retainPtr(scheme)] {
            WebKit::WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme.get());
        });
    }
}

+ (void)unregisterSchemeForCustomProtocol:(NSString *)scheme
{
    if ([NSThread isMainThread])
        WebKit::WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme);
    else {
        // This cannot be RunLoop::main().dispatch because it is called before the main runloop is initialized.  See rdar://problem/73615999
        WorkQueue::main().dispatch([scheme = retainPtr(scheme)] {
            WebKit::WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme.get());
        });
    }
}

- (void)loadRequest:(NSURLRequest *)request
{
    [self loadRequest:request userData:nil];
}

- (void)loadRequest:(NSURLRequest *)request userData:(id)userData
{
    RefPtr<WebKit::ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = WebKit::ObjCObjectGraph::create(userData);

    _page->loadRequest(request, WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow, wkUserData.get());
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory
{
    [self loadFileURL:URL restrictToFilesWithin:allowedDirectory userData:nil];
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory userData:(id)userData
{
    if (![URL isFileURL] || (allowedDirectory && ![allowedDirectory isFileURL]))
        [NSException raise:NSInvalidArgumentException format:@"Attempted to load a non-file URL"];

    RefPtr<WebKit::ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = WebKit::ObjCObjectGraph::create(userData);

    _page->loadFile(bytesAsString(bridge_cast(URL)), bytesAsString(bridge_cast(allowedDirectory)), wkUserData.get());
}

- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL
{
    [self loadHTMLString:HTMLString baseURL:baseURL userData:nil];
}

- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL userData:(id)userData
{
    RefPtr<WebKit::ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = WebKit::ObjCObjectGraph::create(userData);

    NSData *data = [HTMLString dataUsingEncoding:NSUTF8StringEncoding];
    _page->loadData({ static_cast<const uint8_t*>(data.bytes), data.length }, "text/html"_s, "UTF-8"_s, bytesAsString(bridge_cast(baseURL)), wkUserData.get());
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    _page->loadAlternateHTML(WebCore::DataSegment::create((__bridge CFDataRef)data), "UTF-8"_s, baseURL, unreachableURL);
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL
{
    [self loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:baseURL userData:nil];
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL userData:(id)userData
{
    RefPtr<WebKit::ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = WebKit::ObjCObjectGraph::create(userData);

    _page->loadData({ static_cast<const uint8_t*>(data.bytes), data.length }, MIMEType, encodingName, bytesAsString(bridge_cast(baseURL)), wkUserData.get());
}

- (void)stopLoading
{
    _page->stopLoading();
}

- (void)reload
{
    _page->reload({ });
}

- (void)reloadFromOrigin
{
    _page->reload(WebCore::ReloadOption::FromOrigin);
}

- (NSString *)applicationNameForUserAgent
{
    const String& applicationName = _page->applicationNameForUserAgent();
    return !applicationName ? nil : (NSString *)applicationName;
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationNameForUserAgent
{
    _page->setApplicationNameForDesktopUserAgent(applicationNameForUserAgent);
    _page->setApplicationNameForUserAgent(applicationNameForUserAgent);
}

- (NSString *)customUserAgent
{
    const String& customUserAgent = _page->customUserAgent();
    return !customUserAgent ? nil : (NSString *)customUserAgent;
}

- (void)setCustomUserAgent:(NSString *)customUserAgent
{
    _page->setCustomUserAgent(customUserAgent);
}

#pragma mark Back/Forward

- (void)goForward
{
    _page->goForward();
}

- (BOOL)canGoForward
{
    return !!_page->backForwardList().forwardItem();
}

- (void)goBack
{
    _page->goBack();
}

- (BOOL)canGoBack
{
    return !!_page->backForwardList().backItem();
}

- (void)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    _page->goToBackForwardItem(item._item);
}

- (WKBackForwardList *)backForwardList
{
    return wrapper(_page->backForwardList());
}

#pragma mark Active Load Introspection

- (BOOL)isLoading
{
    return _page->pageLoadState().isLoading();
}

- (NSURL *)activeURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().activeURL()];
}

- (NSURL *)provisionalURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().provisionalURL()];
}

- (NSURL *)committedURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().url()];
}

- (NSURL *)unreachableURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().unreachableURL()];
}

- (BOOL)hasOnlySecureContent
{
    return _page->pageLoadState().hasOnlySecureContent();
}

- (double)estimatedProgress
{
    return _page->estimatedProgress();
}

#pragma mark Active Document Introspection

- (NSString *)title
{
    return _page->pageLoadState().title();
}

- (NSArray *)certificateChain
{
    if (WebKit::WebFrameProxy* mainFrame = _page->mainFrame())
        return (__bridge NSArray *)WebCore::CertificateInfo::certificateChainFromSecTrust(mainFrame->certificateInfo().trust().get()).autorelease();

    return nil;
}

#pragma mark Zoom

- (CGFloat)textZoom
{
    return _page->textZoomFactor();
}

- (void)setTextZoom:(CGFloat)textZoom
{
    _page->setTextZoomFactor(textZoom);
}

- (CGFloat)pageZoom
{
    return _page->pageZoomFactor();
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    _page->setPageZoomFactor(pageZoom);
}

static void didStartProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidStartProvisionalLoad:)])
        [loadDelegate browsingContextControllerDidStartProvisionalLoad:browsingContext];
}

static void didReceiveServerRedirectForProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:)])
        [loadDelegate browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:browsingContext];
}

static void didFailProvisionalNavigation(WKPageRef page, WKNavigationRef, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:didFailProvisionalLoadWithError:)])
        [loadDelegate browsingContextController:browsingContext didFailProvisionalLoadWithError:wrapper(*WebKit::toImpl(error))];
}

static void didCommitNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidCommitLoad:)])
        [loadDelegate browsingContextControllerDidCommitLoad:browsingContext];
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerDidFinishLoad:)])
        [loadDelegate browsingContextControllerDidFinishLoad:browsingContext];
}

static void didFailNavigation(WKPageRef page, WKNavigationRef, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:didFailLoadWithError:)])
        [loadDelegate browsingContextController:browsingContext didFailLoadWithError:wrapper(*WebKit::toImpl(error))];
}

static bool canAuthenticateAgainstProtectionSpace(WKPageRef page, WKProtectionSpaceRef protectionSpace, const void *clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:canAuthenticateAgainstProtectionSpace:)])
        return [(id <WKBrowsingContextLoadDelegatePrivate>)loadDelegate browsingContextController:browsingContext canAuthenticateAgainstProtectionSpace:WebKit::toImpl(protectionSpace)->protectionSpace().nsSpace()];

    return false;
}

static void didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextController:didReceiveAuthenticationChallenge:)])
        [(id <WKBrowsingContextLoadDelegatePrivate>)loadDelegate browsingContextController:browsingContext didReceiveAuthenticationChallenge:wrapper(*WebKit::toImpl(authenticationChallenge))];
}

static void processDidCrash(WKPageRef page, const void* clientInfo)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto loadDelegate = browsingContext->_loadDelegate.get();

    if ([loadDelegate respondsToSelector:@selector(browsingContextControllerWebProcessDidCrash:)])
        [(id <WKBrowsingContextLoadDelegatePrivate>)loadDelegate browsingContextControllerWebProcessDidCrash:browsingContext];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static void setUpPageLoaderClient(WKBrowsingContextController *browsingContext, WebKit::WebPageProxy& page)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = (__bridge void*)browsingContext;
    loaderClient.didStartProvisionalNavigation = didStartProvisionalNavigation;
    loaderClient.didReceiveServerRedirectForProvisionalNavigation = didReceiveServerRedirectForProvisionalNavigation;
    loaderClient.didFailProvisionalNavigation = didFailProvisionalNavigation;
    loaderClient.didCommitNavigation = didCommitNavigation;
    loaderClient.didFinishNavigation = didFinishNavigation;
    loaderClient.didFailNavigation = didFailNavigation;
    loaderClient.canAuthenticateAgainstProtectionSpace = canAuthenticateAgainstProtectionSpace;
    loaderClient.didReceiveAuthenticationChallenge = didReceiveAuthenticationChallenge;
    loaderClient.webProcessDidCrash = processDidCrash;

    WKPageSetPageNavigationClient(toAPI(&page), &loaderClient.base);
}

static BlockPtr<void(WKPolicyDecision)> makePolicyDecisionBlock(WKFramePolicyListenerRef listener)
{
    return makeBlockPtr([listener = retainWK(listener)](WKPolicyDecision decision) {
        switch (decision) {
        case WKPolicyDecisionCancel:
            WKFramePolicyListenerIgnore(listener.get());
            break;
        case WKPolicyDecisionAllow:
            WKFramePolicyListenerUse(listener.get());
            break;
        case WKPolicyDecisionBecomeDownload:
            WKFramePolicyListenerDownload(listener.get());
            break;
        };
    });
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static void setUpPagePolicyClient(WKBrowsingContextController *browsingContext, WebKit::WebPageProxy& page)
ALLOW_DEPRECATED_DECLARATIONS_END
{
    WKPagePolicyClientInternal policyClient;
    memset(&policyClient, 0, sizeof(policyClient));

    policyClient.base.version = 2;
    policyClient.base.clientInfo = (__bridge void*)browsingContext;

    policyClient.decidePolicyForNavigationAction = [](WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKFrameRef originatingFrame, WKURLRequestRef originalRequest, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
        ALLOW_DEPRECATED_DECLARATIONS_END
        auto policyDelegate = browsingContext->_policyDelegate.get();

        if ([policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForNavigationAction:decisionHandler:)]) {
            auto actionDictionary = retainPtr(@{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionNavigationTypeKey: @(navigationType),
                WKActionModifierFlagsKey: @(modifiers),
                WKActionMouseButtonKey: @(mouseButton),
                WKActionOriginalURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(originalRequest)).get(),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get()
            });

            if (originatingFrame) {
                actionDictionary = adoptNS([actionDictionary mutableCopy]);
                [(NSMutableDictionary *)actionDictionary.get() setObject:(NSURL *)WebKit::toImpl(originatingFrame)->url() forKey:WKActionOriginatingFrameURLKey];
            }
            
            [policyDelegate browsingContextController:browsingContext decidePolicyForNavigationAction:actionDictionary.get() decisionHandler:makePolicyDecisionBlock(listener).get()];
        } else
            WKFramePolicyListenerUse(listener);
    };

    policyClient.decidePolicyForNewWindowAction = [](WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
        ALLOW_DEPRECATED_DECLARATIONS_END
        auto policyDelegate = browsingContext->_policyDelegate.get();

        if ([policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForNewWindowAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionNavigationTypeKey: @(navigationType),
                WKActionModifierFlagsKey: @(modifiers),
                WKActionMouseButtonKey: @(mouseButton),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get(),
                WKActionFrameNameKey: WebKit::toImpl(frameName)->wrapper()
            };
            
            [policyDelegate browsingContextController:browsingContext decidePolicyForNewWindowAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener).get()];
        } else
            WKFramePolicyListenerUse(listener);
    };

    policyClient.decidePolicyForResponse = [](WKPageRef page, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef request, bool canShowMIMEType, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        auto browsingContext = (__bridge WKBrowsingContextController *)clientInfo;
        ALLOW_DEPRECATED_DECLARATIONS_END
        auto policyDelegate = browsingContext->_policyDelegate.get();

        if ([policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForResponseAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get(),
                WKActionURLResponseKey: adoptNS(WKURLResponseCopyNSURLResponse(response)).get(),
                WKActionCanShowMIMETypeKey: @(canShowMIMEType),
            };

            [policyDelegate browsingContextController:browsingContext decidePolicyForResponseAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener).get()];
        } else
            WKFramePolicyListenerUse(listener);
    };

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKPageSetPagePolicyClient(toAPI(&page), &policyClient.base);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (id <WKBrowsingContextLoadDelegate>)loadDelegate
{
    return _loadDelegate.getAutoreleased();
}

- (void)setLoadDelegate:(id <WKBrowsingContextLoadDelegate>)loadDelegate
{
    _loadDelegate = loadDelegate;

    if (loadDelegate)
        setUpPageLoaderClient(self, *_page);
    else
        WKPageSetPageNavigationClient(toAPI(_page.get()), nullptr);
}

- (id <WKBrowsingContextPolicyDelegate>)policyDelegate
{
    return _policyDelegate.getAutoreleased();
}

- (void)setPolicyDelegate:(id <WKBrowsingContextPolicyDelegate>)policyDelegate
{
    _policyDelegate = policyDelegate;

    if (policyDelegate)
        setUpPagePolicyClient(self, *_page);
    else {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        WKPageSetPagePolicyClient(toAPI(_page.get()), nullptr);
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

- (id <WKBrowsingContextHistoryDelegate>)historyDelegate
{
    return _historyDelegate.getAutoreleased();
}

- (void)setHistoryDelegate:(id <WKBrowsingContextHistoryDelegate>)historyDelegate
{
    _historyDelegate = historyDelegate;
}

+ (NSMutableSet *)customSchemes
{
    static NSMutableSet *customSchemes = [[NSMutableSet alloc] init];
    return customSchemes;
}

- (instancetype)_initWithPageRef:(WKPageRef)pageRef
{
    if (!(self = [super init]))
        return nil;

    _page = WebKit::toImpl(pageRef);

    _pageLoadStateObserver = makeUnique<WebKit::PageLoadStateObserver>(self);
    _page->pageLoadState().addObserver(*_pageLoadStateObserver);

    ASSERT(!browsingContextControllerMap().contains(_page.get()));
    browsingContextControllerMap().set(_page.get(), self);

    return self;
}

+ (WKBrowsingContextController *)_browsingContextControllerForPageRef:(WKPageRef)pageRef
{
    return browsingContextControllerMap().get(WebKit::toImpl(pageRef));
}

@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKBrowsingContextController (Private)
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (WKPageRef)_pageRef
{
    return WebKit::toAPI(_page.get());
}

- (void)setPaginationMode:(WKBrowsingContextPaginationMode)paginationMode
{
    WebCore::Pagination::Mode mode;
    switch (paginationMode) {
    case WKPaginationModeUnpaginated:
        mode = WebCore::Pagination::Unpaginated;
        break;
    case WKPaginationModeLeftToRight:
        mode = WebCore::Pagination::LeftToRightPaginated;
        break;
    case WKPaginationModeRightToLeft:
        mode = WebCore::Pagination::RightToLeftPaginated;
        break;
    case WKPaginationModeTopToBottom:
        mode = WebCore::Pagination::TopToBottomPaginated;
        break;
    case WKPaginationModeBottomToTop:
        mode = WebCore::Pagination::BottomToTopPaginated;
        break;
    default:
        return;
    }

    _page->setPaginationMode(mode);
}

- (WKBrowsingContextPaginationMode)paginationMode
{
    switch (_page->paginationMode()) {
    case WebCore::Pagination::Unpaginated:
        return WKPaginationModeUnpaginated;
    case WebCore::Pagination::LeftToRightPaginated:
        return WKPaginationModeLeftToRight;
    case WebCore::Pagination::RightToLeftPaginated:
        return WKPaginationModeRightToLeft;
    case WebCore::Pagination::TopToBottomPaginated:
        return WKPaginationModeTopToBottom;
    case WebCore::Pagination::BottomToTopPaginated:
        return WKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return WKPaginationModeUnpaginated;
}

- (void)setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns
{
    _page->setPaginationBehavesLikeColumns(behavesLikeColumns);
}

- (BOOL)paginationBehavesLikeColumns
{
    return _page->paginationBehavesLikeColumns();
}

- (void)setPageLength:(CGFloat)pageLength
{
    _page->setPageLength(pageLength);
}

- (CGFloat)pageLength
{
    return _page->pageLength();
}

- (void)setGapBetweenPages:(CGFloat)gapBetweenPages
{
    _page->setGapBetweenPages(gapBetweenPages);
}

- (CGFloat)gapBetweenPages
{
    return _page->gapBetweenPages();
}

- (void)setPaginationLineGridEnabled:(BOOL)lineGridEnabled
{
}

- (BOOL)paginationLineGridEnabled
{
    return NO;
}

- (NSUInteger)pageCount
{
    return _page->pageCount();
}

- (WKBrowsingContextHandle *)handle
{
    return adoptNS([[WKBrowsingContextHandle alloc] _initWithPageProxy:*_page]).autorelease();
}

- (_WKRemoteObjectRegistry *)_remoteObjectRegistry
{
#if PLATFORM(MAC)
    return _page->remoteObjectRegistry();
#else
    return nil;
#endif
}

- (pid_t)processIdentifier
{
    return _page->processIdentifier();
}

- (BOOL)_webProcessIsResponsive
{
    return _page->process().isResponsive();
}

@end
ALLOW_DEPRECATED_DECLARATIONS_END
