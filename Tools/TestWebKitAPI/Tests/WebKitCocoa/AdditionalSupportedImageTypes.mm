/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED && PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>

static void runTest(NSArray<NSString *> *additionalSupportedImageTypes, NSString *imageURL, NSString *imageExtension, CGFloat imageWidth)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAdditionalSupportedImageTypes:additionalSupportedImageTypes];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:imageURL withExtension:imageExtension subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView _test_waitForDidFinishNavigation];

    __block bool isDone = false;
    [webView _doAfterNextPresentationUpdate:^{
        [webView evaluateJavaScript:@"[document.querySelector('img').width]" completionHandler:^(id value, NSError *error) {
            CGFloat width = [[value objectAtIndex:0] floatValue];
            EXPECT_EQ(width, imageWidth);
            isDone = true;
        }];
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WebKit, AddSupportedImageType)
{
    runTest(@[@"public.png"], @"400x400-green", @"png", 400);
}

TEST(WebKit, AddSupportedAndBogusImageTypes)
{
    runTest(@[@"public.png", @"public.bogus"], @"400x400-green", @"png", 400);
}

TEST(WebKit, AddSupportedAndBogusImageTypesTwice)
{
    runTest(@[@"public.png", @"public.bogus", @"public.png", @"public.bogus"], @"400x400-green", @"png", 400);
}

TEST(WebKit, AddUnsupportedImageType)
{
    runTest(@[@"com.truevision.tga-image"], @"100x100-red", @"tga", 100);
}

TEST(WebKit, AddUnsupportedAndBogusImageTypes)
{
    runTest(@[@"com.truevision.tga-image", @"public.bogus"], @"100x100-red", @"tga", 100);
}

TEST(WebKit, AddUnsupportedAndBogusImageTypesTwice)
{
    runTest(@[@"com.truevision.tga-image", @"public.bogus", @"com.truevision.tga-image", @"public.bogus"], @"100x100-red", @"tga", 100);
}

#endif
