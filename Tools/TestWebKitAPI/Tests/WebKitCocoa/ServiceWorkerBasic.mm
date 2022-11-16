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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "ServiceWorkerPageProtocol.h"
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
#import <WebKit/_WKInternalDebugFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreDelegate.h>
#import <wtf/Deque.h>
#import <wtf/FileSystem.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>
#import <wtf/text/StringConcatenateNumbers.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

static NSString *serviceWorkerRegistrationFilename = @"ServiceWorkerRegistrations-8.sqlite3";
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

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationComplete = true;
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    navigationFailed = true;
    navigationComplete = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
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

static constexpr auto mainCacheStorageBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto mainBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto scriptBytes = R"SWRESOURCE(

self.addEventListener("message", (event) => {
    event.source.postMessage("ServiceWorker received: " + event.data);
});

)SWRESOURCE"_s;

static constexpr auto mainForFetchTestBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto scriptHandlingFetchBytes = R"SWRESOURCE(

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("test.html") !== -1) {
        event.respondWith(new Response(new Blob(['Intercepted by worker'], {type: 'text/html'})));
    }
});

)SWRESOURCE"_s;

static constexpr auto scriptInterceptingFirstLoadBytes = R"SWRESOURCE(

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("main.html") !== -1) {
        event.respondWith(new Response(new Blob(['Intercepted by worker <script>window.webkit.messageHandlers.sw.postMessage(\'Intercepted by worker\');</script>'], {type: 'text/html'})));
    }
});

)SWRESOURCE"_s;

static constexpr auto mainForFirstLoadInterceptTestBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto mainRegisteringWorkerBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto mainRegisteringAlreadyExistingWorkerBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto mainBytesForSessionIDTest = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto scriptBytesForSessionIDTest = R"SWRESOURCE(

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

)SWRESOURCE"_s;

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
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
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

static constexpr auto userAgentSWBytes = R"SWRESOURCE(

