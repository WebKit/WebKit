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
#import "TestCocoaImageAndCocoaColor.h"
#import "TestNavigationDelegate.h"
#import "TestPDFDocument.h"
#import "TestWKWebView.h"
#import <WebKit/WebKit.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKWebViewPrintFormatter.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@interface UIPrintFormatter ()
- (NSInteger)_recalcPageCount;
@end

@interface UIPrintPageRenderer ()
@property (nonatomic) CGRect paperRect;
@property (nonatomic) CGRect printableRect;
@end

@interface UIPrintInteractionController ()
- (BOOL)_setupPrintPanel:(void (^)(UIPrintInteractionController *printInteractionController, BOOL completed, NSError *error))completion;
- (void)_generatePrintPreview:(void (^)(NSURL *previewPDF, BOOL shouldRenderOnChosenPaper))completionHandler;
- (void)_cleanPrintState;
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

TEST(WKWebView, PrintToPDFUsingPrintPageRenderer)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    CGRect pageRect = CGRectMake(0, 0, 100, 100);
    auto printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];
    [printPageRenderer setPaperRect:pageRect];
    [printPageRenderer setPrintableRect:pageRect];

    NSMutableData *pdfData = [NSMutableData data];
    UIGraphicsBeginPDFContextToData(pdfData, pageRect, nil);

    NSInteger numberOfPages = [printPageRenderer numberOfPages];
    for (NSInteger i = 0; i < numberOfPages; i++) {
        UIGraphicsBeginPDFPage();
        CGRect bounds = UIGraphicsGetPDFContextBounds();
        [printPageRenderer drawPageAtIndex:i inRect:bounds];
    }

    UIGraphicsEndPDFContext();

    EXPECT_NE([pdfData length], 0UL);
}

#if HAVE(PDFKIT)
TEST(WKWebView, PrintToPDFShouldPrintBackgrounds)
{
    auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
    [config preferences].shouldPrintBackgrounds = NO;
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);

    [webView synchronouslyLoadTestPageNamed:@"red"];
    [webView waitForNextPresentationUpdate];
    
    auto runTest = [&] (BOOL shouldPrintBackgrounds) {
        CGRect pageRect = CGRectMake(0, 0, 100, 100);
        auto printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
        [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];
        [printPageRenderer setPaperRect:pageRect];
        [printPageRenderer setPrintableRect:pageRect];

        NSMutableData *pdfData = [NSMutableData data];
        UIGraphicsBeginPDFContextToData(pdfData, pageRect, nil);

        NSInteger numberOfPages = [printPageRenderer numberOfPages];
        for (NSInteger i = 0; i < numberOfPages; i++) {
            UIGraphicsBeginPDFPage();
            CGRect bounds = UIGraphicsGetPDFContextBounds();
            [printPageRenderer drawPageAtIndex:i inRect:bounds];
        }

        UIGraphicsEndPDFContext();

        RetainPtr pdf = adoptNS([[TestPDFDocument alloc] initFromData:pdfData]);
        RetainPtr page = [pdf pageAtIndex:0];

        if (shouldPrintBackgrounds)
            EXPECT_TRUE(TestWebKitAPI::Util::compareColors([page colorAtPoint:NSMakePoint(99, 99)], [CocoaColor redColor]));
        else
            EXPECT_FALSE(TestWebKitAPI::Util::compareColors([page colorAtPoint:NSMakePoint(99, 99)], [CocoaColor redColor]));
    };
    
    runTest(NO);
    
    [webView configuration].preferences.shouldPrintBackgrounds = YES;
    
    runTest(YES);
}
#endif // HAVE(PDFKIT)

TEST(WKWebView, PrintToPDFUsingPrintInteractionController)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    auto printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    auto printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block NSUInteger pdfDataLength = 0;
    __block bool done = false;

    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            auto pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            pdfDataLength = [pdfData length];

            [printInteractionController _cleanPrintState];
            done = true;
        });
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_NE(pdfDataLength, 0UL);
}

TEST(WKWebView, PrintToPDFUsingMultiplePrintInteractionControllers)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    __block NSUInteger numCompleted = 0;
    __block bool done = false;

    auto printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    auto printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block NSUInteger pdfDataLength = 0;
    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            auto pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            pdfDataLength = [pdfData length];

            [printInteractionController _cleanPrintState];

            if (++numCompleted == 2)
                done = true;
        });
    }];

    auto printPageRenderer2 = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer2 addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    auto printInteractionController2 = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController2 setPrintPageRenderer:printPageRenderer2.get()];

    __block NSUInteger pdfDataLength2 = 0;
    [printInteractionController2 _setupPrintPanel:nil];
    [printInteractionController2 _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            auto pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            pdfDataLength2 = [pdfData length];

            [printInteractionController2 _cleanPrintState];

            if (++numCompleted == 2)
                done = true;
        });
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_NE(pdfDataLength, 0UL);
    EXPECT_NE(pdfDataLength2, 0UL);
}

TEST(WKWebView, PrintToPDFUsingPrintInteractionControllerAndPrintPageRenderer)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    auto printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    auto printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block bool done = false;

    __block NSUInteger printInteractionControllerPDFDataLength = 0;
    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            auto pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            printInteractionControllerPDFDataLength = [pdfData length];

            [printInteractionController _cleanPrintState];
            done = true;
        });
    }];

    CGRect pageRect = CGRectMake(0, 0, 100, 100);
    printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];
    [printPageRenderer setPaperRect:pageRect];
    [printPageRenderer setPrintableRect:pageRect];

    NSMutableData *printPageRendererPDFData = [NSMutableData data];
    UIGraphicsBeginPDFContextToData(printPageRendererPDFData, pageRect, nil);

    NSInteger numberOfPages = [printPageRenderer numberOfPages];
    for (NSInteger i = 0; i < numberOfPages; i++) {
        UIGraphicsBeginPDFPage();
        CGRect bounds = UIGraphicsGetPDFContextBounds();
        [printPageRenderer drawPageAtIndex:i inRect:bounds];
    }

    UIGraphicsEndPDFContext();

    TestWebKitAPI::Util::run(&done);
    EXPECT_NE(printInteractionControllerPDFDataLength, 0UL);
    EXPECT_NE([printPageRendererPDFData length], 0UL);
}

#endif
