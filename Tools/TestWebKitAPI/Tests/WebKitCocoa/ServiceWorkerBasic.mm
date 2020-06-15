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

#import "config.h"

#import "HTTPServer.h"
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
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Deque.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

static bool done;

#if HAVE(NETWORK_FRAMEWORK)
static bool didFinishNavigation;
#endif

static String expectedMessage;
static String retrievedString;

@interface SWMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SWMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(@"Message from worker: ServiceWorker received: Hello from the web page", [message body]);
    done = true;
}
@end

@interface SWMessageHandlerForFetchTest : NSObject <WKScriptMessageHandler>
@end

@implementation SWMessageHandlerForFetchTest
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_TRUE([[message body] isEqualToString:@"Intercepted by worker"]);
    done = true;
}
@end

@interface SWMessageHandlerForRestoreFromDiskTest : NSObject <WKScriptMessageHandler> {
    NSString *_expectedMessage;
}
- (instancetype)initWithExpectedMessage:(NSString *)expectedMessage;
@end

@interface SWMessageHandlerWithExpectedMessage : NSObject <WKScriptMessageHandler>
@end

@implementation SWMessageHandlerWithExpectedMessage
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(message.body, expectedMessage);
    done = true;
}
@end

@implementation SWMessageHandlerForRestoreFromDiskTest

- (instancetype)initWithExpectedMessage:(NSString *)expectedMessage
{
    if (!(self = [super init]))
        return nil;

    _expectedMessage = expectedMessage;

    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(message.body, _expectedMessage);
    done = true;
}
@end

static bool shouldAccept = true;
static bool navigationComplete = false;
static bool navigationFailed = false;

@interface TestSWAsyncNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation TestSWAsyncNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(null_unspecified WKNavigation *)navigation
{
    navigationComplete = true;
}

- (void)webView:(WKWebView *)webView didFailNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error
{
    navigationFailed = true;
    navigationComplete = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error
{
    navigationFailed = true;
    navigationComplete = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
    dispatch_after(when, dispatch_get_main_queue(), ^{
        decisionHandler(shouldAccept ? WKNavigationResponsePolicyAllow : WKNavigationResponsePolicyCancel);
    });
}
@end

static const char* mainCacheStorageBytes = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

async function doTest()
{
    const keys = await window.caches.keys();
    if (!keys.length) {
        const cache = await window.caches.open("my cache");
        log("No cache storage data");
        return;
    }
    if (keys.length !== 1) {
        log("Unexpected cache number");
        return;
    }
    log("Some cache storage data: " + keys[0]);
}
doTest();

</script>
)SWRESOURCE";

static const char* mainBytes = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.addEventListener("message", function(event) {
    log("Message from worker: " + event.data);
});

try {

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    worker = reg.installing ? reg.installing : reg.active;
    worker.postMessage("Hello from the web page");
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

</script>
)SWRESOURCE";

static const char* scriptBytes = R"SWRESOURCE(

self.addEventListener("message", (event) => {
    event.source.postMessage("ServiceWorker received: " + event.data);
});

)SWRESOURCE";

static const char* mainForFetchTestBytes = R"SWRESOURCE(
<html>
<body>
<script>
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

try {

function addFrame()
{
    frame = document.createElement('iframe');
    frame.src = "/test.html";
    frame.onload = function() { window.webkit.messageHandlers.sw.postMessage(frame.contentDocument.body.innerHTML); }
    document.body.appendChild(frame);
}

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    if (reg.active) {
        addFrame();
        return;
    }
    worker = reg.installing;
    worker.addEventListener('statechange', function() {
        if (worker.state == 'activated')
            addFrame();
    });
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

</script>
</body>
</html>
)SWRESOURCE";

static const char* scriptHandlingFetchBytes = R"SWRESOURCE(

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("test.html") !== -1) {
        event.respondWith(new Response(new Blob(['Intercepted by worker'], {type: 'text/html'})));
    }
});

)SWRESOURCE";

static const char* scriptInterceptingFirstLoadBytes = R"SWRESOURCE(

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("main.html") !== -1) {
        event.respondWith(new Response(new Blob(['Intercepted by worker <script>window.webkit.messageHandlers.sw.postMessage(\'Intercepted by worker\');</script>'], {type: 'text/html'})));
    }
});

)SWRESOURCE";

static const char* mainForFirstLoadInterceptTestBytes = R"SWRESOURCE(
 <html>
<body>
<script>
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

