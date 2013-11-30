/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

#import "ObjCObjectGraph.h"
#import "WKBackForwardListInternal.h"
#import "WKBackForwardListItemInternal.h"
#import "WKErrorCF.h"
#import "WKErrorRecoveryAttempting.h"
#import "WKFrame.h"
#import "WKFramePolicyListener.h"
#import "WKNSArray.h"
#import "WKNSError.h"
#import "WKNSURLExtras.h"
#import "WKPagePrivate.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WKURLRequest.h"
#import "WKURLRequestNS.h"
#import "WKURLResponse.h"
#import "WKURLResponseNS.h"
#import "WebContext.h"
#import "WebData.h"
#import "WebPageProxy.h"
#import <WebCore/CFURLExtras.h>
#import <wtf/ObjcRuntimeExtras.h>
#import <wtf/RetainPtr.h>

#import "WKBrowsingContextGroupInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKBrowsingContextLoadDelegate.h"
#import "WKBrowsingContextPolicyDelegate.h"
#import "WKProcessGroupInternal.h"

using namespace WebCore;
using namespace WebKit;

class PageLoadStateObserver : public PageLoadState::Observer {
public:
    PageLoadStateObserver(WKBrowsingContextController *controller)
        : m_controller(controller)
    {
    }

private:
    virtual void willChangeTitle() OVERRIDE
    {
        [m_controller willChangeValueForKey:@"title"];
    }

    virtual void didChangeTitle() OVERRIDE
    {
        [m_controller didChangeValueForKey:@"title"];
    }

    WKBrowsingContextController *m_controller;
};

static inline NSString *autoreleased(WKStringRef string)
{
    return string ? CFBridgingRelease(WKStringCopyCFString(kCFAllocatorDefault, adoptWK(string).get())) : nil;
}

static inline NSURL *autoreleased(WKURLRef url)
{
    return url ? CFBridgingRelease(WKURLCopyCFURL(kCFAllocatorDefault, adoptWK(url).get())) : nil;
}

NSString * const WKActionIsMainFrameKey = @"WKActionIsMainFrameKey";
NSString * const WKActionNavigationTypeKey = @"WKActionNavigationTypeKey";
NSString * const WKActionMouseButtonKey = @"WKActionMouseButtonKey";
NSString * const WKActionModifierFlagsKey = @"WKActionModifierFlagsKey";
NSString * const WKActionURLRequestKey = @"WKActionURLRequestKey";
NSString * const WKActionURLResponseKey = @"WKActionURLResponseKey";
NSString * const WKActionFrameNameKey = @"WKActionFrameNameKey";
NSString * const WKActionOriginatingFrameURLKey = @"WKActionOriginatingFrameURLKey";
NSString * const WKActionCanShowMIMETypeKey = @"WKActionCanShowMIMETypeKey";

static NSString * const frameErrorKey = @"WKBrowsingContextFrameErrorKey";

@interface WKBrowsingContextController () <WKErrorRecoveryAttempting>
@end

@implementation WKBrowsingContextController {
    API::ObjectStorage<WebPageProxy> _page;
    std::unique_ptr<PageLoadStateObserver> _pageLoadStateObserver;
}

@synthesize loadDelegate = _loadDelegate;
@synthesize policyDelegate = _policyDelegate;

- (void)dealloc
{
    _page->pageLoadState().removeObserver(*_pageLoadStateObserver);
    _page->~WebPageProxy();

    [super dealloc];
}

- (void)_finishInitialization
{
    _pageLoadStateObserver = std::make_unique<PageLoadStateObserver>(self);
    _page->pageLoadState().addObserver(*_pageLoadStateObserver);
}

- (WKProcessGroup *)processGroup
{
    WebContext* context = _page->process()->context();
    if (!context)
        return nil;
    return wrapper(*context);
}

- (WKBrowsingContextGroup *)browsingContextGroup
{
    WebPageGroup* pageGroup = _page->pageGroup();
    if (!pageGroup)
        return nil;
    return wrapper(*pageGroup);
}

- (WKPageRef)_pageRef
{
    return toAPI(_page.get());
}

#pragma mark Loading

+ (void)registerSchemeForCustomProtocol:(NSString *)scheme
{
    if (!scheme)
        return;

    NSString *lowercaseScheme = [scheme lowercaseString];
    [[WKBrowsingContextController customSchemes] addObject:lowercaseScheme];
    [[NSNotificationCenter defaultCenter] postNotificationName:SchemeForCustomProtocolRegisteredNotificationName object:lowercaseScheme];
}

