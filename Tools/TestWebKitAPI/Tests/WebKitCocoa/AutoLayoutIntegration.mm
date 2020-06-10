/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

static bool didInvalidateIntrinsicContentSize;
static bool didEvaluateJavaScript;

@interface AutoLayoutWKWebView : WKWebView
@property (nonatomic) BOOL expectingIntrinsicContentSizeChange;
@property (nonatomic) NSSize expectedIntrinsicContentSize;
@end

@implementation AutoLayoutWKWebView

- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    self = [super initWithFrame:frame configuration:configuration];
    if (!self)
        return nil;

    return self;
}

- (void)load:(NSString *)HTMLString withWidth:(CGFloat)width expectingContentSize:(NSSize)size
{
    [self load:HTMLString withWidth:width expectingContentSize:size resettingWidth:YES];
}

- (void)load:(NSString *)HTMLString withWidth:(CGFloat)width expectingContentSize:(NSSize)size resettingWidth:(BOOL)resetAfter
{
    EXPECT_FALSE(_expectingIntrinsicContentSizeChange);

    NSString *baseHTML = @"<style>"
    "body, html { margin: 0; padding: 0; }"
    "div { background-color: green; }"
    ".small { width: 10px; height: 10px; }"
    ".large { width: 100px; height: 100px; }"
    ".veryWide { width: 100px; height: 10px; }"
    ".inline { display: inline-block; }"
    ".viewportUnit { height: 50vh; }"
    "</style>";

    [self loadHTMLString:[baseHTML stringByAppendingString:HTMLString] baseURL:nil];
    [self _test_waitForDidStartProvisionalNavigation];
    [self beginLayoutAtMinimumWidth:width andExpectContentSizeChange:size];
    [self _test_waitForDidFinishNavigation];

    [self waitForContentSizeChangeResettingWidth:resetAfter];
}

- (void)beginLayoutAtMinimumWidth:(CGFloat)width andExpectContentSizeChange:(NSSize)size
{
    [self _setMinimumLayoutWidth:width];

    // NOTE: Each adjacent expected result must be different, or we'll early return and not call invalidateIntrinsicContentSize!
    EXPECT_FALSE(NSEqualSizes(size, self.intrinsicContentSize));

    _expectingIntrinsicContentSizeChange = YES;
    _expectedIntrinsicContentSize = size;
    didInvalidateIntrinsicContentSize = false;
}

- (void)waitForContentSizeChangeResettingWidth:(BOOL)resetAfter
{
    TestWebKitAPI::Util::run(&didInvalidateIntrinsicContentSize);

    NSSize intrinsicContentSize = self.intrinsicContentSize;
    EXPECT_EQ(_expectedIntrinsicContentSize.width, intrinsicContentSize.width);
    EXPECT_EQ(_expectedIntrinsicContentSize.height, intrinsicContentSize.height);

    if (resetAfter)
        [self _setMinimumLayoutWidth:0];
}

- (void)layoutAtMinimumWidth:(CGFloat)width andExpectContentSizeChange:(NSSize)size resettingWidth:(BOOL)resetAfter
{
    [self beginLayoutAtMinimumWidth:width andExpectContentSizeChange:size];
    [self waitForContentSizeChangeResettingWidth:resetAfter];
}

- (void)invalidateIntrinsicContentSize
{
    if (!_expectingIntrinsicContentSizeChange)
        return;

    _expectingIntrinsicContentSizeChange = NO;
    didInvalidateIntrinsicContentSize = true;
}

@end