self.addEventListener("message", (event) => {
    event.source.postMessage(navigator.userAgent);
});

)SWRESOURCE"_s;

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

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, userAgentSWBytes } },
    });

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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { mainRegisteringWorkerBytes } },
        { "/second.html"_s, { mainRegisteringAlreadyExistingWorkerBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request("/first.html"_s)];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request("/second.html"_s)];

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
        { "/"_s, { mainCacheStorageBytes } }
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
        { "/"_s, { mainForFetchTestBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptHandlingFetchBytes } },
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

    TestWebKitAPI::HTTPServer server({
        { "/main.html"_s, { mainForFirstLoadInterceptTestBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptInterceptingFirstLoadBytes } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Service Worker activated"_s;
    [webView loadRequest:server.request("/main.html"_s)];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Intercepted by worker"_s;
    [webView loadRequest:server.request("/main.html"_s)];

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

    TestWebKitAPI::HTTPServer server({
        { "/main.html"_s, { mainForFirstLoadInterceptTestBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptInterceptingFirstLoadBytes } },
    });

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Service Worker activated"_s;
    [webView loadRequest:server.request("/main.html"_s)];

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

    expectedMessage = "Intercepted by worker"_s;
    [webView loadRequest:server.request("/main.html"_s)];

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

    TestWebKitAPI::HTTPServer server({
        { "/main.html"_s, { mainForFirstLoadInterceptTestBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptInterceptingFirstLoadBytes } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Register a service worker and activate it.
    expectedMessage = "Service Worker activated"_s;
    [webView loadRequest:server.request("/main.html"_s)];

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
    expectedMessage = "Intercepted by worker"_s;
    [webView loadRequest:server.request("/main.html"_s)];

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
    [webView loadRequest:server.request("/main.html"_s)];
    TestWebKitAPI::Util::run(&navigationComplete);

    EXPECT_FALSE(navigationFailed);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    shouldAccept = false;
    navigationFailed = false;
    navigationComplete = false;

    // Verify service worker load fails well when policy delegate is not ok.
    [webView loadRequest:server.request("/main.html"_s)];
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

static constexpr auto regularPageWithConnectionBytes = R"SWRESOURCE(
<script>
window.webkit.messageHandlers.regularPage.postMessage("PASS");
</script>
)SWRESOURCE"_s;

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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { regularPageWithConnectionBytes } },
        { "/second.html"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
        { "/third.html"_s, { regularPageWithConnectionBytes } },
        { "/fourth.html"_s, { regularPageWithConnectionBytes } },
    });

    RetainPtr<WKWebView> regularPageWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> newRegularPageWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Test that a regular page does not trigger a service worker connection to network process if there is no registered service worker.
    [regularPageWebView loadRequest:server.request("/first.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Test that a sw scheme page can register a service worker.
    [webView loadRequest:server.request("/second.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
    webView = nullptr;

    // Now that a service worker is registered, the regular page should have a service worker connection.
    [regularPageWebView loadRequest:server.request("/third.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
    regularPageWebView = nullptr;

    [newRegularPageWebView loadRequest:server.request("/fourth.html"_s)];
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

static constexpr auto mainBytesWithScope = R"SWRESOURCE(
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
)SWRESOURCE"_s;

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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { mainBytesWithScope } },
        { "/second.html"_s, { regularPageWithConnectionBytes } },
        { "/third.html"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a page that registers a service worker.
    [webView loadRequest:server.request("/first.html"_s)];
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
    [webView loadRequest:server.request("/second.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading the simple page did not start the service worker process.
    EXPECT_EQ(1u, webView.get().configuration.processPool._serviceWorkerProcessCount);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    [webView loadRequest:server.request("/third.html"_s)];
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

static constexpr auto readCacheBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto writeCacheBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { writeCacheBytes } },
        { "/second.html"_s, { readCacheBytes } },
    });

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request("/first.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request("/second.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

static constexpr auto serviceWorkerCacheAccessEphemeralSessionMainBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto serviceWorkerCacheAccessEphemeralSessionSWBytes = R"SWRESOURCE(
self.addEventListener("message", (event) => {
    try {
        self.caches.keys().then((keys) => {
            event.source.postMessage(keys.length === 0 ? "PASS" : "FAIL: caches is not empty, got: " + JSON.stringify(keys));
        });
    } catch (e) {
         event.source.postMessage("" + e);
    }
});
)SWRESOURCE"_s;

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
    
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { serviceWorkerCacheAccessEphemeralSessionMainBytes } },
        { "/serviceworker-private-browsing-worker.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, serviceWorkerCacheAccessEphemeralSessionSWBytes } },
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

static constexpr auto differentSessionsUseDifferentRegistrationsMainBytes = R"SWRESOURCE(
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
)SWRESOURCE"_s;

static constexpr auto defaultPageMainBytes = R"SWRESOURCE(
<script>
async function getResult()
{
    var result = await internals.hasServiceWorkerRegistration("/test");
    window.webkit.messageHandlers.sw.postMessage(result ? "PASS" : "FAIL");
}
getResult();
</script>
)SWRESOURCE"_s;

static constexpr auto privatePageMainBytes = R"SWRESOURCE(
<script>
async function getResult()
{
    var result = await internals.hasServiceWorkerRegistration("/test");
    window.webkit.messageHandlers.sw.postMessage(result ? "FAIL" : "PASS");
}
getResult();
</script>
)SWRESOURCE"_s;

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
    
    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { differentSessionsUseDifferentRegistrationsMainBytes } },
        { "/empty-worker.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, emptyString() } },
        { "/second.html"_s, { defaultPageMainBytes } },
        { "/third.html"_s, { privatePageMainBytes } },
    });

    auto defaultWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [defaultWebView synchronouslyLoadRequest:server.request("/first.html"_s)];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [defaultWebView synchronouslyLoadRequest:server.request("/second.html"_s)];

    TestWebKitAPI::Util::run(&done);
    done = false;

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [ephemeralWebView synchronouslyLoadRequest:server.request("/third.html"_s)];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

static constexpr auto regularPageGrabbingCacheStorageDirectory = R"SWRESOURCE(
<script>
async function getResult()
{
    var result = await window.internals.cacheStorageEngineRepresentation();
    window.webkit.messageHandlers.sw.postMessage(result);
}
getResult();
</script>
)SWRESOURCE"_s;

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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
        { "/second.html"_s, { regularPageGrabbingCacheStorageDirectory } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request("/first.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
    while (![[configuration websiteDataStore] _hasRegisteredServiceWorker])
        TestWebKitAPI::Util::spinRunLoop();

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request("/second.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_TRUE(retrievedString.contains("/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/CacheStorage"_s));

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
    NSString* tempDirectory = @"/var/tmp";
    [dataStoreConfiguration _setServiceWorkerRegistrationDirectory:[NSURL fileURLWithPath:tempDirectory]];
    [dataStoreConfiguration _setCacheStorageDirectory:[NSURL fileURLWithPath:tempDirectory]];
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr<DirectoryPageMessageHandler> directoryPageMessageHandler = adoptNS([[DirectoryPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:directoryPageMessageHandler.get() name:@"sw"];

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
        { "/second.html"_s, { regularPageGrabbingCacheStorageDirectory } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request("/first.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
    while (![websiteDataStore _hasRegisteredServiceWorker])
        TestWebKitAPI::Util::spinRunLoop();

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request("/second.html"_s)];
    TestWebKitAPI::Util::run(&done);
    done = false;
    NSString* tempDirectoryInUse = FileSystem::realPath(tempDirectory);
    String expectedString = [NSString stringWithFormat:@"\"path\": \"%@\"", tempDirectoryInUse];
    EXPECT_TRUE(retrievedString.contains(expectedString));

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

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytesForSessionIDTest } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytesForSessionIDTest } },
    });

    expectedMessage = "PASS: activation successful"_s;
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
        TestWebKitAPI::Util::runFor(0.1_s);
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

    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });
    TestWebKitAPI::HTTPServer server2({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
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

    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });
    TestWebKitAPI::HTTPServer server2({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
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
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, scriptBytes } }
    });

    auto *processPool = configuration.get().processPool;
    [processPool _setUseSeparateServiceWorkerProcess: useSeparateServiceWorkerProcess];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigationBoolean = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    if (loadAboutBlankBeforePage) {
        didFinishNavigationBoolean = false;
        [webView loadRequest: [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
        TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
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

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });

    auto *processPool = configuration.get().processPool;
    [processPool _setUseSeparateServiceWorkerProcess:(useSeparateServiceWorkerProcess == UseSeparateServiceWorkerProcess::Yes)];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadRequest:server.request()];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] { return [processPool _serviceWorkerProcessCount] == 1; }));

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server.request()];
    [webView2 _test_waitForDidFinishNavigation];

    auto webViewToUpdate = useSeparateServiceWorkerProcess == UseSeparateServiceWorkerProcess::Yes ? webView : webView2;

    [webViewToUpdate _setThrottleStateForTesting:1];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        return ![webViewToUpdate _hasServiceWorkerForegroundActivityForTesting] && [webViewToUpdate _hasServiceWorkerBackgroundActivityForTesting];
    }));

    [webViewToUpdate _setThrottleStateForTesting:2];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        return [webViewToUpdate _hasServiceWorkerForegroundActivityForTesting] && ![webViewToUpdate _hasServiceWorkerBackgroundActivityForTesting];
    }));

    [webViewToUpdate _setThrottleStateForTesting:0];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        return ![webViewToUpdate _hasServiceWorkerForegroundActivityForTesting] && ![webViewToUpdate _hasServiceWorkerBackgroundActivityForTesting];
    }));

    [webView _close];
    webView = nullptr;

    // The service worker process should take activity based on webView2 process.
    [webView2 _setThrottleStateForTesting: 1];
    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        [webView2 _setThrottleStateForTesting:1];
        return ![webView2 _hasServiceWorkerForegroundActivityForTesting] && [webView2 _hasServiceWorkerBackgroundActivityForTesting];
    }));

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        [webView2 _setThrottleStateForTesting:2];
        return [webView2 _hasServiceWorkerForegroundActivityForTesting] && ![webView2 _hasServiceWorkerBackgroundActivityForTesting];
    }));

    EXPECT_TRUE(waitUntilEvaluatesToTrue([&] {
        [webView2 _setThrottleStateForTesting:0];
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
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, scriptBytes } },
    });

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigationBoolean = true;
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

    didFinishNavigationBoolean = false;
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
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

    didFinishNavigationBoolean = false;
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
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

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { mainRegisteringWorkerBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
        { "/second.html"_s, { mainRegisteringAlreadyExistingWorkerBytes } },
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

        [webView loadRequest:server.request("/first.html"_s)];

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

        [webView loadRequest:server.request("/second.html"_s)];

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

    TestWebKitAPI::HTTPServer server({
        { "/first.html"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
        { "/second.html"_s, { mainBytes } },
    });

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestSWAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    done = false;

    // Normal load to get SW registered.
    [webView loadRequest:server.request("/first.html"_s)];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto store = [configuration websiteDataStore];
    auto path = store._configuration._serviceWorkerRegistrationDirectory.path;

    NSURL* directory = [NSURL fileURLWithPath:path isDirectory:YES];
    NSURL *swDBPath = [directory URLByAppendingPathComponent:serviceWorkerRegistrationFilename];

    unsigned timeout = 0;
    while (![[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path] && ++timeout < 100)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [webView.get().configuration.websiteDataStore _sendNetworkProcessWillSuspendImminently];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [webView.get().configuration.websiteDataStore _sendNetworkProcessDidResume];

    [webView loadRequest:server.request("/second.html"_s)];
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
    [[NSFileManager defaultManager] copyItemAtURL:url1 toURL:[swPath URLByAppendingPathComponent:serviceWorkerRegistrationFilename] error:nil];

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

    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });

    WKProcessPool *processPool = configuration.get().processPool;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 loadRequest:server1.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];

    TestWebKitAPI::HTTPServer server2({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server2.requestWithLocalhost()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(2U, processPool._serviceWorkerProcessCount);
}

static constexpr auto contentRuleListWorkerScript =
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
"});"_s;

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
            connection.send(HTTPResponse({ { "Content-Type"_s, "text/html"_s } }, mainBytes).serialize(), [=] {
                connection.receiveHTTPRequest([=](Vector<char>&&) {
                    connection.send(HTTPResponse({ { "Content-Type"_s, "application/javascript"_s } }, contentRuleListWorkerScript).serialize(), [=] {
                        connection.receiveHTTPRequest([=](Vector<char>&& lastRequest) {
                            EXPECT_TRUE(strnstr((const char*)lastRequest.data(), "allowedsubresource", lastRequest.size()));
                            connection.send(HTTPResponse("successful fetch"_s).serialize());
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

    static constexpr auto main =
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
    "</script>"_s;
    
    ASCIILiteral js = ""_s;
    const char* expectedAlert = nullptr;
    size_t expectedServerRequests1 = 0;
    size_t expectedServerRequests2 = 0;

    switch (responseType) {
    case ResponseType::Synthetic:
        js = "self.addEventListener('fetch', (event) => { event.respondWith(new Response(new Blob(['<script>alert(\"synthetic response\")</script>'], {type: 'text/html'}))); })"_s;
        expectedAlert = "synthetic response";
        expectedServerRequests1 = 2;
        expectedServerRequests2 = 2;
        break;
    case ResponseType::Cached:
        js = "self.addEventListener('install', (event) => { event.waitUntil( caches.open('v1').then((cache) => { return cache.addAll(['/cached.html']); }) ); });"
            "self.addEventListener('fetch', (event) => { event.respondWith(caches.match('/cached.html')) });"_s;
        expectedAlert = "loaded from cache";
        expectedServerRequests1 = 3;
        expectedServerRequests2 = 3;
        break;
    case ResponseType::Fetched:
        js = "self.addEventListener('fetch', (event) => { event.respondWith(fetch('/fetched.html')) });"_s;
        expectedAlert = "fetched from server";
        expectedServerRequests1 = 2;
        expectedServerRequests2 = 3;
        break;
    }

    HTTPServer server({
        { "/"_s, { main } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } },
        { "/cached.html"_s, { "<script>alert('loaded from cache')</script>"_s } },
        { "/fetched.html"_s, { "<script>alert('fetched from server')</script>"_s } },
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

    static constexpr auto main =
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
    "</script>"_s;
    static constexpr auto js = ""_s;

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
            { "/"_s, { main } },
            { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } }
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
            { "/"_s, { main } },
            { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, js } }
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

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
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

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, scriptBytes } },
    });

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
    done = false;

    unsigned timeout = 0;
    while (![[NSFileManager defaultManager] fileExistsAtPath:[swPath URLByAppendingPathComponent:serviceWorkerRegistrationFilename].path] && ++timeout < 20)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:[swPath URLByAppendingPathComponent:serviceWorkerRegistrationFilename].path]);

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

    static constexpr auto main =
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
    "</script>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { main } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, emptyString() } }
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
static bool didStartURLSchemeTaskForXMLFile = false;
static bool didStartURLSchemeTaskForImportedScript = false;

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
    _dataMappings.set(urlString, [NSData dataWithBytes:(void*)data length:strlen(data)]);
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if ([(id<WKURLSchemeTaskPrivate>)task _requestOnlyIfCached]) {
        [task didFailWithError:[NSError errorWithDomain:@"TestWebKitAPI" code:1 userInfo:nil]];
        return;
    }

    NSURL *finalURL = task.request.URL;

    if (URL(finalURL).string().endsWith("importedScript.js"_s))
        didStartURLSchemeTaskForImportedScript = true;

    if (URL(finalURL).string().endsWith(".xml"_s))
        didStartURLSchemeTaskForXMLFile = true;

    NSMutableDictionary* headerDictionary = [NSMutableDictionary dictionary];
    if (URL(finalURL).string().endsWith(".js"_s))
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
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/sw.js" toData:"importScripts('sw-ext://ABC/importedScript.js');"];
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/bar.xml" toData:"bar"];
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/importedScript.js" toData:"fetch('sw-ext://ABC/bar.xml');"];

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
    didStartURLSchemeTaskForXMLFile = false;
    didStartURLSchemeTaskForImportedScript = false;
    [webView _loadServiceWorker:[NSURL URLWithString:@"sw-ext://ABC/sw.js"] usingModules:NO completionHandler:^(BOOL success) {
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

    TestWebKitAPI::Util::runFor(0.5_s);

    EXPECT_EQ(webViewConfiguration.processPool._serviceWorkerProcessCount, 1U);
    EXPECT_EQ(webViewConfiguration.processPool._webProcessCountIgnoringPrewarmedAndCached, 1U);

    EXPECT_WK_STREQ([webView URL].absoluteString, @"sw-ext://ABC");

    TestWebKitAPI::Util::run(&didStartURLSchemeTaskForImportedScript);
    TestWebKitAPI::Util::run(&didStartURLSchemeTaskForXMLFile);

    // The service worker should exit if we close/deallocate the view we used to launch it.
    [webView _close];
    webView = nil;
    while (webViewConfiguration.processPool._serviceWorkerProcessCount)
        TestWebKitAPI::Util::spinRunLoop(10);
}

TEST(ServiceWorker, ExtensionServiceWorkerDisableCORS)
{
    using namespace TestWebKitAPI;

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    Util::run(&done);
    done = false;

    bool madeHTTPGetRequest = false;
    bool madeHTTPOptionsRequest = false;
    String filenameRequestedOverHTTP;
    HTTPServer server([&] (Connection connection) {
        connection.receiveHTTPRequest([&, connection](Vector<char>&& bytes) mutable {
            String requestString(bytes.data(), bytes.size());
            if (requestString.startsWithIgnoringASCIICase("OPTIONS"_s)) {
                madeHTTPOptionsRequest = true;
                connection.send(
                    "HTTP/1.1 204 No Content\r\n"
                    "Allow: OPTIONS, GET, HEAD, POST\r\n\r\n"_s
                );
                return;
            }
            if (requestString.startsWithIgnoringASCIICase("GET"_s)) {
                auto requestParts = requestString.split(' ');
                if (requestParts.size() > 2)
                    filenameRequestedOverHTTP = requestParts[1];
                madeHTTPGetRequest = true;
                done = true;
            }
        });
    });

    auto testJS = makeString("fetch('http://127.0.0.1:"_s, server.port(), "/bar.xml', { headers: { 'Custom-Header': 'CustomHeaderValue' } });"_s);

    auto schemeHandler = adoptNS([ServiceWorkerSchemeHandler new]);
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/sw.js" toData:testJS.utf8().data()];

    WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];
    [webViewConfiguration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"sw-ext"];
    webViewConfiguration._corsDisablingPatterns = @[@"*://*/*"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);
    // The service worker script should get loaded over the custom scheme handler.
    done = false;
    didStartURLSchemeTask = false;
    [webView _loadServiceWorker:[NSURL URLWithString:@"sw-ext://ABC/sw.js"] usingModules:NO completionHandler:^(BOOL success) {
        EXPECT_TRUE(success);
        EXPECT_TRUE(didStartURLSchemeTask);
        done = true;
    }];
    Util::run(&done);

    // It should load bar.xml.
    Util::run(&madeHTTPGetRequest);
    EXPECT_STREQ(filenameRequestedOverHTTP.utf8().data(), "/bar.xml");

    // It shouldn't have done a CORS preflight.
    EXPECT_FALSE(madeHTTPOptionsRequest);
}

TEST(ServiceWorker, ExtensionServiceWorkerWithModules)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto schemeHandler = adoptNS([ServiceWorkerSchemeHandler new]);

    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/exports.js" toData:"const x = 805; export { x };"];
    [schemeHandler addMappingFromURLString:@"sw-ext://ABC/sw.js" toData:"import { x } from './exports.js'; x;"];

    WKWebViewConfiguration *webViewConfiguration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ServiceWorkerPagePlugIn"];
    webViewConfiguration.websiteDataStore = [adoptNS([WKWebViewConfiguration new]) websiteDataStore];
    [webViewConfiguration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"sw-ext"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration]);

    auto object = adoptNS([[ServiceWorkerPageRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ServiceWorkerPageProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    // The service worker script should get loaded over the custom scheme handler.
    done = false;
    didStartURLSchemeTask = false;
    [webView _loadServiceWorker:[NSURL URLWithString:@"sw-ext://ABC/sw.js"] usingModules:YES completionHandler:^(BOOL success) {
        EXPECT_TRUE(success);
        EXPECT_TRUE(didStartURLSchemeTask);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
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
    [webView _loadServiceWorker:[NSURL URLWithString:@"sw-ext://ABC/bad-sw.js"] usingModules:NO completionHandler:^(BOOL success) {
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
    [webView _loadServiceWorker:[NSURL URLWithString:@"https://ABC/does-not-exist.js"] usingModules:NO completionHandler:^(BOOL success) {
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

    [webView _loadServiceWorker:[NSURL URLWithString:@"https://ABC/does-not-exist.js"] usingModules:NO completionHandler:^(BOOL success) {
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

        [webView _loadServiceWorker:[NSURL URLWithString:@"https://ABC/does-not-exist.js"] usingModules:NO completionHandler:^(BOOL success) {
            EXPECT_FALSE(success);
            done = true;
        }];
    }
    TestWebKitAPI::Util::run(&done);
}

static constexpr auto ServiceWorkerWindowClientFocusMain =
"<div>test page</div>"
"<script>"
"let worker;"
"async function test() {"
"    try {"
"        const registration = await navigator.serviceWorker.register('/sw.js');"
"        if (registration.active) {"
"            worker = registration.active;"
"            alert('already active');"
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
""
"function focusClient() {"
"    worker.postMessage('start');"
"    navigator.serviceWorker.onmessage = (event) => {"
"        window.webkit.messageHandlers.sw.postMessage(event.data);"
"    }"
"}"
""
"function checkFocusValue(value, name) {"
"    window.webkit.messageHandlers.sw.postMessage(document.hasFocus() === value ? 'PASS' : 'FAIL: expected ' + value + ' for ' + name);"
"}"
"</script>"_s;
static constexpr auto ServiceWorkerWindowClientFocusJS =
"self.addEventListener('message', (event) => {"
"   event.source.focus().then((client) => {"
"       event.source.postMessage('focused');"
"   }, (error) => {"
"       event.source.postMessage('not focused');"
"   });"
"});"_s;

#if PLATFORM(MAC)
void miniaturizeWebView(TestWKWebView* webView)
{
    [[webView hostWindow] miniaturize:[webView hostWindow]];

    int cptr = 0;
    while ([webView hostWindow].isVisible && ++cptr < 1000)
        TestWebKitAPI::Util::spinRunLoop(10);
}
#endif // PLATFORM(MAC)

TEST(ServiceWorker, ServiceWorkerWindowClientFocus)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [configuration preferences];

    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"ServiceWorkersUserGestureEnabled"])
            [preferences _setEnabled:NO forInternalDebugFeature:feature];
    }

    auto messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { ServiceWorkerWindowClientFocusMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientFocusJS } }
    });

    [webView1 loadRequest:server.request()];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "successfully registered");

    [webView2 loadRequest:server.request()];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "already active");

#if PLATFORM(MAC)
    miniaturizeWebView(webView1.get());
    EXPECT_FALSE([webView1 hostWindow].isVisible);

    miniaturizeWebView(webView2.get());
    EXPECT_FALSE([webView2 hostWindow].isVisible);
#endif

    done = false;
    expectedMessage = "focused"_s;
    [webView1 evaluateJavaScript:@"focusClient()" completionHandler: nil];
    TestWebKitAPI::Util::run(&done);
#if PLATFORM(MAC)
    EXPECT_TRUE([webView1 hostWindow].isVisible);
    EXPECT_FALSE([webView2 hostWindow].isVisible);
    EXPECT_FALSE([webView1 hostWindow].isMiniaturized);
    EXPECT_TRUE([webView2 hostWindow].isMiniaturized);

    // FIXME: We should be able to run these tests in iOS once pages are actually visible.
    done = false;
    expectedMessage = "PASS"_s;
    [webView1 evaluateJavaScript:@"checkFocusValue(true, 'webView1')" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);

    done = false;
    expectedMessage = "PASS"_s;
    [webView2 evaluateJavaScript:@"checkFocusValue(false, 'webView2')" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
#endif

    done = false;
    expectedMessage = "focused"_s;
    [webView2 evaluateJavaScript:@"focusClient()" completionHandler: nil];
    TestWebKitAPI::Util::run(&done);
#if PLATFORM(MAC)
    EXPECT_TRUE([webView2 hostWindow].isVisible);
    EXPECT_FALSE([webView1 hostWindow].isMiniaturized);
    EXPECT_FALSE([webView2 hostWindow].isMiniaturized);

    // FIXME: We should be able to run these tests in iOS once pages are actually visible.
    done = false;
    expectedMessage = "PASS"_s;
    [webView2 evaluateJavaScript:@"checkFocusValue(true, 'webView2')" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
#endif
}

TEST(ServiceWorker, ServiceWorkerWindowClientFocusRequiresUserGesture)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [configuration preferences];

    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"ServiceWorkersUserGestureEnabled"])
            [preferences _setEnabled:YES forInternalDebugFeature:feature];
    }

    auto messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { ServiceWorkerWindowClientFocusMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientFocusJS } }
    });

    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully registered");

    done = false;
    expectedMessage = "not focused"_s;
    [webView evaluateJavaScript:@"focusClient()" completionHandler: nil];
    TestWebKitAPI::Util::run(&done);
}

