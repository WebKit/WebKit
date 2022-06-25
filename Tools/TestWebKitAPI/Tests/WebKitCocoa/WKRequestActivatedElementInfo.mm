/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)

namespace TestWebKitAPI {

static void checkElementTypeAndBoundingRect(_WKActivatedElementInfo *elementInfo, _WKActivatedElementType expectedType, CGRect expectedBoundingRect)
{
    auto observedBoundingRect = elementInfo.boundingRect;
    EXPECT_EQ(CGRectGetWidth(expectedBoundingRect), CGRectGetWidth(observedBoundingRect));
    EXPECT_EQ(CGRectGetHeight(expectedBoundingRect), CGRectGetHeight(observedBoundingRect));
    EXPECT_EQ(CGRectGetMinX(expectedBoundingRect), CGRectGetMinX(observedBoundingRect));
    EXPECT_EQ(CGRectGetMinY(expectedBoundingRect), CGRectGetMinY(observedBoundingRect));
    EXPECT_EQ(expectedType, elementInfo.type);
}

TEST(_WKActivatedElementInfo, InfoForLink)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadHTMLString:@"<html><head><meta name='viewport' content='initial-scale=1'></head><body style='margin: 0px;'><a href='testURL.test' style='display: block; height: 100%;' title='HitTestLinkTitle' id='testID'></a></body></html>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {

        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeLink);
        EXPECT_WK_STREQ(elementInfo.URL.absoluteString, "testURL.test");
        EXPECT_WK_STREQ(elementInfo.title, "HitTestLinkTitle");
        EXPECT_WK_STREQ(elementInfo.ID, @"testID");
        EXPECT_NOT_NULL(elementInfo.image);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 320);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 500);
        EXPECT_EQ(elementInfo.image.size.width, 320);
        EXPECT_EQ(elementInfo.image.size.height, 500);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoForImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 215, 174)]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"image" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {

        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeImage);
        EXPECT_WK_STREQ(elementInfo.imageURL.lastPathComponent, "large-red-square.png");
        EXPECT_NOT_NULL(elementInfo.image);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoForMediaDocument)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 215, 174)]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {

        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeImage);
        EXPECT_WK_STREQ(elementInfo.imageURL.lastPathComponent, "icon.png");
        EXPECT_NOT_NULL(elementInfo.image);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 215);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 174);
        EXPECT_EQ(elementInfo.image.size.width, 215);
        EXPECT_EQ(elementInfo.image.size.height, 174);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoForLinkAroundImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"link-with-image" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {

        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeLink);
        EXPECT_WK_STREQ(elementInfo.URL.lastPathComponent, "testURL.test");
        EXPECT_WK_STREQ(elementInfo.title, "HitTestImageTitle");
        EXPECT_WK_STREQ(elementInfo.ID, @"testID");
        EXPECT_NOT_NULL(elementInfo.image);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 320);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 500);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}


TEST(_WKActivatedElementInfo, InfoForRotatedImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto imagePixels = [](CGImageRef image) -> Vector<unsigned> {
        static const size_t bytesPerPixel = 4;
        static const size_t bitsPerComponent = 8;
        size_t width = CGImageGetWidth(image);
        size_t height = CGImageGetHeight(image);
        size_t bytesPerRow = bytesPerPixel * width;

        static_assert(bytesPerPixel == sizeof(unsigned));
        Vector<unsigned> pixels(height * width);

        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
        RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(pixels.data(), width, height, bitsPerComponent, bytesPerRow, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGImageByteOrder32Little));

        CGContextDrawImage(context.get(), CGRectMake(0, 0, width, height), image);
        return pixels;
    };

    auto indexOf = [&](UIImage *image, unsigned x, unsigned y) -> unsigned {
        return y * image.size.width + x;
    };

    [webView synchronouslyLoadHTMLString:@"<body><img src='test.jpg'></body>"];
    RetainPtr originalImageInfo = [webView activatedElementAtPosition:CGPointMake(50, 50)];
    RetainPtr originalImage = [originalImageInfo image];
    auto originalImagePixels = imagePixels([originalImage CGImage]);

    unsigned green = originalImagePixels[indexOf(originalImage.get(), 0, 0)];
    unsigned yellow = originalImagePixels[indexOf(originalImage.get(), [originalImage size].width - 1, 0)];
    unsigned blue = originalImagePixels[indexOf(originalImage.get(), 0, [originalImage size].height - 1)];
    unsigned red = originalImagePixels[indexOf(originalImage.get(), [originalImage size].width - 1, [originalImage size].height - 1)];

    [webView synchronouslyLoadHTMLString:@"<body><img src='exif-orientation-8-llo.jpg'></body>"];
    RetainPtr rotatedImageInfo = [webView activatedElementAtPosition:CGPointMake(50, 50)];
    RetainPtr rotatedImage = [rotatedImageInfo image];

    auto rotatedImagePixels = imagePixels([rotatedImage CGImage]);

    EXPECT_TRUE([rotatedImageInfo type] == _WKActivatedElementTypeImage);
    EXPECT_WK_STREQ([rotatedImageInfo imageURL].lastPathComponent, "exif-orientation-8-llo.jpg");
    EXPECT_NOT_NULL(rotatedImage.get());
    EXPECT_EQ([rotatedImageInfo boundingRect].size.width, 50);
    EXPECT_EQ([rotatedImageInfo boundingRect].size.height, 100);
    EXPECT_EQ([rotatedImage size].width, 50);
    EXPECT_EQ([rotatedImage size].height, 100);

    EXPECT_EQ(rotatedImagePixels[indexOf(rotatedImage.get(), 0, 0)], yellow);
    EXPECT_EQ(rotatedImagePixels[indexOf(rotatedImage.get(), [rotatedImage size].width - 1, 0)], red);
    EXPECT_EQ(rotatedImagePixels[indexOf(rotatedImage.get(), 0, [rotatedImage size].height - 1)], green);
    EXPECT_EQ(rotatedImagePixels[indexOf(rotatedImage.get(), [rotatedImage size].width - 1, [rotatedImage size].height - 1)], blue);
}

