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

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));

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

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));
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

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));

    [webView scrollView].zoomScale = 2;
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));
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

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));

    [webView scrollView].zoomScale = 2;
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"vmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));
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

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));

    [webView scrollView].zoomScale = 2;
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(20.5, viewportUnitLength(webView, @"svmax"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));

    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvw"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvh"));
    EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmin"));
    EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed"), viewportUnitLength(webView, @"dvmax"));
}

#endif // PLATFORM(IOS_FAMILY)