static constexpr auto ServiceWorkerWindowClientOpenWindowMain =
"<div id='log'>test page</div>"
"<script>"
"let worker;"
"async function test() {"
"    log.innerHTML = 'test';"
"    try {"
"        const registration = await navigator.serviceWorker.register('/sw.js');"
"        log.innerHTML = 'registered';"
"        if (registration.active) {"
"            worker = registration.active;"
"            alert('already active');"
"            return;"
"        }"
"        worker = registration.installing;"
"        worker.addEventListener('statechange', () => {"
"           log.innerHTML = 'worker.state=' + worker.state;"
"            if (worker.state == 'activated')"
"                alert('successfully registered');"
"        });"
"    } catch(e) {"
"        alert('Exception: ' + e);"
"    }"
"}"
"window.onload = test;"
""
"function openWindowClient() {"
"    log.innerHTML = 'openWindowClient';"
"    worker.postMessage('start');"
"    navigator.serviceWorker.onmessage = (event) => {"
"        window.webkit.messageHandlers.sw.postMessage(event.data);"
"    }"
"}"
"</script>"_s;
static constexpr auto ServiceWorkerWindowClientOpenWindowJS =
"self.addEventListener('message', (event) => {"
"   self.clients.openWindow('/sw.js').then((client) => {"
"       event.source.postMessage(client ? 'opened with client' : 'opened without client');"
"   }, (error) => {"
"       event.source.postMessage('not opened');"
"   });"
"});"_s;

