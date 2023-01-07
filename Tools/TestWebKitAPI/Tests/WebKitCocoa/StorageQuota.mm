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
#import <WebKit/WebKit.h>

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringConcatenateNumbers.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

using namespace TestWebKitAPI;

@interface QuotaDelegate : NSObject <WKUIDelegate>
-(bool)quotaDelegateCalled;
-(void)grantQuota;
-(void)denyQuota;
@end

static bool receivedQuotaDelegateCalled;

@implementation QuotaDelegate {
    bool _quotaDelegateCalled;
    unsigned long long _currentQuota;
    unsigned long long _expectedUsage;
    BlockPtr<void(unsigned long long newQuota)> _decisionHandler;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _quotaDelegateCalled = false;
    _expectedUsage = 0;
    _currentQuota = 0;
    
    return self;
}

- (void)_webView:(WKWebView *)webView decideDatabaseQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota currentOriginUsage:(unsigned long long)currentOriginUsage currentDatabaseUsage:(unsigned long long)currentUsage expectedUsage:(unsigned long long)expectedUsage decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler
{
    receivedQuotaDelegateCalled = true;
    _quotaDelegateCalled = true;
    _currentQuota = currentQuota;
    _expectedUsage = expectedUsage;
    _decisionHandler = decisionHandler;
}

-(bool)quotaDelegateCalled {
    return _quotaDelegateCalled;
}

-(void)grantQuota {
    if (_quotaDelegateCalled)
        _decisionHandler(_expectedUsage);
    _quotaDelegateCalled = false;
}

-(void)denyQuota {
    if (_quotaDelegateCalled)
        _decisionHandler(_currentQuota);
    _quotaDelegateCalled = false;
}

@end

struct ResourceInfo {
    RetainPtr<NSString> mimeType;
    const char* data;
};

static bool receivedMessage;

@interface QuotaMessageHandler : NSObject <WKScriptMessageHandler>
- (void)setExpectedMessage:(NSString *)message;
- (String)receivedMessage;
@end

@implementation QuotaMessageHandler {
    String _expectedMessage;
    String _message;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    _message = [message body];
    if (!_expectedMessage.isNull()) {
        EXPECT_STREQ(_message.utf8().data(), _expectedMessage.utf8().data());
        _expectedMessage = { };
    }
    receivedMessage = true;
}

- (void)setExpectedMessage:(NSString *)message
{
    _expectedMessage = message;
}

- (String)receivedMessage
{
    return _message;
}
@end

static constexpr auto TestBytes = R"SWRESOURCE(
<script>

async function doTest()
{
    try {
        const cache = await window.caches.open("mycache");
        const promise = cache.put("http://example.org/test", new Response(new ArrayBuffer(1024 * 500)));
        window.webkit.messageHandlers.qt.postMessage("start");
        promise.then(() => {
            window.webkit.messageHandlers.qt.postMessage("pass");
        }, () => {
            window.webkit.messageHandlers.qt.postMessage("fail");
        });
    } catch (e) {
        window.webkit.messageHandlers.qt.postMessage("fail with exception " + e);
    }
}

window.onload = () => {
    if (document.visibilityState === 'visible')
        doTest();
    else {
        document.addEventListener("visibilitychange", function() {
            if (document.visibilityState === 'visible')
                doTest();
        });
    }
}

function doTestAgain()
{
    doTest();
}
</script>
)SWRESOURCE"_s;

static constexpr auto TestUrlBytes = R"SWRESOURCE(
<script>

var index = 0;
async function test(num)
{
    index++;
    url = "http://example.org/test" + index;

    try {
        const cache = await window.caches.open("mycache");
        const promise = cache.put(url, new Response(new ArrayBuffer(num * 1024 * 1024)));
        promise.then(() => {
            window.webkit.messageHandlers.qt.postMessage("pass");
        }, () => {
            window.webkit.messageHandlers.qt.postMessage("fail");
        });
    } catch (e) {
        window.webkit.messageHandlers.qt.postMessage("fail with exception " + e);
    }
}

function doTest(num)
{
    test(num);
}
</script>
)SWRESOURCE"_s;

#if PLATFORM(MAC)
static constexpr auto TestHiddenBytes = R"SWRESOURCE(
<script>

async function test()
{
    url = "http://example.org/test";
    try {
        const cache = await window.caches.open("mycache");
        const promise = cache.put(url, new Response(new ArrayBuffer(1024 * 1024)));
        promise.then(() => {
            window.webkit.messageHandlers.qt.postMessage("put succeeded");
        }, () => {
            window.webkit.messageHandlers.qt.postMessage("put failed");
        });
    } catch (e) {
        window.webkit.messageHandlers.qt.postMessage("fail with exception " + e);
    }
    setTimeout(() => window.webkit.messageHandlers.qt.postMessage("timed out"), 10000);
}

test();
</script>
)SWRESOURCE"_s;
#endif

static inline void setVisible(TestWKWebView *webView)
{
#if PLATFORM(MAC)
    [webView.window setIsVisible:YES];
#else
    webView.window.hidden = NO;
#endif
}

