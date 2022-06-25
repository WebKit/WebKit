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

#import "config.h"
#import "TestWKWebViewController.h"

#if PLATFORM(IOS_FAMILY)

#import <wtf/BlockPtr.h>

@implementation TestWKWebViewControllerWindow

// These internal methods need to be stubbed out to prevent UIKit from throwing inconsistency
// exceptions when making a UIWindow the key window, since TestWebKitAPI is not a UI application.
// These stubs and this class can be removed once https://webkit.org/b/175204 is fixed.
- (void)_beginKeyWindowDeferral
{
}

- (void)_endKeyWindowDeferral
{
}

@end

@implementation TestWKWebViewController {
    RetainPtr<WKWebViewConfiguration> _configuration;
    CGRect _initialViewFrame;
    RetainPtr<TestWKWebView> _webView;
    BlockPtr<void()> _dismissalHandler;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    self = [super init];
    if (!self)
        return nil;

    _initialViewFrame = frame;
    _configuration = configuration ?: adoptNS([[WKWebViewConfiguration alloc] init]);
    return self;
}

- (void)loadView
{
    _webView = adoptNS([[TestWKWebView alloc] initWithFrame:_initialViewFrame configuration:_configuration.get() addToWindow:NO]);
    self.view = _webView.get();
}

- (TestWKWebView *)webView
{
    [self loadViewIfNeeded];
    return _webView.get();
}

- (void)dismissViewControllerAnimated:(BOOL)flag completion:(void (^)())completion
{
    if (_dismissalHandler)
        _dismissalHandler();
    [super dismissViewControllerAnimated:flag completion:completion];
}

- (dispatch_block_t)dismissalHandler
{
    return _dismissalHandler.get();
}

- (void)setDismissalHandler:(dispatch_block_t)dismissalHandler
{
    _dismissalHandler = makeBlockPtr(dismissalHandler);
}

@end

#endif // PLATFORM(IOS_FAMILY)
