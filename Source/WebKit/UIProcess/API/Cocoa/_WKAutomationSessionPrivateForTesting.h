/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import <WebKit/_WKAutomationSession.h>

@class WKWebView;

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

/// Testing-only SPI that lets in-process API tests drive the Automation
/// backend without a RemoteInspector transport. Not intended for production
/// clients; the surface exists solely to exercise WebAutomationSession's
/// dispatch path (and downstream Inspector backend round-trips) from unit
/// tests in TestWebKitAPI.
@interface _WKAutomationSession (PrivateForTesting)

/// Register the given WKWebView as a browsing context for this session and
/// return the handle. The session's process pool must already include the
/// web view's process. Subsequent Automation commands referencing the
/// returned handle will resolve to this WKWebView.
- (NSString *)_registerWebViewForTesting:(WKWebView *)webView;

/// Install a block that captures every outbound JSON-RPC message the
/// session would otherwise send through its FrontendChannel (responses
/// and events). Replaces any previously installed handler. Passing nil
/// detaches the test channel.
- (void)_setMessageToFrontendHandlerForTesting:(nullable void (^)(NSString *message))handler;

/// Drive an inbound JSON-RPC message into the Automation backend dispatcher
/// as if it arrived from the remote driver. Synchronous.
- (void)_dispatchMessageFromRemoteForTesting:(NSString *)message;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
