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

#if PLATFORM(MAC)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WKWebView, FTPMainResource)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get()._shouldSendConsoleLogsToUIProcessForTesting = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    
    RetainPtr consoleMessages = [NSMutableArray arrayWithCapacity:2];
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        [consoleMessages addObject:log];
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"ftp://example.com/main.html"]]];
    [webView _test_waitForDidFailProvisionalNavigation];

    EXPECT_EQ([consoleMessages count], 1u);
    EXPECT_TRUE([consoleMessages.get()[0] isEqualToString:@"FTP URLs are disabled"]);
}

TEST(WKWebView, FTPMainResourceRedirect)
{
    HTTPServer httpServer({
        { "/ftp_redirect"_s, { 301, {{ "Location"_s, "ftp://example.com/"_s }} } },
    });
    
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get()._shouldSendConsoleLogsToUIProcessForTesting = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    
    RetainPtr consoleMessages = [NSMutableArray arrayWithCapacity:2];
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        [consoleMessages addObject:log];
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView loadRequest:httpServer.request("/ftp_redirect"_s)];
    [webView _test_waitForDidFailProvisionalNavigation];

    EXPECT_EQ([consoleMessages count], 1u);
    EXPECT_TRUE([consoleMessages.get()[0] isEqualToString:@"FTP URLs are disabled"]);
}


static const char* subresourceBytes = R"FTPRESOURCE(
Hello
<img src="ftp://example.com/webkitten.png">
Goodbye
)FTPRESOURCE";

TEST(WKWebView, FTPSubresource)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get()._shouldSendConsoleLogsToUIProcessForTesting = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr consoleMessages = [NSMutableArray arrayWithCapacity:2];
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        [consoleMessages addObject:log];
    };
    webView.get().UIDelegate = uiDelegate.get();

    [webView synchronouslyLoadHTMLString:[NSString stringWithUTF8String:subresourceBytes]];

    EXPECT_EQ([consoleMessages count], 2u);
    EXPECT_TRUE([consoleMessages.get()[0] isEqualToString:@"FTP URLs are disabled"]);
    EXPECT_TRUE([consoleMessages.get()[1] isEqualToString:@"Cannot load image ftp://example.com/webkitten.png due to access control checks."]);
}

// Redirect from HTTP to FTP already fails, but let's make sure it keeps failing
TEST(WKWebView, FTPSubresourceRedirect)
{
    HTTPServer httpServer({
        { "/webkitten.png"_s, { 301, {{ "Location"_s, "ftp://example.com/webkitten.png"_s }} } },
    });
        
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get()._shouldSendConsoleLogsToUIProcessForTesting = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    
    // Allow HTTP to redirect away from HTTP for subresources for the purposes of this test
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetRestrictedHTTPResponseAccess(preferences, false);
    
    RetainPtr consoleMessages = [NSMutableArray arrayWithCapacity:2];
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().didReceiveConsoleLogForTesting = ^(NSString *log) {
        [consoleMessages addObject:log];
    };
    webView.get().UIDelegate = uiDelegate.get();

    auto htmlString = makeString("<img src='http://127.0.0.1:"_s, httpServer.port(), "/webkitten.png'>"_s);
    [webView synchronouslyLoadHTMLString:[NSString stringWithUTF8String:htmlString.utf8().data()]];

    EXPECT_EQ([consoleMessages count], 2u);
    EXPECT_TRUE([consoleMessages.get()[0] isEqualToString:@"FTP URLs are disabled"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
