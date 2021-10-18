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
#import "ServiceWorkerPageProtocol.h"
#import "ServiceWorkerTCPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/CertificateInfo.h>
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
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/Deque.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

static bool done;
static bool didFinishNavigation;
static bool serviceWorkerGlobalObjectIsAvailable;

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

enum class ShouldRunServiceWorkersOnMainThread : bool { No, Yes };

static void setViewDataStore(WKWebViewConfiguration* viewConfiguration, ShouldRunServiceWorkersOnMainThread shouldRunServiceWorkersOnMainThread)
{
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setShouldRunServiceWorkersOnMainThreadForTesting:shouldRunServiceWorkersOnMainThread == ShouldRunServiceWorkersOnMainThread::Yes];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];
}

static void runBasicSWTest(ShouldRunServiceWorkersOnMainThread shouldRunServiceWorkersOnMainThread)
{
    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes},
    });

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setViewDataStore(configuration.get(), shouldRunServiceWorkersOnMainThread);

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    // Start with a clean slate data store
    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = nullptr;

    [[configuration websiteDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_WK_STREQ(websiteDataRecords[0].displayName, "127.0.0.1");

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, Basic)
{
    runBasicSWTest(ShouldRunServiceWorkersOnMainThread::No);
}

TEST(ServiceWorkers, BasicWithMainThreadSW)
{
    runBasicSWTest(ShouldRunServiceWorkersOnMainThread::Yes);
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
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    if (navigationAction.targetFrame.mainFrame)
        [websitePolicies _setCustomUserAgent:_userAgent];

    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
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

    messageHandler = adoptNS([[SWUserAgentMessageHandler alloc] initWithExpectedMessage:@"Message from worker: Foo Custom UserAgent"]);
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

    TestWebKitAPI::HTTPServer server({
        { "/", { mainCacheStorageBytes } }
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

    TestWebKitAPI::HTTPServer server({
        { "/", { mainForFetchTestBytes } },
        { "/sw.js", { { { "Content-Type", "application/javascript" } }, scriptHandlingFetchBytes } },
    });

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

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

TEST(ServiceWorkers, MainThreadSWInterceptsLoad)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setViewDataStore(configuration.get(), ShouldRunServiceWorkersOnMainThread::Yes);

    RetainPtr<WKWebsiteDataStore> dataStore = [configuration websiteDataStore];

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainForFirstLoadInterceptTestBytes },
        { "application/javascript", scriptInterceptingFirstLoadBytes },
    }, {
        { "application/javascript", scriptInterceptingFirstLoadBytes },
    });

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Service Worker activated";
    [webView loadRequest:server.request()];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
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
    RetainPtr<WKProcessPool> originalProcessPool = configuration.get().processPool;

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
    EXPECT_EQ(1u, webView.get().configuration.processPool._serviceWorkerProcessCount);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1u, webView.get().configuration.processPool._serviceWorkerProcessCount);
    EXPECT_EQ(1u, originalProcessPool.get()._serviceWorkerProcessCount);
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

    configuration.get().websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]).get();

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

static bool waitUntilEvaluatesToTrue(const Function<bool()>& f)
{
    unsigned timeout = 0;
    do {
        if (f())
            return true;
        TestWebKitAPI::Util::sleep(0.1);
    } while (++timeout < 100);
    return false;
}

TEST(ServiceWorkers, ProcessPerSite)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Normally, service workers get terminated several seconds after their clients are gone.
    // Disable this delay for the purpose of testing.
    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [dataStoreConfiguration setServiceWorkerProcessTerminationDelayEnabled:NO];

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    
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

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [processPool _serviceWorkerProcessCount] == 1; }));
    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    [webView2 loadRequest:aboutBlankRequest];
    TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    [webView1 loadRequest:aboutBlankRequest];
    [webView3 loadRequest:aboutBlankRequest];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![processPool _serviceWorkerProcessCount]; }));
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

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [processPool _serviceWorkerProcessCount] == 2; }));
}

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

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [processPool _serviceWorkerProcessCount] == 1; }));
    return webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed;
}

TEST(ServiceWorkers, OutOfProcessServiceWorker)
{
    bool useSeparateServiceWorkerProcess = true;
    bool firstLoadAboutBlank = true;

    EXPECT_EQ(1u, launchServiceWorkerProcess(!useSeparateServiceWorkerProcess, !firstLoadAboutBlank));
}

