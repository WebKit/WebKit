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

#if WK_API_ENABLED

#import "WebFrameProxy.h"
#import <wtf/RetainPtr.h>

@implementation WKFrameInfo {
    RetainPtr<NSURLRequest> _request;
}

- (instancetype)initWithWebFrameProxy:(WebKit::WebFrameProxy&)webFrameProxy
{
    if (!(self = [super init]))
        return nil;

    _mainFrame = webFrameProxy.isMainFrame();

    // FIXME: This should use the full request of the frame, not just the URL.
    _request = [NSURLRequest requestWithURL:[NSURL URLWithString:webFrameProxy.url()]];

    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; isMainFrame = %s; request = %@>", NSStringFromClass(self.class), self, _mainFrame ? "YES" : "NO", _request.get()];
}

- (NSURLRequest *)request
{
    return _request.get();
}

- (void)setRequest:(NSURLRequest *)request
{
    _request = adoptNS([request copy]);
}

@end

#endif

