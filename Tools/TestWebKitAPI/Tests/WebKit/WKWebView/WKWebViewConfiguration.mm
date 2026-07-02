/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestProtocol.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserScriptPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

#if ENABLE(WK_WEB_EXTENSIONS)
#import <WebKit/WKWebExtensionController.h>
#import <WebKit/WKWebExtensionMatchPattern.h>
#endif

@interface SubclassWebViewConfiguration : WKWebViewConfiguration {
    RetainPtr<NSString> _subclassData;
}
@end

@implementation SubclassWebViewConfiguration

- (NSString *)subclassData
{
    return _subclassData.get();
}

- (void)setSubclassData:(NSString *)data
{
    _subclassData = data;
}

- (id)copyWithZone:(NSZone *)zone
{
    id copy = [super copyWithZone:zone];
    [copy setSubclassData:@"copied"];
    return copy;
}

@end

TEST(WebKit, ConfigurationSubclass)
{
    RetainPtr configuration = adoptNS([SubclassWebViewConfiguration new]);
    [configuration setSubclassData:@"original"];
    EXPECT_WK_STREQ([configuration subclassData], "original");
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    EXPECT_WK_STREQ([(SubclassWebViewConfiguration *)[webView configuration] subclassData], "copied");
}

TEST(WebKit, InvalidConfiguration)
{
    auto shouldThrowExceptionWhenUsed = [](Function<void(WKWebViewConfiguration *)>&& modifier, bool expectException) {
        @try {
            RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
            modifier(configuration.get());
            RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
            EXPECT_FALSE(expectException);
        } @catch (NSException *exception) {
            EXPECT_TRUE(expectException);
            EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        }
    };

    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setProcessPool:nil];
    }, false);
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setPreferences:nil];
    }, false);
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setUserContentController:nil];
    }, false);
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setWebsiteDataStore:nil];
    }, false);
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration _setVisitedLinkStore:nil];
    }, false);
    IGNORE_NULL_CHECK_WARNINGS_END

    // Related WebViews cannot use different data stores.
    RetainPtr configurationForEphemeralView = adoptNS([[WKWebViewConfiguration alloc] init]);
    configurationForEphemeralView.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    RetainPtr ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForEphemeralView.get()]);
    shouldThrowExceptionWhenUsed([&](WKWebViewConfiguration *configuration) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [configuration _setRelatedWebView:ephemeralWebView.get()];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }, true);
}

TEST(WebKit, ConfigurationCopyDoesNotLazilyInitializeProcessPool)
{
    // Create a configuration without explicitly setting processPool.
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    // Copy should not trigger lazy initialization of processPool on the source.
    RetainPtr copy = adoptNS([configuration copy]);

    // Accessing processPool on each should create independent pools,
    // since neither was initialized by the copy operation.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr pool1 = [configuration processPool];
    RetainPtr pool2 = [copy processPool];
    ALLOW_DEPRECATED_DECLARATIONS_END

    // Each configuration should independently lazy-initialize its own pool,
    // rather than sharing a pool that was created as a side effect of copying.
    EXPECT_NE(pool1.get(), pool2.get());
}

TEST(WebKit, ConfigurationCopyPreservesExplicitlySetProcessPool)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr pool = adoptNS([[WKProcessPool alloc] init]);
    [configuration setProcessPool:pool.get()];
    ALLOW_DEPRECATED_DECLARATIONS_END

    RetainPtr copy = adoptNS([configuration copy]);

    // An explicitly set processPool should be shared between source and copy.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    EXPECT_EQ([configuration processPool], [copy processPool]);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

TEST(WebKit, RelatedWebViewSyncsProcessPool)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Capture the expected pool so we ensure one is initialized.
    RetainPtr expectedPool = [configuration processPool];
    ALLOW_DEPRECATED_DECLARATIONS_END

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // A new configuration that hasn't explicitly set a process pool
    // should adopt the related web view's pool when _relatedWebView is set.
    RetainPtr newConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [newConfiguration _setRelatedWebView:webView.get()];
    EXPECT_EQ([newConfiguration processPool], expectedPool.get());

    // Creating a WKWebView with this configuration should not throw.
    RetainPtr relatedWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_TRUE(!!relatedWebView);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

TEST(WebKit, ConfigurationGroupIdentifierIsCopied)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setGroupIdentifier:@"TestGroupIdentifier"];

    RetainPtr configurationCopy = adoptNS([configuration copy]);
    EXPECT_STREQ([configuration _groupIdentifier].UTF8String, [configurationCopy _groupIdentifier].UTF8String);
}

