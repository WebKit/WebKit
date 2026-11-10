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

#if HAVE(UIKIT_PRINTING)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestCocoaImageAndCocoaColor.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestPDFDocument.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
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
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    CGRect pageRect = CGRectMake(0, 0, 100, 100);
    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
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
    RetainPtr config = adoptNS([[WKWebViewConfiguration alloc] init]);
    [config preferences].shouldPrintBackgrounds = NO;
    
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:config.get()]);

    [webView synchronouslyLoadTestPageNamed:@"red"];
    [webView waitForNextPresentationUpdate];
    
    auto runTest = [&] (BOOL shouldPrintBackgrounds) {
        CGRect pageRect = CGRectMake(0, 0, 100, 100);
        RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
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
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    RetainPtr printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block NSUInteger pdfDataLength = 0;
    __block bool done = false;

    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            RetainPtr pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
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
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    __block NSUInteger numCompleted = 0;
    __block bool done = false;

    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    RetainPtr printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block NSUInteger pdfDataLength = 0;
    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            RetainPtr pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
            pdfDataLength = [pdfData length];

            [printInteractionController _cleanPrintState];

            if (++numCompleted == 2)
                done = true;
        });
    }];

    RetainPtr printPageRenderer2 = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer2 addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    RetainPtr printInteractionController2 = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController2 setPrintPageRenderer:printPageRenderer2.get()];

    __block NSUInteger pdfDataLength2 = 0;
    [printInteractionController2 _setupPrintPanel:nil];
    [printInteractionController2 _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            RetainPtr pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
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
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];

    RetainPtr printInteractionController = adoptNS([[UIPrintInteractionController alloc] init]);
    [printInteractionController setPrintPageRenderer:printPageRenderer.get()];

    __block bool done = false;

    __block NSUInteger printInteractionControllerPDFDataLength = 0;
    [printInteractionController _setupPrintPanel:nil];
    [printInteractionController _generatePrintPreview:^(NSURL *pdfURL, BOOL shouldRenderOnChosenPaper) {
        dispatch_async(mainDispatchQueueSingleton(), ^{
            RetainPtr pdfData = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL]);
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

#if HAVE(PDFKIT)
// A quarter inch unprintable edge.
static constexpr CGFloat unprintableEdge = 18;
static constexpr CGSize letterPaperSize { 612, 792 };

struct PrintOptions {
    CGSize paperSize { letterPaperSize };
    UIEdgeInsets contentInsets { UIEdgeInsetsZero };
    CGFloat headerHeight { 0 };
    CGFloat footerHeight { 0 };
    bool onBackgroundThread { false };
};

static RetainPtr<NSData> printToPDF(NSString *html, const PrintOptions& options = { })
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:html];
    [webView waitForNextPresentationUpdate];

    RetainPtr printFormatter = [webView viewPrintFormatter];
    [printFormatter setPerPageContentInsets:options.contentInsets];

    CGRect paperRect = { CGPointZero, options.paperSize };
    CGRect printableRect = CGRectInset(paperRect, unprintableEdge, unprintableEdge);
    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:printFormatter.get() startingAtPageAtIndex:0];
    [printPageRenderer setPaperRect:paperRect];
    [printPageRenderer setPrintableRect:printableRect];
    [printPageRenderer setHeaderHeight:options.headerHeight];
    [printPageRenderer setFooterHeight:options.footerHeight];

    RetainPtr pdfData = adoptNS([[NSMutableData alloc] init]);
    auto print = [&] {
        UIGraphicsBeginPDFContextToData(pdfData.get(), paperRect, nil);
        NSInteger numberOfPages = [printPageRenderer numberOfPages];
        for (NSInteger i = 0; i < numberOfPages; ++i) {
            UIGraphicsBeginPDFPage();
            [printPageRenderer drawPageAtIndex:i inRect:printableRect];
        }
        UIGraphicsEndPDFContext();
    };

    if (!options.onBackgroundThread) {
        print();
        return pdfData;
    }

    __block bool done = false;
    dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), ^{
        print();
        dispatch_async(mainDispatchQueueSingleton(), ^{
            done = true;
        });
    });
    TestWebKitAPI::Util::run(&done);
    return pdfData;
}

