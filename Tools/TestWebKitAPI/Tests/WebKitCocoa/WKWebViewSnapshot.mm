/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "CGImagePixelReader.h"
#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebCore/Color.h>
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKSnapshotConfigurationPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKJSHandle.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

static NSInteger getPixelIndex(NSInteger x, NSInteger y, NSInteger width)
{
    return (y * width + x) * 4;
}

@interface TestSnapshotWrapper : NSObject
@property (nonatomic, readonly) RetainPtr<CGImageRef> image;
@property (nonatomic, readonly) RetainPtr<NSError> error;
@end

@implementation TestSnapshotWrapper

- (void)takeSnapshotWithWebView:(WKWebView *)webView configuration:(WKSnapshotConfiguration *)configuration completionHandler:(void (^)(void))completionHandler
{
    auto completionBlock = makeBlockPtr(completionHandler);
    [webView takeSnapshotWithConfiguration:configuration completionHandler:^(TestWebKitAPI::Util::PlatformImage *snapshotImage, NSError *error) {
        _image = TestWebKitAPI::Util::convertToCGImage(snapshotImage);
        _error = error;
        completionBlock();
    }];
}

@end

namespace TestWebKitAPI {

struct SnapshotResult {
    RetainPtr<TestWebKitAPI::Util::PlatformImage> image;
    RetainPtr<NSError> error;
};

}

@interface WKWebView (SnapshotTests)
- (TestWebKitAPI::SnapshotResult)takeSnapshotOfNode:(_WKJSHandle *)nodeHandle;
@end

@implementation WKWebView (SnapshotTests)

- (TestWebKitAPI::SnapshotResult)takeSnapshotOfNode:(_WKJSHandle *)nodeHandle
{
    __block bool done = false;
    __block TestWebKitAPI::SnapshotResult result;
    [self _takeSnapshotOfNode:nodeHandle completionHandler:^(TestWebKitAPI::Util::PlatformImage *snapshotImage, NSError *error) {
        result = { { snapshotImage }, { error } };
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

@end

namespace TestWebKitAPI {

TEST(WKWebView, SnapshotImageError)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);
    
    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    [webView _killWebContentProcessAndResetState];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(snapshotImage);
        EXPECT_WK_STREQ(@"WKErrorDomain", error.domain);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotImageEmptyRect)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto pid = [webView _webProcessIdentifier];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, 0, 0)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(0, snapshotImage.size.width);
        EXPECT_EQ(0, snapshotImage.size.height);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(pid, [webView _webProcessIdentifier]); // Make sure the WebProcess did not crash.
}

TEST(WKWebView, SnapshotImageZeroWidth)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto pid = [webView _webProcessIdentifier];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(0.0)]; // Will fall back to the view width.

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(viewWidth, snapshotImage.size.width);
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(pid, [webView _webProcessIdentifier]); // Make sure the WebProcess did not crash.
}

TEST(WKWebView, SnapshotImageZeroSizeView)
{
    CGFloat viewWidth = 0;
    CGFloat viewHeight = 0;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto pid = [webView _webProcessIdentifier];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(100)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(0, snapshotImage.size.width);
        EXPECT_EQ(0, snapshotImage.size.height);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(pid, [webView _webProcessIdentifier]); // Make sure the WebProcess did not crash.
}

