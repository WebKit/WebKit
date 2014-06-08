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
#import "WKScriptMessageInternal.h"

#if WK_API_ENABLED

#import "WKFrameInfo.h"
#import "WeakObjCPtr.h"
#import <wtf/RetainPtr.h>

@implementation WKScriptMessage {
    RetainPtr<id> _body;
    WebKit::WeakObjCPtr<WKWebView> _webView;
    RetainPtr<WKFrameInfo> _frameInfo;
    RetainPtr<NSString> _name;
}

- (instancetype)_initWithBody:(id)body webView:(WKWebView *)webView frameInfo:(WKFrameInfo *)frameInfo name:(NSString *)name
{
    if (!(self = [super init]))
        return nil;

    _body = [body copy];
    _webView = webView;
    _frameInfo = frameInfo;
    _name = adoptNS([name copy]);

    return self;

}

- (id)body
{
    return _body.get();
}

- (WKWebView *)webView
{
    return _webView.getAutoreleased();
}

- (WKFrameInfo *)frameInfo
{
    return _frameInfo.get();
}

- (NSString *)name
{
    return _name.get();
}

@end

#endif

