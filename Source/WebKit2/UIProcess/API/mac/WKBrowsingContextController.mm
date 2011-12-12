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

#import "WKErrorCF.h"
#import "WKFrame.h"
#import "WKPagePrivate.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WKURLRequest.h"
#import "WKURLRequestNS.h"
#import <wtf/RetainPtr.h>

#import "WKBrowsingContextLoadDelegate.h"

static inline NSString *autoreleased(WKStringRef string)
{
    WKRetainPtr<WKStringRef> wkString = adoptWK(string);
    return [(NSString *)WKStringCopyCFString(kCFAllocatorDefault, wkString.get()) autorelease];
}

static inline NSURL *autoreleased(WKURLRef url)
{
    WKRetainPtr<WKURLRef> wkURL = adoptWK(url);
    return [(NSURL *)WKURLCopyCFURL(kCFAllocatorDefault, wkURL.get()) autorelease];
}


@interface WKBrowsingContextControllerData : NSObject {
@public
    // Underlying WKPageRef.
    WKRetainPtr<WKPageRef> _pageRef;
    
    // Delegate for load callbacks.
    id<WKBrowsingContextLoadDelegate> _loadDelegate;
}
@end

@implementation WKBrowsingContextControllerData
@end


@interface WKBrowsingContextController ()

@property(readonly) WKPageRef _pageRef;

@end


@implementation WKBrowsingContextController

- (void)dealloc
{
    [_data release];
    [super dealloc];
}

- (WKPageRef)_pageRef
{
    return _data->_pageRef.get();
}

#pragma mark Delegates

- (id<WKBrowsingContextLoadDelegate>)loadDelegate
{
    return _data->_loadDelegate;
}

- (void)setLoadDelegate:(id<WKBrowsingContextLoadDelegate>)loadDelegate
{
    _data->_loadDelegate = loadDelegate;
}

#pragma mark Loading

- (void)loadRequest:(NSURLRequest *)request
{
    WKRetainPtr<WKURLRequestRef> wkRequest = adoptWK(WKURLRequestCreateWithNSURLRequest(request));
    WKPageLoadURLRequest(self._pageRef, wkRequest.get());
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory
{
    if (![URL isFileURL])
        return;

    /* FIXME: Implement restrictions. */

    WKRetainPtr<WKURLRef> wkURL = adoptWK(WKURLCreateWithCFURL((CFURLRef)URL));
    WKPageLoadURL(self._pageRef, wkURL.get());
}

- (void)stopLoading
{
    WKPageStopLoading(self._pageRef);
}

- (void)reload
{
    WKPageReload(self._pageRef);
}

- (void)reloadFromOrigin
{
    WKPageReloadFromOrigin(self._pageRef);
}

#pragma mark Back/Forward

- (void)goForward
{
    WKPageGoForward(self._pageRef);
}

- (BOOL)canGoForward
{
    return WKPageCanGoForward(self._pageRef);
}

- (void)goBack
{
    WKPageGoBack(self._pageRef);
}

- (BOOL)canGoBack
{
    return WKPageCanGoBack(self._pageRef);
}


#pragma mark Active Load Introspection

- (NSURL *)activeURL
{
    return autoreleased(WKPageCopyActiveURL(self._pageRef));
}

- (NSURL *)provisionalURL
{
    return autoreleased(WKPageCopyProvisionalURL(self._pageRef));
}

- (NSURL *)committedURL
{
    return autoreleased(WKPageCopyCommittedURL(self._pageRef));
}

#pragma mark Active Document Introspection

- (NSString *)title
{
    return autoreleased(WKPageCopyTitle(self._pageRef));
}

#pragma mark Zoom

- (CGFloat)textZoom
{
    return WKPageGetTextZoomFactor(self._pageRef);
}

- (void)setTextZoom:(CGFloat)textZoom
{
    return WKPageSetTextZoomFactor(self._pageRef, textZoom);
}

- (CGFloat)pageZoom
{
    return WKPageGetPageZoomFactor(self._pageRef);
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    return WKPageSetPageZoomFactor(self._pageRef, pageZoom);
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
    case WKPaginationModeHorizontal:
        mode = kWKPaginationModeHorizontal;
        break;
    case WKPaginationModeVertical:
        mode = kWKPaginationModeVertical;
        break;
    default:
        return;
    }

    WKPageSetPaginationMode(self._pageRef, mode);
}

- (WKBrowsingContextPaginationMode)paginationMode
{
    switch (WKPageGetPaginationMode(self._pageRef)) {
    case kWKPaginationModeUnpaginated:
        return WKPaginationModeUnpaginated;
    case kWKPaginationModeHorizontal:
        return WKPaginationModeHorizontal;
    case kWKPaginationModeVertical:
        return WKPaginationModeVertical;
    }

    ASSERT_NOT_REACHED();
    return WKPaginationModeUnpaginated;
}

- (void)setPageLength:(CGFloat)pageLength
{
    WKPageSetPageLength(self._pageRef, pageLength);
}

- (CGFloat)pageLength
{
    return WKPageGetPageLength(self._pageRef);
}

- (void)setGapBetweenPages:(CGFloat)gapBetweenPages
{
    WKPageSetGapBetweenPages(self._pageRef, gapBetweenPages);
}

- (CGFloat)gapBetweenPages
{
    return WKPageGetGapBetweenPages(self._pageRef);
}

- (NSUInteger)pageCount
{
    return WKPageGetPageCount(self._pageRef);
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
        RetainPtr<CFErrorRef> cfError(AdoptCF, WKErrorCopyCFError(kCFAllocatorDefault, error));
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
        RetainPtr<CFErrorRef> cfError(AdoptCF, WKErrorCopyCFError(kCFAllocatorDefault, error));
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

    _data = [[WKBrowsingContextControllerData alloc] init];
    _data->_pageRef = pageRef;

    setUpPageLoaderClient(self, pageRef);

    return self;
}

@end