TEST(ServiceWorkers, InProcessServiceWorker)
{
    bool useSeparateServiceWorkerProcess = true;
    bool firstLoadAboutBlank = true;

    EXPECT_EQ(2u, launchServiceWorkerProcess(useSeparateServiceWorkerProcess, !firstLoadAboutBlank));
}

TEST(ServiceWorkers, LoadAboutBlankBeforeNavigatingThroughOutOfProcessServiceWorker)
{
    bool useSeparateServiceWorkerProcess = true;
    bool firstLoadAboutBlank = true;

    EXPECT_EQ(1u, launchServiceWorkerProcess(!useSeparateServiceWorkerProcess, firstLoadAboutBlank));
}

TEST(ServiceWorkers, LoadAboutBlankBeforeNavigatingThroughInProcessServiceWorker)
{
    bool useSeparateServiceWorkerProcess = true;
    bool firstLoadAboutBlank = true;

    EXPECT_EQ(2u, launchServiceWorkerProcess(useSeparateServiceWorkerProcess, firstLoadAboutBlank));
}

enum class UseSeparateServiceWorkerProcess : bool { No, Yes };
void testSuspendServiceWorkerProcessBasedOnClientProcesses(UseSeparateServiceWorkerProcess useSeparateServiceWorkerProcess)
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
    [processPool _setUseSeparateServiceWorkerProcess:(useSeparateServiceWorkerProcess == UseSeparateServiceWorkerProcess::Yes)];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [processPool _serviceWorkerProcessCount] == 1; }));

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server.request()];

    auto webViewToUpdate = useSeparateServiceWorkerProcess == UseSeparateServiceWorkerProcess::Yes ? webView : webView2;
    [webViewToUpdate _setAssertionTypeForTesting: 1];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![webViewToUpdate _hasServiceWorkerForegroundActivityForTesting]; }));
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [webViewToUpdate _hasServiceWorkerBackgroundActivityForTesting]; }));

    [webViewToUpdate _setAssertionTypeForTesting: 3];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [webViewToUpdate _hasServiceWorkerForegroundActivityForTesting]; }));
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![webViewToUpdate _hasServiceWorkerBackgroundActivityForTesting]; }));

    [webViewToUpdate _setAssertionTypeForTesting: 0];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![webViewToUpdate _hasServiceWorkerForegroundActivityForTesting]; }));
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return ![webViewToUpdate _hasServiceWorkerBackgroundActivityForTesting]; }));

    [webView _close];
    webView = nullptr;

    // The service worker process should take activity based on webView2 process.
    [webView2 _setAssertionTypeForTesting: 1];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        [webView2 _setAssertionTypeForTesting:1];
        return ![webView2 _hasServiceWorkerForegroundActivityForTesting] && [webView2 _hasServiceWorkerBackgroundActivityForTesting];
    }));

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        [webView2 _setAssertionTypeForTesting:3];
        return [webView2 _hasServiceWorkerForegroundActivityForTesting] && ![webView2 _hasServiceWorkerBackgroundActivityForTesting];
    }));

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        [webView2 _setAssertionTypeForTesting:0];
        return ![webView2 _hasServiceWorkerForegroundActivityForTesting] && ![webView2 _hasServiceWorkerBackgroundActivityForTesting];
    }));
}

TEST(ServiceWorkers, SuspendServiceWorkerProcessBasedOnClientProcessesWithSeparateServiceWorkerProcess)
{
    testSuspendServiceWorkerProcessBasedOnClientProcesses(UseSeparateServiceWorkerProcess::Yes);
}