TEST(WebKit, DefaultConfigurationEME)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _legacyEncryptedMediaAPIEnabled]);
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
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
            requestBytes = WTF::move(bytes);
            done = true;
        });
    });

    auto runTest = [&] (bool upgrade) {
        done = false;
        RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [storeConfiguration setAllowsServerPreconnect:NO];
        [storeConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port()),
            (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port()),
        }];
        RetainPtr store = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        [configuration setWebsiteDataStore:store.get()];
        [configuration setUpgradeKnownHostsToHTTPS:upgrade];
        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://www.opengl.org/"]];
        [webView loadRequest:request];
        Util::run(&done);
    };

    auto checkRequestBytesStartsWith = [&] (const char* string) {
        EXPECT_GT(requestBytes.size(), strlen(string));
        if (requestBytes.size() <= strlen(string))
            return;
        requestBytes[strlen(string)] = 0;
        EXPECT_WK_STREQ(requestBytes.span().data(), string);
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
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body onload=\"document.write('FAIL');\">PASS</body>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"PASS");
}

TEST(WebKit, ConfigurationDisableJavaScriptNestedBody)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<table><body onload=\"document.write('FAIL');\"></table>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"<table></table>");
}

TEST(WebKit, ConfigurationDisableJavaScriptSVGAnimateElement)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\" to=\"javascript:document.write('FAIL')\"/></a></svg>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\"></animate></a></svg>");
}

TEST(WebKit, ConfigurationDisableJavaScriptSVGAnimateElementComplex)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _allowsJavaScriptMarkup]);
    [configuration _setAllowsJavaScriptMarkup:NO];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\" dur=\"4s\" calcMode=\"spline\" repeatCount=\"indefinite\" values=\"javascript:document.write('FAIL'); javascript:document.write('FAIL'); javascript:document.write(location.href); javascript:document.write('FAIL'); javascript:document.write('FAIL')\" keyTimes=\"0; 0.25; 0.5; 0.75; 1\" keySplines=\"0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1\"/></a></svg>"];
    NSString *bodyHTML = [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"];
    EXPECT_WK_STREQ(bodyHTML, @"<svg><a><rect fill=\"green\" width=\"10\" height=\"10\"></rect><animate attributeName=\"href\" dur=\"4s\" calcMode=\"spline\" repeatCount=\"indefinite\" keyTimes=\"0; 0.25; 0.5; 0.75; 1\" keySplines=\"0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1; 0.5 0 0.5 1\"></animate></a></svg>");
}

TEST(WebKit, ConfigurationMaskedURLSchemes)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);

    EXPECT_NS_EQUAL([configuration _maskedURLSchemes], [NSSet set]);

#if ENABLE(WK_WEB_EXTENSIONS)
    RetainPtr extensionController = adoptNS([[WKWebExtensionController alloc] init]);
    [configuration setWebExtensionController:extensionController.get()];

    EXPECT_NS_EQUAL([configuration _maskedURLSchemes], [NSSet setWithObject:@"webkit-extension"]);

    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-scheme"];

    EXPECT_NS_EQUAL([configuration _maskedURLSchemes], ([NSSet setWithObjects:@"webkit-extension", @"test-scheme", nil]));