static RetainPtr<TestPDFPage> onlyPage(NSData *data)
{
    RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:data]);
    EXPECT_EQ([document pageCount], 1);
    if ([document pageCount] != 1)
        return nil;

    RetainPtr page = [document pageAtIndex:0];
    EXPECT_GT([page characterCount], 0);
    if (![page characterCount])
        return nil;

    return page;
}

// Measured from the paper's top left.
static Vector<CGRect> characterRects(TestPDFPage *page)
{
    Vector<CGRect> rects;
    for (NSInteger i = 0; i < [page characterCount]; ++i) {
        CGRect rect = [page rectForCharacterAtIndex:i];
        if (CGRectIsEmpty(rect))
            continue;
        rect.origin.y = CGRectGetMaxY([page bounds]) - CGRectGetMaxY(rect);
        rects.append(rect);
    }
    return rects;
}

static std::optional<CGPoint> firstCharacterPosition(NSString *styleSheet, const PrintOptions& options = { })
{
    RetainPtr page = onlyPage(printToPDF([NSString stringWithFormat:@"<style>%@ body { margin: 0 }</style><div>Hello</div>", styleSheet], options).get());
    if (!page)
        return std::nullopt;
    auto rects = characterRects(page.get());
    EXPECT_FALSE(rects.isEmpty());
    if (rects.isEmpty())
        return std::nullopt;
    return rects[0].origin;
}

static void expectFirstCharacterOffset(NSString *styleSheet, NSString *referenceStyleSheet, CGFloat left, CGFloat top, const PrintOptions& options = { })
{
    auto position = firstCharacterPosition(styleSheet, options);
    auto referencePosition = firstCharacterPosition(referenceStyleSheet, options);
    if (!position || !referencePosition)
        return;
    EXPECT_NEAR(position->x - referencePosition->x, left, 0.05);
    EXPECT_NEAR(position->y - referencePosition->y, top, 0.05);
}

TEST(WKWebView, PrintToPDFUsesPageMargins)
{
    for (bool onBackgroundThread : { false, true }) {
        PrintOptions options { .onBackgroundThread = onBackgroundThread };

        // 1in = 96px = 72pt.
        expectFirstCharacterOffset(@"@page { margin: 0 } div { margin-left: 1in; padding-top: 1in }", @"@page { margin: 0 }", 72, 72, options);
        expectFirstCharacterOffset(@"@page { margin: 1in 0 0 0.5in }", @"@page { margin: 0 }", 36, 72, options);

        // Margins narrower than the printer's unprintable edge are not widened.
        expectFirstCharacterOffset(@"@page { margin: 0.125in 0 0 0.125in }", @"@page { margin: 0 }", 9, 9, options);
    }
}

TEST(WKWebView, PrintToPDFUsesContentInsetsForAutoPageMargins)
{
    PrintOptions withContentInsets { .contentInsets = UIEdgeInsetsMake(54, 54, 54, 54) };

    expectFirstCharacterOffset(@"", @"@page { margin: 0 }", 54, 54, withContentInsets);
    expectFirstCharacterOffset(@"@page { margin-left: 1in }", @"@page { margin: 0 }", 72, 54, withContentInsets);
}

