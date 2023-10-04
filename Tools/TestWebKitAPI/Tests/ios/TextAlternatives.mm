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

#if PLATFORM(IOS_FAMILY)

#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>

@interface TestWKWebView (TextAlternatives)
- (NSUInteger)dictationAlternativesMarkerCount:(NSString *)evaluateNodeExpression;
@end

@implementation TestWKWebView (TextAlternatives)

- (NSUInteger)dictationAlternativesMarkerCount:(NSString *)evaluateNodeExpression
{
    NSString *scriptToEvaluate = [NSString stringWithFormat:@"internals.markerCountForNode((() => %@)(), 'dictationalternatives')", evaluateNodeExpression];
    NSNumber *result = [self objectByEvaluatingJavaScript:scriptToEvaluate];
    return result.unsignedIntegerValue;
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewForTestingTextAlternatives()
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    [webView _setEditable:YES];
    return webView;
}

TEST(TextAlternatives, AddAndRemoveTextAlternativesWithMatch)
{
    auto webView = createWebViewForTestingTextAlternatives();
    [webView synchronouslyLoadHTMLString:@"<body>hello world&nbsp;</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    auto alternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:@"hello world" alternativeStrings:@[ @"👋🌎" ]]);
    [[webView textInputContentView] addTextAlternatives:alternatives.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);

    // This should not remove the text alternatives above, since the selection does not intersect with the dictation alternative marker.
    [[webView textInputContentView] removeEmojiAlternatives];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);

    [[webView textInputContentView] moveByOffset:-1];
    [webView waitForNextPresentationUpdate];
    [[webView textInputContentView] removeEmojiAlternatives];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(TextAlternatives, AddAndRemoveTextAlternativesWithTextAndEmojis)
{
    auto webView = createWebViewForTestingTextAlternatives();
    [webView synchronouslyLoadHTMLString:@"<body>hello world</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    auto alternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:@"hello world" alternativeStrings:@[ @"👋🌎", @"Hello world" ]]);
    [[webView textInputContentView] addTextAlternatives:alternatives.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);

    // This should only remove one of the two text alternative strings, which leaves the document marker intact.
    [[webView textInputContentView] removeEmojiAlternatives];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(TextAlternatives, AddTextAlternativesWithSelectedMatch)
{
    auto webView = createWebViewForTestingTextAlternatives();
    [webView synchronouslyLoadHTMLString:@"<body>hello world&nbsp;</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];

    auto alternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:@"hello world" alternativeStrings:@[ @"👋🌎" ]]);
    [[webView textInputContentView] addTextAlternatives:alternatives.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(1U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);
}

TEST(TextAlternatives, AddTextAlternativesWithoutMatch)
{
    auto webView = createWebViewForTestingTextAlternatives();
    [webView synchronouslyLoadHTMLString:@"<body>hello world&nbsp;</body>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];

    auto alternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:@"goodbye world" alternativeStrings:@[ @"👋🌎" ]]);
    [[webView textInputContentView] addTextAlternatives:alternatives.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(0U, [webView dictationAlternativesMarkerCount:@"document.body.childNodes[0]"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
