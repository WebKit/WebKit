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

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <wtf/text/WTFString.h>

static bool receivedMessage = false;
static String gMessage;

@interface WebCryptoMasterKeyNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation WebCryptoMasterKeyNavigationDelegate

- (NSData *)_webCryptoMasterKeyForWebView:(WKWebView *)webView
{
    return nil;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    gMessage = message;
    receivedMessage = true;
    completionHandler();
}

@end

namespace TestWebKitAPI {

TEST(WebKit, WebCryptoNilMasterKey)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"navigation-client-default-crypto" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([[WebCryptoMasterKeyNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&receivedMessage);
    EXPECT_WK_STREQ("DataCloneError", gMessage);
}

} // namespace TestWebKitAPI
