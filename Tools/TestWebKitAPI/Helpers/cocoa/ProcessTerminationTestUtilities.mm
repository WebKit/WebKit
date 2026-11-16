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

#import "config.h"
#import "Helpers/cocoa/ProcessTerminationTestUtilities.h"

#import <signal.h>
#import <wtf/Seconds.h>
#import <wtf/Threading.h>

namespace TestWebKitAPI {

bool processStillRunning(pid_t pid)
{
    return !kill(pid, 0);
}

bool waitForProcessExitWithoutSpinningRunLoop(pid_t pid)
{
    for (unsigned i = 0; i < 100; ++i) {
        if (kill(pid, 0) && errno == ESRCH)
            return true;
        WTF::sleep(100_ms);
    }
    return false;
}

} // namespace TestWebKitAPI

@interface TestNavigationDelegate (ProcessTerminationTestUtilities)
- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason;
@end

@implementation CrashCountingNavigationDelegate {
    BOOL _hasCrashHandler;
}

- (instancetype)initWithCrashHandler:(BOOL)hasCrashHandler
{
    if (!(self = [super init]))
        return nil;
    _hasCrashHandler = hasCrashHandler;
    return self;
}

- (instancetype)init
{
    return [self initWithCrashHandler:YES];
}

- (BOOL)respondsToSelector:(SEL)selector
{
    if (!_hasCrashHandler && selector == @selector(_webView:webContentProcessDidTerminateWithReason:))
        return NO;
    return [super respondsToSelector:selector];
}

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    ++_crashCount;
    _lastCrashReason = reason;
    [super _webView:webView webContentProcessDidTerminateWithReason:reason];
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    ++_didStartProvisionalNavigationCount;
    [super webView:webView didStartProvisionalNavigation:navigation];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    ++_didFinishNavigationCount;
    [super webView:webView didFinishNavigation:navigation];
}

@end
