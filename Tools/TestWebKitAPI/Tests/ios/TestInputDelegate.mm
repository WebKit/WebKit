/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TestInputDelegate.h"

#if PLATFORM(IOS_FAMILY)

#import <wtf/BlockPtr.h>

@implementation TestInputDelegate {
    BlockPtr<_WKFocusStartsInputSessionPolicy(WKWebView *, id <_WKFocusedElementInfo>)> _focusStartsInputSessionPolicyHandler;
    BlockPtr<void(WKWebView *, id <_WKFormInputSession>)> _willStartInputSessionHandler;
    BlockPtr<void(WKWebView *, id <_WKFormInputSession>)> _didStartInputSessionHandler;
    BlockPtr<NSDictionary<id, NSString *> *(WKWebView *)> _webViewAdditionalContextForStrongPasswordAssistanceHandler;
    BlockPtr<BOOL(WKWebView *, id <_WKFocusedElementInfo>)> _focusRequiresStrongPasswordAssistanceHandler;
}

- (void)setFocusStartsInputSessionPolicyHandler:(_WKFocusStartsInputSessionPolicy (^)(WKWebView *, id <_WKFocusedElementInfo>))handler
{
    _focusStartsInputSessionPolicyHandler = makeBlockPtr(handler);
}

- (_WKFocusStartsInputSessionPolicy (^)(WKWebView *, id <_WKFocusedElementInfo>))focusStartsInputSessionPolicyHandler
{
    return _focusStartsInputSessionPolicyHandler.get();
}

- (void)setDidStartInputSessionHandler:(void (^)(WKWebView *, id <_WKFormInputSession>))handler
{
    _didStartInputSessionHandler = makeBlockPtr(handler);
}

- (void (^)(WKWebView *, id <_WKFormInputSession>))didStartInputSessionHandler
{
    return _didStartInputSessionHandler.get();
}

- (void)setWillStartInputSessionHandler:(void (^)(WKWebView *, id <_WKFormInputSession>))willStartInputSessionHandler
{
    _willStartInputSessionHandler = makeBlockPtr(willStartInputSessionHandler);
}

- (void (^)(WKWebView *, id <_WKFormInputSession>))willStartInputSessionHandler
{
    return _willStartInputSessionHandler.get();
}

- (_WKFocusStartsInputSessionPolicy)_webView:(WKWebView *)webView decidePolicyForFocusedElement:(id <_WKFocusedElementInfo>)info
{
    return self.focusStartsInputSessionPolicyHandler ? self.focusStartsInputSessionPolicyHandler(webView, info) : _WKFocusStartsInputSessionPolicyAuto;
}

- (void)_webView:(WKWebView *)webView willStartInputSession:(id <_WKFormInputSession>)inputSession
{
    if (_willStartInputSessionHandler)
        _willStartInputSessionHandler(webView, inputSession);
}

- (void)_webView:(WKWebView *)webView didStartInputSession:(id <_WKFormInputSession>)inputSession
{
    if (_didStartInputSessionHandler)
        _didStartInputSessionHandler(webView, inputSession);
}

- (void)setWebViewAdditionalContextForStrongPasswordAssistanceHandler:(NSDictionary<id, NSString *> * (^)(WKWebView *))webViewAdditionalContextForStrongPasswordAssistanceHandler
{
    _webViewAdditionalContextForStrongPasswordAssistanceHandler = makeBlockPtr(webViewAdditionalContextForStrongPasswordAssistanceHandler);
}

- (NSDictionary<id, NSString *> * (^)(WKWebView *))webViewAdditionalContextForStrongPasswordAssistanceHandler
{
    return _webViewAdditionalContextForStrongPasswordAssistanceHandler.get();
}

- (NSDictionary<id, NSString *> *)_webViewAdditionalContextForStrongPasswordAssistance:(WKWebView *)webView
{
    if (_webViewAdditionalContextForStrongPasswordAssistanceHandler)
        return _webViewAdditionalContextForStrongPasswordAssistanceHandler(webView);
    return @{ };
}

- (void)setFocusRequiresStrongPasswordAssistanceHandler:(BOOL (^)(WKWebView *, id <_WKFocusedElementInfo>))handler
{
    _focusRequiresStrongPasswordAssistanceHandler = makeBlockPtr(handler);
}

- (BOOL (^)(WKWebView *, id <_WKFocusedElementInfo>))focusRequiresStrongPasswordAssistanceHandler
{
    return _focusRequiresStrongPasswordAssistanceHandler.get();
}

- (BOOL)_webView:(WKWebView *)webView focusRequiresStrongPasswordAssistance:(id <_WKFocusedElementInfo>)info
{
    if (_focusRequiresStrongPasswordAssistanceHandler)
        return _focusRequiresStrongPasswordAssistanceHandler(webView, info);
    return NO;
}

@end

#endif // PLATFORM(IOS_FAMILY)