TEST(ServiceWorker, openWindowWithoutDelegate)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [configuration preferences];

    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"ServiceWorkersUserGestureEnabled"])
            [preferences _setEnabled:NO forInternalDebugFeature:feature];
    }

    auto messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { ServiceWorkerWindowClientOpenWindowMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientOpenWindowJS } }
    });

    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully registered");

    done = false;
    expectedMessage = "opened without client"_s;
    [webView evaluateJavaScript:@"openWindowClient()" completionHandler: nil];
    TestWebKitAPI::Util::run(&done);
}

static constexpr auto ServiceWorkerWindowClientNavigateMain =
"<div>test page</div>"
"<script>"
"let worker;"
"async function registerServiceWorker() {"
"    try {"
"        const registration = await navigator.serviceWorker.register('/sw.js');"
"        if (registration.active) {"
"            worker = registration.active;"
"            alert('already active');"
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
"window.onload = registerServiceWorker;"
""
"function navigateOtherClientToURL(url) {"
"    worker.postMessage({navigateOtherClientToURL: url});"
"    navigator.serviceWorker.onmessage = (event) => {"
"        alert(event.data);"
"    };"
"}"
""
"function countServiceWorkerClients() {"
"    worker.postMessage('countServiceWorkerClients');"
"    navigator.serviceWorker.onmessage = (event) => {"
"        alert(event.data);"
"    };"
"}"
""
"function openWindowToURL(url) {"
"    worker.postMessage({openWindowToURL: url});"
"    navigator.serviceWorker.onmessage = (event) => {"
"        alert(event.data);"
"    };"
"}"
"</script>"_s;

