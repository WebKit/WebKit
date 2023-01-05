/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserScriptPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, InvalidConfiguration)
{
    auto shouldThrowExceptionWhenUsed = [](Function<void(WKWebViewConfiguration *)>&& modifier) {
        @try {
            auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
            modifier(configuration.get());
            auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
            EXPECT_TRUE(false);
        } @catch (NSException *exception) {
            EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        }
    };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setProcessPool:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setPreferences:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setUserContentController:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setWebsiteDataStore:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration _setVisitedLinkStore:nil];
    });
#pragma clang diagnostic pop

    // Related WebViews cannot use different data stores.
    auto configurationForEphemeralView = adoptNS([[WKWebViewConfiguration alloc] init]);
    configurationForEphemeralView.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForEphemeralView.get()]);
    shouldThrowExceptionWhenUsed([&](WKWebViewConfiguration *configuration) {
        [configuration _setRelatedWebView:ephemeralWebView.get()];
    });
}

TEST(WebKit, ConfigurationGroupIdentifierIsCopied)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setGroupIdentifier:@"TestGroupIdentifier"];

    auto configuationCopy = adoptNS([configuration copy]);
    EXPECT_STREQ([configuration _groupIdentifier].UTF8String, [configuationCopy _groupIdentifier].UTF8String);
}

TEST(WebKit, DefaultConfigurationEME)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _legacyEncryptedMediaAPIEnabled]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView loadHTMLString:@"<html>hi</html>" baseURL:nil];
    __block bool done = false;
    [webView evaluateJavaScript:@"window.WebKitMediaKeys ? 'ENABLED' : 'DISABLED'" completionHandler:^(id result, NSError *){
        EXPECT_TRUE([result isEqualToString:@"ENABLED"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, ConfigurationHTTPSUpgrade)
{
    using namespace TestWebKitAPI;
    bool done = false;
    Vector<char> requestBytes;
    HTTPServer server([&] (Connection connection) {
        connection.receiveHTTPRequest([&, connection](Vector<char>&& bytes) mutable {
            requestBytes = WTFMove(bytes);
            done = true;
        });
    });

    auto runTest = [&] (bool upgrade) {
        done = false;
        auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [storeConfiguration setAllowsServerPreconnect:NO];
        [storeConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port()),
            (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port()),
        }];
        auto store = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
        auto configuration = adoptNS([WKWebViewConfiguration new]);
        [configuration setWebsiteDataStore:store.get()];
        [configuration setUpgradeKnownHostsToHTTPS:upgrade];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://www.opengl.org/"]];
        [webView loadRequest:request];
        Util::run(&done);
    };

    auto checkRequestBytesStartsWith = [&] (const char* string) {
        EXPECT_GT(requestBytes.size(), strlen(string));
        if (requestBytes.size() <= strlen(string))
            return;
        requestBytes[strlen(string)] = 0;
        EXPECT_WK_STREQ(requestBytes.data(), string);
    };

    runTest(true);
    checkRequestBytesStartsWith("CONNECT www.opengl.org:443 HTTP/1.1\r\n");
    runTest(false);
    checkRequestBytesStartsWith("GET http://www.opengl.org/ HTTP/1.1\r\n");
    
    EXPECT_TRUE([WKWebView _willUpgradeToHTTPS:[NSURL URLWithString:@"http://www.opengl.org/"]]);
    EXPECT_FALSE([WKWebView _willUpgradeToHTTPS:[NSURL URLWithString:@"https://www.opengl.org/"]]);
    EXPECT_FALSE([WKWebView _willUpgradeToHTTPS:[NSURL URLWithString:@"custom-scheme://www.opengl.org/"]]);
    EXPECT_FALSE([WKWebView _willUpgradeToHTTPS:[NSURL URLWithString:@"http://example.com/"]]);
}

TEST(WebKit, ConfigurationDisableJavaScript)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body onload=\"document.write('FAIL');\">PASS</body>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"PASS");
}

TEST(WebKit, ConfigurationDisableJavaScriptNestedBody)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<table><body onload=\"document.write('FAIL');\"></table>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"<table></table>");
}

TEST(WebKit, ConfigurationDisableJavaScriptSVGAnimateElement)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\" to=\"javascript:document.write('FAIL')\"/></a></svg>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\"></animate></a></svg>");
}

TEST(WebKit, ConfigurationDisableJavaScriptSVGAnimateElementComplex)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\" dur=\"4s\" calcMode=\"spline\" repeatCount=\"indefinite\" values=\"javascript:document.write('FAIL'); javascript:document.write('FAIL'); javascript:document.write(location.href); javascript:document.write('FAIL'); javascript:document.write('FAIL')\" keyTimes=\"0; 0.25; 0.5; 0.75; 1\" keySplines=\"0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1\"/></a></svg>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\" dur=\"4s\" calcMode=\"spline\" repeatCount=\"indefinite\" keyTimes=\"0; 0.25; 0.5; 0.75; 1\" keySplines=\"0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1\"></animate></a></svg>");
}