try {

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    if (reg.active) {
        window.webkit.messageHandlers.sw.postMessage('Service Worker activated');
        return;
    }

    worker = reg.installing;
    worker.addEventListener('statechange', function() {
        if (worker.state == 'activated')
            window.webkit.messageHandlers.sw.postMessage('Service Worker activated');
    });
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

</script>
</body>
</html>
)SWRESOURCE";

static const char* mainRegisteringWorkerBytes = R"SWRESOURCE(
<script>
try {
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    if (reg.active) {
        log("FAIL: Registration already has an active worker");
        return;
    }
    worker = reg.installing;
    worker.addEventListener('statechange', function() {
        if (worker.state == 'activated')
            log("PASS: Registration was successful and service worker was activated");
    });
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}
</script>
)SWRESOURCE";

static const char* mainRegisteringAlreadyExistingWorkerBytes = R"SWRESOURCE(
<script>
try {
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    if (reg.installing) {
        log("FAIL: Registration had an installing worker");
        return;
    }
    if (reg.active) {
        if (reg.active.state == "activated")
            log("PASS: Registration already has an active worker");
        else
            log("FAIL: Registration has an active worker but its state is not activated");
    } else
        log("FAIL: Registration does not have an active worker");
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}
</script>
)SWRESOURCE";

static const char* mainBytesForSessionIDTest = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.addEventListener("message", function(event) {
    log(event.data);
});

try {

navigator.serviceWorker.register('/sw.js').then(function(reg) {
    worker = reg.installing;
    worker.addEventListener('statechange', function() {
        if (worker.state == 'activated')
            worker.postMessage("TEST");
    });
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

</script>
)SWRESOURCE";

static const char* scriptBytesForSessionIDTest = R"SWRESOURCE(

var wasActivated = false;

self.addEventListener("activate", event => {
    event.waitUntil(clients.claim().then( () => {
        wasActivated = true;
    }));
});

self.addEventListener("message", (event) => {
    if (wasActivated && registration.active)
        event.source.postMessage("PASS: activation successful");
    else
        event.source.postMessage("FAIL: failed to activate");
});

)SWRESOURCE";

TEST(ServiceWorkers, Basic)
{
    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes},
    });

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = nullptr;

    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_WK_STREQ(websiteDataRecords[0].displayName, "127.0.0.1");

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

@interface SWCustomUserAgentDelegate : NSObject <WKNavigationDelegate> {
    NSString *_userAgent;
}
- (instancetype)initWithUserAgent:(NSString *)userAgent;
@end

@implementation SWCustomUserAgentDelegate

- (instancetype)initWithUserAgent:(NSString *)userAgent
{
    self = [super init];
    _userAgent = userAgent;
    return self;
}

- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences userInfo:(id <NSSecureCoding>)userInfo decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    WKWebpagePreferences *websitePolicies = [[[WKWebpagePreferences alloc] init] autorelease];
    if (navigationAction.targetFrame.mainFrame)
        [websitePolicies _setCustomUserAgent:_userAgent];

    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies);
}

@end

@interface SWUserAgentMessageHandler : NSObject <WKScriptMessageHandler> {
@public
    NSString *expectedMessage;
}
- (instancetype)initWithExpectedMessage:(NSString *)expectedMessage;
@end

@implementation SWUserAgentMessageHandler

- (instancetype)initWithExpectedMessage:(NSString *)_expectedMessage
{
    self = [super init];
    expectedMessage = _expectedMessage;
    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(expectedMessage, [message body]);
    done = true;
}
@end

static const char* userAgentSWBytes = R"SWRESOURCE(

self.addEventListener("message", (event) => {
    event.source.postMessage(navigator.userAgent);
});

)SWRESOURCE";

