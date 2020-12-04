/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

TEST(WKWebViewDisableSelection, DoubleClickDoesNotSelectWhenTextInteractionsAreDisabled)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"try-text-select-with-disabled-text-interaction"];

    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:2];
    [webView waitForPendingMouseEvents];

    NSString *selectedText = [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"];
    EXPECT_WK_STREQ("Hello", selectedText);

    // Clear the selection by clicking once.
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
    [webView waitForPendingMouseEvents];

    // Disable text selection then double click again. This should result in no text selected.
    [webView configuration].preferences.textInteractionEnabled = NO;

    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:2];
    [webView waitForPendingMouseEvents];

    NSString *selectedText2 = [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"];
    EXPECT_WK_STREQ("", selectedText2);
}

static NSString *clickAndDragToSelectText(TestWKWebView *webView)
{
    [webView mouseEnterAtPoint:CGPointMake(150, 200)];
    [webView mouseMoveToPoint:CGPointMake(150, 200) withFlags:0];
    [webView mouseDownAtPoint:CGPointMake(150, 200) simulatePressure:NO];
    [webView waitForPendingMouseEvents];

    [webView mouseDragToPoint:CGPointMake(900, 200)];
    [webView waitForPendingMouseEvents];

    [webView mouseUpAtPoint:CGPointMake(900, 200)];
    [webView waitForPendingMouseEvents];
    
    return [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"];
}

TEST(WKWebViewDisableSelection, DragDoesNotSelectWhenTextInteractionsAreDisabled)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"try-text-select-with-disabled-text-interaction"];

    NSString *selectedText = clickAndDragToSelectText(webView.get());
    EXPECT_WK_STREQ("Hello", selectedText);
    
    // Clear the selection by clicking once.
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:1];
    [webView waitForPendingMouseEvents];
    
    // Disable text selection then click and drag again. This should result in no text selected.
    [webView configuration].preferences.textInteractionEnabled = NO;

    NSString *selectedText2 = clickAndDragToSelectText(webView.get());
    EXPECT_WK_STREQ("", selectedText2);
}

#endif // PLATFORM(MAC)