TEST(WKWebView, PrintToPDFPageMarginsDoNotCoverHeaderOrFooter)
{
    constexpr CGFloat headerAndFooterHeight = 36;
    PrintOptions withHeaderAndFooter { .headerHeight = headerAndFooterHeight, .footerHeight = headerAndFooterHeight };
    constexpr CGFloat headerAndFooterMargin = unprintableEdge + headerAndFooterHeight;

    constexpr CGFloat lineHeight = 36;
    auto firstPage = [](NSString *pageStyle, const PrintOptions& options) -> RetainPtr<TestPDFPage> {
        NSMutableString *html = [NSMutableString stringWithFormat:@"<style>@page { %@ } body { margin: 0; line-height: 0.5in }</style>", pageStyle];
        for (unsigned i = 0; i < 30; ++i)
            [html appendString:@"<div>X</div>"];
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:printToPDF(html, options).get()]);
        EXPECT_GE([document pageCount], 2);
        return [document pageAtIndex:0];
    };

    auto expectLines = [&](NSString *pageStyle, CGFloat top, CGFloat bottom) {
        RetainPtr page = firstPage(pageStyle, withHeaderAndFooter);
        RetainPtr reference = firstPage(@"margin: 0", { });
        ASSERT_TRUE(page && reference);
        auto lines = characterRects(page.get());
        auto referenceLines = characterRects(reference.get());
        ASSERT_FALSE(lines.isEmpty() || referenceLines.isEmpty());

        EXPECT_NEAR(CGRectGetMinX(lines[0]), CGRectGetMinX(referenceLines[0]), 0.05) << pageStyle.UTF8String;
        EXPECT_NEAR(CGRectGetMinY(lines[0]) - CGRectGetMinY(referenceLines[0]), top, 0.05) << pageStyle.UTF8String;
        EXPECT_EQ(lines.size(), static_cast<size_t>((letterPaperSize.height - top - bottom) / lineHeight)) << pageStyle.UTF8String;
        EXPECT_LE(CGRectGetMaxY(lines.last()), letterPaperSize.height - bottom) << pageStyle.UTF8String;
    };

    expectLines(@"margin: 0", headerAndFooterMargin, headerAndFooterMargin);
    expectLines(@"margin: 1in 0", 72, 72);
    expectLines(@"margin: 0.25in 0 1in", headerAndFooterMargin, 72);
}

TEST(WKWebView, PrintToPDFShrinksPagesLargerThanPaper)
{
    RetainPtr page = onlyPage(printToPDF(@"<style>@page { margin: -0.5in } body { margin: 0 } div { display: flex; justify-content: space-between }</style><div><span>L</span><span>R</span></div>").get());
    ASSERT_TRUE(page);
    auto rects = characterRects(page.get());
    ASSERT_GE(rects.size(), 2u);

    EXPECT_GE(CGRectGetMinX(rects[0]), 0);
    EXPECT_LT(CGRectGetMinX(rects[0]), 1);
    EXPECT_GE(CGRectGetMinY(rects[0]), 0);
    EXPECT_LE(CGRectGetMaxX(rects.last()), letterPaperSize.width + 0.5);
    EXPECT_GT(CGRectGetMaxX(rects.last()), letterPaperSize.width - 4);
}

TEST(WKWebView, PrintFormatterDrawsPageInRequestedRect)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<style>@page { margin: 0 } body { margin: 0 } div { display: flex; justify-content: space-between }</style><div><span>L</span><span>R</span></div>"];
    [webView waitForNextPresentationUpdate];

    RetainPtr printFormatter = [webView viewPrintFormatter];
    CGRect paperRect = { CGPointZero, letterPaperSize };
    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:printFormatter.get() startingAtPageAtIndex:0];
    [printPageRenderer setPaperRect:paperRect];
    [printPageRenderer setPrintableRect:CGRectInset(paperRect, unprintableEdge, unprintableEdge)];
    ASSERT_EQ([printPageRenderer numberOfPages], 1);

    CGRect rect = CGRectMake(100, 100, letterPaperSize.width / 2, letterPaperSize.height / 2);
    RetainPtr pdfData = adoptNS([[NSMutableData alloc] init]);
    UIGraphicsBeginPDFContextToData(pdfData.get(), paperRect, nil);
    UIGraphicsBeginPDFPage();
    [printFormatter drawInRect:rect forPageAtIndex:0];
    UIGraphicsEndPDFContext();

    RetainPtr page = onlyPage(pdfData.get());
    ASSERT_TRUE(page);
    auto rects = characterRects(page.get());
    ASSERT_GE(rects.size(), 2u);

    EXPECT_NEAR(CGRectGetMinX(rects[0]), CGRectGetMinX(rect), 1);
    EXPECT_GE(CGRectGetMinY(rects[0]), CGRectGetMinY(rect));
    EXPECT_LT(CGRectGetMinY(rects[0]), CGRectGetMinY(rect) + 6);
    EXPECT_LE(CGRectGetMaxX(rects.last()), CGRectGetMaxX(rect) + 0.5);
    EXPECT_GT(CGRectGetMaxX(rects.last()), CGRectGetMaxX(rect) - 6);
}

