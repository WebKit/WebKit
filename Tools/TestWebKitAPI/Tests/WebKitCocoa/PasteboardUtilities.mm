/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "PasteboardUtilities.h"
#import "TestCocoa.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>

#import "TestWKWebView.h"

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#endif

RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataEnabled(bool colorFilterEnabled)
{
#if PLATFORM(IOS_FAMILY)
    TestWebKitAPI::Util::instantiateUIApplicationIfNeeded();
#endif

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration _setColorFilterEnabled:colorFilterEnabled];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:webViewConfiguration.get()]);
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, true);
    return webView;
}

#if PLATFORM(MAC)
NSString *readURLFromPasteboard()
{
    if (NSURL *url = [NSURL URLFromPasteboard:NSPasteboard.generalPasteboard])
        return url.absoluteString;
    NSURL *url = [NSURL URLWithString:[NSPasteboard.generalPasteboard stringForType:NSURLPboardType]];
    return url.absoluteString;
}

NSString *readTitleFromPasteboard()
{
    NSArray *urlsAndTitles = [NSPasteboard.generalPasteboard propertyListForType:@"WebURLsWithTitlesPboardType"];
    if (!urlsAndTitles || ![urlsAndTitles isKindOfClass:[NSArray class]] || urlsAndTitles.count != 2)
        return nil;
    NSArray *titles = urlsAndTitles.lastObject;
    if (!titles || ![titles isKindOfClass:[NSArray class]] || titles.count != 1)
        return nil;
    return titles.firstObject;
}

void clearPasteboard()
{
    [NSPasteboard.generalPasteboard clearContents];
}
#else
NSString *readURLFromPasteboard()
{
    return UIPasteboard.generalPasteboard.URL.absoluteString;
}

NSString *readTitleFromPasteboard()
{
    return UIPasteboard.generalPasteboard.URL._title;
}

void clearPasteboard()
{
    UIPasteboard.generalPasteboard.items = @[];
}
#endif
