/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, ConfigurationCPULimit)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    EXPECT_EQ([configuration _cpuLimit], 0);
    [configuration _setCPULimit:0.75];
    EXPECT_EQ([configuration _cpuLimit], 0.75);
    auto other = adoptNS([configuration copy]);
    EXPECT_EQ([other _cpuLimit], 0.75);
}

TEST(WebKit, ConfigurationDrawsBackground)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    EXPECT_EQ([configuration _drawsBackground], YES);
    [configuration _setDrawsBackground:NO];
    EXPECT_EQ([configuration _drawsBackground], NO);

    auto other = adoptNS([configuration copy]);
    EXPECT_EQ([other _drawsBackground], NO);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect]);
    EXPECT_EQ([webView _drawsBackground], YES);

    auto configedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    EXPECT_EQ([configedWebView _drawsBackground], NO);
}

#endif