TEST(WKWebView, SnapshotImageZeroSizeViewNoConfiguration)
{
    CGFloat viewWidth = 0;
    CGFloat viewHeight = 0;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto pid = [webView _webProcessIdentifier];

    isDone = false;
    [webView takeSnapshotWithConfiguration:nil completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_EQ(0, snapshotImage.size.width);
        EXPECT_EQ(0, snapshotImage.size.height);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
    EXPECT_EQ(pid, [webView _webProcessIdentifier]); // Make sure the WebProcess did not crash.
}

TEST(WKWebView, SnapshotImageEmptyWithOutOfScopeCompletionHandler)
{
    CGFloat viewWidth = 0;
    CGFloat viewHeight = 0;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto pid = [webView _webProcessIdentifier];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, 0, 0)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];

    isDone = false;

    auto snapshotWrapper = adoptNS([[TestSnapshotWrapper alloc] init]);
    [snapshotWrapper takeSnapshotWithWebView:webView.get() configuration:snapshotConfiguration.get() completionHandler:^{
        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);

    EXPECT_NULL([snapshotWrapper error]);

    auto image = [snapshotWrapper image].get();
    EXPECT_EQ(0UL, CGImageGetWidth(image));
    EXPECT_EQ(0UL, CGImageGetHeight(image));

    EXPECT_EQ(pid, [webView _webProcessIdentifier]); // Make sure the WebProcess did not crash.
}

TEST(WKWebView, SnapshotImageBaseCase)
{
    NSInteger viewWidth = 800;
    NSInteger viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    RetainPtr<Util::PlatformWindow> window;
    CGFloat backingScaleFactor;

#if PLATFORM(MAC)
    window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    backingScaleFactor = [window backingScaleFactor];
#elif PLATFORM(IOS_FAMILY)
    window = adoptNS([[UIWindow alloc] initWithFrame:[webView frame]]);
    [window addSubview:webView.get()];
    backingScaleFactor = [[window screen] scale];
#endif

    [webView loadHTMLString:@"<body style='background-color:red;'><div style='background-color:blue; position:absolute; width:100px; height:100px; top:50px; left:50px'></div></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);

        EXPECT_EQ(viewWidth, snapshotImage.size.width);

        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());

        NSInteger pixelIndex = getPixelIndex(0, 0, viewWidthInPixels);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        // Inside the blue div (50, 50, 100, 100)
        pixelIndex = getPixelIndex(55 * backingScaleFactor, 55 * backingScaleFactor, viewWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(255, rgba[pixelIndex + 2]);

        pixelIndex = getPixelIndex(155 * backingScaleFactor, 155 * backingScaleFactor, viewWidthInPixels);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        free(rgba);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotImageScale)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    CGFloat scaleFactor = 2;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth * scaleFactor)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(snapshotImage);
        EXPECT_EQ(viewWidth * scaleFactor, snapshotImage.size.width);
        EXPECT_EQ(viewHeight * scaleFactor, snapshotImage.size.height);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotImageNilConfiguration)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    isDone = false;
    [webView takeSnapshotWithConfiguration:nil completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(snapshotImage);
        EXPECT_EQ([webView bounds].size.width, snapshotImage.size.width);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotImageUninitializedConfiguration)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(snapshotImage);
        EXPECT_EQ([webView bounds].size.width, snapshotImage.size.width);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotImageUninitializedSnapshotWidth)
{
    CGFloat viewWidth = 800;
    CGFloat viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(snapshotImage);
        EXPECT_EQ([snapshotConfiguration rect].size.width, snapshotImage.size.width);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotImageLargeAsyncDecoding)
{
    NSInteger viewWidth = 800;
    NSInteger viewHeight = 600;
    // FIXME: This test fails when adopting TestWKWebView; it might be interesting to investigate why.
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    NSURL *fileURL = [NSBundle.test_resourcesBundle URLForResource:@"large-red-square-image" withExtension:@"html"];
    [webView loadFileURL:fileURL allowingReadAccessToURL:fileURL];
    [webView _test_waitForDidFinishNavigation];

    auto snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);

        EXPECT_EQ(viewWidth, snapshotImage.size.width);

        auto cgImage = Util::convertToCGImage(snapshotImage);
        auto colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        uint8_t *rgba = (unsigned char *)calloc(viewWidth * viewHeight * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidth, viewHeight, 8, 4 * viewWidth, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidth, viewHeight), cgImage.get());

        // Top-left corner of the div (0, 0, 100, 100)
        NSInteger pixelIndex = getPixelIndex(0, 0, viewWidth);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        // Right-bottom corner of the div (0, 0, 100, 100)
        pixelIndex = getPixelIndex(99, 99, viewWidth);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);

        // Outside the div (0, 0, 100, 100)
        pixelIndex = getPixelIndex(100, 100, viewWidth);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(255, rgba[pixelIndex + 1]);
        EXPECT_EQ(255, rgba[pixelIndex + 2]);

        free(rgba);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotAfterScreenUpdates)
{
    // The API tests currently cannot truly test SnapshotConfiguration.afterScreenUpdates since it is only needed
    // on iOS devices, and we do not currently run API tests on iOS devices. So we expect this test to pass with
    // afterScreenUpdates set to YES or NO on the configuration. On device, afterScreenUpdates must be YES in order
    // pass this test.
    NSInteger viewWidth = 800;
    NSInteger viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);
    
    RetainPtr<Util::PlatformWindow> window;
    CGFloat backingScaleFactor;
    
