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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WebKit.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKWebViewPrintFormatter.h>
#import <wtf/RetainPtr.h>

@interface UIPrintFormatter ()
- (NSInteger)_recalcPageCount;
@end

@interface UIPrintPageRenderer ()
@property (nonatomic) CGRect paperRect;
@property (nonatomic) CGRect printableRect;
@end

TEST(WKWebView, PrintFormatterCanRecalcPageCountWhilePrinting)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    RetainPtr<_WKWebViewPrintFormatter> printFormatter = [webView _webViewPrintFormatter];
    [printFormatter setSnapshotFirstPage:YES];

    RetainPtr<UIPrintPageRenderer> printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    
    [printPageRenderer addPrintFormatter:printFormatter.get() startingAtPageAtIndex:0];

    [printPageRenderer setPaperRect:CGRectMake(0, 0, 100, 100)];
    [printPageRenderer setPrintableRect:CGRectMake(0, 0, 100, 100)];

    EXPECT_EQ([printFormatter _recalcPageCount], 1);
    EXPECT_EQ([printFormatter _recalcPageCount], 1);
}


TEST(WKWebView, PrintFormatterHangsIfWebProcessCrashesBeforeWaiting)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    RetainPtr<_WKWebViewPrintFormatter> printFormatter = [webView _webViewPrintFormatter];

    RetainPtr<UIPrintPageRenderer> printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);

    [printPageRenderer addPrintFormatter:printFormatter.get() startingAtPageAtIndex:0];

    [printPageRenderer setPaperRect:CGRectMake(0, 0, 100, 100)];
    [printPageRenderer setPrintableRect:CGRectMake(0, 0, 100, 100)];

    RetainPtr<UIGraphicsImageRenderer> graphicsRenderer = adoptNS([[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(100, 100)]);

    EXPECT_EQ([printFormatter _recalcPageCount], 1);

    [webView _killWebContentProcess];

    [graphicsRenderer imageWithActions:^(UIGraphicsImageRendererContext *rendererContext) {
        UIGraphicsPushContext(rendererContext.CGContext);
        [printFormatter drawInRect:CGRectMake(0, 0, 100, 100) forPageAtIndex:0];
        UIGraphicsPopContext();
    }];
}

#endif