+ (void)unregisterSchemeForCustomProtocol:(NSString *)scheme
{
    if (!scheme)
        return;

    NSString *lowercaseScheme = [scheme lowercaseString];
    [[WKBrowsingContextController customSchemes] removeObject:lowercaseScheme];
    [[NSNotificationCenter defaultCenter] postNotificationName:SchemeForCustomProtocolUnregisteredNotificationName object:lowercaseScheme];
}

- (void)loadRequest:(NSURLRequest *)request
{
    [self loadRequest:request userData:nil];
}

- (void)loadRequest:(NSURLRequest *)request userData:(id)userData
{
    WKRetainPtr<WKURLRequestRef> wkRequest = adoptWK(WKURLRequestCreateWithNSURLRequest(request));

    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    WKPageLoadURLRequestWithUserData(toAPI(_page.get()), wkRequest.get(), (WKTypeRef)wkUserData.get());
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory
{
    [self loadFileURL:URL restrictToFilesWithin:allowedDirectory userData:nil];
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory userData:(id)userData
{
    if (![URL isFileURL] || (allowedDirectory && ![allowedDirectory isFileURL]))
        [NSException raise:NSInvalidArgumentException format:@"Attempted to load a non-file URL"];

    WKRetainPtr<WKURLRef> wkURL = adoptWK(WKURLCreateWithCFURL((CFURLRef)URL));
    WKRetainPtr<WKURLRef> wkAllowedDirectory = adoptWK(WKURLCreateWithCFURL((CFURLRef)allowedDirectory));
    
    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    WKPageLoadFileWithUserData(toAPI(_page.get()), wkURL.get(), wkAllowedDirectory.get(), (WKTypeRef)wkUserData.get());
}

- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL
{
    [self loadHTMLString:HTMLString baseURL:baseURL userData:nil];
}

- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL userData:(id)userData
{
    WKRetainPtr<WKStringRef> wkHTMLString;
    if (HTMLString)
        wkHTMLString = adoptWK(WKStringCreateWithCFString((CFStringRef)HTMLString));

    WKRetainPtr<WKURLRef> wkBaseURL;
    if (baseURL)
        wkBaseURL = adoptWK(WKURLCreateWithCFURL((CFURLRef)baseURL));

    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    WKPageLoadHTMLStringWithUserData(toAPI(_page.get()), wkHTMLString.get(), wkBaseURL.get(), (WKTypeRef)wkUserData.get());
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    CString baseURLString;
    getURLBytes((CFURLRef)baseURL, baseURLString);

    CString unreachableURLString;
    getURLBytes((CFURLRef)unreachableURL, unreachableURLString);

    _page->loadAlternateHTMLString(string, String::fromUTF8(baseURLString), String::fromUTF8(unreachableURLString));
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL
{
    [self loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:baseURL userData:nil];
}

static void releaseNSData(unsigned char*, const void* data)
{
    [(NSData *)data release];
}

- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL userData:(id)userData
{
    RefPtr<WebData> wkData;
    if (data) {
        [data retain];
        wkData = WebData::createWithoutCopying((const unsigned char*)[data bytes], [data length], releaseNSData, data);
    }

    WKRetainPtr<WKStringRef> wkMIMEType;
    if (MIMEType)
        wkMIMEType = adoptWK(WKStringCreateWithCFString((CFStringRef)MIMEType));

    WKRetainPtr<WKStringRef> wkEncodingName;
    if (encodingName)
        wkEncodingName = adoptWK(WKStringCreateWithCFString((CFStringRef)encodingName));

    WKRetainPtr<WKURLRef> wkBaseURL;
    if (baseURL)
        wkBaseURL = adoptWK(WKURLCreateWithCFURL((CFURLRef)baseURL));

    RefPtr<ObjCObjectGraph> wkUserData;
    if (userData)
        wkUserData = ObjCObjectGraph::create(userData);

    WKPageLoadDataWithUserData(toAPI(_page.get()), toAPI(wkData.get()), wkMIMEType.get(), wkEncodingName.get(), wkBaseURL.get(), (WKTypeRef)wkUserData.get());
}

- (void)stopLoading
{
    WKPageStopLoading(toAPI(_page.get()));
}

- (void)reload
{
    WKPageReload(toAPI(_page.get()));
}

- (void)reloadFromOrigin
{
    WKPageReloadFromOrigin(toAPI(_page.get()));
}

#pragma mark Back/Forward

- (void)goForward
{
    WKPageGoForward(toAPI(_page.get()));
}

- (BOOL)canGoForward
{
    return WKPageCanGoForward(toAPI(_page.get()));
}

- (void)goBack
{
    WKPageGoBack(toAPI(_page.get()));
}

- (BOOL)canGoBack
{
    return WKPageCanGoBack(toAPI(_page.get()));
}

- (void)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    _page->goToBackForwardItem(&item._item);
}

- (WKBackForwardList *)backForwardList
{
    WebBackForwardList* list = _page->backForwardList();
    if (!list)
        return nil;

    return wrapper(*list);
}

#pragma mark Active Load Introspection

- (NSURL *)activeURL
{
    return autoreleased(WKPageCopyActiveURL(toAPI(_page.get())));
}

- (NSURL *)provisionalURL
{
    return autoreleased(WKPageCopyProvisionalURL(toAPI(_page.get())));
}

- (NSURL *)committedURL
{
    return autoreleased(WKPageCopyCommittedURL(toAPI(_page.get())));
}

- (NSURL *)unreachableURL
{
    return [NSURL _web_URLWithWTFString:_page->pageLoadState().unreachableURL() relativeToURL:nil];
}

- (double)estimatedProgress
{
    return _page->estimatedProgress();
}

#pragma mark Active Document Introspection

- (NSString *)title
{
    return autoreleased(WKPageCopyTitle(toAPI(_page.get())));
}

#pragma mark Zoom

- (CGFloat)textZoom
{
    return WKPageGetTextZoomFactor(toAPI(_page.get()));
}

- (void)setTextZoom:(CGFloat)textZoom
{
    return WKPageSetTextZoomFactor(toAPI(_page.get()), textZoom);
}

- (CGFloat)pageZoom
{
    return WKPageGetPageZoomFactor(toAPI(_page.get()));
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    return WKPageSetPageZoomFactor(toAPI(_page.get()), pageZoom);
}

static NSError *createErrorWithRecoveryAttempter(WKErrorRef wkError, WKFrameRef frame, WKBrowsingContextController *browsingContext)
{
    NSError *error = wrapper(*toImpl(wkError));
    NSMutableDictionary *userInfo = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        browsingContext, WKRecoveryAttempterErrorKey,
        toImpl(frame)->wrapper(), frameErrorKey,
    nil];

    if (NSDictionary *originalUserInfo = error.userInfo)
        [userInfo addEntriesFromDictionary:originalUserInfo];

    return [[NSError alloc] initWithDomain:error.domain code:error.code userInfo:userInfo];
}

static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidStartProvisionalLoad:)])
        [browsingContext.loadDelegate browsingContextControllerDidStartProvisionalLoad:browsingContext];
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:)])
        [browsingContext.loadDelegate browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:browsingContext];
}

