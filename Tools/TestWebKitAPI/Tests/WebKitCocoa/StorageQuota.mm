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

#import "PlatformUtilities.h"
#import "ServiceWorkerTCPServer.h"
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

static bool didFinishNavigation;

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
-(void)setExpectedMessage:(NSString *)message;
@end

@implementation QuotaMessageHandler {
    NSString *_expectedMessage;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (_expectedMessage) {
        EXPECT_TRUE([[message body] isEqualToString:_expectedMessage]);
        _expectedMessage = nil;
    }
    receivedMessage = true;
}

-(void)setExpectedMessage:(NSString *)message {
    _expectedMessage = message;
}
@end

static const char* TestBytes = R"SWRESOURCE(
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
doTest();

function doTestAgain()
{
    doTest();
}
</script>
)SWRESOURCE";

static const char* TestUrlBytes = R"SWRESOURCE(
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
)SWRESOURCE";

static bool done;

static inline void setVisible(TestWKWebView *webView)
{
#if PLATFORM(MAC)
    [webView.window setIsVisible:YES];
#else
    webView.window.hidden = NO;
#endif
}

TEST(WebKit, QuotaDelegate)
{
    done = false;
    _WKWebsiteDataStoreConfiguration *storeConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    storeConfiguration.perOriginStorageQuota = 1024 * 400;
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration] autorelease];
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    ServiceWorkerTCPServer server({
        { "text/html", TestBytes }
    }, {
        { "text/html", TestBytes }
    });

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate1 = adoptNS([[QuotaDelegate alloc] init]);
    [webView1 setUIDelegate:delegate1.get()];
    setVisible(webView1.get());

    NSLog(@"QuotaDelegate 1");

    receivedQuotaDelegateCalled = false;
    [webView1 loadRequest:server.request()];
    Util::run(&receivedQuotaDelegateCalled);

    NSLog(@"QuotaDelegate 2");

    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate2 = adoptNS([[QuotaDelegate alloc] init]);
    [webView2 setUIDelegate:delegate2.get()];
    setVisible(webView2.get());

    receivedMessage = false;
    [webView2 loadRequest:server.requestWithLocalhost()];
    [messageHandler setExpectedMessage: @"start"];
    Util::run(&receivedMessage);

    NSLog(@"QuotaDelegate 3");

    EXPECT_FALSE(delegate2.get().quotaDelegateCalled);
    [delegate1 grantQuota];

    [messageHandler setExpectedMessage: @"pass"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    NSLog(@"QuotaDelegate 4");

    while (!delegate2.get().quotaDelegateCalled)
        TestWebKitAPI::Util::sleep(0.1);

    NSLog(@"QuotaDelegate 5");

    [delegate2 denyQuota];

    [messageHandler setExpectedMessage: @"fail"];
    receivedMessage = false;
    Util::run(&receivedMessage);

    NSLog(@"QuotaDelegate 6");
}

TEST(WebKit, QuotaDelegateReload)
{
    done = false;
    _WKWebsiteDataStoreConfiguration *storeConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    storeConfiguration.perOriginStorageQuota = 1024 * 400;
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration] autorelease];
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    
    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];
    
    ServiceWorkerTCPServer server({
        { "text/html", TestBytes },
        { "text/html", TestBytes }
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

TEST(WebKit, QuotaDelegateNavigateFragment)
{
    done = false;
    _WKWebsiteDataStoreConfiguration *storeConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    storeConfiguration.perOriginStorageQuota = 1024 * 400;
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration] autorelease];
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    ServiceWorkerTCPServer server({
        { "text/html", TestBytes }
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
    [webView loadRequest:server.requestWithFragment()];
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
    _WKWebsiteDataStoreConfiguration *storeConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    WKWebsiteDataStore *dataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration] autorelease];

    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[QuotaMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"qt"];

    ServiceWorkerTCPServer server({
        { "text/html", TestUrlBytes }
    });

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[QuotaDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
    setVisible(webView.get());

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    didFinishNavigation = false;
    [webView loadRequest:server.request()];
    Util::run(&didFinishNavigation);

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
