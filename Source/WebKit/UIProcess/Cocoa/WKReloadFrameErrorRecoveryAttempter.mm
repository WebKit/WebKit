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
#import "WKReloadFrameErrorRecoveryAttempter.h"

#if WK_API_ENABLED

#import "_WKErrorRecoveryAttempting.h"
#import "_WKFrameHandleInternal.h"
#import "WKWebViewInternal.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@interface WKReloadFrameErrorRecoveryAttempter () <_WKErrorRecoveryAttempting>
@end

@implementation WKReloadFrameErrorRecoveryAttempter {
    WeakObjCPtr<WKWebView> _webView;
    RetainPtr<_WKFrameHandle> _frameHandle;
    String _urlString;
}

- (id)initWithWebView:(WKWebView *)webView frameHandle:(_WKFrameHandle *)frameHandle urlString:(const String&)urlString
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _frameHandle = frameHandle;
    _urlString = urlString;

    return self;
}

- (BOOL)attemptRecovery
{
    auto webView = _webView.get();
    if (!webView)
        return NO;

    uint64_t frameID = [_frameHandle _frameID];
    WebKit::WebFrameProxy* webFrameProxy = webView->_page->process().webFrame(frameID);
    if (!webFrameProxy)
        return NO;

    webFrameProxy->loadURL(URL(URL(), _urlString));
    return YES;
}

@end

#endif