static bool isRed(CGPDFPageRef page, CGPoint pointFromTopLeft)
{
    constexpr CGFloat pixelsPerPoint = 4;
    std::array<uint8_t, 4> pixel { };
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr context = adoptCF(CGBitmapContextCreate(pixel.data(), 1, 1, 8, 4, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    CGRect cropBox = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
    CGContextScaleCTM(context.get(), pixelsPerPoint, pixelsPerPoint);
    CGContextTranslateCTM(context.get(), -pointFromTopLeft.x, -(CGRectGetMaxY(cropBox) - pointFromTopLeft.y));
    CGContextDrawPDFPage(context.get(), page);
    return pixel[0] > 200 && pixel[1] < 60 && pixel[2] < 60 && pixel[3] > 200;
}

TEST(WKWebView, PrintToPDFWithZeroPageMarginsFillsPaper)
{
    // https://bugs.webkit.org/attachment.cgi?id=475110. Each page's red outline is flush with the paper.
    NSString *html = @"<!DOCTYPE html><style>"
        "* { margin: 0px; padding: 0px }"
        "div { outline: 1px solid red; outline-offset: -1px; display: flex; align-items: center; justify-content: center; width: 100vw; height: 100vh }"
        "@page { size: letter landscape; margin: 0pt; break-after: always }"
        "@media print { div { width: 792pt; height: 612pt } }"
        "</style><div>Page 1</div><div>Page 2</div><div>Page 3</div>";
    CGSize paperSize = CGSizeMake(792, 612);
    RetainPtr data = printToPDF(html, { .paperSize = paperSize });

    RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:data.get()]);
    ASSERT_EQ([document pageCount], 3);
    RetainPtr provider = adoptCF(CGDataProviderCreateWithCFData(bridge_cast(data.get())));
    RetainPtr pdf = adoptCF(CGPDFDocumentCreateWithProvider(provider.get()));
    for (NSInteger i = 0; i < 3; ++i) {
        RetainPtr page = [document pageAtIndex:i];
        EXPECT_WK_STREQ([page text], ([NSString stringWithFormat:@"Page %ld", static_cast<long>(i + 1)]));
        ASSERT_GT([page characterCount], 0);

        CGRect label = CGRectUnion([page rectForCharacterAtIndex:0], [page rectForCharacterAtIndex:[page characterCount] - 1]);
        EXPECT_NEAR(CGRectGetMidX(label), paperSize.width / 2, 1);
        EXPECT_NEAR(CGRectGetMaxY([page bounds]) - CGRectGetMidY(label), paperSize.height / 2, 1);

        CGPDFPageRef pdfPage = CGPDFDocumentGetPage(pdf.get(), i + 1);
        ASSERT_TRUE(pdfPage);
        EXPECT_TRUE(isRed(pdfPage, CGPointMake(0.25, paperSize.height / 2)));
        EXPECT_TRUE(isRed(pdfPage, CGPointMake(paperSize.width - 0.5, paperSize.height / 2)));
        EXPECT_TRUE(isRed(pdfPage, CGPointMake(paperSize.width / 2, 0.5)));
        EXPECT_TRUE(isRed(pdfPage, CGPointMake(paperSize.width / 2, paperSize.height - 0.25)));
        EXPECT_FALSE(isRed(pdfPage, CGPointMake(4, paperSize.height / 2)));
    }
}
#endif // HAVE(PDFKIT)

