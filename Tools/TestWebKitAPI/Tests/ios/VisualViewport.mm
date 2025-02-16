/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(VisualViewport, RotationResize)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 430, 812) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:
        @"<!DOCTYPE html>"
        @"<html>"
        @"<head>"
            @"<meta name='viewport' width='700' content='initial-scale=1'/>"
            @"<script>"
                @"window.resizeCount = 0;"
                @"let metaTagNewWidth;"
                @"const visualViewport = window.visualViewport;"
                @"window.visualViewport.addEventListener(\"resize\", function() {"
                    @"var newWidth = Math.round(visualViewport.width * visualViewport.scale);"
                    @"newWidth = 300 >= newWidth ? 300 : 700 <= newWidth ? 700 : \"device-width\";"
                    @"metaTagNewWidth = document.querySelector(\"meta[name=viewport]\").content = \"width=\" + newWidth;"
                    @"window.resizeCount = window.resizeCount + 1;"
                    @"if (window.resizeCount == 2 && (window.innerWidth != 812 || window.innerHeight != 430 || visualViewport.scale != 1))"
                        @"window.webkit.messageHandlers.testHandler.postMessage(\"fail\");"
                @"});"
            @"</script>"
        @"</head>"
        @"</html>"];

    [webView performAfterReceivingMessage:@"fail" action:^{
        FAIL();
    }];

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.height, [webView frame].size.width)];
    }];

    [webView _endAnimatedResize];
    [webView waitForNextPresentationUpdate];

    int windowResizeCount = [[webView objectByEvaluatingJavaScript:@"window.resizeCount"] intValue];

    EXPECT_EQ(windowResizeCount, 2);
}
}

#endif
