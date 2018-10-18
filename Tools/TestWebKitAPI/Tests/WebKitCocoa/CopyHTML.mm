
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

#if WK_API_ENABLED && PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

@interface WKWebView ()
- (void)copy:(id)sender;
- (void)paste:(id)sender;
@end

#if PLATFORM(MAC)
NSString *readHTMLFromPasteboard()
{
    return [[NSPasteboard generalPasteboard] stringForType:NSHTMLPboardType];
}
#else
NSString *readHTMLFromPasteboard()
{
    id value = [[UIPasteboard generalPasteboard] valueForPasteboardType:(__bridge NSString *)kUTTypeHTML];
    if ([value isKindOfClass:[NSData class]])
        value = [[[NSString alloc] initWithData:(NSData *)value encoding:NSUTF8StringEncoding] autorelease];
    ASSERT([value isKindOfClass:[NSString class]]);
    return (NSString *)value;
}
#endif

static RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataEnabled()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, true);
    return webView;
}

TEST(CopyHTML, Sanitizes)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"copy-html"];
    [webView stringByEvaluatingJavaScript:@"HTMLToCopy = '<meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b>"
        "<!-- secret-->, world<script>dangerousCode()</script>';"];
    [webView copy:nil];
    [webView paste:nil];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"didCopy"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"didPaste"].boolValue);
    EXPECT_WK_STREQ("<meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b><!-- secret-->, world<script>dangerousCode()</script>",
        [webView stringByEvaluatingJavaScript:@"pastedHTML"]);
    String htmlInNativePasteboard = readHTMLFromPasteboard();
    EXPECT_TRUE(htmlInNativePasteboard.contains("hello"));
    EXPECT_TRUE(htmlInNativePasteboard.contains(", world"));
    EXPECT_FALSE(htmlInNativePasteboard.contains("secret"));
    EXPECT_FALSE(htmlInNativePasteboard.contains("dangerousCode"));
}

#endif // WK_API_ENABLED && PLATFORM(MAC)