static void didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidFailProvisionalLoad:withError:)]) {
        RetainPtr<NSError> nsError = adoptNS(createErrorWithRecoveryAttempter(error, frame, browsingContext));
        [browsingContext.loadDelegate browsingContextControllerDidFailProvisionalLoad:browsingContext withError:nsError.get()];
    }
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidCommitLoad:)])
        [browsingContext.loadDelegate browsingContextControllerDidCommitLoad:browsingContext];
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidFinishLoad:)])
        [browsingContext.loadDelegate browsingContextControllerDidFinishLoad:browsingContext];
}

static void didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidFailLoad:withError:)]) {
        RetainPtr<NSError> nsError = adoptNS(createErrorWithRecoveryAttempter(error, frame, browsingContext));
        [browsingContext.loadDelegate browsingContextControllerDidFailLoad:browsingContext withError:nsError.get()];
    }
}

static void didStartProgress(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidStartProgress:)])
        [browsingContext.loadDelegate browsingContextControllerDidStartProgress:browsingContext];
}

static void didChangeProgress(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextController:estimatedProgressChangedTo:)])
        [browsingContext.loadDelegate browsingContextController:browsingContext estimatedProgressChangedTo:toImpl(page)->estimatedProgress()];
}

static void didFinishProgress(WKPageRef page, const void* clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if ([browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidFinishProgress:)])
        [browsingContext.loadDelegate browsingContextControllerDidFinishProgress:browsingContext];
}

static void didChangeBackForwardList(WKPageRef page, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void *clientInfo)
{
    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    if (![browsingContext.loadDelegate respondsToSelector:@selector(browsingContextControllerDidChangeBackForwardList:addedItem:removedItems:)])
        return;

    WKBackForwardListItem *added = addedItem ? wrapper(*toImpl(addedItem)) : nil;
    NSArray *removed = removedItems ? wrapper(*toImpl(removedItems)) : nil;
    [browsingContext.loadDelegate browsingContextControllerDidChangeBackForwardList:browsingContext addedItem:added removedItems:removed];
}