TEST(WKWebView, PrintFormatterLaunchesPrintProcess)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/mainframe"_s, { "<body>Mainframe<iframe src='https://b.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<body>Subframe</body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingEnabled", true);
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingPrintProcessEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 400, 600));
    [webView _setAlwaysUsePrintProcessForRemoteSnapshottingForTesting:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    CGRect pageRect = CGRectMake(0, 0, 612, 792);
    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];
    [printPageRenderer setPaperRect:pageRect];
    [printPageRenderer setPrintableRect:pageRect];
    EXPECT_GE([printPageRenderer numberOfPages], 1);

    __block pid_t printProcess = 0;
    __block bool done = false;
    [webView _printProcessIdentifierForTesting:^(pid_t identifier) {
        printProcess = identifier;
        done = true;
    }];
    Util::run(&done);
    EXPECT_NE(printProcess, 0);
    EXPECT_NE(printProcess, [webView mainFrame].info._processIdentifier);
    EXPECT_NE(printProcess, [webView mainFrame].childFrames.firstObject.info._processIdentifier);
}

#if HAVE(PDFKIT)
TEST(WKWebView, PrintFormatterDrawsInPrintProcess)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/mainframe"_s, { "<body>Mainframe<iframe src='https://b.com/subframe'></iframe></body>"_s } },
        { "/subframe"_s, { "<body>Subframe</body>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingEnabled", true);
    setFeatureEnabled(configuration.get(), @"RemoteSnapshottingPrintProcessEnabled", true);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 400, 600));
    [webView _setAlwaysUsePrintProcessForRemoteSnapshottingForTesting:YES];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigationAndLoadInSubframe];

    CGRect pageRect = CGRectMake(0, 0, 612, 792);
    RetainPtr printPageRenderer = adoptNS([[UIPrintPageRenderer alloc] init]);
    [printPageRenderer addPrintFormatter:[webView viewPrintFormatter] startingAtPageAtIndex:0];
    [printPageRenderer setPaperRect:pageRect];
    [printPageRenderer setPrintableRect:pageRect];
    NSInteger numberOfPages = [printPageRenderer numberOfPages];
    EXPECT_GE(numberOfPages, 1);

    __block pid_t printProcess = 0;
    __block bool done = false;
    [webView _printProcessIdentifierForTesting:^(pid_t identifier) {
        printProcess = identifier;
        done = true;
    }];
    Util::run(&done);
    EXPECT_NE(printProcess, 0);

    auto drawingCounts = [&] {
        __block std::pair<NSUInteger, NSUInteger> result;
        __block bool gotCounts = false;
        [webView _printProcessDrawingCountsForTesting:^(NSUInteger drawnInPrintProcess, NSUInteger drawnAfterPrintProcessFailed) {
            result = { drawnInPrintProcess, drawnAfterPrintProcessFailed };
            gotCounts = true;
        }];
        Util::run(&gotCounts);
        return result;
    };
    auto [drawnInPrintProcess, drawnAfterPrintProcessFailed] = drawingCounts();

    RetainPtr data = adoptNS([[NSMutableData alloc] init]);
    UIGraphicsBeginPDFContextToData(data.get(), pageRect, nil);
    for (NSInteger i = 0; i < numberOfPages; ++i) {
        UIGraphicsBeginPDFPage();
        [printPageRenderer drawPageAtIndex:i inRect:UIGraphicsGetPDFContextBounds()];
    }
    UIGraphicsEndPDFContext();

    RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:data.get()]);
    RetainPtr text = [[document pageAtIndex:0] text];
    EXPECT_TRUE([text containsString:@"Mainframe"]);
    EXPECT_TRUE([text containsString:@"Subframe"]);

    auto counts = drawingCounts();
    EXPECT_GT(counts.first, drawnInPrintProcess);
    EXPECT_EQ(counts.second, drawnAfterPrintProcessFailed);
}
#endif // HAVE(PDFKIT)

#endif