#if PLATFORM(MAC)
TEST(WebKit, QuotaDelegateHidden)
{
    done = false;
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setPerOriginStorageQuota:1024 * 400];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { TestHiddenBytes } },
    });

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[QuotaDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    setVisible(webView.get());

    auto *hostWindow = [webView hostWindow];
    [hostWindow miniaturize:hostWindow];

    NSLog(@"QuotaDelegateHidden 1");

    receivedQuotaDelegateCalled = false;
    receivedMessage = false;
    [webView loadRequest:server.request()];
    Util::run(&receivedMessage);
    EXPECT_STREQ([messageHandler receivedMessage].utf8().data(), "put failed");

    NSLog(@"QuotaDelegateHidden 2");

    EXPECT_FALSE(receivedQuotaDelegateCalled);

    [hostWindow deminiaturize:hostWindow];

    receivedQuotaDelegateCalled = false;
    receivedMessage = false;
    [webView reload];
    Util::run(&receivedQuotaDelegateCalled);

    NSLog(@"QuotaDelegateHidden 3");

    [delegate grantQuota];
    Util::run(&receivedMessage);
    EXPECT_STREQ([messageHandler receivedMessage].utf8().data(), "put succeeded");

    NSLog(@"QuotaDelegateHidden 4");
}
#endif

TEST(WebKit, QuotaDelegate)
{
    done = false;
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setPerOriginStorageQuota:1024 * 400];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { TestBytes } },
    });

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate1 = adoptNS([[QuotaDelegate alloc] init]);
    [webView1 setUIDelegate:delegate1.get()];
    setVisible(webView1.get());

    receivedQuotaDelegateCalled = false;
    [webView1 loadRequest:server.request()];
    Util::run(&receivedQuotaDelegateCalled);

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate2 = adoptNS([[QuotaDelegate alloc] init]);
    [webView2 setUIDelegate:delegate2.get()];
    setVisible(webView2.get());

    receivedMessage = false;
    [webView2 loadRequest:server.requestWithLocalhost()];
    [messageHandler setExpectedMessage: @"start"];
    Util::run(&receivedMessage);

    EXPECT_FALSE(delegate2.get().quotaDelegateCalled);
    [delegate1 grantQuota];

    [messageHandler setExpectedMessage: @"pass"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    while (!delegate2.get().quotaDelegateCalled)
        TestWebKitAPI::Util::runFor(0.1_s);

    [delegate2 denyQuota];

    [messageHandler setExpectedMessage: @"fail"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    NSLog(@"QuotaDelegate 6");
}

// FIXME: Re-enable this test for iOS once webkit.org/b/250228 is resolved
#if PLATFORM(IOS)
TEST(WebKit, DISABLED_QuotaDelegateReload)
#else
TEST(WebKit, QuotaDelegateReload)
#endif
{
    done = false;
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setPerOriginStorageQuota:1024 * 400];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    
    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];
    
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { TestBytes } },
    });
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[QuotaDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    setVisible(webView.get());

    receivedQuotaDelegateCalled = false;
    [webView loadRequest:server.request()];
    Util::run(&receivedQuotaDelegateCalled);

    [delegate denyQuota];

    [messageHandler setExpectedMessage: @"fail"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    receivedQuotaDelegateCalled = false;
    [webView reload];
    Util::run(&receivedQuotaDelegateCalled);

    [delegate grantQuota];

    [messageHandler setExpectedMessage: @"pass"];
    receivedMessage = false;
    Util::run(&receivedMessage);
}

// FIXME: Re-enable this test for iOS once webkit.org/b/250228 is resolved
#if PLATFORM(IOS)
TEST(WebKit, DISABLED_QuotaDelegateNavigateFragment)
#else
TEST(WebKit, QuotaDelegateNavigateFragment)
#endif
{
    done = false;
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [storeConfiguration setPerOriginStorageQuota:1024 * 400];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    TestWebKitAPI::HTTPServer server({
        { "/main.html"_s, { TestBytes } },
    });

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[QuotaDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    setVisible(webView.get());

    receivedQuotaDelegateCalled = false;
    [webView loadRequest:server.request("/main.html"_s)];
    Util::run(&receivedQuotaDelegateCalled);

    [delegate denyQuota];

    [messageHandler setExpectedMessage: @"fail"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    receivedQuotaDelegateCalled = false;
    [webView loadRequest:server.request("/main.html#fragment"_s)];
    [webView stringByEvaluatingJavaScript:@"doTestAgain()"];

    [messageHandler setExpectedMessage: @"start"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    [messageHandler setExpectedMessage: @"fail"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    EXPECT_FALSE(receivedQuotaDelegateCalled);
}

TEST(WebKit, DefaultQuota)
{
    done = false;
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);

    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { TestUrlBytes } },
    });

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[QuotaDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    setVisible(webView.get());

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigationBoolean = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    didFinishNavigationBoolean = false;
    [webView loadRequest:server.request()];
    Util::run(&didFinishNavigationBoolean);

    receivedQuotaDelegateCalled = false;

    // Storing 10 entries of 10 MB should not hit the default quota which is 1GB
    for (int i = 0; i < 10; ++i) {
        [webView stringByEvaluatingJavaScript:makeString("doTest(10)")];
        [messageHandler setExpectedMessage: @"pass"];
        receivedMessage = false;
        Util::run(&receivedMessage);
    }
    EXPECT_FALSE(receivedQuotaDelegateCalled);
}
