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

#if WK_API_ENABLED && PLATFORM(IOS)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

using namespace TestWebKitAPI;

@implementation TestWKWebView (ViewportTestingHelpers)

- (BOOL)scaleIsApproximately:(CGFloat)expectedScale
{
    static const double maximumExpectedScaleDifference = 0.001;
    return ABS(self.scrollView.zoomScale - expectedScale) < maximumExpectedScaleDifference;
}

- (void)expectScaleToBecome:(CGFloat)expectedScale
{
    while (![self scaleIsApproximately:expectedScale])
        [self waitForNextPresentationUpdate];
}

@end

static NSString *viewportTestPageMarkup(NSString *viewportMetaContent, int contentWidth, int contentHeight)
{
    return [NSString stringWithFormat:@"<meta name='viewport' content='%@'><body style='margin: 0'><div style='width: %dpx; height: %dpx; background-color: red'></div>", viewportMetaContent, contentWidth, contentHeight];
}

namespace TestWebKitAPI {

TEST(ViewportSizingTests, ForceShrinkToFitViewportOverridesViewportParameters)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView _setAllowsViewportShrinkToFit:YES];
    [webView _setForceHorizontalViewportShrinkToFit:NO];
    [webView synchronouslyLoadHTMLString:viewportTestPageMarkup(@"width=device-width, shrink-to-fit=no", 600, 100)];
    [webView expectScaleToBecome:1];

    [webView _setForceHorizontalViewportShrinkToFit:YES];
    [webView expectScaleToBecome:0.667];

    [webView _setForceHorizontalViewportShrinkToFit:NO];
    [webView expectScaleToBecome:1];
}

TEST(ViewportSizingTests, ShrinkToFitViewportWithMinimumAllowedLayoutWidth)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView _setAllowsViewportShrinkToFit:YES];
    [webView _setForceHorizontalViewportShrinkToFit:YES];
    [webView _setMinimumAllowedLayoutWidth:0];
    [webView synchronouslyLoadHTMLString:viewportTestPageMarkup(@"width=device-width, initial-scale=1", 400, 100)];
    [webView expectScaleToBecome:0.5];
    EXPECT_WK_STREQ("200", [webView stringByEvaluatingJavaScript:@"document.body.clientWidth"]);

    [webView _setMinimumAllowedLayoutWidth:400];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("400", [webView stringByEvaluatingJavaScript:@"document.body.clientWidth"]);
    EXPECT_TRUE([webView scaleIsApproximately:0.5]);

    [webView _setMinimumAllowedLayoutWidth:100];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("200", [webView stringByEvaluatingJavaScript:@"document.body.clientWidth"]);
    EXPECT_TRUE([webView scaleIsApproximately:0.5]);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS)