#if PLATFORM(MAC)
    window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    backingScaleFactor = [window backingScaleFactor];
#elif PLATFORM(IOS_FAMILY)
    window = adoptNS([[UIWindow alloc] initWithFrame:[webView frame]]);
    [window addSubview:webView.get()];
    backingScaleFactor = [[window screen] scale];
#endif
    
    [webView loadHTMLString:@"<body style='margin:0'><div id='change-me' style='background-color:red; position:fixed; width:100%; height:100%'></div></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    
    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];
    [snapshotConfiguration setAfterScreenUpdates:YES];

    [webView evaluateJavaScript:@"var div = document.getElementById('change-me');div.style.backgroundColor = 'blue';" completionHandler:nil];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        
        EXPECT_EQ(viewWidth, snapshotImage.size.width);
        
        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
        
        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;
        
        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());
        
        NSInteger pixelIndex = getPixelIndex(0, 0, viewWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(255, rgba[pixelIndex + 2]);

        free(rgba);
        
        isDone = true;
    }];
    
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotWithoutAfterScreenUpdates)
{
    // SnapshotConfiguration.afterScreenUpdates tests currently cannot truly test this API since it is only needed
    // on iOS devices, and we do not currently run API tests on iOS devices. The expectations below are based on
    // what we expect in the simulator and on macOS, which is that setting afterScreenUpdates to NO will still
    // result in a snapshot that includes the recent screen updates. If we get these tests running on iOS device,
    // then we would expect the pixels to be red instead of blue.
    NSInteger viewWidth = 800;
    NSInteger viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);
    
    RetainPtr<Util::PlatformWindow> window;
    CGFloat backingScaleFactor;
    
#if PLATFORM(MAC)
    window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    backingScaleFactor = [window backingScaleFactor];
#elif PLATFORM(IOS_FAMILY)
    window = adoptNS([[UIWindow alloc] initWithFrame:[webView frame]]);
    [window addSubview:webView.get()];
    backingScaleFactor = [[window screen] scale];
#endif
    
    [webView loadHTMLString:@"<body style='margin:0'><div id='change-me' style='background-color:red; position:fixed; width:100%; height:100%'></div></body>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
    
    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];
    [snapshotConfiguration setAfterScreenUpdates:NO];
    
    [webView evaluateJavaScript:@"var div = document.getElementById('change-me');div.style.backgroundColor = 'blue';" completionHandler:nil];
    
    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);
        
        EXPECT_EQ(viewWidth, snapshotImage.size.width);
        
        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
        
        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;
        
        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());

        NSInteger pixelIndex = getPixelIndex(0, 0, viewWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(255, rgba[pixelIndex + 2]);

        free(rgba);
        
        isDone = true;
    }];
    
    TestWebKitAPI::Util::run(&isDone);
}

