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

static double evaluateForNumber(RetainPtr<TestWKWebView>& webView, NSString *script)
{
    return [(NSNumber *)[webView objectByEvaluatingJavaScript:script] doubleValue];
}

static double dimensionOfElement(RetainPtr<TestWKWebView>& webView, NSString *element, NSString *dimension)
{
    return evaluateForNumber(webView, [NSString stringWithFormat:@"%@.getBoundingClientRect().%@", element, dimension]);
}

static double widthOfElementWithID(RetainPtr<TestWKWebView>& webView, NSString *elementID)
{
    return dimensionOfElement(webView, [NSString stringWithFormat:@"document.getElementById('%@')", elementID], @"width");
}

static double heightOfElementWithID(RetainPtr<TestWKWebView>& webView, NSString *elementID)
{
    return dimensionOfElement(webView, [NSString stringWithFormat:@"document.getElementById('%@')", elementID], @"height");
}

static double viewportUnitLength(RetainPtr<TestWKWebView>& webView, NSString *viewportUnit)
{
    return heightOfElementWithID(webView, viewportUnit);
}

TEST(CSSViewportUnits, AllSame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

#if PLATFORM(IOS_FAMILY)
    [webView scrollView].zoomScale = 2;
#elif PLATFORM(MAC)
    [webView setAllowsMagnification:YES];
    [webView setMagnification:2];
#endif
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.writingMode = 'vertical-lr'"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }
}

#if PLATFORM(IOS_FAMILY)

TEST(CSSViewportUnits, EmptyUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5)
                              maximumUnobscuredSizeOverride:CGSizeZero];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView scrollView].zoomScale = 2;
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.writingMode = 'vertical-lr'"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, SameUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5)
                              maximumUnobscuredSizeOverride:CGSizeMake(10.5, 20.5)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView scrollView].zoomScale = 2;
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.writingMode = 'vertical-lr'"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, DifferentUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5)
                              maximumUnobscuredSizeOverride:CGSizeMake(30.5, 40.5)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView scrollView].zoomScale = 2;
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.writingMode = 'vertical-lr'"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed");
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }
}

#endif // PLATFORM(IOS_FAMILY)

TEST(CSSViewportUnits, SVGDocument)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"CSSViewportUnits" withExtension:@"svg" subdirectory:@"TestWebKitAPI.resources"]]];
    [webView _test_waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    {
        double vw = viewportUnitLength(webView, @"vw");
        double vh = viewportUnitLength(webView, @"vh");
        EXPECT_GE(vw, 320);
        EXPECT_GE(vh, 500);
        EXPECT_FLOAT_EQ(vw, viewportUnitLength(webView, @"vmin"));
        EXPECT_FLOAT_EQ(vh, viewportUnitLength(webView, @"vmax"));
        EXPECT_FLOAT_EQ(vh, viewportUnitLength(webView, @"vb"));
        EXPECT_FLOAT_EQ(vw, viewportUnitLength(webView, @"vi"));

        double svw = viewportUnitLength(webView, @"svw");
        double svh = viewportUnitLength(webView, @"svh");
        EXPECT_FLOAT_EQ(vw, svw);
        EXPECT_FLOAT_EQ(vh, svh);
        EXPECT_FLOAT_EQ(svw, viewportUnitLength(webView, @"svmin"));
        EXPECT_FLOAT_EQ(svh, viewportUnitLength(webView, @"svmax"));
        EXPECT_FLOAT_EQ(svh, viewportUnitLength(webView, @"svb"));
        EXPECT_FLOAT_EQ(svw, viewportUnitLength(webView, @"svi"));

        double lvw = viewportUnitLength(webView, @"lvw");
        double lvh = viewportUnitLength(webView, @"lvh");
        EXPECT_FLOAT_EQ(vw, lvw);
        EXPECT_FLOAT_EQ(vh, lvh);
        EXPECT_FLOAT_EQ(lvw, viewportUnitLength(webView, @"lvmin"));
        EXPECT_FLOAT_EQ(lvh, viewportUnitLength(webView, @"lvmax"));
        EXPECT_FLOAT_EQ(lvh, viewportUnitLength(webView, @"lvb"));
        EXPECT_FLOAT_EQ(lvw, viewportUnitLength(webView, @"lvi"));

        double dvw = viewportUnitLength(webView, @"dvw");
        double dvh = viewportUnitLength(webView, @"dvh");
        EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), dvw);
        EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), dvh);
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvi"));
    };

#if PLATFORM(IOS_FAMILY)
    [webView scrollView].zoomScale = 2;