static constexpr auto ServiceWorkerWindowClientNavigateJS =
"self.addEventListener('message', async (event) => {"
"   if (event.data && event.data.navigateOtherClientToURL) {"
"       let otherClient;"
"       let currentClients = await self.clients.matchAll();"
"       for (let client of currentClients) {"
"           if (client.id !== event.source.id)"
"               otherClient = client;"
"       }"
"       if (!otherClient) {"
"           event.source.postMessage('failed, no other client, client number = ' + currentClients.length);"
"           return;"
"       }"
"       await otherClient.navigate(event.data.navigateOtherClientToURL).then((client) => {"
"           event.source.postMessage(client ? 'client' : 'none');"
"       }, (e) => {"
"           event.source.postMessage('failed');"
"       });"
"       return;"
"   }"
"   if (event.data === 'countServiceWorkerClients') {"
"       let currentClients = await self.clients.matchAll();"
"       event.source.postMessage(currentClients.length + ' client(s)');"
"       return;"
"   }"
"   if (event.data && event.data.openWindowToURL) {"
"       await self.clients.openWindow(event.data.openWindowToURL).then((client) => {"
"           event.source.postMessage(client ? 'client' : 'none');"
"       }, (e) => {"
"           event.source.postMessage('failed');"
"       });"
"       return;"
"   }"
"});"_s;

