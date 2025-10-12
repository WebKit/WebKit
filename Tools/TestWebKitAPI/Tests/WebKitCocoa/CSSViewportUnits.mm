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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
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

static void changeCSSPropertyOfElements(RetainPtr<TestWKWebView>& webView, NSString *selector, NSString *property, NSString *value)
{
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelectorAll('%@').forEach((element) => element.style['%@'] = %@)", selector, property, value]];
}

static float scrollbarSize()
{
    static float result = 0;
#if PLATFORM(MAC)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        RetainPtr scroller = [NSScrollerImp scrollerImpWithStyle:NSScroller.preferredScrollerStyle controlSize:NSControlSizeRegular horizontal:NO replacingScrollerImp:nil];
        result = [scroller trackBoxWidth];
    });
#endif
    return result;
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"body", @"writingMode", @"'vertical-lr'");
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"div", @"writingMode", @"'horizontal-tb'");
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

#if USE(APPKIT)
#define CocoaEdgeInsetsMake NSEdgeInsetsMake
#endif

#if PLATFORM(IOS_FAMILY)
#define CocoaEdgeInsetsMake UIEdgeInsetsMake
#endif

#define CocoaEdgeInsetsZero CocoaEdgeInsetsMake(0, 0, 0, 0)

TEST(CSSViewportUnits, NegativeMinimumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    bool didThrow = false;
    @try {
        [webView setMinimumViewportInset:CocoaEdgeInsetsMake(-11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_TRUE(didThrow);
}

TEST(CSSViewportUnits, NegativeMaximumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    bool didThrow = false;
    @try {
        [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(-12, 22, 32, 42)];
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_TRUE(didThrow);
}

TEST(CSSViewportUnits, MinimumViewportInsetLargerThanMaximumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    bool didThrow = false;
    @try {
        [webView setMinimumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42) maximumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41)];
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_TRUE(didThrow);
}

TEST(CSSViewportUnits, MinimumViewportInsetThanLargerFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    bool didThrow = false;
    @try {
        [webView setMinimumViewportInset:CocoaEdgeInsetsMake(1100, 2100, 3100, 4100) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_TRUE(didThrow);
}

TEST(CSSViewportUnits, MaximumViewportInsetThanLargerFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    bool didThrow = false;
    @try {
        [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(1200, 2200, 3200, 4200)];
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_TRUE(didThrow);
}

TEST(CSSViewportUnits, MinimumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MinimumViewportInsetWithZoom)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

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

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInsetWithZoom)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

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

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MinimumViewportInsetWithWritingMode)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    changeCSSPropertyOfElements(webView, @"body", @"writingMode", @"'vertical-lr'");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"div", @"writingMode", @"'horizontal-tb'");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInsetWithWritingMode)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    changeCSSPropertyOfElements(webView, @"body", @"writingMode", @"'vertical-lr'");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"div", @"writingMode", @"'horizontal-tb'");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MinimumViewportInsetWithFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView setFrame:CGRectMake(0, 0, 220, 400)];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(156, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(356, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(156, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(356, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(356, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(156, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(158, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(358, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(158, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(358, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(358, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(158, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInsetWithFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView setFrame:CGRectMake(0, 0, 220, 400)];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(156, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(356, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(156, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(356, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(356, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(156, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(400, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(220, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MinimumViewportInsetWithBounds)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView setBounds:CGRectMake(50, 60, 320, 500)];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInsetWithBounds)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView setBounds:CGRectMake(50, 60, 320, 500)];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed"); // No horizontal overflow.
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

#if PLATFORM(MAC)

// rdar://136706717
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 140000
TEST(CSSViewportUnits, DISABLED_TopContentInset)
#else
TEST(CSSViewportUnits, TopContentInset)
#endif
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:CGRectMake(0, 0, 320, 500) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView _setTopContentInset:10];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MinimumViewportInsetWithTopContentInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:CGRectMake(0, 0, 320, 500) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView _setTopContentInset:10];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(446, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(446, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(446, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(448, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(448, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(448, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

// rdar://136706717
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 140000
TEST(CSSViewportUnits, DISABLED_MaximumViewportInsetWithTopContentInset)
#else
TEST(CSSViewportUnits, MaximumViewportInsetWithTopContentInset)
#endif
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:CGRectMake(0, 0, 320, 500) styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    [window makeKeyAndOrderFront:nil];

    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView _setAutomaticallyAdjustsContentInsets:NO];
    [webView _setTopContentInset:10];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(446, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(446, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(446, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(490, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed");
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

TEST(CSSViewportUnits, MinimumViewportInsetWithContentInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView scrollView].contentInset = CocoaEdgeInsetsMake(50, 60, 70, 80);

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInsetWithContentInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView scrollView].contentInset = CocoaEdgeInsetsMake(50, 60, 70, 80);

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MinimumViewportInsetWithSafeAreaInsets)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    [viewController setAdditionalSafeAreaInsets:CocoaEdgeInsetsMake(50, 60, 70, 80)];
    [viewController setView:webView.get()];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(458, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(258, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, MaximumViewportInsetWithSafeAreaInsets)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    [viewController setAdditionalSafeAreaInsets:CocoaEdgeInsetsMake(50, 60, 70, 80)];
    [viewController setView:webView.get()];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(456, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(256, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(500, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(320, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, UnobscuredSizeOverridesIgnoreMinimumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsMake(11, 21, 31, 41) maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5) minimumUnobscuredSizeOverride:CGSizeMake(10.5, 30.5) maximumUnobscuredSizeOverride:CGSizeMake(30.5, 40.5)];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, UnobscuredSizeOverridesIgnoreMaximumViewportInset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setMinimumViewportInset:CocoaEdgeInsetsZero maximumViewportInset:CocoaEdgeInsetsMake(12, 22, 32, 42)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];

    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5) minimumUnobscuredSizeOverride:CGSizeMake(10.5, 30.5) maximumUnobscuredSizeOverride:CGSizeMake(30.5, 40.5)];

    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, EmptyUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5) minimumUnobscuredSizeOverride:CGSizeZero maximumUnobscuredSizeOverride:CGSizeZero];
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"body", @"writingMode", @"'vertical-lr'");
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"div", @"writingMode", @"'horizontal-tb'");
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, SameUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto unobscuredSizeOverride = CGSizeMake(10.5, 20.5);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:unobscuredSizeOverride minimumUnobscuredSizeOverride:unobscuredSizeOverride maximumUnobscuredSizeOverride:unobscuredSizeOverride];
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"body", @"writingMode", @"'vertical-lr'");
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"div", @"writingMode", @"'horizontal-tb'");
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
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

TEST(CSSViewportUnits, DifferentUnobscuredSizeOverrides)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _overrideLayoutParametersWithMinimumLayoutSize:CGSizeMake(10.5, 20.5) minimumUnobscuredSizeOverride:CGSizeMake(10.5, 30.5) maximumUnobscuredSizeOverride:CGSizeMake(30.5, 40.5)];
    [webView synchronouslyLoadTestPageNamed:@"CSSViewportUnits"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
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
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"body", @"writingMode", @"'vertical-lr'");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"div", @"writingMode", @"'horizontal-tb'");
    [webView waitForNextPresentationUpdate];

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"vb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"vi"));

    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svw"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svh"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svmin"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svmax"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"svb"));
    EXPECT_FLOAT_EQ(10.5, viewportUnitLength(webView, @"svi"));

    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvw"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvh"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvmin"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvmax"));
    EXPECT_FLOAT_EQ(40.5, viewportUnitLength(webView, @"lvb"));
    EXPECT_FLOAT_EQ(30.5, viewportUnitLength(webView, @"lvi"));

    {
        double fixedWidth = widthOfElementWithID(webView, @"fixed") + scrollbarSize();
        double fixedHeight = heightOfElementWithID(webView, @"fixed") + scrollbarSize();
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvw"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvh"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(fixedHeight, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(fixedWidth, viewportUnitLength(webView, @"dvi"));
    }
}

#endif // PLATFORM(IOS_FAMILY)

TEST(CSSViewportUnits, SVGDocument)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"CSSViewportUnits" withExtension:@"svg"]]];
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
        EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed") + scrollbarSize(), dvw);
        EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed") + scrollbarSize(), dvh);
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"svg", @"writingMode", @"'vertical-lr'");
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
        EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed") + scrollbarSize(), dvw);
        EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed") + scrollbarSize(), dvh);
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvi"));
    }

    changeCSSPropertyOfElements(webView, @"rect", @"writingMode", @"'horizontal-tb'");
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
        EXPECT_FLOAT_EQ(widthOfElementWithID(webView, @"fixed") + scrollbarSize(), dvw);
        EXPECT_FLOAT_EQ(heightOfElementWithID(webView, @"fixed") + scrollbarSize(), dvh);
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvmin"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvmax"));
        EXPECT_FLOAT_EQ(dvh, viewportUnitLength(webView, @"dvb"));
        EXPECT_FLOAT_EQ(dvw, viewportUnitLength(webView, @"dvi"));
    }

}
