/*
 * Copyright (C) 2021 Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static int evaluateForInt(WKWebView *webView, NSString *script)
{
    return [(NSNumber *)[webView objectByEvaluatingJavaScript:script] intValue];
}

static int getElementHeight(WKWebView *webView, NSString *elementID)
{
    return evaluateForInt(webView, [NSString stringWithFormat:@"document.getElementById('%@').getBoundingClientRect().height", elementID]);
}

TEST(CSSViewportUnits, AllSame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    EXPECT_EQ(320, getElementHeight(webView.get(), @"vw"));
    EXPECT_EQ(500, getElementHeight(webView.get(), @"vh"));
    EXPECT_EQ(320, getElementHeight(webView.get(), @"vmin"));
    EXPECT_EQ(500, getElementHeight(webView.get(), @"vmax"));

    EXPECT_EQ(320, getElementHeight(webView.get(), @"svw"));
    EXPECT_EQ(500, getElementHeight(webView.get(), @"svh"));
    EXPECT_EQ(320, getElementHeight(webView.get(), @"svmin"));
    EXPECT_EQ(500, getElementHeight(webView.get(), @"svmax"));

    EXPECT_EQ(320, getElementHeight(webView.get(), @"lvw"));
    EXPECT_EQ(500, getElementHeight(webView.get(), @"lvh"));
    EXPECT_EQ(320, getElementHeight(webView.get(), @"lvmin"));
    EXPECT_EQ(500, getElementHeight(webView.get(), @"lvmax"));

    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvw"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvh"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvmin"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvmax"));
}

#if PLATFORM(IOS_FAMILY)

TEST(CSSViewportUnits, EmptyUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10, 20)
                              maximumUnobscuredSizeOverride:CGSizeZero];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    EXPECT_EQ(10, getElementHeight(webView.get(), @"vw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"vh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"vmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"vmax"));

    EXPECT_EQ(10, getElementHeight(webView.get(), @"svw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"svh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"svmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"svmax"));

    EXPECT_EQ(10, getElementHeight(webView.get(), @"lvw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"lvh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"lvmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"lvmax"));

    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvw"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvh"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvmin"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvmax"));
}

TEST(CSSViewportUnits, SameUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10, 20)
                              maximumUnobscuredSizeOverride:CGSizeMake(10, 20)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    EXPECT_EQ(10, getElementHeight(webView.get(), @"vw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"vh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"vmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"vmax"));

    EXPECT_EQ(10, getElementHeight(webView.get(), @"svw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"svh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"svmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"svmax"));

    EXPECT_EQ(10, getElementHeight(webView.get(), @"lvw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"lvh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"lvmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"lvmax"));

    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvw"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvh"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvmin"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvmax"));
}

TEST(CSSViewportUnits, DifferentUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10, 20)
                              maximumUnobscuredSizeOverride:CGSizeMake(30, 40)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    EXPECT_EQ(30, getElementHeight(webView.get(), @"vw"));
    EXPECT_EQ(40, getElementHeight(webView.get(), @"vh"));
    EXPECT_EQ(30, getElementHeight(webView.get(), @"vmin"));
    EXPECT_EQ(40, getElementHeight(webView.get(), @"vmax"));

    EXPECT_EQ(10, getElementHeight(webView.get(), @"svw"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"svh"));
    EXPECT_EQ(10, getElementHeight(webView.get(), @"svmin"));
    EXPECT_EQ(20, getElementHeight(webView.get(), @"svmax"));

    EXPECT_EQ(30, getElementHeight(webView.get(), @"lvw"));
    EXPECT_EQ(40, getElementHeight(webView.get(), @"lvh"));
    EXPECT_EQ(30, getElementHeight(webView.get(), @"lvmin"));
    EXPECT_EQ(40, getElementHeight(webView.get(), @"lvmax"));

    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvw"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvh"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerWidth"), getElementHeight(webView.get(), @"dvmin"));
    EXPECT_EQ(evaluateForInt(webView.get(), @"window.innerHeight"), getElementHeight(webView.get(), @"dvmax"));
}

#endif // PLATFORM(IOS_FAMILY)