TEST(ServiceWorkers, SuspendServiceWorkerProcessBasedOnClientProcessesWithoutSeparateServiceWorkerProcess)
{
    testSuspendServiceWorkerProcessBasedOnClientProcesses(UseSeparateServiceWorkerProcess::No);
}

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

    RetainPtr<WKProcessPool> protectedProcessPool;
    RetainPtr<WKWebsiteDataStore> protectedWebsiteDataStore;

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
        protectedWebsiteDataStore = webView.get().configuration.websiteDataStore;

        [webView loadRequest:server.request()];

        TestWebKitAPI::Util::run(&done);
        done = false;

        [webView.get().configuration.processPool _terminateServiceWorkers];
    }

    // Make us more likely to lose any races with service worker initialization.
    TestWebKitAPI::Util::spinRunLoop(10);
    usleep(10000);
    TestWebKitAPI::Util::spinRunLoop(10);

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        configuration.get().websiteDataStore = protectedWebsiteDataStore.get();

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
    NSURL *swDBPath = [directory URLByAppendingPathComponent:@"ServiceWorkerRegistrations-7.sqlite3"];

    unsigned timeout = 0;
    while (![[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path] && ++timeout < 100)
        TestWebKitAPI::Util::sleep(0.1);

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [webView.get().configuration.websiteDataStore _sendNetworkProcessWillSuspendImminently];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [webView.get().configuration.websiteDataStore _sendNetworkProcessDidResume];

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
    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[swPath URLByAppendingPathComponent:@"ServiceWorkerRegistrations-7.sqlite3"] error:nil];

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
    HTTPServer server([] (Connection connection) {
        connection.receiveHTTPRequest([=](Vector<char>&&) {
            connection.send(HTTPResponse({{ "Content-Type", "text/html" }}, mainBytes).serialize(), [=] {
                connection.receiveHTTPRequest([=](Vector<char>&&) {
                    connection.send(HTTPResponse({{ "Content-Type", "application/javascript" }}, contentRuleListWorkerScript).serialize(), [=] {
                        connection.receiveHTTPRequest([=](Vector<char>&& lastRequest) {
                            EXPECT_TRUE(strnstr((const char*)lastRequest.data(), "allowedsubresource", lastRequest.size()));
                            connection.send(HTTPResponse("successful fetch").serialize());
                        });
                    });
                });
            });
        });
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

static bool isTestServerTrust(SecTrustRef trust)
{
    if (!trust)
        return false;
    if (SecTrustGetCertificateCount(trust) != 1)
        return false;

#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
    auto chain = adoptCF(SecTrustCopyCertificateChain(trust));
    auto certificate = checked_cf_cast<SecCertificateRef>(CFArrayGetValueAtIndex(chain.get(), 0));
#else
    auto certificate = SecTrustGetCertificateAtIndex(trust, 0);
#endif
    if (![adoptNS((NSString *)SecCertificateCopySubjectSummary(certificate)) isEqualToString:@"Me"])
        return false;

    return true;
}

enum class ResponseType { Synthetic, Cached, Fetched };
static void runTest(ResponseType responseType)
{
    using namespace TestWebKitAPI;
    
    __block bool removedAnyExistingData = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        removedAnyExistingData = true;
    }];
    TestWebKitAPI::Util::run(&removedAnyExistingData);

    static const char* main =
    "<script>"
    "try {"
    "    navigator.serviceWorker.register('/sw.js').then(function(reg) {"
    "        if (reg.active) {"
    "            alert('worker unexpectedly already active');"
    "            return;"
    "        }"
    "        worker = reg.installing;"
    "        worker.addEventListener('statechange', function() {"
    "            if (worker.state == 'activated')"
    "                alert('successfully registered');"
    "        });"
    "    }).catch(function(error) {"
    "        alert('Registration failed with: ' + error);"
    "    });"
    "} catch(e) {"
    "    alert('Exception: ' + e);"
    "}"
    "</script>";
    
    const char* js = nullptr;
    const char* expectedAlert = nullptr;
    size_t expectedServerRequests1 = 0;
    size_t expectedServerRequests2 = 0;

    switch (responseType) {
    case ResponseType::Synthetic:
        js = "self.addEventListener('fetch', (event) => { event.respondWith(new Response(new Blob(['<script>alert(\"synthetic response\")</script>'], {type: 'text/html'}))); })";
        expectedAlert = "synthetic response";
        expectedServerRequests1 = 2;
        expectedServerRequests2 = 2;
        break;
    case ResponseType::Cached:
        js = "self.addEventListener('install', (event) => { event.waitUntil( caches.open('v1').then((cache) => { return cache.addAll(['/cached.html']); }) ); });"
            "self.addEventListener('fetch', (event) => { event.respondWith(caches.match('/cached.html')) });";
        expectedAlert = "loaded from cache";
        expectedServerRequests1 = 3;
        expectedServerRequests2 = 3;
        break;
    case ResponseType::Fetched:
        js = "self.addEventListener('fetch', (event) => { event.respondWith(fetch('/fetched.html')) });";
        expectedAlert = "fetched from server";
        expectedServerRequests1 = 2;
        expectedServerRequests2 = 3;
        break;
    }

    HTTPServer server({
        { "/", { main } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, js } },
        { "/cached.html", { "<script>alert('loaded from cache')</script>" } },
        { "/fetched.html", { "<script>alert('fetched from server')</script>" } },
    }, HTTPServer::Protocol::Https);

    auto webView = adoptNS([WKWebView new]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully registered");

    EXPECT_EQ(server.totalRequests(), expectedServerRequests1);
    EXPECT_TRUE(isTestServerTrust(webView.get().serverTrust));
    if (responseType != ResponseType::Fetched)
        server.cancel();

    [webView reload];
    EXPECT_WK_STREQ([webView _test_waitForAlert], expectedAlert);
    EXPECT_EQ(server.totalRequests(), expectedServerRequests2);
    EXPECT_TRUE(isTestServerTrust(webView.get().serverTrust));
}

