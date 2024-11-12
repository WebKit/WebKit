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

#import "CocoaImage.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/Expected.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/Base64.h>

namespace TestWebKitAPI {

static bool done;

TEST(WebKit, LoadAndDecodeImage)
{
    auto contentsToVector = [] (NSURL *url) {
        NSData *data = [NSData dataWithContentsOfURL:url];
        Vector<uint8_t> result;
        for (NSUInteger i = 0; i < data.length; i++)
            result.append(static_cast<const uint8_t*>(data.bytes)[i]);
        return result;
    };
    auto pngData = [&] {
        return contentsToVector([NSBundle.test_resourcesBundle URLForResource:@"icon" withExtension:@"png"]);
    };
    auto untaggedPNGData = [&] {
        return contentsToVector([NSBundle.test_resourcesBundle URLForResource:@"400x400-green" withExtension:@"png"]);
    };
    auto gifData = [&] {
        return contentsToVector([NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"]);
    };

    HTTPServer server {
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingResponse } },
        { "/test_png"_s, { pngData() } },
        { "/test_untagged_png"_s, { untaggedPNGData() } },
        { "/test_gif"_s, { gifData() } },
        { "/redirect"_s, { 302, { { "Location"_s, "/test_png"_s } }, "redirecting..."_s } },
        { "/not_image"_s, { "this is not an image"_s } }
    };
    auto webView = adoptNS([WKWebView new]);

    auto imageOrError = [&] (auto requestPath, CGSize size = CGSizeZero) -> Expected<RetainPtr<Util::PlatformImage>, RetainPtr<NSError>> {
        __block RetainPtr<Util::PlatformImage> image;
        __block RetainPtr<NSError> error;
        __block bool done { false };
        [webView _loadAndDecodeImage:server.request(requestPath) constrainedToSize:size maximumBytesFromNetwork:std::numeric_limits<size_t>::max() completionHandler:^(Util::PlatformImage *imageResult, NSError *errorResult) {
            image = imageResult;
            error = errorResult;
            done = true;
        }];
        Util::run(&done);
        EXPECT_NE(!image, !error);
        if (image)
            return image;
        EXPECT_NOT_NULL(error);
        return makeUnexpected(error);
    };

    auto colorSpaceForImage = [&] (Util::PlatformImage *image) -> RetainPtr<CGColorSpaceRef> {
        return CGImageGetColorSpace(Util::convertToCGImage(image).get());
    };

    auto colorSpaceDescriptionForImage = [&] (Util::PlatformImage *image) -> NSString * {
        return (__bridge NSString *)adoptCF(CFCopyDescription(colorSpaceForImage(image).get())).autorelease();
    };

    auto result1 = imageOrError("/terminate"_s);
    EXPECT_WK_STREQ(result1.error().get().domain, NSURLErrorDomain);
    EXPECT_EQ(result1.error().get().code, NSURLErrorNetworkConnectionLost);

    auto result2 = imageOrError("/test_png"_s);
    EXPECT_EQ(result2->get().size.height, 174);
    EXPECT_EQ(result2->get().size.width, 215);

    auto result3 = imageOrError("/not_image"_s);
    EXPECT_WK_STREQ(result3.error().get().domain, NSURLErrorDomain);
    EXPECT_EQ(result3.error().get().code, NSURLErrorCannotDecodeContentData);

    auto result4 = imageOrError("/test_png"_s, CGSizeMake(100, 100));
    EXPECT_EQ(result4->get().size.height, 80);
    EXPECT_EQ(result4->get().size.width, 100);
    EXPECT_TRUE([colorSpaceDescriptionForImage(result4->get()) containsString:@"Calibrated RGB"]);

    auto result5 = imageOrError("/test_png"_s, CGSizeMake(1000, 1000));
    EXPECT_EQ(result5->get().size.height, 174);
    EXPECT_EQ(result5->get().size.width, 215);

    auto result6 = imageOrError("/test_gif"_s);
    EXPECT_EQ(result6->get().size.height, 64);
    EXPECT_EQ(result6->get().size.width, 52);

    auto result7 = imageOrError("/redirect"_s);
    EXPECT_EQ(result7->get().size.height, 174);
    EXPECT_EQ(result7->get().size.width, 215);

    auto result8 = imageOrError("/test_untagged_png"_s, CGSizeMake(100, 100));
    EXPECT_EQ(result8->get().size.height, 100);
    EXPECT_EQ(result8->get().size.width, 100);
    EXPECT_TRUE([colorSpaceDescriptionForImage(result8->get()) containsString:@"sRGB"]);

    HTTPServer tlsServer { {
        { "/"_s, { pngData() } },
    }, HTTPServer::Protocol::Https };
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    __block bool done { false };
    [webView _loadAndDecodeImage:tlsServer.request() constrainedToSize:CGSizeZero maximumBytesFromNetwork:36541 completionHandler:^(Util::PlatformImage *image, NSError *error) {
        EXPECT_EQ(image.size.height, 174);
        EXPECT_EQ(image.size.width, 215);
        EXPECT_NULL(error);
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView _loadAndDecodeImage:tlsServer.request() constrainedToSize:CGSizeZero maximumBytesFromNetwork:36540 completionHandler:^(Util::PlatformImage *image, NSError *error) {
        EXPECT_NULL(image);
        EXPECT_EQ(error.code, NSURLErrorDataLengthExceedsMaximum);
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView _loadAndDecodeImage:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://127.0.0.1:1/"]] constrainedToSize:CGSizeZero maximumBytesFromNetwork:std::numeric_limits<size_t>::max() completionHandler:^(Util::PlatformImage *image, NSError *error) {
        EXPECT_NULL(image);
        EXPECT_EQ(error.code, 103);
        EXPECT_WK_STREQ(error.domain, "WebKitErrorDomain");
        done = true;
    }];
    Util::run(&done);
}

TEST(WebKit, GetInformationFromImageData)
{
    RetainPtr webView = adoptNS([WKWebView new]);
    done = false;
    RetainPtr pngData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"icon" withExtension:@"png"]];
    [webView _getInformationFromImageData:pngData.get() completionHandler:^(NSString *typeIdentifier, NSArray<NSValue *> *availableSizes, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([typeIdentifier isEqualToString:UTTypePNG.identifier]);
        EXPECT_EQ(1u, availableSizes.count);
        NSValue *size = [availableSizes firstObject];
        EXPECT_EQ(215, [size sizeValue].width);
        EXPECT_EQ(174, [size sizeValue].height);
        done = true;
    }];
    Util::run(&done);