static bool shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
static bool shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
@interface ServiceWorkerPSONNavigationDelegate : NSObject <WKNavigationDelegatePrivate> {
    @public void (^decidePolicyForNavigationAction)(WKNavigationAction *, void (^)(WKNavigationActionPolicy));
    @public void (^didStartProvisionalNavigationHandler)();
    @public void (^didCommitNavigationHandler)();
}
@end

@implementation ServiceWorkerPSONNavigationDelegate

- (instancetype) init
{
    self = [super init];
    return self;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(shouldServiceWorkerPSONNavigationDelegateAllowNavigation ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse ? WKNavigationResponsePolicyAllow : WKNavigationResponsePolicyCancel);
}

@end

TEST(ServiceWorker, WindowClientNavigate)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"CrossOriginOpenerPolicyEnabled"])
            [[configuration preferences] _setEnabled:YES forExperimentalFeature:feature];
        else if ([feature.key isEqualToString:@"CrossOriginEmbedderPolicyEnabled"])
            [[configuration preferences] _setEnabled:YES forExperimentalFeature:feature];
    }

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/?test"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/?fail1"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/?fail2"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/?swap"_s, { {{ "Content-Type"_s, "application/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, ServiceWorkerWindowClientNavigateMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientNavigateJS } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 8091);

    [webView1 loadRequest:server.request()];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "successfully registered");
    [webView2 loadRequest:server.request()];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "already active");

    auto navigationDelegate = adoptNS([[ServiceWorkerPSONNavigationDelegate alloc] init]);
    [webView2 setNavigationDelegate:navigationDelegate.get()];

    auto *baseURL = [[server.request() URL] absoluteString];

    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@')", baseURL] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "client");

    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@#test')", baseURL] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "client");

    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@?test')", baseURL] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "client");

    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@?swap')", baseURL] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "client");

    [webView1 evaluateJavaScript:@"countServiceWorkerClients()" completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "1 client(s)");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = false;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@?fail1')", baseURL] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "none");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = false;
    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@?fail2')", baseURL] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "none");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
}