TEST(ServiceWorkers, ServerTrust)
{
    runTest(ResponseType::Synthetic);
    runTest(ResponseType::Cached);
    runTest(ResponseType::Fetched);
}

TEST(ServiceWorkers, ChangeOfServerCertificate)
{
    __block bool removedAnyExistingData = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        removedAnyExistingData = true;
    }];
    TestWebKitAPI::Util::run(&removedAnyExistingData);

    static const char* main =
    "<script>"
    "async function test() {"
    "    try {"
    "        const registration = await navigator.serviceWorker.register('/sw.js');"
    "        if (registration.active) {"
    "            registration.onupdatefound = () => alert('new worker');"
    "            setTimeout(() => alert('no update found'), 5000);"
    "            return;"
    "        }"
    "        worker = registration.installing;"
    "        worker.addEventListener('statechange', () => {"
    "            if (worker.state == 'activated')"
    "                alert('successfully registered');"
    "        });"
    "    } catch(e) {"
    "        alert('Exception: ' + e);"
    "    }"
    "}"
    "window.onload = test;"
    "</script>";
    static const char* js = "";

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    webView1.get().navigationDelegate = delegate.get();
    webView2.get().navigationDelegate = delegate.get();

    uint16_t serverPort;

    // Load webView1 with a first server.
    {
        TestWebKitAPI::HTTPServer server1({
            { "/", { main } },
            { "/sw.js", { {{ "Content-Type", "application/javascript" }}, js } }
        }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, testIdentity());
        serverPort = server1.port();

        [webView1 loadRequest:server1.request()];
        EXPECT_WK_STREQ([webView1 _test_waitForAlert], "successfully registered");

        server1.cancel();
    }

    // Load webView2 with a second server on same port with a different certificate
    // This should trigger installing a new worker.
    {
        TestWebKitAPI::HTTPServer server2({
            { "/", { main } },
            { "/sw.js", { {{ "Content-Type", "application/javascript" }}, js } }
        }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, testIdentity2(), serverPort);

        [webView2 loadRequest:server2.request()];
        EXPECT_WK_STREQ([webView2 _test_waitForAlert], "new worker");
    }
}

TEST(ServiceWorkers, ClearDOMCacheAlsoIncludesServiceWorkerRegistrations)
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

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Fetch SW records
    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeServiceWorkerRegistrations]]);
    static bool readyToContinue;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // Clear DOM Cache
    auto typesToRemove = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeFetchCache]]);
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:typesToRemove.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Fetch SW records again
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;
}