    done = false;
    RetainPtr gifData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"apple" withExtension:@"gif"]];
    [webView _getInformationFromImageData:gifData.get() completionHandler:^(NSString *typeIdentifier, NSArray<NSValue *> *availableSizes, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([typeIdentifier isEqualToString:UTTypeGIF.identifier]);
        EXPECT_EQ(1u, availableSizes.count);
        for (NSValue *size in availableSizes) {
            EXPECT_EQ(52, [size sizeValue].width);
            EXPECT_EQ(64, [size sizeValue].height);
        }
        done = true;
    }];
    Util::run(&done);

    done = false;
    RetainPtr svgData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"AllAhem" withExtension:@"svg"]];
    [webView _getInformationFromImageData:svgData.get() completionHandler:^(NSString *typeIdentifier, NSArray<NSValue *> *availableSizes, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([typeIdentifier isEqualToString:UTTypeSVG.identifier]);
        EXPECT_EQ(0u, availableSizes.count);
        done = true;
    }];
    Util::run(&done);

    done = false;
    RetainPtr pdfData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView _getInformationFromImageData:pdfData.get() completionHandler:^(NSString *typeIdentifier, NSArray<NSValue *> *availableSizes, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NULL(typeIdentifier);
        EXPECT_EQ(0u, availableSizes.count);
        done = true;
    }];
    Util::run(&done);
}

TEST(WebKit, CreateIconDataFromImageData)
{
    RetainPtr webView = adoptNS([WKWebView new]);
    RetainPtr imageData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"icon" ofType:@"png"]];
    RetainPtr sizes = adoptNS([[NSMutableArray alloc] init]);
    RetainPtr length1 = [NSNumber numberWithUnsignedInt:16];
    RetainPtr length2 = [NSNumber numberWithUnsignedInt:256];
    NSArray *lengths = @[length1.get(), length2.get()];
    __block RetainPtr<NSData> iconData;
    done = false;
    [webView _createIconDataFromImageData:imageData.get() withLengths:lengths completionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        iconData = result;
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView _decodeImageData:iconData.get() preferredSize:[NSValue valueWithSize:NSMakeSize(16, 16)] completionHandler:^(CocoaImage *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result.size.width, 16);
        EXPECT_EQ(result.size.height, 16);
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView _decodeImageData:iconData.get() preferredSize:[NSValue valueWithSize:NSMakeSize(32, 32)] completionHandler:^(CocoaImage *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_EQ(result.size.width, 256);
        EXPECT_EQ(result.size.height, 256);
        done = true;
    }];
    Util::run(&done);
}

}
