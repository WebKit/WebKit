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

#import "InstanceMethodSwizzler.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKWebViewPrivate.h>

#if PLATFORM(IOS_FAMILY)

static CGRect mainScreenReferenceBoundsOverride(id, SEL)
{
    return CGRectMake(0, 0, 320, 568);
}

TEST(TextAutosizingBoost, ChangeAutosizingBoostAtRuntime)
{
    static NSString *testMarkup = @"<meta name='viewport' content='width=device-width'><body style='margin: 0'><span id='top'>Hello world</span><br><span id='bottom'>Goodbye world</span></body>";

    InstanceMethodSwizzler screenSizeSwizzler(UIScreen.class, @selector(_referenceBounds), reinterpret_cast<IMP>(mainScreenReferenceBoundsOverride));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 960, 360)]);
    WKPreferencesSetTextAutosizingEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    WKPreferencesSetTextAutosizingUsesIdempotentMode((__bridge WKPreferencesRef)[webView configuration].preferences, false);
    [webView synchronouslyLoadHTMLString:testMarkup];
    CGSize regularSize {
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().width"] floatValue]),
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().height"] floatValue])
    };

    [webView configuration].preferences._shouldEnableTextAutosizingBoost = YES;
    [webView waitForNextPresentationUpdate];
    CGSize boostedSize {
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().width"] floatValue]),
        roundf([[webView objectByEvaluatingJavaScript:@"document.getElementById('top').getBoundingClientRect().height"] floatValue])
    };

    EXPECT_EQ(125, regularSize.width);
    EXPECT_EQ(30, regularSize.height);
    EXPECT_EQ(159, boostedSize.width);
    EXPECT_EQ(38, boostedSize.height);
}

// Regression test for rdar://113801810: autosized text must not stay inflated at
// landscape size after rotation to portrait.
TEST(TextAutosizingBoost, RotationDoesNotPersistAutosizedFontSize)
{
    // Inline body width set from script (cleared in the resize handler) reproduces
    // the stale-width condition during rotation. Multi-line content is required
    // because a single-line block does not inflate regardless of block width.
    static NSString *testMarkup =
        @"<meta name='viewport' content='width=device-width'>"
        "<body style='margin: 0'>"
        "<span id='top'>Hello world</span>"
        "<br>"
        "<span>Goodbye world</span>"
        "<script>"
        "document.body.style.width='960px';"
        "window.addEventListener('resize',function(){document.body.style.removeProperty('width');});"
        "</script>"
        "</body>";

    InstanceMethodSwizzler screenSizeSwizzler(UIScreen.class, @selector(_referenceBounds), reinterpret_cast<IMP>(mainScreenReferenceBoundsOverride));

    CGSize wideSize { 960, 360 };
    CGSize narrowSize { 320, 568 };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, wideSize.width, wideSize.height)]);
    WKPreferencesSetTextAutosizingEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    WKPreferencesSetTextAutosizingUsesIdempotentMode((__bridge WKPreferencesRef)[webView configuration].preferences, false);
    [webView synchronouslyLoadHTMLString:testMarkup];
    // _beginAnimatedResizeWithUpdates: only takes the animated path once the first
    // frame has been committed.
    [webView waitForNextPresentationUpdate];

    float wideFontSize = [[webView objectByEvaluatingJavaScript:@"parseFloat(getComputedStyle(document.getElementById('top')).fontSize)"] floatValue];

    // Animated resize is the only path that exercises the fix.
    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, narrowSize.width, narrowSize.height)];
    }];
    [webView _endAnimatedResize];
    [webView waitForNextPresentationUpdate];

    float narrowFontSize = [[webView objectByEvaluatingJavaScript:@"parseFloat(getComputedStyle(document.getElementById('top')).fontSize)"] floatValue];

    EXPECT_GT(wideFontSize, 16.0);
    EXPECT_EQ(16.0, narrowFontSize);
}

#endif // PLATFORM(IOS_FAMILY)
