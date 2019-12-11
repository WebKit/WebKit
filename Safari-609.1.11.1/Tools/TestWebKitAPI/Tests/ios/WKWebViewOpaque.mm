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

#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>

@interface OpaqueTestWKWebView : TestWKWebView
@end

@implementation OpaqueTestWKWebView

- (BOOL)isOpaque
{
    return YES;
}

@end

@interface NonOpaqueTestWKWebView : TestWKWebView
@end

@implementation NonOpaqueTestWKWebView

- (BOOL)isOpaque
{
    return NO;
}

@end

static void isOpaque(TestWKWebView *webView, BOOL opaque)
{
    EXPECT_EQ([webView isOpaque], opaque);
    EXPECT_EQ([[webView wkContentView] isOpaque], opaque);

    CGFloat backgroundColorAlpha = 0;
    [[webView scrollView].backgroundColor getRed:nullptr green:nullptr blue:nullptr alpha:&backgroundColorAlpha];

    EXPECT_EQ(backgroundColorAlpha, opaque ? 1 : 0);
}

TEST(WKWebView, IsOpaqueDefault)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    isOpaque(webView.get(), YES);
}

TEST(WKWebView, SetOpaqueYes)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    [webView setOpaque:YES];

    isOpaque(webView.get(), YES);
}

TEST(WKWebView, SetOpaqueNo)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    [webView setOpaque:NO];

    isOpaque(webView.get(), NO);
}

TEST(WKWebView, IsOpaqueYesSubclassOverridden)
{
    auto webView = adoptNS([[OpaqueTestWKWebView alloc] initWithFrame:NSZeroRect]);

    isOpaque(webView.get(), YES);
}

TEST(WKWebView, IsOpaqueNoSubclassOverridden)
{
    auto webView = adoptNS([[NonOpaqueTestWKWebView alloc] initWithFrame:NSZeroRect]);

    isOpaque(webView.get(), NO);
}

TEST(WKWebView, IsOpaqueYesDecodedFromArchive)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    [webView setOpaque:YES];

    auto data = insecurelyArchivedDataWithRootObject(webView.get());
    TestWKWebView *decodedWebView = insecurelyUnarchiveObjectFromData(data);

    isOpaque(decodedWebView, YES);
}

TEST(WKWebView, IsOpaqueNoDecodedFromArchive)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    [webView setOpaque:NO];

    auto data = insecurelyArchivedDataWithRootObject(webView.get());
    TestWKWebView *decodedWebView = insecurelyUnarchiveObjectFromData(data);

    isOpaque(decodedWebView, NO);
}

TEST(WKWebView, IsOpaqueDrawsBackgroundYesConfiguration)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    [configuration _setDrawsBackground:YES];

    EXPECT_EQ([configuration _drawsBackground], YES);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);

    isOpaque(webView.get(), YES);
}

TEST(WKWebView, IsOpaqueDrawsBackgroundNoConfiguration)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    [configuration _setDrawsBackground:NO];

    EXPECT_EQ([configuration _drawsBackground], NO);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);

    isOpaque(webView.get(), NO);
}

#endif
