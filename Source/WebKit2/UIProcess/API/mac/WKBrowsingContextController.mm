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
#import "WKBrowsingContextControllerInternal.h"

#import "WKFrame.h"
#import "WKPage.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"
#import "WKURLCF.h"
#import "WKURLRequest.h"
#import "WKURLRequestNS.h"

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

@implementation WKBrowsingContextController

- (void)dealloc
{
    [_data release];
    [super dealloc];
}

- (WKPageRef)pageRef
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
    WKPageLoadURLRequest(self.pageRef, wkRequest.get());
}

- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory
{
    /* FIXME: Implement. */
}

- (void)stopLoading
{
    WKPageStopLoading(self.pageRef);
}

- (void)reload
{
    WKPageReload(self.pageRef);
}

- (void)reloadFromOrigin
{
    WKPageReloadFromOrigin(self.pageRef);
}

#pragma mark Back/Forward

- (void)goForward
{
    WKPageGoForward(self.pageRef);
}

- (BOOL)canGoForward
{
    return WKPageCanGoForward(self.pageRef);
}

- (void)goBack
{
    WKPageGoBack(self.pageRef);
}

- (BOOL)canGoBack
{
    return WKPageCanGoBack(self.pageRef);
}


#pragma mark Active Load Introspection

- (NSURL *)activeURL
{
    /* FIXME: Implement. */
    return nil;
}

- (NSURL *)pendingURL
{
    return autoreleased(WKPageCopyPendingAPIRequestURL(self.pageRef));
}

- (NSURL *)provisionalURL
{
    return autoreleased(WKFrameCopyProvisionalURL(WKPageGetMainFrame(self.pageRef)));
}

- (NSURL *)commitedURL
{
    return autoreleased(WKFrameCopyURL(WKPageGetMainFrame(self.pageRef)));
}

- (NSURL *)unreachableURL
{
    return autoreleased(WKFrameCopyUnreachableURL(WKPageGetMainFrame(self.pageRef)));
}


#pragma mark Active Document Introspection

- (NSString *)title
{
    return autoreleased(WKPageCopyTitle(self.pageRef));
}

#pragma mark Zoom

- (CGFloat)textZoom
{
    return WKPageGetTextZoomFactor(self.pageRef);
}

- (void)setTextZoom:(CGFloat)textZoom
{
    return WKPageSetTextZoomFactor(self.pageRef, textZoom);
}

- (CGFloat)pageZoom
{
    return WKPageGetPageZoomFactor(self.pageRef);
}

- (void)setPageZoom:(CGFloat)pageZoom
{
    return WKPageSetPageZoomFactor(self.pageRef, pageZoom);
}

@end


@implementation WKBrowsingContextController (Internal)

static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    [browsingContext.loadDelegate browsingContextControllerDidStartProvisionalLoad:browsingContext];
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    [browsingContext.loadDelegate browsingContextControllerDidCommitLoad:browsingContext];
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    WKBrowsingContextController *browsingContext = (WKBrowsingContextController *)clientInfo;
    [browsingContext.loadDelegate browsingContextControllerDidFinishLoad:browsingContext];
}

static void setUpPageLoaderClient(WKBrowsingContextController *browsingContext, WKPageRef pageRef)
{
    WKPageLoaderClient loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    
    loaderClient.version = kWKPageLoaderClientCurrentVersion;
    loaderClient.clientInfo = browsingContext;
    loaderClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loaderClient.didCommitLoadForFrame = didCommitLoadForFrame;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;

    WKPageSetPageLoaderClient(pageRef, &loaderClient);
}


/* This should only be called from associate view. */

- (id)initWithPageRef:(WKPageRef)pageRef
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
