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

#if HAVE(PDFKIT)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestPDFDocument.h"
#import "TestWKWebView.h"
#import <WebCore/Color.h>
#import <WebKit/WKPDFConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

TEST(PDFSnapshot, FullContent)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 1u);
        auto page = document->page(0);
        EXPECT_NE(page, nullptr);
        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 800, 600)));

        EXPECT_EQ(page->characterCount(), 5u);
        EXPECT_EQ(page->text()[0], 'H');
        EXPECT_EQ(page->text()[4], 'o');

        // The entire page should be green. Pick a point in the middle to check.
        EXPECT_TRUE(page->colorAtPoint(400, 300) == WebCore::Color::green);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, Subregions)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>"];

    // Snapshot a subregion contained entirely within the view
    auto configuration = adoptNS([[WKPDFConfiguration alloc] init]);
    [configuration setRect:NSMakeRect(200, 150, 400, 300)];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 1u);
        auto page = document->page(0);
        EXPECT_NE(page, nullptr);
        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 400, 300)));

        EXPECT_EQ(page->characterCount(), 0u);

        // The entire page should be green. Pick a point in the middle to check.
        EXPECT_TRUE(page->colorAtPoint(200, 150) == WebCore::Color::green);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    // Snapshot a region larger than the view
    [configuration setRect:NSMakeRect(0, 0, 1200, 1200)];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 1u);
        auto page = document->page(0);
        EXPECT_NE(page, nullptr);
        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 1200, 1200)));

        // A pixel that was in the view should be green. Pick a point in the middle to check.
        EXPECT_TRUE(page->colorAtPoint(200, 150) == WebCore::Color::green);

        // A pixel that was outside the view should also be green (we extend background color out). Pick a point in the middle to check.
        EXPECT_TRUE(page->colorAtPoint(900, 700) == WebCore::Color::green);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

}

TEST(PDFSnapshot, Over200Inches)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 29400)]);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 3u);

        auto page = document->page(0);
        EXPECT_NE(page, nullptr);
        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 800, 14400)));
        EXPECT_TRUE(page->colorAtPoint(400, 300) == WebCore::Color::green);
        EXPECT_EQ(page->characterCount(), 5u);

        page = document->page(1);
        EXPECT_NE(page, nullptr);
        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 800, 14400)));
        EXPECT_TRUE(page->colorAtPoint(400, 300) == WebCore::Color::green);

        EXPECT_EQ(page->characterCount(), 0u);

        page = document->page(2);
        EXPECT_NE(page, nullptr);
        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 800, 600)));
        EXPECT_TRUE(page->colorAtPoint(400, 300) == WebCore::Color::green);
        EXPECT_EQ(page->characterCount(), 0u);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, Links)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 15000)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><div style=\"-webkit-line-box-contain: glyphs\"><a href=\"https://webkit.org/\">Click me</a></div>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 2u);

        auto page = document->page(0);
        EXPECT_NE(page, nullptr);

        EXPECT_TRUE(CGRectEqualToRect(page->bounds(), CGRectMake(0, 0, 800, 14400)));
        EXPECT_TRUE(page->colorAtPoint(400, 300) == WebCore::Color::white);

        EXPECT_EQ(page->characterCount(), 8u);
        EXPECT_EQ(page->text()[0], 'C');
        EXPECT_EQ(page->text()[7], 'e');

        auto annotations = page->annotations();
        EXPECT_EQ(annotations.size(), 1u);
        if (annotations.size()) {
            EXPECT_TRUE(annotations[0].isLink());
            EXPECT_TRUE([annotations[0].linkURL() isEqual:[NSURL URLWithString:@"https://webkit.org/"]]);

            auto cRect = page->rectForCharacterAtIndex(1);
            auto cMidpoint = CGPointMake(CGRectGetMidX(cRect), CGRectGetMidY(cRect));
            auto annotationBounds = annotations[0].bounds();

            EXPECT_TRUE(CGRectContainsPoint(annotationBounds, cMidpoint));
        }

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, InlineLinks)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><a href=\"https://webkit.org/\">Click me</a>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 1u);

        auto page = document->page(0);
        EXPECT_NE(page, nullptr);

        // FIXME <rdar://problem/55086988>: There should be a link here, but due to the way we gather links for
        // annotation using the RenderInline tree it is missed.

//        auto annotations = page->annotations();
//        EXPECT_EQ(annotations.size(), 1u);
//        EXPECT_TRUE(annotations[0].isLink());
//        EXPECT_TRUE([annotations[0].linkURL() isEqual:[NSURL URLWithString:@"https://webkit.org/"]]);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

}

#endif // HAVE(PDFKIT)
