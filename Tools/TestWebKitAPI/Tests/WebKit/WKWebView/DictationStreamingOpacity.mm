/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>

@interface UIView (DictationStreamingOpacityTesting)
- (void)_setDictationStreamingOpacity:(CGFloat)opacity forHypothesisText:(NSString *)hypothesisText streamingRange:(NSRange)streamingRange;
- (void)_clearDictationStreamingOpacity;
@end

@interface TestWKWebView (DictationStreamingOpacity)
- (NSUInteger)dictationStreamingOpacityMarkerCount:(NSString *)nodeExpression;
@end

@implementation TestWKWebView (DictationStreamingOpacity)

- (NSUInteger)dictationStreamingOpacityMarkerCount:(NSString *)nodeExpression
{
    RetainPtr script = [NSString stringWithFormat:@"internals.markerCountForNode((() => %@)(), 'dictationstreamingopacity')", nodeExpression];
    return [[self objectByEvaluatingJavaScript:script.get()] unsignedIntegerValue];
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewForDictationStreamingOpacity()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView _setEditable:YES];
    return webView;
}

TEST(DictationStreamingOpacity, SetAndClearMarker)
{
    RetainPtr webView = createWebViewForDictationStreamingOpacity();
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1U, [webView dictationStreamingOpacityMarkerCount:@"document.body.childNodes[0]"]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationStreamingOpacityMarker(0, 5)"] boolValue]);

    [[webView textInputContentView] _clearDictationStreamingOpacity];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0U, [webView dictationStreamingOpacityMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(DictationStreamingOpacity, MarkerAppliedToCorrectSubRange)
{
    RetainPtr webView = createWebViewForDictationStreamingOpacity();
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(5, 6)];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationStreamingOpacityMarker(5, 6)"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationStreamingOpacityMarker(0, 5)"] boolValue]);
}

TEST(DictationStreamingOpacity, UpdateReplacesOldMarker)
{
    RetainPtr webView = createWebViewForDictationStreamingOpacity();
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationStreamingOpacityMarker(0, 5)"] boolValue]);

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(5, 6)];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"internals.hasDictationStreamingOpacityMarker(5, 6)"] boolValue]);
    EXPECT_EQ(1U, [webView dictationStreamingOpacityMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(DictationStreamingOpacity, HypothesisTextMismatch)
{
    RetainPtr webView = createWebViewForDictationStreamingOpacity();
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"goodbye" streamingRange:NSMakeRange(0, 7)];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0U, [webView dictationStreamingOpacityMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(DictationStreamingOpacity, NonEditableContent)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0U, [webView dictationStreamingOpacityMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(DictationStreamingOpacity, EmptyHypothesisText)
{
    RetainPtr webView = createWebViewForDictationStreamingOpacity();
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"" streamingRange:NSMakeRange(0, 0)];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0U, [webView dictationStreamingOpacityMarkerCount:@"document.body.childNodes[0]"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
