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

#include "config.h"

#if PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#include <MobileCoreServices/MobileCoreServices.h>
#endif

@interface WKWebView ()
- (void)copy:(id)sender;
- (void)paste:(id)sender;
@end

#if PLATFORM(MAC)
NSString *readURLFromPasteboard()
{
    if (NSURL *url = [NSURL URLFromPasteboard:[NSPasteboard generalPasteboard]])
        return url.absoluteString;
    NSURL *url = [NSURL URLWithString:[[NSPasteboard generalPasteboard] stringForType:NSURLPboardType]];
    return url.absoluteString;
}
#else
NSString *readURLFromPasteboard()
{
    return [UIPasteboard generalPasteboard].URL.absoluteString;
}
#endif

static RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataEnabled()
{
#if PLATFORM(IOS_FAMILY)
    UIApplicationInitialize();
#endif

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, true);
    return webView;
}

TEST(CopyURL, ValidURL)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"copy-url"];
    [webView stringByEvaluatingJavaScript:@"URLToCopy = 'http://webkit.org/b/123';"];
    [webView copy:nil];
    [webView paste:nil];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"window.didCopy"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"window.didPaste"].boolValue);
    EXPECT_WK_STREQ(@"http://webkit.org/b/123", [webView stringByEvaluatingJavaScript:@"window.pastedURL"]);
    EXPECT_WK_STREQ(@"http://webkit.org/b/123", readURLFromPasteboard());
}

TEST(CopyURL, UnescapedURL)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"copy-url"];
    [webView stringByEvaluatingJavaScript:@"URLToCopy = 'http://webkit.org/b/\u4F60\u597D;?x=8 + 6';"];
    [webView copy:nil];
    [webView paste:nil];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"window.didCopy"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"window.didPaste"].boolValue);
    EXPECT_WK_STREQ(@"http://webkit.org/b/\u4F60\u597D;?x=8 + 6", [webView stringByEvaluatingJavaScript:@"window.pastedURL"]);
    EXPECT_WK_STREQ(@"http://webkit.org/b/%E4%BD%A0%E5%A5%BD;?x=8%20+%206", readURLFromPasteboard());
}

TEST(CopyURL, MalformedURL)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"copy-url"];
    [webView stringByEvaluatingJavaScript:@"URLToCopy = 'bad url';"];
    [webView copy:nil];
    [webView paste:nil];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"window.didCopy"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"window.didPaste"].boolValue);
    EXPECT_WK_STREQ(@"bad url", [webView stringByEvaluatingJavaScript:@"window.pastedURL"]);
    EXPECT_WK_STREQ(@"", readURLFromPasteboard());
}

#endif // PLATFORM(MAC)