static void setUpPageLoaderClient(WKBrowsingContextController *browsingContext, WKPageRef pageRef)
{
    WKPageLoaderClient loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.version = kWKPageLoaderClientCurrentVersion;
    loaderClient.clientInfo = browsingContext;
    loaderClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loaderClient.didReceiveServerRedirectForProvisionalLoadForFrame = didReceiveServerRedirectForProvisionalLoadForFrame;
    loaderClient.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrame;
    loaderClient.didCommitLoadForFrame = didCommitLoadForFrame;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loaderClient.didFailLoadWithErrorForFrame = didFailLoadWithErrorForFrame;

    loaderClient.didStartProgress = didStartProgress;
    loaderClient.didChangeProgress = didChangeProgress;
    loaderClient.didFinishProgress = didFinishProgress;
    loaderClient.didChangeBackForwardList = didChangeBackForwardList;

    WKPageSetPageLoaderClient(pageRef, &loaderClient);
}

static WKPolicyDecisionHandler makePolicyDecisionBlock(WKFramePolicyListenerRef listener)
{
    WKRetain(listener); // Released in the decision handler below.

    return [[^(WKPolicyDecision decision) {
        switch (decision) {
        case WKPolicyDecisionCancel:
            WKFramePolicyListenerIgnore(listener);                    
            break;
        
        case WKPolicyDecisionAllow:
            WKFramePolicyListenerUse(listener);
            break;
        
        case WKPolicyDecisionBecomeDownload:
            WKFramePolicyListenerDownload(listener);
            break;
        };

        WKRelease(listener); // Retained in the context above.
    } copy] autorelease];
}

static void setUpPagePolicyClient(WKBrowsingContextController *browsingContext, WKPageRef pageRef)
{
    WKPagePolicyClient policyClient;
    memset(&policyClient, 0, sizeof(policyClient));

    policyClient.version = kWKPagePolicyClientCurrentVersion;
    policyClient.clientInfo = browsingContext;

    policyClient.decidePolicyForNavigationAction = [](WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKFrameRef originatingFrame, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
        if ([browsingContext.policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForNavigationAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionNavigationTypeKey: @(navigationType),
                WKActionModifierFlagsKey: @(modifiers),
                WKActionMouseButtonKey: @(mouseButton),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get()
            };

            if (originatingFrame) {
                actionDictionary = [[actionDictionary mutableCopy] autorelease];
                [(NSMutableDictionary *)actionDictionary setObject:[NSURL _web_URLWithWTFString:toImpl(originatingFrame)->url() relativeToURL:nil] forKey:WKActionOriginatingFrameURLKey];
            }
            
            [browsingContext.policyDelegate browsingContextController:browsingContext decidePolicyForNavigationAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener)];
        } else
            WKFramePolicyListenerUse(listener);
    };

    policyClient.decidePolicyForNewWindowAction = [](WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
        if ([browsingContext.policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForNewWindowAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionNavigationTypeKey: @(navigationType),
                WKActionModifierFlagsKey: @(modifiers),
                WKActionMouseButtonKey: @(mouseButton),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get(),
                WKActionFrameNameKey: toImpl(frameName)->wrapper()
            };
            
            [browsingContext.policyDelegate browsingContextController:browsingContext decidePolicyForNewWindowAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener)];
        } else
            WKFramePolicyListenerUse(listener);
    };

    policyClient.decidePolicyForResponse = [](WKPageRef page, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef request, bool canShowMIMEType, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
    {
        WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
        if ([browsingContext.policyDelegate respondsToSelector:@selector(browsingContextController:decidePolicyForResponseAction:decisionHandler:)]) {
            NSDictionary *actionDictionary = @{
                WKActionIsMainFrameKey: @(WKFrameIsMainFrame(frame)),
                WKActionURLRequestKey: adoptNS(WKURLRequestCopyNSURLRequest(request)).get(),
                WKActionURLResponseKey: adoptNS(WKURLResponseCopyNSURLResponse(response)).get(),
                WKActionCanShowMIMETypeKey: @(canShowMIMEType),
            };

            [browsingContext.policyDelegate browsingContextController:browsingContext decidePolicyForResponseAction:actionDictionary decisionHandler:makePolicyDecisionBlock(listener)];
        } else
            WKFramePolicyListenerUse(listener);
    };

    WKPageSetPagePolicyClient(pageRef, &policyClient);
}

