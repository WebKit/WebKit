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
#import "TestCocoaImageAndCocoaColor.h"
#import "TestPDFDocument.h"
#import "TestWKWebView.h"
#import <WebKit/WKPDFConfiguration.h>

namespace TestWebKitAPI {

TEST(PDFSnapshot, FullContent)
{
    static bool didTakeSnapshot;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);
        EXPECT_EQ([document pageCount], 1);
        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);
        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 800, 600)));

        EXPECT_EQ([page characterCount], 5);
        EXPECT_EQ([[page text] characterAtIndex:0], 'H');
        EXPECT_EQ([[page text] characterAtIndex:4], 'o');

        // The entire page should be green. Pick a point in the middle to check.
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(400, 300)], [CocoaColor greenColor]));

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, Subregions)
{
    static bool didTakeSnapshot;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>"];

    // Snapshot a subregion contained entirely within the view
    RetainPtr configuration = adoptNS([[WKPDFConfiguration alloc] init]);
    [configuration setRect:NSMakeRect(200, 150, 400, 300)];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);
        EXPECT_EQ([document pageCount], 1);
        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);
        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 400, 300)));

        EXPECT_EQ([page characterCount], 0);

        // The entire page should be green. Pick a point in the middle to check.
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(200, 150)], [CocoaColor greenColor]));

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;

    // Snapshot a region larger than the view
    [configuration setRect:NSMakeRect(0, 0, 1200, 1200)];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);
        EXPECT_EQ([document pageCount], 1);
        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);
        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 1200, 1200)));

        // A pixel that was in the view should be green. Pick a point in the middle to check.
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(200, 150)], [CocoaColor greenColor]));

        // A pixel that was outside the view should also be green (we extend background color out). Pick a point in the middle to check.
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(900, 700)], [CocoaColor greenColor]));

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
    didTakeSnapshot = false;
}

TEST(PDFSnapshot, Over200Inches)
{
    static bool didTakeSnapshot;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 29400)]);

    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);

        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);
        EXPECT_EQ([document pageCount], 3);
        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);
        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 800, 14400)));
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(400, 300)], [CocoaColor greenColor]));
        EXPECT_EQ([page characterCount], 5);

        page = [document pageAtIndex:1];
        EXPECT_NE(page.get(), nil);
        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 800, 14400)));
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(400, 300)], [CocoaColor greenColor]));
        EXPECT_EQ([page characterCount], 0);

        page = [document pageAtIndex:2];
        EXPECT_NE(page.get(), nil);
        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 800, 600)));
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(400, 300)], [CocoaColor greenColor]));
        EXPECT_EQ([page characterCount], 0);

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, Links)
{
    static bool didTakeSnapshot;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 15000)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><div style=\"-webkit-line-box-contain: glyphs\"><a href=\"https://webkit.org/\">Click me</a></div>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);
        EXPECT_EQ([document pageCount], 2);

        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);

        EXPECT_TRUE(CGRectEqualToRect([page bounds], CGRectMake(0, 0, 800, 14400)));
        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(400, 300)], [CocoaColor whiteColor]));

        EXPECT_EQ([page characterCount], 8);
        EXPECT_EQ([[page text] characterAtIndex:0], 'C');
        EXPECT_EQ([[page text] characterAtIndex:7], 'e');

        RetainPtr annotations = [page annotations];
        EXPECT_EQ([annotations count], 1u);
        if ([annotations count]) {
            EXPECT_TRUE([[annotations objectAtIndex:0] isLink]);
            EXPECT_TRUE([[[annotations objectAtIndex:0] linkURL] isEqual:[NSURL URLWithString:@"https://webkit.org/"]]);

            auto cRect = [page rectForCharacterAtIndex:1];
            auto cMidpoint = CGPointMake(CGRectGetMidX(cRect), CGRectGetMidY(cRect));
            auto annotationBounds = [[annotations objectAtIndex:0] bounds];

            EXPECT_TRUE(CGRectContainsPoint(annotationBounds, cMidpoint));
        }

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, InlineLinks)
{
    static bool didTakeSnapshot;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><a href=\"https://webkit.org/\">Click me</a>"];

    [webView createPDFWithConfiguration:nil completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        EXPECT_NULL(error);
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);
        EXPECT_EQ([document pageCount], 1);
        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);

        RetainPtr annotations = [page annotations];
        EXPECT_EQ([annotations count], 1u);
        if ([annotations count]) {
            EXPECT_TRUE([[annotations objectAtIndex:0] isLink]);
            EXPECT_TRUE([[[annotations objectAtIndex:0] linkURL] isEqual:[NSURL URLWithString:@"https://webkit.org/"]]);

            auto cRect = [page rectForCharacterAtIndex:1];
            auto cMidpoint = CGPointMake(CGRectGetMidX(cRect), CGRectGetMidY(cRect));
            auto annotationBounds = [[annotations objectAtIndex:0] bounds];

            EXPECT_TRUE(CGRectContainsPoint(annotationBounds, cMidpoint));
        }

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

TEST(PDFSnapshot, AllowTransparentBackground)
{
    static bool didTakeSnapshot;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><svg height=\"210\" width=\"500\"><polygon points=\"200,10 250,190 160,210\" style=\"fill:lime\" /></svg>"];

    RetainPtr configuration = adoptNS([[WKPDFConfiguration alloc] init]);
    [configuration setAllowTransparentBackground:YES];

    [webView createPDFWithConfiguration:configuration.get() completionHandler:^(NSData *pdfSnapshotData, NSError *error) {
        RetainPtr document = adoptNS([[TestPDFDocument alloc] initFromData:pdfSnapshotData]);

        RetainPtr page = [document pageAtIndex:0];
        EXPECT_NE(page.get(), nil);

        EXPECT_TRUE(Util::compareColors([page colorAtPoint:CGPointMake(1, 1)], [CocoaColor colorWithWhite:0 alpha:0]));

        didTakeSnapshot = true;
    }];

    Util::run(&didTakeSnapshot);
}

}

#endif // HAVE(PDFKIT)