TEST(ServiceWorkers, CustomDataStorePathsVersusCompletionHandlers)
{
    NSURL *swPath = [NSURL fileURLWithPath:[@"~/Library/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/ServiceWorkers/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:swPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:swPath.path]);

    [[NSFileManager defaultManager] createDirectoryAtURL:swPath withIntermediateDirectories:YES attributes:nil error:nil];

    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swPath;
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    ServiceWorkerTCPServer server({
        { "text/html", mainBytes },
        { "application/javascript", scriptBytes },
    });

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    unsigned timeout = 0;
    while (![[NSFileManager defaultManager] fileExistsAtPath:[swPath URLByAppendingPathComponent:@"ServiceWorkerRegistrations-7.sqlite3"].path] && ++timeout < 20)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:[swPath URLByAppendingPathComponent:@"ServiceWorkerRegistrations-7.sqlite3"].path]);

    // Fetch SW records
    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeServiceWorkerRegistrations]]);
    static bool readyToContinue;
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // Fetch records again, this time releasing our reference to the data store while the request is in flight.
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1U, dataRecords.count);
        readyToContinue = true;
    }];
    dataStore = nil;
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // Delete all SW records, releasing our reference to the data store while the request is in flight.
    dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [dataStore removeDataOfTypes:websiteDataTypes.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    dataStore = nil;
    TestWebKitAPI::Util::run(&readyToContinue);
    readyToContinue = false;

    // The records should have been deleted, and the callback should have been made.
    // Now refetch the records to verify they are gone.
    dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [dataStore fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0U, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(ServiceWorkers, WebProcessCache)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    processPoolConfiguration.get().usesWebProcessCache = YES;
    processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    static const char* main =
    "<script>"
    "function registerServiceWorker()"
    "{"
    "   try {"
    "       navigator.serviceWorker.register('/sw.js').then(function(reg) {"
    "           if (reg.active) {"
    "               alert('worker unexpectedly already active');"
    "               return;"
    "           }"
    "           worker = reg.installing;"
    "           worker.addEventListener('statechange', function() {"
    "               if (worker.state == 'activated')"
    "                   alert('successfully registered');"
    "               });"
    "           }).catch(function(error) {"
    "               alert('Registration failed with: ' + error);"
    "           });"
    "   } catch(e) {"
    "       alert('Exception: ' + e);"
    "   }"
    "}"
    "alert('loaded');"
    "</script>";

    TestWebKitAPI::HTTPServer server({
        { "/", { main } },
        { "/sw.js", { {{ "Content-Type", "application/javascript" }}, "" } }
    });

    // Create a first web view and load a page
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 loadRequest:server.request()];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "loaded");

    // Create a second web view and load a page
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server.request()];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "loaded");

    // Close the first page to put it in the cache
    [webView1 _close];

    // Register a service worker, it should go in webView1 process.
    [webView2 stringByEvaluatingJavaScript:@"registerServiceWorker()"];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "successfully registered");

    // Clearing web process cache should not kill the service worker process
    [configuration.get().processPool _clearWebProcessCache];

    EXPECT_EQ(1U, configuration.get().processPool._serviceWorkerProcessCount);
}

static bool didStartURLSchemeTask = false;

@interface ServiceWorkerSchemeHandler : NSObject <WKURLSchemeHandler> {
    const char* _bytes;
    HashMap<String, RetainPtr<NSData>> _dataMappings;
}
- (instancetype)initWithBytes:(const char*)bytes;
- (void)addMappingFromURLString:(NSString *)urlString toData:(const char*)data;
@end

@implementation ServiceWorkerSchemeHandler

- (instancetype)initWithBytes:(const char*)bytes
{
    self = [super init];
    _bytes = bytes;
    return self;
}

- (void)addMappingFromURLString:(NSString *)urlString toData:(const char*)data
{
    _dataMappings.set(urlString, [NSData dataWithBytesNoCopy:(void*)data length:strlen(data) freeWhenDone:NO]);
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([(id<WKURLSchemeTaskPrivate>)task _requestOnlyIfCached]) {
        [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
        return;
    }

    NSURL *finalURL = task.request.URL;

    NSMutableDictionary* headerDictionary = [NSMutableDictionary dictionary];
    if (URL(finalURL).string().endsWith(".js"))
        [headerDictionary setObject:@"text/javascript" forKey:@"Content-Type"];
    else
        [headerDictionary setObject:@"text/html" forKey:@"Content-Type"];
    [headerDictionary setObject:@"1" forKey:@"Content-Length"];
    auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:finalURL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:headerDictionary]);
    [task didReceiveResponse:response.get()];

    if (auto data = _dataMappings.get([finalURL absoluteString]))
        [task didReceiveData:data.get()];
    else if (_bytes) {
        RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytesNoCopy:(void *)_bytes length:strlen(_bytes) freeWhenDone:NO]);
        [task didReceiveData:data.get()];
    } else
        [task didReceiveData:[@"Hello" dataUsingEncoding:NSUTF8StringEncoding]];

    [task didFinish];

    didStartURLSchemeTask = true;
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

@interface ServiceWorkerPageRemoteObject : NSObject <ServiceWorkerPageProtocol>
@end

@implementation ServiceWorkerPageRemoteObject

- (void)serviceWorkerGlobalObjectIsAvailable
{
    serviceWorkerGlobalObjectIsAvailable = true;
}

@end