TEST(ServiceWorker, WindowClientNavigateCrossOrigin)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server1({
        { "/"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/?test"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/?swap"_s, { {{ "Content-Type"_s, "application/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, ServiceWorkerWindowClientNavigateMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientNavigateJS } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 8091);

    TestWebKitAPI::HTTPServer server2({
        { "/"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientNavigateJS } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 9091);

    [webView1 loadRequest:server1.request()];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "successfully registered");
    [webView2 loadRequest:server1.request()];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "already active");
    [webView1 evaluateJavaScript:[NSString stringWithFormat:@"navigateOtherClientToURL('%@')", [[server2.request() URL] absoluteString]] completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "none");
}

@interface ServiceWorkerOpenWindowWebsiteDataStoreDelegate: NSObject <_WKWebsiteDataStoreDelegate> {
@private
    WKWebViewConfiguration* _configuration;
    RetainPtr<ServiceWorkerPSONNavigationDelegate> _navigationDelegate;
    RetainPtr<TestWKWebView> _webView;
}
- (instancetype)initWithConfiguration:(WKWebViewConfiguration*)configuration;
@end

@implementation ServiceWorkerOpenWindowWebsiteDataStoreDelegate { }
- (instancetype)initWithConfiguration:(WKWebViewConfiguration*)configuration
{
    _configuration = configuration;
    return self;
}

- (void)websiteDataStore:(WKWebsiteDataStore *)dataStore openWindow:(NSURL *)url fromServiceWorkerOrigin:(WKSecurityOrigin *)serviceWorkerOrigin completionHandler:(void (^)(WKWebView *newWebView))completionHandler
{
    _navigationDelegate = adoptNS([[ServiceWorkerPSONNavigationDelegate alloc] init]);
    _webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:_configuration addToWindow:YES]);
    [_webView setNavigationDelegate:_navigationDelegate.get()];

    [_webView loadRequest:[NSURLRequest requestWithURL:url]];
    completionHandler(_webView.get());
}

@end

TEST(ServiceWorker, OpenWindowWebsiteDataStoreDelegate)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto preferences = [configuration preferences];
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"ServiceWorkersUserGestureEnabled"])
            [preferences _setEnabled:NO forInternalDebugFeature:feature];
    }

    auto dataStoreDelegate = adoptNS([[ServiceWorkerOpenWindowWebsiteDataStoreDelegate alloc] initWithConfiguration:configuration.get()]);
    [[configuration websiteDataStore] set_delegate:dataStoreDelegate.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientNavigateJS } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 8091);

    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully registered");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
    [webView evaluateJavaScript:[NSString stringWithFormat:@"openWindowToURL('%@')", [[server.request() URL] absoluteString]] completionHandler: nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "client");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = false;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
    [webView evaluateJavaScript:[NSString stringWithFormat:@"openWindowToURL('%@')", [[server.request() URL] absoluteString]] completionHandler: nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "none");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = false;
    [webView evaluateJavaScript:[NSString stringWithFormat:@"openWindowToURL('%@')", [[server.request() URL] absoluteString]] completionHandler: nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "none");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
}