TEST(ServiceWorkers, UserAgentOverride)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWUserAgentMessageHandler alloc] initWithExpectedMessage:@"Message from worker: Foo Custom UserAgent"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", userAgentSWBytes },
    }, {
        { "text/html", mainBytes },
        { "application/javascript", userAgentSWBytes },
    }, 2, { "Foo Custom UserAgent"_s, "Bar Custom UserAgent"_s });

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[SWCustomUserAgentDelegate alloc] initWithUserAgent:@"Foo Custom UserAgent"]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // Restore from disk.
    webView = nullptr;
    delegate = nullptr;
    messageHandler = nullptr;
    configuration = nullptr;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    messageHandler = adoptNS([[SWUserAgentMessageHandler alloc] initWithExpectedMessage:@"Message from worker: Bar Custom UserAgent"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    delegate = adoptNS([[SWCustomUserAgentDelegate alloc] initWithUserAgent:@"Bar Custom UserAgent"]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_EQ(server.userAgentsChecked(), 3ull);
}

TEST(ServiceWorkers, RestoreFromDisk)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];
    
    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerForRestoreFromDiskTest> messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration was successful and service worker was activated"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainRegisteringWorkerBytes },
        { "application/javascript", scriptBytes },
    }, {
        { "text/html", mainRegisteringAlreadyExistingWorkerBytes },
        { "application/javascript", scriptBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, CacheStorageRestoreFromDisk)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [WKWebsiteDataStore _deleteDefaultDataStoreForTesting];

    ServiceWorkerTCPServer server({
        { "text/html", mainCacheStorageBytes }
    }, {
        { "text/html", mainCacheStorageBytes }
    });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"No cache storage data"]);

    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"Some cache storage data: my cache"]);

    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, FetchAfterRestoreFromDisk)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerForFetchTest> messageHandler = adoptNS([[SWMessageHandlerForFetchTest alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainForFetchTestBytes },
        { "application/javascript", scriptHandlingFetchBytes },
    }, {
        { "text/html", mainForFetchTestBytes },
        { "application/javascript", scriptHandlingFetchBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerForFetchTest alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, InterceptFirstLoadAfterRestoreFromDisk)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainForFirstLoadInterceptTestBytes },
        { "application/javascript", scriptInterceptingFirstLoadBytes },
    }, {
        { "application/javascript", scriptInterceptingFirstLoadBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Service Worker activated";
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Intercepted by worker";
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, WaitForPolicyDelegate)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainForFirstLoadInterceptTestBytes },
        { "application/javascript", scriptInterceptingFirstLoadBytes },
    }, {
        { "application/javascript", scriptInterceptingFirstLoadBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Register a service worker and activate it.
    expectedMessage = "Service Worker activated";
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Verify service worker is intercepting load.
    expectedMessage = "Intercepted by worker";
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestSWAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    shouldAccept = true;
    navigationFailed = false;
    navigationComplete = false;

    // Verify service worker load goes well when policy delegate is ok.
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&navigationComplete);

    EXPECT_FALSE(navigationFailed);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    shouldAccept = false;
    navigationFailed = false;
    navigationComplete = false;

    // Verify service worker load fails well when policy delegate is not ok.
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&navigationComplete);

    EXPECT_TRUE(navigationFailed);
}

#if WK_HAVE_C_SPI

void setConfigurationInjectedBundlePath(WKWebViewConfiguration* configuration)
{
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.processPool = (WKProcessPool *)context.get();
}

@interface RegularPageMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation RegularPageMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_TRUE([[message body] isEqualToString:@"PASS"]);
    done = true;
}
@end

static const char* regularPageWithConnectionBytes = R"SWRESOURCE(
<script>
window.webkit.messageHandlers.regularPage.postMessage("PASS");
</script>
)SWRESOURCE";

