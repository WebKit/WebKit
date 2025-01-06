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
#import "WKFrameInfoInternal.h"

#import "WKSecurityOriginInternal.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "_WKFrameHandleInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

@implementation WKFrameInfo

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKFrameInfo.class, self))
        return;

    _frameInfo->~FrameInfo();

    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; webView = %p; isMainFrame = %s; request = %@>", NSStringFromClass(self.class), self, self.webView, self.mainFrame ? "YES" : "NO", self.request];
}

- (BOOL)isMainFrame
{
    return _frameInfo->isMainFrame();
}

- (NSURLRequest *)request
{
    return _frameInfo->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (WKSecurityOrigin *)securityOrigin
{
    auto& data = _frameInfo->securityOrigin();
    auto apiOrigin = API::SecurityOrigin::create(data);
    return retainPtr(wrapper(apiOrigin.get())).autorelease();
}

- (WKWebView *)webView
{
    auto page = _frameInfo->page();
    return page ? page->cocoaView().autorelease() : nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_frameInfo;
}

@end

@implementation WKFrameInfo (WKPrivate)

- (_WKFrameHandle *)_handle
{
    return wrapper(_frameInfo->handle()).autorelease();
}

- (_WKFrameHandle *)_parentFrameHandle
{
    return wrapper(_frameInfo->parentFrameHandle()).autorelease();
}

- (NSUUID *)_documentIdentifier
{
    return _frameInfo->documentID()->object();
}

- (pid_t)_processIdentifier
{
    return _frameInfo->processID();
}

- (BOOL)_isLocalFrame
{
    return _frameInfo->isLocalFrame();
}

- (BOOL)_isFocused
{
    return _frameInfo->isFocused();
}

- (BOOL)_errorOccurred
{
    return _frameInfo->errorOccurred();
}

- (NSString *)_title
{
    return _frameInfo->title();
}

- (BOOL)_isScrollable
{
    return _frameInfo->frameInfoData().frameMetrics.isScrollable == WebKit::IsScrollable::Yes;
}

- (CGSize)_contentSize
{
    return (CGSize)_frameInfo->frameInfoData().frameMetrics.contentSize;
}

- (CGSize)_visibleContentSize
{
    return (CGSize)_frameInfo->frameInfoData().frameMetrics.visibleContentSize;
}

- (CGSize)_visibleContentSizeExcludingScrollbars
{
    return (CGSize)_frameInfo->frameInfoData().frameMetrics.visibleContentSizeExcludingScrollbars;
}

@end