TEST(_WKActivatedElementInfo, InfoForBlank)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadHTMLString:@"<html><head><meta name='viewport' content='initial-scale=1'></head><body style='margin: 0px;'></body></html>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {

        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeUnspecified);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 320);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 500);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoForBrokenImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView loadHTMLString:@"<html><head><meta name='viewport' content='initial-scale=1'></head><body style='margin: 0px;'><img  src='missing.gif' height='100' width='100'></body></html>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(50, 50) completionBlock: ^(_WKActivatedElementInfo *elementInfo) {

        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeUnspecified);
        EXPECT_EQ(elementInfo.boundingRect.size.width, 100);
        EXPECT_EQ(elementInfo.boundingRect.size.height, 100);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoForAttachment)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView loadHTMLString:@"<html><head><meta name='viewport' content='initial-scale=1'></head><body style='margin: 0px;'><attachment  /></body></html>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(20, 20) completionBlock:^(_WKActivatedElementInfo *elementInfo) {
        EXPECT_TRUE(elementInfo.type == _WKActivatedElementTypeAttachment);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoWithNestedSynchronousUpdates)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='initial-scale=1'><style>body { margin:0 } a { display:block; width:200px; height:200px }</style><a href='https://www.apple.com'>FOO</a>"];

    __block bool finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 100) completionBlock:^(_WKActivatedElementInfo *elementInfo) {
        _WKActivatedElementInfo *synchronousElementInfo = [webView activatedElementAtPosition:CGPointMake(300, 300)];
        EXPECT_EQ(_WKActivatedElementTypeUnspecified, synchronousElementInfo.type);
        checkElementTypeAndBoundingRect(elementInfo, _WKActivatedElementTypeLink, CGRectMake(0, 0, 200, 200));
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);

    finished = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 100) completionBlock:^(_WKActivatedElementInfo *elementInfo) {
        _WKActivatedElementInfo *synchronousElementInfo = [webView activatedElementAtPosition:CGPointMake(100, 100)];
        checkElementTypeAndBoundingRect(synchronousElementInfo, _WKActivatedElementTypeLink, CGRectMake(0, 0, 200, 200));
        checkElementTypeAndBoundingRect(elementInfo, _WKActivatedElementTypeLink, CGRectMake(0, 0, 200, 200));
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
}

TEST(_WKActivatedElementInfo, InfoWithNestedRequests)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    __block bool finishedWithInner = false;
    __block bool finishedWithOuter = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKActivatedElementInfo *outerElementInfo) {
        [webView _requestActivatedElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKActivatedElementInfo *innerElementInfo) {
            checkElementTypeAndBoundingRect(innerElementInfo, _WKActivatedElementTypeImage, CGRectMake(0, 0, 200, 200));
            finishedWithInner = true;
        }];
        checkElementTypeAndBoundingRect(outerElementInfo, _WKActivatedElementTypeImage, CGRectMake(0, 0, 200, 200));
        finishedWithOuter = true;
    }];
    TestWebKitAPI::Util::run(&finishedWithOuter);
    TestWebKitAPI::Util::run(&finishedWithInner);

    finishedWithInner = false;
    finishedWithOuter = false;
    [webView _requestActivatedElementAtPosition:CGPointMake(100, 50) completionBlock:^(_WKActivatedElementInfo *outerElementInfo) {
        [webView _requestActivatedElementAtPosition:CGPointMake(300, 300) completionBlock:^(_WKActivatedElementInfo *innerElementInfo) {
            EXPECT_EQ(_WKActivatedElementTypeUnspecified, innerElementInfo.type);
            finishedWithInner = true;
        }];
        checkElementTypeAndBoundingRect(outerElementInfo, _WKActivatedElementTypeImage, CGRectMake(0, 0, 200, 200));
        finishedWithOuter = true;
    }];
    TestWebKitAPI::Util::run(&finishedWithOuter);
    TestWebKitAPI::Util::run(&finishedWithInner);
}

TEST(_WKActivatedElementInfo, HitTestPointOutsideView)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    [webView synchronouslyLoadHTMLString:@R"(
        <!DOCTYPE html>
        <html>
            <meta name='viewport' content='width = device-width, initial-scale = 1'>
            <head>
            <style>
            html, body {
                margin: 0;
                width: 100%;
                height: 100%;
            }
            div {
                position: absolute;
                top: 0;
                left: 0;
                width: 100px;
                height: 100px;
                border: 1px solid tomato;
                box-sizing: border-box;
            }
            </style>
            </head>
            <body><div onclick='return true;'></body>
        </html>
    )"];

    auto elementRect = [webView activatedElementAtPosition:CGPointMake(101, 101)].boundingRect;
    EXPECT_TRUE(CGPointEqualToPoint(elementRect.origin, CGPointZero));
    EXPECT_TRUE(CGSizeEqualToSize(elementRect.size, CGSizeMake(100, 100)));
}

}

#endif
