/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>

TEST(ProcessInfo, BasicTest)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"Hello world!"];

#if ENABLE(GPU_PROCESS_DOM_RENDERING_BY_DEFAULT)
    RetainPtr gpuProcessInfo = [WKProcessPool _gpuProcessInfo];
    EXPECT_TRUE(gpuProcessInfo);
    EXPECT_NE([gpuProcessInfo pid], pid_t { });
    EXPECT_GT([gpuProcessInfo physicalFootprint], 0u);
#endif

    RetainPtr networkProcessInfo = [WKProcessPool _networkingProcessInfo];
    EXPECT_GT([networkProcessInfo count], 0u);
    for (_WKProcessInfo *info in networkProcessInfo.get()) {
        EXPECT_NE([info pid], pid_t { });
        EXPECT_GT([info physicalFootprint], 0u);
    }

    bool foundWebView = false;
    RetainPtr webContentProcessInfo = [WKProcessPool _webContentProcessInfo];
    EXPECT_GT([webContentProcessInfo count], 0u);
    for (_WKWebContentProcessInfo *info in webContentProcessInfo.get()) {
        EXPECT_NE([info pid], pid_t { });
        EXPECT_GT([info physicalFootprint], 0u);

        if ([[info webViews] containsObject:webView.get()])
            foundWebView = true;
    }
    EXPECT_TRUE(foundWebView);
}

#endif
