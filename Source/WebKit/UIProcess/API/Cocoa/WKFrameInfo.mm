/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "_WKFrameHandleInternal.h"

@implementation WKFrameInfo

- (void)dealloc
{
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
    return wrapper(_frameInfo->securityOrigin());
}

- (WKWebView *)webView
{
    if (WebKit::WebPageProxy* page = _frameInfo->page())
        return [[fromWebPageProxy(*page) retain] autorelease];
    return nil;
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
    return wrapper(_frameInfo->handle());
}

@end
