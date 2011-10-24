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

#import "WKPage.h"
#import "WKRetainPtr.h"
#import "WKURLRequest.h"
#import "WKURLRequestNS.h"
#import "WKStringCF.h"

@interface WKBrowsingContextControllerData : NSObject {
@public
    WKRetainPtr<WKPageRef> _pageRef;
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

#pragma mark Active Document Introspection

- (NSString *)title
{
    WKRetainPtr<WKStringRef> wkTitle = adoptWK(WKPageCopyTitle(self.pageRef));
    return [(NSString *)WKStringCopyCFString(kCFAllocatorDefault, wkTitle.get()) autorelease];
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

/* This should only be called from associate view. */

- (id)initWithPageRef:(WKPageRef)pageRef
{
    self = [super init];
    if (!self)
        return nil;

    _data = [[WKBrowsingContextControllerData alloc] init];
    _data->_pageRef = pageRef;

    return self;
}

@end