- (id <WKBrowsingContextLoadDelegate>)loadDelegate
{
    return _loadDelegate;
}

- (void)setLoadDelegate:(id <WKBrowsingContextLoadDelegate>)loadDelegate
{
    _loadDelegate = loadDelegate;
    if (_loadDelegate)
        setUpPageLoaderClient(self, toAPI(_page.get()));
    else
        WKPageSetPageLoaderClient(toAPI(_page.get()), nullptr);;
}

- (id <WKBrowsingContextPolicyDelegate>)policyDelegate
{
    return _policyDelegate;
}

- (void)setPolicyDelegate:(id <WKBrowsingContextPolicyDelegate>)policyDelegate
{
    _policyDelegate = policyDelegate;
    if (_policyDelegate)
        setUpPagePolicyClient(self, toAPI(_page.get()));
    else
        WKPageSetPagePolicyClient(toAPI(_page.get()), nullptr);;
}

+ (NSMutableSet *)customSchemes
{
    static NSMutableSet *customSchemes = [[NSMutableSet alloc] init];
    return customSchemes;
}

#pragma mark WKErrorRecoveryAttempting

- (BOOL)attemptRecoveryFromError:(NSError *)error
{
    NSDictionary *userInfo = error.userInfo;

    NSString *failingURLString = userInfo[NSURLErrorFailingURLStringErrorKey];
    if (!failingURLString)
        return NO;

    NSObject <WKObject> *frame = userInfo[frameErrorKey];
    if (![frame conformsToProtocol:@protocol(WKObject)])
        return NO;

    if (frame._apiObject.type() != API::Object::Type::Frame)
        return NO;

    WebFrameProxy& webFrame = *static_cast<WebFrameProxy*>(&frame._apiObject);
    webFrame.loadURL(failingURLString);

    return YES;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *reinterpret_cast<API::Object*>(&_page);
}

@end

@implementation WKBrowsingContextController (Private)

- (void)setPaginationMode:(WKBrowsingContextPaginationMode)paginationMode
{
    WKPaginationMode mode;
    switch (paginationMode) {
    case WKPaginationModeUnpaginated:
        mode = kWKPaginationModeUnpaginated;
        break;
    case WKPaginationModeLeftToRight:
        mode = kWKPaginationModeLeftToRight;
        break;
    case WKPaginationModeRightToLeft:
        mode = kWKPaginationModeRightToLeft;
        break;
    case WKPaginationModeTopToBottom:
        mode = kWKPaginationModeTopToBottom;
        break;
    case WKPaginationModeBottomToTop:
        mode = kWKPaginationModeBottomToTop;
        break;
    default:
        return;
    }

    WKPageSetPaginationMode(toAPI(_page.get()), mode);
}

- (WKBrowsingContextPaginationMode)paginationMode
{
    switch (WKPageGetPaginationMode(toAPI(_page.get()))) {
    case kWKPaginationModeUnpaginated:
        return WKPaginationModeUnpaginated;
    case kWKPaginationModeLeftToRight:
        return WKPaginationModeLeftToRight;
    case kWKPaginationModeRightToLeft:
        return WKPaginationModeRightToLeft;
    case kWKPaginationModeTopToBottom:
        return WKPaginationModeTopToBottom;
    case kWKPaginationModeBottomToTop:
        return WKPaginationModeBottomToTop;
    }

    ASSERT_NOT_REACHED();
    return WKPaginationModeUnpaginated;
}

- (void)setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns
{
    WKPageSetPaginationBehavesLikeColumns(toAPI(_page.get()), behavesLikeColumns);
}

- (BOOL)paginationBehavesLikeColumns
{
    return WKPageGetPaginationBehavesLikeColumns(toAPI(_page.get()));
}

- (void)setPageLength:(CGFloat)pageLength
{
    WKPageSetPageLength(toAPI(_page.get()), pageLength);
}

- (CGFloat)pageLength
{
    return WKPageGetPageLength(toAPI(_page.get()));
}

- (void)setGapBetweenPages:(CGFloat)gapBetweenPages
{
    WKPageSetGapBetweenPages(toAPI(_page.get()), gapBetweenPages);
}

- (CGFloat)gapBetweenPages
{
    return WKPageGetGapBetweenPages(toAPI(_page.get()));
}

- (NSUInteger)pageCount
{
    return WKPageGetPageCount(toAPI(_page.get()));
}

- (WKBrowsingContextHandle *)handle
{
    return [[[WKBrowsingContextHandle alloc] _initWithPageID:_page->pageID()] autorelease];
}

@end

#endif // WK_API_ENABLED