// FIXME: Confirm that this works on iOS too.
#if PLATFORM(MAC)
TEST(WKWebView, SnapshotWebGL)
{
    NSInteger viewWidth = 800;
    NSInteger viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    RetainPtr<Util::PlatformWindow> window;
    CGFloat backingScaleFactor;

#if PLATFORM(MAC)
    window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    backingScaleFactor = [window backingScaleFactor];
#elif PLATFORM(IOS_FAMILY)
    window = adoptNS([[UIWindow alloc] initWithFrame:[webView frame]]);
    [window addSubview:webView.get()];
    backingScaleFactor = [[window screen] scale];
#endif

    [webView loadHTMLString:@"<style>body{margin:0;padding:0}</style><canvas></canvas><script>window.addEventListener('load', () => { let ctx = document.querySelector('canvas').getContext('webgl'); ctx.clearColor(0, 1, 0, 1); ctx.clear(ctx.COLOR_BUFFER_BIT); }, false);</script>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:NSMakeRect(0, 0, viewWidth, viewHeight)];
    [snapshotConfiguration setSnapshotWidth:@(viewWidth)];
    [snapshotConfiguration setAfterScreenUpdates:YES];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);

        EXPECT_EQ(viewWidth, snapshotImage.size.width);

        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());

        NSInteger pixelIndex = getPixelIndex(0, 0, viewWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(255, rgba[pixelIndex + 1]);
        EXPECT_EQ(0, rgba[pixelIndex + 2]);
        EXPECT_EQ(255, rgba[pixelIndex + 3]);

        free(rgba);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}
#endif

