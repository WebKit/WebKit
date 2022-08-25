/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "TestWKWebView.h"
#import <wtf/text/StringBuilder.h>

static unsigned clientPermissionRequestCount;
static bool didReceiveMessage;
static bool didReceiveQueryPermission;
static bool isDone;

@interface PermissionsAPIMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation PermissionsAPIMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ([message body], @"prompt");
    didReceiveMessage = true;
}
@end


@interface PermissionsAPIUIDelegate : NSObject<WKUIDelegate>
- (void)_webView:(WKWebView *)webView queryPermission:(NSString*) name forOrigin:(WKSecurityOrigin *)origin completionHandler:(void (^)(WKPermissionDecision state))completionHandler;
@end

@implementation PermissionsAPIUIDelegate
- (void)_webView:(WKWebView *)webView queryPermission:(NSString*) name forOrigin:(WKSecurityOrigin *)origin completionHandler:(void (^)(WKPermissionDecision state))completionHandler {
    didReceiveQueryPermission = true;
    completionHandler(WKPermissionDecisionDeny);
}
@end

namespace TestWebKitAPI {

static void urlEncodeIfNeeded(uint8_t byte, StringBuilder& buffer)
{
    if (byte < '0' || (byte > '9' && byte < 'A') || (byte > 'Z' && byte < 'a') || byte > 'z') {
        buffer.append('%');
        buffer.append(upperNibbleToASCIIHexDigit(byte));
        buffer.append(lowerNibbleToASCIIHexDigit(byte));
        return;
    }
    buffer.append(byte);
}

TEST(PermissionsAPI, DataURL)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowTopNavigationToDataURLs = YES;
    auto messageHandler = adoptNS([[PermissionsAPIMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"msg"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto delegate = adoptNS([[PermissionsAPIUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    isDone = false;
    didReceiveMessage = false;
    didReceiveQueryPermission = false;
    char script[] = "<script>\
        navigator.permissions.query({ name : 'geolocation' }).then((result) => { \
            window.webkit.messageHandlers.msg.postMessage(result.state);\
        }, () => { \
            window.webkit.messageHandlers.msg.postMessage('FAIL');\
        });\
    </script>";

    StringBuilder buffer;
    buffer.append("data:text/html,");
    for (size_t cptr = 0; cptr < sizeof(script) - 1; ++cptr)
        urlEncodeIfNeeded(script[cptr], buffer);

    auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:buffer.toString()]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&didReceiveMessage);
    EXPECT_FALSE(didReceiveQueryPermission);
}

} // namespace TestWebKitAPI