#elif PLATFORM(MAC)
    [webView setAllowsMagnification:YES];
    [webView setMagnification:2];
#endif
    [webView waitForNextPresentationUpdate];

    {
        double vw = viewportUnitLength(webView, @"vw");
        double vh = viewportUnitLength(webView, @"vh");
        EXPECT_GE(vw, 320);
        EXPECT_GE(vh, 500);
        EXPECT_FLOAT_EQ(vw, viewportUnitLength(webView, @"vmin"));
        EXPECT_FLOAT_EQ(vh, viewportUnitLength(webView, @"vmax"));
        EXPECT_FLOAT_EQ(vh, viewportUnitLength(webView, @"vb"));
        EXPECT_FLOAT_EQ(vw, viewportUnitLength(webView, @"vi"));

        double svw = viewportUnitLength(webView, @"svw");
        double svh = viewportUnitLength(webView, @"svh");
        EXPECT_FLOAT_EQ(vw, svw);
        EXPECT_FLOAT_EQ(vh, svh);
        EXPECT_FLOAT_EQ(svw, viewportUnitLength(webView, @"svmin"));
        EXPECT_FLOAT_EQ(svh, viewportUnitLength(webView, @"svmax"));
        EXPECT_FLOAT_EQ(svh, viewportUnitLength(webView, @"svb"));
        EXPECT_FLOAT_EQ(svw, viewportUnitLength(webView, @"svi"));

        double lvw = viewportUnitLength(webView, @"lvw");
        double lvh = viewportUnitLength(webView, @"lvh");
        EXPECT_FLOAT_EQ(vw, lvw);
        EXPECT_FLOAT_EQ(vh, lvh);
        EXPECT_FLOAT_EQ(lvw, viewportUnitLength(webView, @"lvmin"));
        EXPECT_FLOAT_EQ(lvh, viewportUnitLength(webView, @"lvmax"));
        EXPECT_FLOAT_EQ(lvh, viewportUnitLength(webView, @"lvb"));
        EXPECT_FLOAT_EQ(lvw, viewportUnitLength(webView, @"lvi"));

        double dvw = viewportUnitLength(webView, @"dvw");
        double dvh = viewportUnitLength(webView, @"dvh");
        EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), dvw);
        EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), dvh);
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvi"));
    }

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.writingMode = 'vertical-lr'"];
    [webView waitForNextPresentationUpdate];

    {
        double vw = viewportUnitLength(webView, @"vw");
        double vh = viewportUnitLength(webView, @"vh");
        EXPECT_GE(vw, 320);
        EXPECT_GE(vh, 500);
        EXPECT_FLOAT_EQ(vw, viewportUnitLength(webView, @"vmin"));
        EXPECT_FLOAT_EQ(vh, viewportUnitLength(webView, @"vmax"));
        EXPECT_FLOAT_EQ(vw, viewportUnitLength(webView, @"vb"));
        EXPECT_FLOAT_EQ(vh, viewportUnitLength(webView, @"vi"));

        double svw = viewportUnitLength(webView, @"svw");
        double svh = viewportUnitLength(webView, @"svh");
        EXPECT_FLOAT_EQ(vw, svw);
        EXPECT_FLOAT_EQ(vh, svh);
        EXPECT_FLOAT_EQ(svw, viewportUnitLength(webView, @"svmin"));
        EXPECT_FLOAT_EQ(svh, viewportUnitLength(webView, @"svmax"));
        EXPECT_FLOAT_EQ(svw, viewportUnitLength(webView, @"svb"));
        EXPECT_FLOAT_EQ(svh, viewportUnitLength(webView, @"svi"));

        double lvw = viewportUnitLength(webView, @"lvw");
        double lvh = viewportUnitLength(webView, @"lvh");
        EXPECT_FLOAT_EQ(vw, lvw);
        EXPECT_FLOAT_EQ(vh, lvh);
        EXPECT_FLOAT_EQ(lvw, viewportUnitLength(webView, @"lvmin"));
        EXPECT_FLOAT_EQ(lvh, viewportUnitLength(webView, @"lvmax"));
        EXPECT_FLOAT_EQ(lvw, viewportUnitLength(webView, @"lvb"));
        EXPECT_FLOAT_EQ(lvh, viewportUnitLength(webView, @"lvi"));

        double dvw = viewportUnitLength(webView, @"dvw");
        double dvh = viewportUnitLength(webView, @"dvh");
        EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), dvw);
        EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), dvh);
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvi"));
    }
}