TEST(WebKit, AutoLayoutIntegration)
{
    RetainPtr<AutoLayoutWKWebView> webView = adoptNS([[AutoLayoutWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 1000)]);

    // 10x10 rect with the constraint (width >= 50) -> 50x10
    [webView load:@"<div class='small'></div>" withWidth:50 expectingContentSize:NSMakeSize(50, 10)];

    // 100x100 rect with the constraint (width >= 50) -> 100x100
    [webView load:@"<div class='large'></div>" withWidth:50 expectingContentSize:NSMakeSize(100, 100)];

    // 100x10 rect with the constraint (width >= 50) -> 100x10
    [webView load:@"<div class='veryWide'></div>" withWidth:50 expectingContentSize:NSMakeSize(100, 10)];

    // 100px height + 50vh -> 150px
    [webView _setViewportSizeForCSSViewportUnits:CGSizeMake(100, 100)];
    [webView load:@"<div class='viewportUnit'></div><div class='large'></div>" withWidth:100 expectingContentSize:NSMakeSize(100, 150)];

    // Ten 10x10 rects, inline, should result in two rows of five; with the constraint (width >= 50) -> 50x20
    [webView load:@"<div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div>" withWidth:50 expectingContentSize:NSMakeSize(50, 20)];

    // Changing the width to 10 should result in ten rows of one; with the constraint (width >= 10) -> 10x100
    [webView layoutAtMinimumWidth:10 andExpectContentSizeChange:NSMakeSize(10, 100) resettingWidth:YES];

    // Changing the width to 100 should result in one rows of ten; with the constraint (width >= 100) -> 100x10
    [webView layoutAtMinimumWidth:100 andExpectContentSizeChange:NSMakeSize(100, 10) resettingWidth:YES];

    // One 100x100 rect and ten 10x10 rects, inline; with the constraint (width >= 20) -> 100x150
    [webView load:@"<div class='large'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div><div class='small inline'></div>" withWidth:20 expectingContentSize:NSMakeSize(100, 150)];

    // Test content with a subframe
    [webView load:@"<object type=\"text/html\" data=\"about:blank\">" withWidth:300 expectingContentSize:NSMakeSize(300, 150)];

    // With _shouldExpandContentToViewHeightForAutoLayout off (the default), the page should lay out to the intrinsic height
    // of the content.
    [webView load:@"<div class='small'></div>" withWidth:50 expectingContentSize:NSMakeSize(50, 10) resettingWidth:NO];
    [webView evaluateJavaScript:@"window.innerHeight" completionHandler:^(id value, NSError *error) {
        EXPECT_TRUE([value isKindOfClass:[NSNumber class]]);
        EXPECT_EQ(10, [value integerValue]);
        didEvaluateJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&didEvaluateJavaScript);
    didEvaluateJavaScript = false;

    // Enabling _shouldExpandContentToViewHeightForAutoLayout should make the page lay out to the view height, regardless
    // of the intrinsic height of the content. We have to load differently-sized content so that we can wait for
    // the intrinsic size change callback.
    [webView _setShouldExpandContentToViewHeightForAutoLayout:YES];
    // 100px _is_the_expected_ height because we intentionally report stale value to avoid unstable layout.
    // See FrameView::autoSizeIfEnabled().
    [webView load:@"<div class='large'></div>" withWidth:50 expectingContentSize:NSMakeSize(100, 100) resettingWidth:NO];
    [webView evaluateJavaScript:@"window.innerHeight" completionHandler:^(id value, NSError *error) {
        EXPECT_TRUE([value isKindOfClass:[NSNumber class]]);
        EXPECT_EQ(1000, [value integerValue]);
        didEvaluateJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&didEvaluateJavaScript);
    didEvaluateJavaScript = false;
    [webView _setShouldExpandContentToViewHeightForAutoLayout:NO];
}

TEST(WebKit, AutoLayoutRenderingProgressRelativeOrdering)
{
    RetainPtr<AutoLayoutWKWebView> webView = adoptNS([[AutoLayoutWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 1000)]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];
    __block bool didFirstLayout = false;
    [navigationDelegate setRenderingProgressDidChange:^(WKWebView *, _WKRenderingProgressEvents progressEvents) {
        if (progressEvents & _WKRenderingProgressEventFirstLayout) {
            didFirstLayout = true;
            EXPECT_TRUE(didInvalidateIntrinsicContentSize);
        }
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView _setMinimumLayoutWidth:100];
    didInvalidateIntrinsicContentSize = false;
    [webView setExpectingIntrinsicContentSizeChange:YES];
    [webView setExpectedIntrinsicContentSize:NSMakeSize(100, 400)];
    [webView loadHTMLString:@"<body style='margin: 0; height: 400px;'></body>" baseURL:nil];
    TestWebKitAPI::Util::run(&didInvalidateIntrinsicContentSize);
    TestWebKitAPI::Util::run(&didFirstLayout);
    TestWebKitAPI::Util::run(&didFinishNavigation);
    [webView setNavigationDelegate:nil];
}

#endif