TEST(ServiceWorker, OpenWindowCOOP)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto preferences = [configuration preferences];
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"ServiceWorkersUserGestureEnabled"])
            [preferences _setEnabled:NO forInternalDebugFeature:feature];
    }

    auto dataStoreDelegate = adoptNS([[ServiceWorkerOpenWindowWebsiteDataStoreDelegate alloc] initWithConfiguration:configuration.get()]);
    [[configuration websiteDataStore] set_delegate:dataStoreDelegate.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { ServiceWorkerWindowClientNavigateMain } },
        { "/sw.js"_s, { {{ "Content-Type"_s, "application/javascript"_s }}, ServiceWorkerWindowClientNavigateJS } },
        { "/?swap"_s, { {{ "Content-Type"_s, "application/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, ServiceWorkerWindowClientNavigateMain } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http, nullptr, nullptr, 8091);

    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully registered");

    shouldServiceWorkerPSONNavigationDelegateAllowNavigation = true;
    shouldServiceWorkerPSONNavigationDelegateAllowNavigationResponse = true;
    [webView evaluateJavaScript:[NSString stringWithFormat:@"openWindowToURL('%@?swap')", [[server.request() URL] absoluteString]] completionHandler: nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "client");
}

#if WK_HAVE_C_SPI

static constexpr auto serviceWorkerStorageTimingMainBytes = R"SWRESOURCE(
<script>
function log(msg)
{
    window.webkit.messageHandlers.sw.postMessage(msg);
}

navigator.serviceWorker.addEventListener("message", function(event) {
    log("Message from worker: " + event.data);
});

let registration;
try {
navigator.serviceWorker.register('/sw.js').then(function(reg) {
    registration = reg;
    worker = reg.installing ? reg.installing : reg.active;
    worker.postMessage("Hello from the web page");
}).catch(function(error) {
    log("Registration failed with: " + error);
});
} catch(e) {
    log("Exception: " + e);
}

function storeRegistration()
{
    if (!window.internals) {
        alert("no internals");
        return;
    }
    internals.storeRegistrationsOnDisk().then(() => {
        alert("ok");
    }, () => {
        alert("ko");
    });
}

function waitForWaitingWorker(counter)
{
    try {
        if (registration.waiting) {
            alert("ok");
            return;
        }
        if (!counter)
            counter = 0;
        else if (counter > 100) {
            alert("ko");
            return;
        }
        setTimeout(() => waitForWaitingWorker(++counter), 50);
    } catch (e) {
        alert("error: " + e);
        return;
    }
}

</script>
)SWRESOURCE"_s;

static constexpr auto serviceWorkerStorageTimingScriptBytesV1 = R"SWRESOURCE(
self.addEventListener("message", (event) => {
    event.source.postMessage("V1");
});
)SWRESOURCE"_s;

static constexpr auto serviceWorkerStorageTimingScriptBytesV2 = R"SWRESOURCE(
self.addEventListener("message", (event) => {
    event.source.postMessage("V2");
});
)SWRESOURCE"_s;

TEST(ServiceWorkers, ServiceWorkerStorageTiming)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Cache-Control"_s, "no-cache"_s } }, serviceWorkerStorageTimingMainBytes } },
        { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, serviceWorkerStorageTimingScriptBytesV1 } },
    });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    done = false;
    expectedMessage = "Message from worker: V1"_s;
    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow: YES]);
    [webView1 loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);

    HashMap<String, String> sourceHeaders;
    sourceHeaders.add("Cache-Control"_s, "no-cache"_s);
    sourceHeaders.add("Content-Type"_s, "application/javascript"_s);
    server.setResponse("/sw.js"_s, TestWebKitAPI::HTTPResponse { WTFMove(sourceHeaders), serviceWorkerStorageTimingScriptBytesV2 });

    done = false;
    expectedMessage = "Message from worker: V1"_s;
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);

    // Let's wait for a V2 service worker.
    [webView1 evaluateJavaScript:@"waitForWaitingWorker()" completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "ok");

    // Let's ensure we store it on disk.
    [webView1 evaluateJavaScript:@"storeRegistration()" completionHandler: nil];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "ok");

    [[webView1 configuration].websiteDataStore _terminateNetworkProcess];

    done = false;
    expectedMessage = "Message from worker: V2"_s;
    auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView3 loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
}

#endif // WK_HAVE_C_SPI
