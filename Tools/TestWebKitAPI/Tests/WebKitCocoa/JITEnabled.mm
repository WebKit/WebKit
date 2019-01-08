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

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, JITEnabled)
{
    auto checkJITEnabled = [] (RetainPtr<WKWebView>&& webView, BOOL expectedValue) {
        __block bool done = false;
        [webView evaluateJavaScript:@"for(i=0;i<100000;++i);'abc'" completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE(error == nil);
            EXPECT_STREQ([result UTF8String], "abc");
            [webView _isJITEnabled:^(BOOL enabled) {
                EXPECT_TRUE(enabled == expectedValue);
                done = true;
            }];
        }];
        TestWebKitAPI::Util::run(&done);
    };

    auto processPoolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
    [processPoolConfiguration setJITEnabled:NO];
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setProcessPool:[[[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()] autorelease]];
    auto webViewNoJIT = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    checkJITEnabled(WTFMove(webViewNoJIT), NO);
    checkJITEnabled(adoptNS([WKWebView new]), YES);
}

#endif