TEST(WebKit, ConfigurationMaskedURLSchemes)
{
    [TestProtocol registerWithScheme:@"https"];

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_EQ([configuration _maskedURLSchemes].count, 0U);

    [configuration _setMaskedURLSchemes:[NSSet setWithObjects:@"test-scheme", @"another-scheme", nil]];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    auto delegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<img src=\"test-scheme://foo.com/bar.jpg\"><img src=\"baz.png\">" baseURL:[NSURL URLWithString:@"https://example.com"]];

    NSString *imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[0].src"];
    EXPECT_WK_STREQ(imageSource, @"webkit-masked-url://hidden/");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[0].getAttribute(\"src\")"];
    EXPECT_WK_STREQ(imageSource, @"webkit-masked-url://hidden/");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[0].getAttributeNode(\"src\").value"];
    EXPECT_WK_STREQ(imageSource, @"webkit-masked-url://hidden/");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].src"];
    EXPECT_WK_STREQ(imageSource, @"https://example.com/baz.png");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].getAttribute(\"src\")"];
    EXPECT_WK_STREQ(imageSource, @"baz.png");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].getAttributeNS(null, \"src\")"];
    EXPECT_WK_STREQ(imageSource, @"baz.png");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].getAttributeNode(\"src\").value"];
    EXPECT_WK_STREQ(imageSource, @"baz.png");

    [webView synchronouslyLoadHTMLString:@"<img srcset=\"test-scheme://foo.com/bar.jpg 1x, bar.jpg 2x, another-scheme://foo.com/bar.gif 3x\"><img srcset=\"https://apple.com/baz.png 2x, baz.png 1x\">" baseURL:[NSURL URLWithString:@"https://example.com"]];

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[0].srcset"];
    EXPECT_WK_STREQ(imageSource, @"webkit-masked-url://hidden/ 1x, bar.jpg 2x, webkit-masked-url://hidden/ 3x");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[0].getAttribute(\"srcset\")"];
    EXPECT_WK_STREQ(imageSource, @"webkit-masked-url://hidden/ 1x, bar.jpg 2x, webkit-masked-url://hidden/ 3x");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[0].getAttributeNode(\"srcset\").value"];
    EXPECT_WK_STREQ(imageSource, @"webkit-masked-url://hidden/ 1x, bar.jpg 2x, webkit-masked-url://hidden/ 3x");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].srcset"];
    EXPECT_WK_STREQ(imageSource, @"https://apple.com/baz.png 2x, baz.png 1x");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].getAttribute(\"srcset\")"];
    EXPECT_WK_STREQ(imageSource, @"https://apple.com/baz.png 2x, baz.png 1x");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].getAttributeNS(null, \"srcset\")"];
    EXPECT_WK_STREQ(imageSource, @"https://apple.com/baz.png 2x, baz.png 1x");

    imageSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"img\")[1].getAttributeNode(\"srcset\").value"];
    EXPECT_WK_STREQ(imageSource, @"https://apple.com/baz.png 2x, baz.png 1x");

    [webView synchronouslyLoadHTMLString:@"<script src=\"another-scheme://foo.com/bar.js\"></script><script src=\"http://apple.com/baz.js\"></script>" baseURL:[NSURL URLWithString:@"https://example.com"]];

    NSString *scriptSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"script\")[0].src"];
    EXPECT_WK_STREQ(scriptSource, @"webkit-masked-url://hidden/");

    scriptSource = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll(\"script\")[1].src"];
    EXPECT_WK_STREQ(scriptSource, @"http://apple.com/baz.js");

    [webView synchronouslyLoadHTMLString:@"<iframe src=\"test-scheme://foo.com/bar.html\"></iframe><iframe src=\"http://apple.com/baz.html\"></iframe>" baseURL:[NSURL URLWithString:@"https://example.com"]];

    NSString *htmlSource = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_EQ([htmlSource containsString:@"<iframe src=\"webkit-masked-url://hidden/\"></iframe>"], YES);

    [webView synchronouslyLoadHTMLString:@""];

    NSURL *scriptURL = [NSURL URLWithString:@"another-scheme://foo.com/bar.js"];
    auto userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"alert((new Error).stack)" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:scriptURL contentWorld:nil deferRunningUntilNotification:NO]);

    [[webView configuration].userContentController _addUserScriptImmediately:userScript.get()];

    EXPECT_WK_STREQ([delegate waitForAlert], "global code@webkit-masked-url://hidden/:1:17");

    scriptURL = [NSURL URLWithString:@"https://example.com/foo.js"];
    userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"alert((new Error).stack)" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:scriptURL contentWorld:nil deferRunningUntilNotification:NO]);

    [[webView configuration].userContentController _addUserScriptImmediately:userScript.get()];

    EXPECT_WK_STREQ([delegate waitForAlert], "global code@https://example.com/foo.js:1:17");
}

TEST(WebKit, ConfigurationWebViewToCloneSessionStorageFrom)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>sessionStorage.setItem('key', 'value')</script>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    [configuration _setWebViewToCloneSessionStorageFrom:webView.get()];
    auto copiedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [copiedWebView synchronouslyLoadHTMLString:@"" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    __block bool done = false;
    [copiedWebView evaluateJavaScript:@"sessionStorage.getItem('key')" completionHandler:^(id result, NSError *error) {
        EXPECT_TRUE(!error);
        NSString* value = result;
        EXPECT_TRUE(value);
        EXPECT_WK_STREQ(@"value", result);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}
