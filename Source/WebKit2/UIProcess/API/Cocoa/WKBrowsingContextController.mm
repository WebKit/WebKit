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
    // Underlying WKPageRef.
    WKRetainPtr<WKPageRef> _pageRef;

    std::unique_ptr<PageLoadStateObserver> _pageLoadStateObserver;
}

- (void)dealloc
{
    toImpl(_pageRef.get())->pageLoadState().removeObserver(*_pageLoadStateObserver);
    WKPageSetPageLoaderClient(_pageRef.get(), nullptr);
    WKPageSetPagePolicyClient(_pageRef.get(), nullptr);

    [super dealloc];
}

- (WKProcessGroup *)processGroup
{
    WebContext* context = toImpl(_pageRef.get())->process()->context();
    if (!context)
        return nil;
    return wrapper(*context);
}

- (WKBrowsingContextGroup *)browsingContextGroup
{
    WebPageGroup* pageGroup = toImpl(_pageRef.get())->pageGroup();
    if (!pageGroup)
        return nil;
    return wrapper(*pageGroup);
}

- (WKPageRef)_pageRef
{
    return _pageRef.get();
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

    WKPageLoadURLRequestWithUserData(_pageRef.get(), wkRequest.get(), (WKTypeRef)wkUserData.get());
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

    WKPageLoadFileWithUserData(_pageRef.get(), wkURL.get(), wkAllowedDirectory.get(), (WKTypeRef)wkUserData.get());
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

    WKPageLoadHTMLStringWithUserData(_pageRef.get(), wkHTMLString.get(), wkBaseURL.get(), (WKTypeRef)wkUserData.get());
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL
{
    CString baseURLString;
    getURLBytes((CFURLRef)baseURL, baseURLString);

    CString unreachableURLString;
    getURLBytes((CFURLRef)unreachableURL, unreachableURLString);

    toImpl(_pageRef.get())->loadAlternateHTMLString(string, String::fromUTF8(baseURLString), String::fromUTF8(unreachableURLString));
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

    WKPageLoadDataWithUserData(_pageRef.get(), toAPI(wkData.get()), wkMIMEType.get(), wkEncodingName.get(), wkBaseURL.get(), (WKTypeRef)wkUserData.get());
}

- (void)stopLoading
{
    WKPageStopLoading(_pageRef.get());
}

- (void)reload
{
    WKPageReload(_pageRef.get());
}

- (void)reloadFromOrigin
{
    WKPageReloadFromOrigin(_pageRef.get());
}

#pragma mark Back/Forward

- (void)goForward
{
    WKPageGoForward(_pageRef.get());
}

- (BOOL)canGoForward
{
    return WKPageCanGoForward(_pageRef.get());
}

- (void)goBack
{
    WKPageGoBack(_pageRef.get());
}

- (BOOL)canGoBack
{
    return WKPageCanGoBack(_pageRef.get());
}

- (void)goToBackForwardListItem:(WKBackForwardListItem *)item
{
    toImpl(_pageRef.get())->goToBackForwardItem(&item._item);
}

- (WKBackForwardList *)backForwardList
{
    WebBackForwardList* list = toImpl(_pageRef.get())->backForwardList();
    if (!list)
        return nil;

    return wrapper(*list);
}

#pragma mark Active Load Introspection

- (NSURL *)activeURL
{
    return autoreleased(WKPageCopyActiveURL(_pageRef.get()));
}

- (NSURL *)provisionalURL
{
    return autoreleased(WKPageCopyProvisionalURL(_pageRef.get()));
}

- (NSURL *)committedURL
{
    return autoreleased(WKPageCopyCommittedURL(_pageRef.get()));
}

- (NSURL *)unreachableURL
{
    return [NSURL _web_URLWithWTFString:toImpl(_pageRef.get())->pageLoadState().unreachableURL() relativeToURL:nil];
}

- (double)estimatedProgress
{
    return toImpl(_pageRef.get())->estimatedProgress();
}

#pragma mark Active Document Introspection

- (NSString *)title
{
    return autoreleased(WKPageCopyTitle(_pageRef.get()));
}

#pragma mark Zoom

- (CGFloat)textZoom
{
    return WKPageGetTextZoomFactor(_pageRef.get());
}

- (void)setTextZoom:(CGFloat)textZoom
{
    return WKPageSetTextZoomFactor(_pageRef.get(), textZoom);
}

- (CGFloat)pageZoom
{
    return WKPageGetPageZoomFactor(_pageRef.get());
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    return WKPageSetPageZoomFactor(_pageRef.get(), pageZoom);
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

/* This should only be called from associate view. */

- (id)_initWithPageRef:(WKPageRef)pageRef
{
    self = [super init];
    if (!self)
        return nil;

    _pageRef = pageRef;

    _pageLoadStateObserver = std::make_unique<PageLoadStateObserver>(self);
    toImpl(_pageRef.get())->pageLoadState().addObserver(*_pageLoadStateObserver);

    setUpPageLoaderClient(self, pageRef);
    setUpPagePolicyClient(self, pageRef);

    return self;
}

+ (WKBrowsingContextController *)_browsingContextControllerForPageRef:(WKPageRef)pageRef
{
    return (WKBrowsingContextController *)WebKit::toImpl(pageRef)->loaderClient().client().clientInfo;
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

    WKPageSetPaginationMode(_pageRef.get(), mode);
}

- (WKBrowsingContextPaginationMode)paginationMode
{
    switch (WKPageGetPaginationMode(_pageRef.get())) {
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
    WKPageSetPaginationBehavesLikeColumns(_pageRef.get(), behavesLikeColumns);
}

- (BOOL)paginationBehavesLikeColumns
{
    return WKPageGetPaginationBehavesLikeColumns(_pageRef.get());
}

- (void)setPageLength:(CGFloat)pageLength
{
    WKPageSetPageLength(_pageRef.get(), pageLength);
}

- (CGFloat)pageLength
{
    return WKPageGetPageLength(_pageRef.get());
}

- (void)setGapBetweenPages:(CGFloat)gapBetweenPages
{
    WKPageSetGapBetweenPages(_pageRef.get(), gapBetweenPages);
}

- (CGFloat)gapBetweenPages
{
    return WKPageGetGapBetweenPages(_pageRef.get());
}

- (NSUInteger)pageCount
{
    return WKPageGetPageCount(_pageRef.get());
}

- (WKBrowsingContextHandle *)handle
{
    return [[[WKBrowsingContextHandle alloc] _initWithPageID:toImpl(_pageRef.get())->pageID()] autorelease];
}

@end

#endif // WK_API_ENABLED