TEST(ServiceWorkers, SWProcessConnectionCreation)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());

    done = false;

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [[configuration websiteDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(0u, [websiteDataRecords count]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    RetainPtr<RegularPageMessageHandler> regularPageMessageHandler = adoptNS([[RegularPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:regularPageMessageHandler.get() name:@"regularPage"];

    ServiceWorkerTCPServer server({
        { "text/html", regularPageWithConnectionBytes },
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
        { "text/html", regularPageWithConnectionBytes },
        { "text/html", regularPageWithConnectionBytes },
    });

    RetainPtr<WKWebView> regularPageWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> newRegularPageWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Test that a regular page does not trigger a service worker connection to network process if there is no registered service worker.
    [regularPageWebView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Test that a sw scheme page can register a service worker.
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    webView = nullptr;

    // Now that a service worker is registered, the regular page should have a service worker connection.
    [regularPageWebView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    regularPageWebView = nullptr;

    [newRegularPageWebView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    newRegularPageWebView = nullptr;

    [[configuration websiteDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_WK_STREQ(websiteDataRecords[0].displayName, "127.0.0.1");

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

static const char* mainBytesWithScope = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.addEventListener("message", function(event) {
    log("Message from worker: " + event.data);
});

try {

navigator.serviceWorker.register('/sw.js', {scope: 'whateverscope'}).then(function(reg) {
    reg.installing.postMessage("Hello from the web page");
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

</script>
)SWRESOURCE";

TEST(ServiceWorkers, ServiceWorkerProcessCreation)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());

    done = false;

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [[configuration websiteDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    RetainPtr<RegularPageMessageHandler> regularPageMessageHandler = adoptNS([[RegularPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:regularPageMessageHandler.get() name:@"regularPage"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytesWithScope },
        { "application/javascript", scriptBytes },
    }, {
        { "text/html", regularPageWithConnectionBytes },
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a page that registers a service worker.
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    webView = nullptr;

    // Now that a sw is registered, let's create a new configuration and try loading a regular page, there should be no service worker process created.
    RetainPtr<WKWebViewConfiguration> newConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(newConfiguration.get());
    newConfiguration.get().websiteDataStore = [configuration websiteDataStore];

    [[newConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [[newConfiguration userContentController] addScriptMessageHandler:regularPageMessageHandler.get() name:@"regularPage"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(1u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading the simple page did not start the service worker process.
    EXPECT_EQ(0u, webView.get().configuration.processPool._serviceWorkerProcessCount);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading this page did start the service worker process.
    EXPECT_EQ(1u, webView.get().configuration.processPool._serviceWorkerProcessCount);

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static const char* readCacheBytes = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

window.caches.keys().then(keys => {
    log(keys.length && keys[0] === "test" ? "PASS" : "FAIL");
}, () => {
    log("FAIL");
});

</script>
)SWRESOURCE";

static const char* writeCacheBytes = R"SWRESOURCE(
<script>

function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

window.caches.open("test").then(() => {
    log("PASS");
}, () => {
    log("FAIL");
});

</script>
)SWRESOURCE";

@interface SWMessageHandlerForCacheStorage : NSObject <WKScriptMessageHandler>
@end

@implementation SWMessageHandlerForCacheStorage
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_WK_STREQ(@"PASS", [message body]);
    done = true;
}
@end

TEST(ServiceWorkers, CacheStorageInPrivateBrowsingMode)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandlerForCacheStorage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", writeCacheBytes },
        { "text/html", readCacheBytes },
    });

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static const char* serviceWorkerCacheAccessEphemeralSessionMainBytes = R"SWRESOURCE(
<script>
try {
    navigator.serviceWorker.addEventListener("message", (event) => {
        webkit.messageHandlers.sw.postMessage(event.data);
    });

    navigator.serviceWorker.register("serviceworker-private-browsing-worker.js", { scope : "my private backyard" }).then((registration) => {
        activeWorker = registration.installing;
        activeWorker.addEventListener('statechange', () => {
            if (activeWorker.state === "activated") {
                activeWorker.postMessage("TESTCACHE");
            }
        });
    });
} catch (e) {
    webkit.messageHandlers.sw.postMessage("" + e);
}
</script>
)SWRESOURCE";

static const char* serviceWorkerCacheAccessEphemeralSessionSWBytes = R"SWRESOURCE(
self.addEventListener("message", (event) => {
    try {
        self.caches.keys().then((keys) => {
            event.source.postMessage(keys.length === 0 ? "PASS" : "FAIL: caches is not empty, got: " + JSON.stringify(keys));
        });
    } catch (e) {
         event.source.postMessage("" + e);
    }
});
)SWRESOURCE";

// Opens a cache in the default session and checks that an ephemeral service worker
// does not have access to it.
TEST(ServiceWorkers, ServiceWorkerCacheAccessEphemeralSession)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    
    auto defaultPreferences = [configuration preferences];
    [defaultPreferences _setSecureContextChecksEnabled:NO];
    
    ServiceWorkerTCPServer server({
        { "text/html", serviceWorkerCacheAccessEphemeralSessionMainBytes },
        { "application/javascript", serviceWorkerCacheAccessEphemeralSessionSWBytes },
    });

    auto defaultWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [defaultWebView synchronouslyLoadHTMLString:@"foo" baseURL:server.request().URL];

    bool openedCache = false;
    [defaultWebView evaluateJavaScript:@"self.caches.open('test');" completionHandler: [&] (id innerText, NSError *error) {
        openedCache = true;
    }];
    TestWebKitAPI::Util::run(&openedCache);

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto messageHandler = adoptNS([[SWMessageHandlerForCacheStorage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [ephemeralWebView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static const char* differentSessionsUseDifferentRegistrationsMainBytes = R"SWRESOURCE(
<script>
try {
    navigator.serviceWorker.register("empty-worker.js", { scope : "/test" }).then((registration) => {
        activeWorker = registration.installing;
        activeWorker.addEventListener('statechange', () => {
            if (activeWorker.state === "activated")
                webkit.messageHandlers.sw.postMessage("PASS");
        });
    });
} catch (e) {
    webkit.messageHandlers.sw.postMessage("" + e);
}
</script>
)SWRESOURCE";

static const char* defaultPageMainBytes = R"SWRESOURCE(
<script>
async function getResult()
{
    var result = await internals.hasServiceWorkerRegistration("/test");
    window.webkit.messageHandlers.sw.postMessage(result ? "PASS" : "FAIL");
}
getResult();
</script>
)SWRESOURCE";

static const char* privatePageMainBytes = R"SWRESOURCE(
<script>
async function getResult()
{
    var result = await internals.hasServiceWorkerRegistration("/test");
    window.webkit.messageHandlers.sw.postMessage(result ? "FAIL" : "PASS");
}
getResult();
</script>
)SWRESOURCE";

TEST(ServiceWorkers, DifferentSessionsUseDifferentRegistrations)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());
    
    auto messageHandler = adoptNS([[SWMessageHandlerForCacheStorage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    
    ServiceWorkerTCPServer server({
        { "text/html", differentSessionsUseDifferentRegistrationsMainBytes },
        { "application/javascript", "" },
        { "text/html", defaultPageMainBytes }
    }, {
        { "text/html", privatePageMainBytes }
    });

    auto defaultWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [defaultWebView synchronouslyLoadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [defaultWebView synchronouslyLoadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [ephemeralWebView synchronouslyLoadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

static const char* regularPageGrabbingCacheStorageDirectory = R"SWRESOURCE(
<script>
async function getResult()
{
    var result = await window.internals.cacheStorageEngineRepresentation();
    window.webkit.messageHandlers.sw.postMessage(result);
}
getResult();
</script>
)SWRESOURCE";

@interface DirectoryPageMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation DirectoryPageMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    retrievedString = [message body];
    done = true;
}
@end

TEST(ServiceWorkers, ServiceWorkerAndCacheStorageDefaultDirectories)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());

    RetainPtr<DirectoryPageMessageHandler> directoryPageMessageHandler = adoptNS([[DirectoryPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:directoryPageMessageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
        { "text/html", regularPageGrabbingCacheStorageDirectory },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    while (![[configuration websiteDataStore] _hasRegisteredServiceWorker])
        TestWebKitAPI::Util::spinRunLoop();

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_TRUE(retrievedString.contains("/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/CacheStorage"));

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, ServiceWorkerAndCacheStorageSpecificDirectories)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());
    auto dataStoreConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [dataStoreConfiguration _setServiceWorkerRegistrationDirectory:[NSURL fileURLWithPath:@"/var/tmp"]];
    [dataStoreConfiguration _setCacheStorageDirectory:[NSURL fileURLWithPath:@"/var/tmp"]];
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr<DirectoryPageMessageHandler> directoryPageMessageHandler = adoptNS([[DirectoryPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:directoryPageMessageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
        { "text/html", regularPageGrabbingCacheStorageDirectory },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    while (![websiteDataStore _hasRegisteredServiceWorker])
        TestWebKitAPI::Util::spinRunLoop();

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_TRUE(retrievedString.contains("\"path\": \"/var/tmp\""));

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

#endif // WK_HAVE_C_SPI

TEST(ServiceWorkers, NonDefaultSessionID)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    NSURL *serviceWorkersPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/ServiceWorkers/" stringByExpandingTildeInPath] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:serviceWorkersPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:serviceWorkersPath.path]);
    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:idbPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<_WKWebsiteDataStoreConfiguration> websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = serviceWorkersPath;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath;

    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytesForSessionIDTest },
        { "application/javascript", scriptBytesForSessionIDTest },
    });

    expectedMessage = "PASS: activation successful";
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView _close];
    webView = nullptr;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:serviceWorkersPath.path]);

    [configuration.get().websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_WK_STREQ(websiteDataRecords[0].displayName, "127.0.0.1");

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

void waitUntilServiceWorkerProcessCount(WKProcessPool *processPool, unsigned processCount)
{
    do {
        if (processPool._serviceWorkerProcessCount == processCount)
            return;
        TestWebKitAPI::Util::spinRunLoop(1);
    } while (true);
}

TEST(ServiceWorkers, ProcessPerSite)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Normally, service workers get terminated several seconds after their clients are gone.
    // Disable this delay for the purpose of testing.
    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    dataStoreConfiguration.serviceWorkerProcessTerminationDelayEnabled = NO;

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration]);
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = dataStore.get();

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server1({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
        { "text/html", mainBytes },
        { "text/html", mainBytes },
    });
    ServiceWorkerTCPServer server2({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    WKProcessPool *processPool = configuration.get().processPool;

    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView1 loadRequest:server1.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server1.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    RetainPtr<WKWebView> webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView3 loadRequest:server1.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    RetainPtr<WKWebView> webView4 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView4 loadRequest:server2.requestWithLocalhost()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(2U, processPool._serviceWorkerProcessCount);

    NSURLRequest *aboutBlankRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]];
    [webView4 loadRequest:aboutBlankRequest];

    waitUntilServiceWorkerProcessCount(processPool, 1);
    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    [webView2 loadRequest:aboutBlankRequest];
    TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    [webView1 loadRequest:aboutBlankRequest];
    [webView3 loadRequest:aboutBlankRequest];
    waitUntilServiceWorkerProcessCount(processPool, 0);
    EXPECT_EQ(0U, processPool._serviceWorkerProcessCount);
}

TEST(ServiceWorkers, ParallelProcessLaunch)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server1({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });
    ServiceWorkerTCPServer server2({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    auto *processPool = configuration.get().processPool;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView1 loadRequest:server1.request()];
    [webView2 loadRequest:server2.requestWithLocalhost()];

    waitUntilServiceWorkerProcessCount(processPool, 2);
}

#if HAVE(NETWORK_FRAMEWORK)

static size_t launchServiceWorkerProcess(bool useSeparateServiceWorkerProcess, bool loadAboutBlankBeforePage)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    TestWebKitAPI::HTTPServer server({
        { "/", { mainBytes } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, scriptBytes } }
    });

    auto *processPool = configuration.get().processPool;
    [processPool _setUseSeparateServiceWorkerProcess: useSeparateServiceWorkerProcess];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    if (loadAboutBlankBeforePage) {
        didFinishNavigation = false;
        [webView loadRequest: [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
        TestWebKitAPI::Util::run(&didFinishNavigation);
    }

    [webView loadRequest:server.request()];

    waitUntilServiceWorkerProcessCount(processPool, 1);
    return webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed;
}

TEST(ServiceWorkers, OutOfAndInProcessServiceWorker)
{
    bool useSeparateServiceWorkerProcess = true;
    bool firstLoadAboutBlank = true;

    EXPECT_EQ(1u, launchServiceWorkerProcess(!useSeparateServiceWorkerProcess, !firstLoadAboutBlank));
    EXPECT_EQ(2u, launchServiceWorkerProcess(useSeparateServiceWorkerProcess, !firstLoadAboutBlank));
}

TEST(ServiceWorkers, LoadAboutBlankBeforeNavigatingThroughServiceWorker)
{
    bool useSeparateServiceWorkerProcess = true;
    bool firstLoadAboutBlank = true;

    EXPECT_EQ(1u, launchServiceWorkerProcess(!useSeparateServiceWorkerProcess, firstLoadAboutBlank));
    EXPECT_EQ(2u, launchServiceWorkerProcess(useSeparateServiceWorkerProcess, firstLoadAboutBlank));
}

#endif // HAVE(NETWORK_FRAMEWORK)

void waitUntilServiceWorkerProcessForegroundActivityState(WKWebView *page, bool shouldHaveActivity)
{
    do {
        if (page._hasServiceWorkerForegroundActivityForTesting == shouldHaveActivity)
            return;
        TestWebKitAPI::Util::spinRunLoop(1);
    } while (true);
}

void waitUntilServiceWorkerProcessBackgroundActivityState(WKWebView *page, bool shouldHaveActivity)
{
    do {
        if (page._hasServiceWorkerBackgroundActivityForTesting == shouldHaveActivity)
            return;
        TestWebKitAPI::Util::spinRunLoop(1);
    } while (true);
}

void testSuspendServiceWorkerProcessBasedOnClientProcesses(bool useSeparateServiceWorkerProcess)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    auto *processPool = configuration.get().processPool;
    [processPool _setUseSeparateServiceWorkerProcess: useSeparateServiceWorkerProcess];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    waitUntilServiceWorkerProcessCount(processPool, 1);

    [webView _setAssertionTypeForTesting: 1];
    waitUntilServiceWorkerProcessForegroundActivityState(webView.get(), false);
    waitUntilServiceWorkerProcessBackgroundActivityState(webView.get(), true);

    [webView _setAssertionTypeForTesting: 3];
    waitUntilServiceWorkerProcessForegroundActivityState(webView.get(), true);
    waitUntilServiceWorkerProcessBackgroundActivityState(webView.get(), false);

    [webView _setAssertionTypeForTesting: 0];
    waitUntilServiceWorkerProcessBackgroundActivityState(webView.get(), false);
    waitUntilServiceWorkerProcessForegroundActivityState(webView.get(), false);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView2 loadRequest:server.request()];

    [webView _close];
    webView = nullptr;

    // The service worker process should take activity based on webView2 process.
    [webView2 _setAssertionTypeForTesting: 1];
    while (webView2.get()._hasServiceWorkerForegroundActivityForTesting || !webView2.get()._hasServiceWorkerBackgroundActivityForTesting) {
        [webView2 _setAssertionTypeForTesting: 1];
        TestWebKitAPI::Util::spinRunLoop(1);
    }

    while (!webView2.get()._hasServiceWorkerForegroundActivityForTesting || webView2.get()._hasServiceWorkerBackgroundActivityForTesting) {
        [webView2 _setAssertionTypeForTesting: 3];
        TestWebKitAPI::Util::spinRunLoop(1);
    }

    while (webView2.get()._hasServiceWorkerForegroundActivityForTesting || webView2.get()._hasServiceWorkerBackgroundActivityForTesting) {
        [webView2 _setAssertionTypeForTesting: 0];
        TestWebKitAPI::Util::spinRunLoop(1);
    }
}

TEST(ServiceWorkers, SuspendServiceWorkerProcessBasedOnClientProcesses)
{
    bool useSeparateServiceWorkerProcess = false;
    testSuspendServiceWorkerProcessBasedOnClientProcesses(useSeparateServiceWorkerProcess);

    useSeparateServiceWorkerProcess = true;
    testSuspendServiceWorkerProcessBasedOnClientProcesses(useSeparateServiceWorkerProcess);
}


#if HAVE(NETWORK_FRAMEWORK)

TEST(ServiceWorkers, ThrottleCrash)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);

    TestWebKitAPI::HTTPServer server({
        { "/", { mainBytes } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, scriptBytes } },
    });

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(MAC)
    [[configuration preferences] _setAppNapEnabled:YES];
#endif
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto *processPool = configuration.get().processPool;

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow: YES]);
    [webView1 setNavigationDelegate:navigationDelegate.get()];

    // Let's make it so that webView1 be app nappable after loading is completed.
    [webView1.get().window resignKeyWindow];
#if PLATFORM(MAC)
    [webView1.get().window orderOut:nil];
#endif

    [webView1 loadRequest:server.request()];

    didFinishNavigation = false;
    TestWebKitAPI::Util::run(&didFinishNavigation);
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webView2Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(MAC)
    [[webView2Configuration preferences] _setAppNapEnabled:NO];
#endif
    [webView2Configuration setProcessPool:processPool];
    [[webView2Configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    webView2Configuration.get()._relatedWebView = webView1.get();

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView2Configuration.get()]);
    [webView2 setNavigationDelegate:navigationDelegate.get()];

    [webView2 loadRequest:server.request()];

    didFinishNavigation = false;
    TestWebKitAPI::Util::run(&didFinishNavigation);
}

#endif // HAVE(NETWORK_FRAMEWORK)

TEST(ServiceWorkers, LoadData)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestSWAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    done = false;

    // Normal load to get SW registered.
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // Now try a data load.
    NSData *data = [NSData dataWithBytes:mainBytes length:strlen(mainBytes)];
    [webView loadData:data MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:server.request().URL];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, RestoreFromDiskNonDefaultStore)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    NSURL *swDBPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/com.apple.WebKit.TestWebKitAPI/CustomWebsiteData/ServiceWorkers/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:swDBPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);
    [[NSFileManager defaultManager] createDirectoryAtURL:swDBPath withIntermediateDirectories:YES attributes:nil error:nil];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    // We protect the process pool so that it outlives the WebsiteDataStore.
    RetainPtr<WKProcessPool> protectedProcessPool;

    ServiceWorkerTCPServer server({
        { "text/html", mainRegisteringWorkerBytes },
        { "application/javascript", scriptBytes },
    }, {
        { "text/html", mainRegisteringAlreadyExistingWorkerBytes },
        { "application/javascript", scriptBytes },
    });

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swDBPath;
        auto nonDefaultDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        configuration.get().websiteDataStore = nonDefaultDataStore.get();

        auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration was successful and service worker was activated"]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        protectedProcessPool = webView.get().configuration.processPool;

        [webView loadRequest:server.request()];

        TestWebKitAPI::Util::run(&done);
        done = false;

        [webView.get().configuration.processPool _terminateServiceWorkers];
    }

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swDBPath;
        auto nonDefaultDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        configuration.get().websiteDataStore = nonDefaultDataStore.get();

        auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        [webView loadRequest:server.request()];

        TestWebKitAPI::Util::run(&done);
        done = false;
    }
}

