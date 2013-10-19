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
#import "WKBrowsingContextController.h"
#import "WKBrowsingContextControllerPrivate.h"
#import "WKBrowsingContextControllerInternal.h"

#if WK_API_ENABLED

#import "ObjCObjectGraph.h"
#import "WKErrorCF.h"
#import "WKFrame.h"
#import "WKPagePrivate.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WKURLRequest.h"
#import "WKURLRequestNS.h"
#import "WebContext.h"
#import "WebData.h"
#import "WebPageProxy.h"
#import <wtf/ObjcRuntimeExtras.h>
#import <wtf/RetainPtr.h>

#import "WKBrowsingContextLoadDelegate.h"

using namespace WebKit;

static inline NSString *autoreleased(WKStringRef string)
{
    return string ? CFBridgingRelease(WKStringCopyCFString(kCFAllocatorDefault, adoptWK(string).get())) : nil;
}

static inline NSURL *autoreleased(WKURLRef url)
{
    return url ? CFBridgingRelease(WKURLCopyCFURL(kCFAllocatorDefault, adoptWK(url).get())) : nil;
}

@implementation WKBrowsingContextController {
    // Underlying WKPageRef.
    WKRetainPtr<WKPageRef> _pageRef;
}

- (void)dealloc
{
    WKPageSetPageLoaderClient(_pageRef.get(), 0);

    [super dealloc];
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

@end

@implementation WKBrowsingContextController (Internal)

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
        RetainPtr<CFErrorRef> cfError = adoptCF(WKErrorCopyCFError(kCFAllocatorDefault, error));
        [browsingContext.loadDelegate browsingContextControllerDidFailProvisionalLoad:browsingContext withError:(NSError *)cfError.get()];
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
        RetainPtr<CFErrorRef> cfError = adoptCF(WKErrorCopyCFError(kCFAllocatorDefault, error));
        [browsingContext.loadDelegate browsingContextControllerDidFailLoad:browsingContext withError:(NSError *)cfError.get()];
    }
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

    WKPageSetPageLoaderClient(pageRef, &loaderClient);
}


/* This should only be called from associate view. */

- (id)_initWithPageRef:(WKPageRef)pageRef
{
    self = [super init];
    if (!self)
        return nil;

    _pageRef = pageRef;

    setUpPageLoaderClient(self, pageRef);

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
 
@end

#endif // WK_API_ENABLED