#endif

    [configuration _setMaskedURLSchemes:[NSSet setWithObject:@"test-scheme"]];
    EXPECT_NS_EQUAL([configuration _maskedURLSchemes], [NSSet setWithObject:@"test-scheme"]);

    [configuration _setMaskedURLSchemes:[NSSet set]];
    EXPECT_NS_EQUAL([configuration _maskedURLSchemes], [NSSet set]);

    [configuration _setMaskedURLSchemes:[NSSet setWithObjects:@"test-scheme", @"another-scheme", nil]];
    EXPECT_NS_EQUAL([configuration _maskedURLSchemes], ([NSSet setWithObjects:@"test-scheme", @"another-scheme", nil]));

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([TestUIDelegate new]);
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

    NSString *serializedHTML = [webView stringByEvaluatingJavaScript:@"new XMLSerializer().serializeToString(document.querySelectorAll(\"img\")[0])"];
    EXPECT_TRUE([serializedHTML containsString:@"webkit-masked-url://hidden/"]);
    EXPECT_FALSE([serializedHTML containsString:@"test-scheme://"]);

    serializedHTML = [webView stringByEvaluatingJavaScript:@"new XMLSerializer().serializeToString(document.querySelectorAll(\"img\")[1])"];
    EXPECT_TRUE([serializedHTML containsString:@"baz.png"]);
    EXPECT_FALSE([serializedHTML containsString:@"webkit-masked-url://"]);
    EXPECT_FALSE([serializedHTML containsString:@"test-scheme://"]);

    serializedHTML = [webView stringByEvaluatingJavaScript:@"new XMLSerializer().serializeToString(document)"];
    EXPECT_TRUE([serializedHTML containsString:@"webkit-masked-url://hidden/"]);
    EXPECT_TRUE([serializedHTML containsString:@"baz.png"]);
    EXPECT_FALSE([serializedHTML containsString:@"test-scheme://"]);

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

    NSString *xmlSource = [webView stringByEvaluatingJavaScript:@"(new XMLSerializer()).serializeToString(document.body)"];
    EXPECT_WK_STREQ(xmlSource, @"<body xmlns=\"http://www.w3.org/1999/xhtml\"><iframe src=\"webkit-masked-url://hidden/\"></iframe><iframe src=\"http://apple.com/baz.html\"></iframe></body>");

    [webView synchronouslyLoadHTMLString:@""];

    NSURL *scriptURL = [NSURL URLWithString:@"another-scheme://foo.com/bar.js"];
    RetainPtr userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"alert((new Error).stack)" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:scriptURL contentWorld:nil deferRunningUntilNotification:NO]);

    [[webView configuration].userContentController _addUserScriptImmediately:userScript.get()];

    EXPECT_WK_STREQ([delegate waitForAlert], "global code@webkit-masked-url://hidden/:1:8");

    scriptURL = [NSURL URLWithString:@"https://example.com/foo.js"];
    userScript = adoptNS([[WKUserScript alloc] _initWithSource:@"alert((new Error).stack)" injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES includeMatchPatternStrings:@[] excludeMatchPatternStrings:@[] associatedURL:scriptURL contentWorld:nil deferRunningUntilNotification:NO]);

    [[webView configuration].userContentController _addUserScriptImmediately:userScript.get()];

    EXPECT_WK_STREQ([delegate waitForAlert], "global code@https://example.com/foo.js:1:8");
}

TEST(WebKit, ConfigurationWebViewToCloneSessionStorageFrom)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>sessionStorage.setItem('key', 'value')</script>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];

    [configuration _setWebViewToCloneSessionStorageFrom:webView.get()];
    RetainPtr copiedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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

TEST(WebKit, OverrideReferrer)
{
    using namespace TestWebKitAPI;
    bool done { false };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        if (path == "/next_navigation"_s) {
            EXPECT_FALSE(strnstr(request.span().data(), "\r\nReferer: overridereferer\r\n", request.size()));
            co_await connection.awaitableSend(HTTPResponse("<script>alert('main frame loaded without referrer')</script>"_s).serialize());
            continue;
        }
        EXPECT_TRUE(strnstr(request.span().data(), "\r\nReferer: overridereferer\r\n", request.size()));
        EXPECT_FALSE(strnstr(request.span().data(), "eferrer", request.size()));
        EXPECT_EQ(String(request.span()).split("Referer"_s).size(), 2u);
        if (path == "/example"_s) {
            co_await connection.awaitableSend(HTTPResponse("<script>window.location = 'https://webkit.org/webkit'</script>"_s).serialize());
            continue;
        }
        if (path == "/webkit"_s) {
            co_await connection.awaitableSend(HTTPResponse("<script>fetch('/subresource')</script>"_s).serialize());
            continue;
        }
        if (path == "/subresource"_s) {
            done = true;
            continue;
        }
        EXPECT_FALSE(true);
    } }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:server.httpsProxyConfiguration()]);
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    auto addReferrer = ^(WKNavigationAction *, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        preferences._overrideReferrerForAllRequests = @"overridereferer";
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    delegate.get().decidePolicyForNavigationActionWithPreferences = addReferrer;
    [delegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&done);

    delegate.get().decidePolicyForNavigationActionWithPreferences = nil;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/next_navigation"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame loaded without referrer");

    done = false;
    delegate.get().decidePolicyForNavigationActionWithPreferences = addReferrer;
    [webView evaluateJavaScript:@"const iframe = document.createElement('iframe'); iframe.src = 'https://example.com/webkit'; document.body.appendChild(iframe)" completionHandler:nil];
    Util::run(&done);

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&done);
}
