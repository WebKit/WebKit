/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKTextInputContext.h>
#import <wtf/RetainPtr.h>

#define EXPECT_RECT_EQ(xExpected, yExpected, widthExpected, heightExpected, rect) \
    EXPECT_DOUBLE_EQ(xExpected, rect.origin.x); \
    EXPECT_DOUBLE_EQ(yExpected, rect.origin.y); \
    EXPECT_DOUBLE_EQ(widthExpected, rect.size.width); \
    EXPECT_DOUBLE_EQ(heightExpected, rect.size.height);

@implementation WKWebView (SynchronousTextInputContext)

- (NSArray<_WKTextInputContext *> *)synchronouslyRequestTextInputContextsInRect:(CGRect)rect
{
    __block bool finished = false;
    __block RetainPtr<NSArray<_WKTextInputContext *>> result;
    [self _requestTextInputContextsInRect:rect completionHandler:^(NSArray<_WKTextInputContext *> *contexts) {
        result = contexts;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (BOOL)synchronouslyFocusTextInputContext:(_WKTextInputContext *)context
{
    __block bool finished = false;
    __block bool success = false;
    [self _focusTextInputContext:context completionHandler:^(BOOL innerSuccess) {
        success = innerSuccess;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return success;
}

@end

static NSString *applyStyle(NSString *HTMLString)
{
    return [@"<style>body { margin: 0; } iframe { border: none; }</style><meta name='viewport' content='initial-scale=1'>" stringByAppendingString:HTMLString];
}

static NSString *applyIframe(NSString *HTMLString)
{
    return applyStyle([NSString stringWithFormat:@"<iframe src=\"data:text/html,%@\" style='position: absolute; top: 200px;'>", [applyStyle(HTMLString) stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLPathAllowedCharacterSet]]]);
}

TEST(WebKit, RequestTextInputContext)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetThreadedScrollingEnabled((WKPreferencesRef)[configuration preferences], false);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSArray<_WKTextInputContext *> *contexts;

    // Basic inputs.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 0, 50, 50, contexts[0].boundingRect);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<textarea style='width: 100px; height: 100px;'></textarea>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 0, 100, 100, contexts[0].boundingRect);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<div contenteditable style='width: 100px; height: 100px;'></div>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 0, 100, 100, contexts[0].boundingRect);

    // Basic inputs inside subframe.

    [webView synchronouslyLoadHTMLString:applyIframe(@"<input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 200, 50, 50, contexts[0].boundingRect);

    [webView synchronouslyLoadHTMLString:applyIframe(@"<textarea style='width: 100px; height: 100px;'></textarea>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 200, 100, 100, contexts[0].boundingRect);

    [webView synchronouslyLoadHTMLString:applyIframe(@"<div contenteditable style='width: 100px; height: 100px;'></div>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 200, 100, 100, contexts[0].boundingRect);

    // Read only inputs; should not be included.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;' readonly>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(0UL, contexts.count);

    [webView synchronouslyLoadHTMLString:applyStyle(@"<textarea style='width: 100px; height: 100px;' readonly>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(0UL, contexts.count);

    // Inputs outside the requested rect; should not be included.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:CGRectMake(100, 100, 100, 100)];
    EXPECT_EQ(0UL, contexts.count);

    // Inputs scrolled outside the requested rect; should not be included.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'><br><div style='width: 100px; height: 5000px;'></div>")];
#if PLATFORM(MAC)
    [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 5000);"];
#else
    [webView scrollView].contentOffset = CGPointMake(0, 5000);
#endif
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(0UL, contexts.count);

    // Inputs scrolled into the requested rect.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px; position: absolute; top: 5000px;'><br><div style='width: 100px; height: 10000px;'></div>")];
#if PLATFORM(MAC)
    [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 5000);"];
#else
    [webView scrollView].contentOffset = CGPointMake(0, 5000);
#endif
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 0, 50, 50, contexts[0].boundingRect);

    // Multiple inputs.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input type='text' style='width: 50px; height: 50px;'><br/><input type='text' style='width: 50px; height: 50px;'><br/><input type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(3UL, contexts.count);
    EXPECT_RECT_EQ(0, 0, 50, 50, contexts[0].boundingRect);
    EXPECT_RECT_EQ(0, 50, 50, 50, contexts[1].boundingRect);
    EXPECT_RECT_EQ(0, 100, 50, 50, contexts[2].boundingRect);

    // Nested <input>-inside-contenteditable. Only the contenteditable is considered.

    [webView synchronouslyLoadHTMLString:applyStyle(@"<div contenteditable style='width: 100px; height: 100px;'><input type='text' style='width: 50px; height: 50px;'></div>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    EXPECT_RECT_EQ(0, 0, 100, 100, contexts[0].boundingRect);
}

TEST(WebKit, DISABLED_FocusTextInputContext)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferencesSetThreadedScrollingEnabled((WKPreferencesRef)[configuration preferences], false);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSArray<_WKTextInputContext *> *contexts;

    [webView synchronouslyLoadHTMLString:applyStyle(@"<input id='test' type='text' style='width: 50px; height: 50px;'>")];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> originalInput = contexts[0];
    EXPECT_TRUE([webView synchronouslyFocusTextInputContext:originalInput.get()]);
    EXPECT_WK_STREQ("test", [webView objectByEvaluatingJavaScript:@"document.activeElement.id"]);

    // The _WKTextInputContext should still work even after another request.
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_TRUE([contexts[0] isEqual:originalInput.get()]);
    EXPECT_TRUE([webView synchronouslyFocusTextInputContext:originalInput.get()]);

    // Replace the <input> with a <textarea> with script; the <input> should no longer be focusable.
    [webView objectByEvaluatingJavaScript:@"document.body.innerHTML = '<textarea id=\"area\">';"];
    contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView frame]];
    EXPECT_EQ(1UL, contexts.count);
    RetainPtr<_WKTextInputContext> textArea = contexts[0];
    EXPECT_FALSE([textArea isEqual:originalInput.get()]);
    EXPECT_FALSE([webView synchronouslyFocusTextInputContext:originalInput.get()]);
    EXPECT_TRUE([webView synchronouslyFocusTextInputContext:textArea.get()]);
    EXPECT_WK_STREQ("area", [webView objectByEvaluatingJavaScript:@"document.activeElement.id"]);

    // Destroy the <textarea> by navigating away; we can no longer focus it.
    [webView synchronouslyLoadHTMLString:@""];
    EXPECT_FALSE([webView synchronouslyFocusTextInputContext:textArea.get()]);
}