TEST(ServiceWorker, ExtensionServiceWorker)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto schemeHandler = adoptNS([ServiceWorkerSchemeHandler new]);
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/other.html" toData:"foo"];
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/sw.js" toData:""];

    WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];

    auto otherViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    otherViewConfiguration.get().processPool = webViewConfiguration.processPool;
    [otherViewConfiguration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"sw-ext"];
    auto otherWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:otherViewConfiguration.get()]);
    [otherWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"sw-ext://ABC/other.html"]]];
    [otherWebView _test_waitForDidFinishNavigation];

    auto otherViewPID = [otherWebView _webProcessIdentifier];

    webViewConfiguration.websiteDataStore = [otherViewConfiguration websiteDataStore];
    webViewConfiguration._relatedWebView = otherWebView.get();
    [webViewConfiguration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"sw-ext"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);

    auto object = adoptNS([[ServiceWorkerPageRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    // The service worker script should get loaded over the custom scheme handler.
    done = false;
    didStartURLSchemeTask = false;
    [webView _loadServiceWorker:[NSURL URLWithString:@"sw-ext://ABC/sw.js"] completionHandler:^(BOOL success) {
        EXPECT_TRUE(success);
        EXPECT_TRUE(didStartURLSchemeTask);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // The injected bundle should have been notified that the service worker's global object was available.
    TestWebKitAPI::Util::run(&serviceWorkerGlobalObjectIsAvailable);

    // Both views should be sharing the same WebProcess, which should also host the service worker.
    ASSERT_EQ(otherViewPID, [webView _webProcessIdentifier]);
    while (!webViewConfiguration.processPool._serviceWorkerProcessCount)
        TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(webViewConfiguration.processPool._serviceWorkerProcessCount, 1U);
    EXPECT_EQ(webViewConfiguration.processPool._webProcessCountIgnoringPrewarmedAndCached, 1U);

    TestWebKitAPI::Util::sleep(0.5);

    EXPECT_EQ(webViewConfiguration.processPool._serviceWorkerProcessCount, 1U);
    EXPECT_EQ(webViewConfiguration.processPool._webProcessCountIgnoringPrewarmedAndCached, 1U);

    EXPECT_WK_STREQ([webView URL].absoluteString, @"sw-ext://ABC");

    // The service worker should exit if we close/deallocate the view we used to launch it.
    [webView _close];
    webView = nil;
    while (webViewConfiguration.processPool._serviceWorkerProcessCount)
        TestWebKitAPI::Util::spinRunLoop(10);
}

TEST(ServiceWorker, ExtensionServiceWorkerFailureBadScript)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto schemeHandler = adoptNS([ServiceWorkerSchemeHandler new]);
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/bad-sw.js" toData:"1 = 1;"];

    WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];
    [webViewConfiguration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"sw-ext"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);

    auto object = adoptNS([[ServiceWorkerPageRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    // The service worker script should get loaded over the custom scheme handler.
    done = false;
    didStartURLSchemeTask = false;
    [webView _loadServiceWorker:[NSURL URLWithString:@"sw-ext://ABC/bad-sw.js"] completionHandler:^(BOOL success) {
        EXPECT_FALSE(success);
        EXPECT_TRUE(didStartURLSchemeTask);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ServiceWorker, ExtensionServiceWorkerFailureBadURL)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);

    auto object = adoptNS([[ServiceWorkerPageRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    done = false;
    [webView _loadServiceWorker:[NSURL URLWithString:@"https://ABC/does-not-exist.js"] completionHandler:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(ServiceWorker, ExtensionServiceWorkerFailureViewClosed)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);

    auto object = adoptNS([[ServiceWorkerPageRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    [webView _loadServiceWorker:[NSURL URLWithString:@"https://ABC/does-not-exist.js"] completionHandler:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];
    [webView _close];
    TestWebKitAPI::Util::run(&done);
}

TEST(ServiceWorker, ExtensionServiceWorkerFailureViewDestroyed)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    @autoreleasepool {
        WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);

        auto object = adoptNS([[ServiceWorkerPageRemoteObject alloc] init]);
        _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
        [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

        [webView _loadServiceWorker:[NSURL URLWithString:@"https://ABC/does-not-exist.js"] completionHandler:^(BOOL success) {
            EXPECT_FALSE(success);
            done = true;
        }];
    }
    TestWebKitAPI::Util::run(&done);
}
