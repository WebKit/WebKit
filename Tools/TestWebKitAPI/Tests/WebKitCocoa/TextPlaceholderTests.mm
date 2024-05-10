/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if HAVE(NSTEXTPLACEHOLDER_RECTS)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSTextInputContextSPI.h>

// FIXME: Remove this after rdar://126379463 lands
@interface WKTextSelectionRect : NSObject

@property (nonatomic, readonly) NSRect rect;
@property (nonatomic, readonly) NSWritingDirection writingDirection;
@property (nonatomic, readonly) BOOL isVertical;
@property (nonatomic, readonly) NSAffineTransform *transform;

@end
#endif

RetainPtr<WKWebView> createWebViewForNSTextPlaceholder()
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>Test</body><script>document.body.focus()</script>"];
    return webView;
}

TEST(NSTextPlaceholder, InsertTextPlaceholder)
{
    auto webView = createWebViewForNSTextPlaceholder();

    __block bool isDone = false;
    [(id<NSTextInputClient_Async_staging_126696059>)webView.get() insertTextPlaceholderWithSize:CGSizeMake(50, 100) completionHandler:^(NSTextPlaceholder * placeholder) {
        EXPECT_WK_STREQ("<div style=\"display: inline-block; vertical-align: top; visibility: hidden !important; width: 50px; height: 100px;\"></div>Test<script>document.body.focus()</script>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
        isDone = true;
        EXPECT_TRUE(CGSizeEqualToSize([(WKTextSelectionRect *)[[placeholder rects] firstObject] rect].size, CGSizeMake(50, 100)));
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(NSTextPlaceholder, InsertAndRemoveTextPlaceholderWithoutIncomingText)
{
    auto webView = createWebViewForNSTextPlaceholder();

    __block bool isDone = false;
    [(id<NSTextInputClient_Async_staging_126696059>)webView.get() insertTextPlaceholderWithSize:CGSizeMake(50, 100) completionHandler:^(NSTextPlaceholder *placeholder) {
        EXPECT_WK_STREQ("<div style=\"display: inline-block; vertical-align: top; visibility: hidden !important; width: 50px; height: 100px;\"></div>Test<script>document.body.focus()</script>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
        EXPECT_TRUE(CGSizeEqualToSize([(WKTextSelectionRect *)[[placeholder rects] firstObject] rect].size, CGSizeMake(50, 100)));
        [(id<NSTextInputClient_Async_staging_126696059>)webView.get() removeTextPlaceholder:placeholder willInsertText:NO completionHandler:^{
            EXPECT_WK_STREQ("Test<script>document.body.focus()</script>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
            isDone = true;
        }];
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(NSTextPlaceholder, InsertAndRemoveTextPlaceholderWithIncomingText)
{
    auto webView = createWebViewForNSTextPlaceholder();

    __block bool isDone = false;
    [(id<NSTextInputClient_Async_staging_126696059>)webView.get() insertTextPlaceholderWithSize:CGSizeMake(50, 100) completionHandler:^(NSTextPlaceholder *placeholder) {
        EXPECT_WK_STREQ("<div style=\"display: inline-block; vertical-align: top; visibility: hidden !important; width: 50px; height: 100px;\"></div>Test<script>document.body.focus()</script>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
        EXPECT_TRUE(CGSizeEqualToSize([(WKTextSelectionRect *)[[placeholder rects] firstObject] rect].size, CGSizeMake(50, 100)));
        [(id<NSTextInputClient_Async_staging_126696059>)webView.get() removeTextPlaceholder:placeholder willInsertText:YES completionHandler:^{
            EXPECT_WK_STREQ("Test<script>document.body.focus()</script>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
            isDone = true;
        }];
    }];
    TestWebKitAPI::Util::run(&isDone);
}

#endif