TEST(ServiceWorkers, SuspendNetworkProcess)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
        { "text/html", mainBytes },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestSWAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    done = false;

    // Normal load to get SW registered.
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto store = [configuration websiteDataStore];
    auto path = store._configuration._serviceWorkerRegistrationDirectory.path;

    NSURL* directory = [NSURL fileURLWithPath:path isDirectory:YES];
    NSURL *swDBPath = [directory URLByAppendingPathComponent:@"ServiceWorkerRegistrations-4.sqlite3"];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [ webView.get().configuration.processPool _sendNetworkProcessWillSuspendImminently];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [ webView.get().configuration.processPool _sendNetworkProcessDidResume];

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);
}

TEST(WebKit, ServiceWorkerDatabaseWithRecordsTableButUnexpectedSchema)
{
    // Copy the baked database files to the database directory
    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"BadServiceWorkerRegistrations-4" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *swPath = [NSURL fileURLWithPath:[@"~/Library/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/ServiceWorkers/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:swPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:swPath.path]);

    [[NSFileManager defaultManager] createDirectoryAtURL:swPath withIntermediateDirectories:YES attributes:nil error:nil];
    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[swPath URLByAppendingPathComponent:@"ServiceWorkerRegistrations-4.sqlite3"] error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swPath;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    // Fetch SW records
    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeServiceWorkerRegistrations]]);
    static bool readyToContinue;
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;
}