#if PLATFORM(MAC)
TEST(WKWebView, SnapshotWithoutSelectionHighlighting)
{
    NSInteger viewWidth = 800;
    NSInteger viewHeight = 600;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    RetainPtr<Util::PlatformWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    CGFloat backingScaleFactor = [window backingScaleFactor];

    // Select a line of underscore characters so we have a large selection highlight area that doesn't intersect with the character glyphs.
    [webView loadHTMLString:@"<body> <div id='selectThis'>________</div> </body> <script> window.getSelection().selectAllChildren( document.getElementById('selectThis')); </script>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration _setIncludesSelectionHighlighting:NO];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);

        EXPECT_EQ(viewWidth, snapshotImage.size.width);

        RetainPtr cgImage = Util::convertToCGImage(snapshotImage);
        RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());

        NSInteger viewWidthInPixels = viewWidth * backingScaleFactor;
        NSInteger viewHeightInPixels = viewHeight * backingScaleFactor;

        uint8_t *rgba = (unsigned char *)calloc(viewWidthInPixels * viewHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, viewWidthInPixels, viewHeightInPixels, 8, 4 * viewWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, viewWidthInPixels, viewHeightInPixels), cgImage.get());

        // Get a pixel from inside where the selection highlight would normally be and verify that the highlight isn't in the snapshot.
        NSInteger pixelIndex = getPixelIndex(20 * backingScaleFactor, 20 * backingScaleFactor, viewWidthInPixels);
        EXPECT_EQ(255, rgba[pixelIndex]);
        EXPECT_EQ(255, rgba[pixelIndex + 1]);
        EXPECT_EQ(255, rgba[pixelIndex + 2]);

        free(rgba);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, SnapshotWithContentsRect)
{
    CGFloat viewWidth = 200;
    CGFloat viewHeight = 200;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, viewWidth, viewHeight)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];
    CGFloat backingScaleFactor = [window backingScaleFactor];

    [webView loadHTMLString:@"<body><div style='position: absolute; left: 200px; width: 800px; height: 600px; background-color: blue;'></div>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration _setUsesContentsRect:YES];

    isDone = false;
    [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(Util::PlatformImage *snapshotImage, NSError *error) {
        EXPECT_NULL(error);

        NSInteger snapshotWidthInPixels = snapshotImage.size.width;
        NSInteger snapshotHeightInPixels = snapshotImage.size.height;
        EXPECT_GT(snapshotWidthInPixels, viewWidth);
        EXPECT_GT(snapshotHeightInPixels, viewHeight);

        auto cgImage = Util::convertToCGImage(snapshotImage);
        auto colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
        uint8_t *rgba = (unsigned char *)calloc(snapshotWidthInPixels * snapshotHeightInPixels * 4, sizeof(unsigned char));
        auto context = adoptCF(CGBitmapContextCreate(rgba, snapshotWidthInPixels, snapshotHeightInPixels, 8, 4 * snapshotWidthInPixels, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
        CGContextDrawImage(context.get(), CGRectMake(0, 0, snapshotWidthInPixels, snapshotHeightInPixels), cgImage.get());

        // Inside the blue div.
        auto pixelIndex = getPixelIndex(300 * backingScaleFactor, 300 * backingScaleFactor, snapshotWidthInPixels);
        EXPECT_EQ(0, rgba[pixelIndex]);
        EXPECT_EQ(0, rgba[pixelIndex + 1]);
        EXPECT_EQ(255, rgba[pixelIndex + 2]);
        free(rgba);

        isDone = true;
    }];

    TestWebKitAPI::Util::run(&isDone);
}
#endif

TEST(WKWebView, SnapshotNodeByJSHandle)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"node-snapshotting"];

    RetainPtr worldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    [worldConfiguration setAllowJSHandleCreation:YES];

    RetainPtr world = [WKContentWorld _worldWithConfiguration:worldConfiguration.get()];
    auto querySelector = [&](ASCIILiteral selector) -> RetainPtr<_WKJSHandle> {
        RetainPtr script = [NSString stringWithFormat:@"webkit.createJSHandle(document.querySelector('%s'))", selector.characters()];
        return dynamic_objc_cast<_WKJSHandle>([webView objectByEvaluatingJavaScript:script.get() inFrame:nil inContentWorld:world.get()]);
    };

    {
        auto [image, error] = [webView takeSnapshotOfNode:querySelector(".red-box"_s).get()];
        EXPECT_NULL(error);

        CGImagePixelReader reader { Util::convertToCGImage(image.get()).get() };
        EXPECT_WK_STREQ("rgb(255, 0, 0)", reader.cssColorAt(2, 2));
        EXPECT_WK_STREQ("rgb(255, 0, 0)", reader.cssColorAt(reader.width() / 2, reader.height() / 2));
    }

    {
        auto [image, error] = [webView takeSnapshotOfNode:querySelector("img"_s).get()];
        EXPECT_NULL(error);

        CGImagePixelReader reader { Util::convertToCGImage(image.get()).get() };
        EXPECT_TRUE(reader.isTransparentBlack(2, 2));
        EXPECT_FALSE(reader.isTransparentBlack(reader.width() / 2, reader.height() / 2));
    }

    {
        auto [image, error] = [webView takeSnapshotOfNode:querySelector("h2"_s).get()];
        EXPECT_NULL(error);

        CGImagePixelReader reader { Util::convertToCGImage(image.get()).get() };
        EXPECT_WK_STREQ("rgb(0, 0, 0)", reader.cssColorAt(2, 2));
        EXPECT_FALSE(reader.isTransparentBlack(reader.width() / 2, reader.height() / 2));
    }

    {
        auto [image, error] = [webView takeSnapshotOfNode:nil];
        EXPECT_NOT_NULL(error);
        EXPECT_NULL(image);
    }

    {
        NSString *scriptToRun = @"window.randomObject = {'foo': 1}; webkit.createJSHandle(randomObject)";
        RetainPtr handle = dynamic_objc_cast<_WKJSHandle>([webView objectByEvaluatingJavaScript:scriptToRun inFrame:nil inContentWorld:world.get()]);
        auto [image, error] = [webView takeSnapshotOfNode:handle.get()];
        EXPECT_NOT_NULL(error);
        EXPECT_NULL(image);
    }
}

} // namespace TestWebKitAPI
