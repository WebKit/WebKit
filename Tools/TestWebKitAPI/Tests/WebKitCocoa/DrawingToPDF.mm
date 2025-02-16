/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if USE(PDFKIT_FOR_TESTING)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestPDFDocument.h"
#import "TestWKWebView.h"
#import <WebCore/Color.h>
#import <WebKit/WKPDFConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/text/TextStream.h>

namespace TestWebKitAPI {

TEST(DrawingToPDF, GradientIntoPDF)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<style>body { margin: 0 } div { width: 100px; height: 100px; border: 2px solid black; background: linear-gradient(blue, blue 90%, green); print-color-adjust: exact; }</style> <div></div>"];

    auto configuration = adoptNS([[WKPDFConfiguration alloc] init]);
    [configuration setRect:NSMakeRect(0, 0, 100, 100)];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 1u);
        auto page = document->page(0);
        EXPECT_NE(page, nullptr);

        EXPECT_TRUE(page->colorAtPoint(50, 50) == WebCore::Color::blue);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(DrawingToPDF, BackgroundClipText)
{
    static bool didTakeSnapshot;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<style>@font-face { font-family: Ahem; src: url(Ahem.ttf); }"
        "body { margin: 0 }"
        "div { font-family: Ahem; font-size: 20px; padding: 10px; color: transparent; background: blue; background-clip: text; print-color-adjust: exact; }</style>"
        "<div>A</div>"];

    auto configuration = adoptNS([[WKPDFConfiguration alloc] init]);
    [configuration setRect:NSMakeRect(0, 0, 40, 40)];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        auto document = TestPDFDocument::createFromData(pdfSnapshotData);
        EXPECT_EQ(document->pageCount(), 1u);
        auto page = document->page(0);
        EXPECT_NE(page, nullptr);

        EXPECT_TRUE(page->colorAtPoint(2, 2) == WebCore::Color::white);
        // We can't test for blue because the colors are affected by colorspace conversions.
        EXPECT_TRUE(page->colorAtPoint(25, 25) != WebCore::Color::white);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

} // namespace TestWebKitAPI

#endif // USE(PDFKIT_FOR_TESTING)