TEST(ServiceWorkers, ProcessPerSession)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server1({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    WKProcessPool *processPool = configuration.get().processPool;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 loadRequest:server1.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    ServiceWorkerTCPServer server2({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server2.requestWithLocalhost()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(2U, processPool._serviceWorkerProcessCount);
}

static const char* contentRuleListWorkerScript =
"self.addEventListener('message', (event) => {"
"    fetch('blockedsubresource').then(() => {"
"        event.source.postMessage('FAIL - should have blocked first request');"
"    }).catch(() => {"
"        fetch('allowedsubresource').then(() => {"
"            event.source.postMessage('PASS - blocked first request, allowed second');"
"        }).catch(() => {"
"            event.source.postMessage('FAIL - should have allowed second request');"
"        });"
"    });"
"});";

TEST(ServiceWorkers, ContentRuleList)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    __block bool doneCompiling = false;
    __block RetainPtr<WKContentRuleList> contentRuleList;
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"ServiceWorkerRuleList" encodedContentRuleList:@"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"blockedsubresource\"}}]" completionHandler:^(WKContentRuleList *list, NSError *error) {
        EXPECT_NOT_NULL(list);
        EXPECT_NULL(error);
        contentRuleList = list;
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [[configuration userContentController] addContentRuleList:contentRuleList.get()];

    using namespace TestWebKitAPI;
    TCPServer server([] (int socket) {
        auto respond = [socket] (const char* body, const char* mimeType) {
            NSString *format = @"HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n\r\n"
            "%s";
            NSString *response = [NSString stringWithFormat:format, mimeType, strlen(body), body];
            TCPServer::write(socket, response.UTF8String, response.length);
        };
        TCPServer::read(socket);
        respond(mainBytes, "text/html");
        TCPServer::read(socket);
        respond(contentRuleListWorkerScript, "application/javascript");
        auto lastRequest = TCPServer::read(socket);
        EXPECT_TRUE(strnstr((const char*)lastRequest.data(), "allowedsubresource", lastRequest.size()));
        respond("successful fetch", "application/octet-stream");
    });

    expectedMessage = @"Message from worker: PASS - blocked first request, allowed second";

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    TestWebKitAPI::Util::run(&done);
    
    __block bool doneRemoving = false;
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"ServiceWorkerRuleList" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}
