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

#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsitePolicies.h>
#import <wtf/Deque.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

struct ResourceInfo {
    RetainPtr<NSString> mimeType;
    const char* data;
};

static bool done;
static bool didFinishNavigation;

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
    EXPECT_TRUE([[message body] isEqualToString:_expectedMessage]);
    done = true;
}
@end

@interface SWSchemes : NSObject <WKURLSchemeHandler> {
@public
    HashMap<String, ResourceInfo> resources;
    NSString *expectedUserAgent;
}

-(size_t)handledRequests;
@end

@implementation SWSchemes {
    size_t _handledRequests;
}

-(size_t)handledRequests
{
    return _handledRequests;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    if (expectedUserAgent)
        EXPECT_WK_STREQ(expectedUserAgent, [[task.request valueForHTTPHeaderField:@"User-Agent"] UTF8String]);

    auto entry = resources.find([task.request.URL absoluteString]);
    if (entry == resources.end()) {
        NSLog(@"Did not find resource entry for URL %@", task.request.URL);
        return;
    }

    ++_handledRequests;
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:entry->value.mimeType.get() expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];

    [task didReceiveData:[NSData dataWithBytesNoCopy:(void*)entry->value.data length:strlen(entry->value.data) freeWhenDone:NO]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
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

static const char* testBytes = R"SWRESOURCE(
<body>
NOT intercepted by worker
</body>
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

static WKWebsiteDataStore *dataStoreWithRegisteredServiceWorkerScheme(NSString *scheme)
{
    _WKWebsiteDataStoreConfiguration *configuration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    [configuration registerURLSchemeServiceWorkersCanHandleForTesting:scheme];
    return [[[WKWebsiteDataStore alloc] _initWithConfiguration:configuration] autorelease];
}

TEST(ServiceWorkers, Basic)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = nullptr;

    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_TRUE([websiteDataRecords[0].displayName isEqualToString:@"sw host"]);

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, BasicDefaultSession)
{
    using namespace TestWebKitAPI;
    TCPServer server([] (int socket) {
        NSString *format = @"HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n\r\n"
        "%s";
        NSString *firstResponse = [NSString stringWithFormat:format, "text/html", strlen(mainBytes), mainBytes];
        NSString *secondResponse = [NSString stringWithFormat:format, "application/javascript", strlen(scriptBytes), scriptBytes];

        TCPServer::read(socket);
        TCPServer::write(socket, firstResponse.UTF8String, firstResponse.length);
        TCPServer::read(socket);
        TCPServer::write(socket, secondResponse.UTF8String, secondResponse.length);
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

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

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

- (void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction userInfo:(id <NSSecureCoding>)userInfo decisionHandler:(void (^)(WKNavigationActionPolicy, _WKWebsitePolicies *))decisionHandler
{
    _WKWebsitePolicies *websitePolicies = [[[_WKWebsitePolicies alloc] init] autorelease];
    if (navigationAction.targetFrame.mainFrame)
        [websitePolicies setCustomUserAgent:_userAgent];

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

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[SWUserAgentMessageHandler alloc] initWithExpectedMessage:@"Message from worker: Foo Custom UserAgent"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", userAgentSWBytes });
    handler->expectedUserAgent = @"Foo Custom UserAgent";
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[SWCustomUserAgentDelegate alloc] initWithUserAgent:@"Foo Custom UserAgent"]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // Restore from disk.
    webView = nullptr;
    delegate = nullptr;
    handler = nullptr;
    messageHandler = nullptr;
    configuration = nullptr;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    messageHandler = adoptNS([[SWUserAgentMessageHandler alloc] initWithExpectedMessage:@"Message from worker: Bar Custom UserAgent"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", userAgentSWBytes });
    handler->expectedUserAgent = @"Bar Custom UserAgent";
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    delegate = adoptNS([[SWCustomUserAgentDelegate alloc] initWithUserAgent:@"Bar Custom UserAgent"]);
    [webView setNavigationDelegate:delegate.get()];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, RestoreFromDisk)
{
    ASSERT(mainRegisteringWorkerBytes);
    ASSERT(scriptBytes);
    ASSERT(mainRegisteringAlreadyExistingWorkerBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandlerForRestoreFromDiskTest> messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration was successful and service worker was activated"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringWorkerBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;
    handler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringAlreadyExistingWorkerBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, CacheStorageRestoreFromDisk)
{
    ASSERT(mainCacheStorageBytes);
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [WKWebsiteDataStore _deleteDefaultDataStoreForTesting];

    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainCacheStorageBytes });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"No cache storage data"]);

    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Trigger creation of network process.
    [webView.get().configuration.processPool _syncNetworkProcessCookies];

    auto *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"Some cache storage data: my cache"]);

    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}


TEST(ServiceWorkers, FetchAfterRestoreFromDisk)
{
    ASSERT(mainForFetchTestBytes);
    ASSERT(scriptHandlingFetchBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandlerForFetchTest> messageHandler = adoptNS([[SWMessageHandlerForFetchTest alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFetchTestBytes });
    handler->resources.set("sw://host/test.html", ResourceInfo { @"text/html", testBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptHandlingFetchBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;
    handler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    messageHandler = adoptNS([[SWMessageHandlerForFetchTest alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFetchTestBytes });
    handler->resources.set("sw://host/test.html", ResourceInfo { @"text/html", testBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptHandlingFetchBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, InterceptFirstLoadAfterRestoreFromDisk)
{
    ASSERT(mainForFirstLoadInterceptTestBytes);
    ASSERT(scriptHandlingFetchBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Service Worker activated";
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;
    handler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    expectedMessage = "Intercepted by worker";
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, WaitForPolicyDelegate)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Register a service worker and activate it.
    expectedMessage = "Service Worker activated";
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;
    handler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Verify service worker is intercepting load.
    expectedMessage = "Intercepted by worker";
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

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
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&navigationComplete);

    EXPECT_FALSE(navigationFailed);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    shouldAccept = false;
    navigationFailed = false;
    navigationComplete = false;

    // Verify service worker load fails well when policy delegate is not ok.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&navigationComplete);

    EXPECT_TRUE(navigationFailed);
}
#if WK_HAVE_C_SPI

void setConfigurationInjectedBundlePath(WKWebViewConfiguration* configuration)
{
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.processPool = (WKProcessPool *)context.get();
    configuration.websiteDataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
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

static const char* regularPageWithoutConnectionBytes = R"SWRESOURCE(
<script>
var result = window.internals.hasServiceWorkerConnection() ? "FAIL" : "PASS";
window.webkit.messageHandlers.regularPage.postMessage(result);
</script>
)SWRESOURCE";

static const char* regularPageWithConnectionBytes = R"SWRESOURCE(
<script>
var result = window.internals.hasServiceWorkerConnection() ? "PASS" : "FAIL";
window.webkit.messageHandlers.regularPage.postMessage(result);
</script>
)SWRESOURCE";

TEST(ServiceWorkers, SWProcessConnectionCreation)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

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

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/regularPageWithoutConnection.html", ResourceInfo { @"text/html", regularPageWithoutConnectionBytes });
    handler->resources.set("sw://host/regularPageWithConnection.html", ResourceInfo { @"text/html", regularPageWithConnectionBytes });
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> regularPageWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> newRegularPageWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Test that a regular page does not trigger a service worker connection to network process if there is no registered service worker.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithoutConnection.html"]];

    [regularPageWebView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Test that a sw scheme page can register a service worker.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    webView = nullptr;

    // Now that a service worker is registered, the regular page should have a service worker connection.
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithConnection.html"]];

    [regularPageWebView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    regularPageWebView = nullptr;

    [newRegularPageWebView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    newRegularPageWebView = nullptr;

    [[configuration websiteDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_TRUE([websiteDataRecords[0].displayName isEqualToString:@"sw host"]);

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

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/regularPageWithoutConnection.html", ResourceInfo { @"text/html", regularPageWithoutConnectionBytes });
    handler->resources.set("sw://host/regularPageWithConnection.html", ResourceInfo { @"text/html", regularPageWithConnectionBytes });
    handler->resources.set("sw://host/mainWithScope.html", ResourceInfo { @"text/html", mainBytesWithScope });
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a page that registers a service worker.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/mainWithScope.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    webView = nullptr;

    // Now that a sw is registered, let's create a new configuration and try loading a regular page, there should be no service worker process created.
    RetainPtr<WKWebViewConfiguration> newConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(newConfiguration.get());
    newConfiguration.get().websiteDataStore = [configuration websiteDataStore];

    [[newConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [[newConfiguration userContentController] addScriptMessageHandler:regularPageMessageHandler.get() name:@"regularPage"];
    [newConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(1u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithConnection.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading the simple page did not start the service worker process.
    EXPECT_EQ(0u, webView.get().configuration.processPool._serviceWorkerProcessCount);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];
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

TEST(ServiceWorkers, HasServiceWorkerRegistrationBit)
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

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/regularPageWithoutConnection.html", ResourceInfo { @"text/html", regularPageWithoutConnectionBytes });
    handler->resources.set("sw://host/regularPageWithConnection.html", ResourceInfo { @"text/html", regularPageWithConnectionBytes });
    handler->resources.set("sw://host/mainWithScope.html", ResourceInfo { @"text/html", mainBytesWithScope });
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Load a page that registers a service worker.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/mainWithScope.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    webView = nullptr;

    // Now that a sw is registered, let's create a new configuration and try loading a regular page
    RetainPtr<WKWebViewConfiguration> newConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(newConfiguration.get());

    [[newConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [[newConfiguration userContentController] addScriptMessageHandler:regularPageMessageHandler.get() name:@"regularPage"];
    [newConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    newConfiguration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [newConfiguration.get().websiteDataStore _setServiceWorkerRegistrationDirectory: @"~/nonexistingfolder"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithoutConnection.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Let's use the web site data store that has service worker and load a page.
    newConfiguration.get().websiteDataStore = [configuration websiteDataStore];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithConnection.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading the simple page did not start the service worker process.
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed);

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

    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/readCache.html", ResourceInfo { @"text/html", readCacheBytes });
    handler->resources.set("sw://host/writeCache.html", ResourceInfo { @"text/html", writeCacheBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration] autorelease];
    [dataStoreConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw"];
    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration] autorelease];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/writeCache.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/readCache.html"]];
    [webView loadRequest:request];
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

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    [configuration setWebsiteDataStore:dataStore];
    
    auto defaultPreferences = [configuration preferences];
    [defaultPreferences _setSecureContextChecksEnabled:NO];
    
    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/openCache.html", ResourceInfo { @"text/html", "foo" });
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", serviceWorkerCacheAccessEphemeralSessionMainBytes });
    handler->resources.set("sw://host/serviceworker-private-browsing-worker.js", ResourceInfo { @"application/javascript", serviceWorkerCacheAccessEphemeralSessionSWBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    auto defaultWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/openCache.html"]];
    [defaultWebView synchronouslyLoadRequest:request];
    
    bool openedCache = false;
    [defaultWebView evaluateJavaScript:@"self.caches.open('test');" completionHandler: [&] (id innerText, NSError *error) {
        openedCache = true;
    }];
    TestWebKitAPI::Util::run(&openedCache);

    _WKWebsiteDataStoreConfiguration *nonPersistentConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration] autorelease];
    [nonPersistentConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw"];
    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:nonPersistentConfiguration] autorelease];

    auto messageHandler = adoptNS([[SWMessageHandlerForCacheStorage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];

    [ephemeralWebView loadRequest:request];
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
    
    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", differentSessionsUseDifferentRegistrationsMainBytes });
    handler->resources.set("sw://host/main2.html", ResourceInfo { @"text/html", defaultPageMainBytes });
    handler->resources.set("sw://host/empty-worker.js", ResourceInfo { @"application/javascript", "" });
    handler->resources.set("sw://host/private.html", ResourceInfo { @"text/html", privatePageMainBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    auto defaultWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [defaultWebView synchronouslyLoadRequest:request];
    
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main2.html"]];
    [defaultWebView synchronouslyLoadRequest:request];
    
    TestWebKitAPI::Util::run(&done);
    done = false;
    
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/private.html"]];
    [ephemeralWebView synchronouslyLoadRequest:request];

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
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
    setConfigurationInjectedBundlePath(configuration.get());

    RetainPtr<DirectoryPageMessageHandler> directoryPageMessageHandler = adoptNS([[DirectoryPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:directoryPageMessageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/regularPageGrabbingCacheStorageDirectory.html", ResourceInfo { @"text/html", regularPageGrabbingCacheStorageDirectory });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    while (![[configuration websiteDataStore] _hasRegisteredServiceWorker])
        TestWebKitAPI::Util::spinRunLoop(0.1);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageGrabbingCacheStorageDirectory.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_TRUE(retrievedString.contains("/Caches/TestWebKitAPI/WebKit/CacheStorage"));

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, ServiceWorkerAndCacheStorageSpecificDirectories)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore defaultDataStore];
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());
    auto websiteDataStore = [configuration websiteDataStore];
    [websiteDataStore _setCacheStorageDirectory:@"/var/tmp"];
    [websiteDataStore _setServiceWorkerRegistrationDirectory:@"/var/tmp"];

    RetainPtr<DirectoryPageMessageHandler> directoryPageMessageHandler = adoptNS([[DirectoryPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:directoryPageMessageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/regularPageGrabbingCacheStorageDirectory.html", ResourceInfo { @"text/html", regularPageGrabbingCacheStorageDirectory });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    while (![websiteDataStore _hasRegisteredServiceWorker])
        TestWebKitAPI::Util::spinRunLoop(0.1);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageGrabbingCacheStorageDirectory.html"]];

    [webView loadRequest:request];
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

    NSURL *serviceWorkersPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/ServiceWorkers/" stringByExpandingTildeInPath] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:serviceWorkersPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:serviceWorkersPath.path]);
    NSURL *idbPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/IndexedDB/" stringByExpandingTildeInPath] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:idbPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:idbPath.path]);

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<_WKWebsiteDataStoreConfiguration> websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = serviceWorkersPath;
    websiteDataStoreConfiguration.get()._indexedDBDatabaseDirectory = idbPath;
    [websiteDataStoreConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw"];

    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytesForSessionIDTest });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytesForSessionIDTest });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    expectedMessage = "PASS: activation successful";
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView _close];
    webView = nullptr;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:serviceWorkersPath.path]);

    [configuration.get().websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_TRUE([websiteDataRecords[0].displayName isEqualToString:@"sw host"]);

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
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Normally, service workers get terminated several seconds after their clients are gone.
    // Disable this delay for the purpose of testing.
    _WKWebsiteDataStoreConfiguration *dataStoreConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] init] autorelease];
    dataStoreConfiguration.serviceWorkerProcessTerminationDelayEnabled = NO;
    [dataStoreConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw1"];
    [dataStoreConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw2"];

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

    RetainPtr<SWSchemes> handler1 = adoptNS([[SWSchemes alloc] init]);
    handler1->resources.set("sw1://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    handler1->resources.set("sw1://host2/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host2/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler1.get() forURLScheme:@"sw1"];

    RetainPtr<SWSchemes> handler2 = adoptNS([[SWSchemes alloc] init]);
    handler2->resources.set("sw2://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler2->resources.set("sw2://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler2.get() forURLScheme:@"sw2"];

    WKProcessPool *processPool = configuration.get().processPool;

    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request1 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw1://host/main.html"]];
    [webView1 loadRequest:request1];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:request1];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    RetainPtr<WKWebView> webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request2 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw2://host/main.html"]];
    [webView3 loadRequest:request2];

    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    RetainPtr<WKWebView> webView4 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request3 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw1://host2/main.html"]];
    [webView4 loadRequest:request3];

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

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw1");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto handler1 = adoptNS([[SWSchemes alloc] init]);
    handler1->resources.set("sw1://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    handler1->resources.set("sw1://host2/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host2/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler1.get() forURLScheme:@"sw1"];

    auto *processPool = configuration.get().processPool;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request1 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw1://host/main.html"]];

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request2 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw1://host2/main.html"]];

    [webView1 loadRequest:request1];
    [webView2 loadRequest:request2];

    waitUntilServiceWorkerProcessCount(processPool, 2);
}

static size_t launchServiceWorkerProcess(bool useSeparateServiceWorkerProcess)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto handler1 = adoptNS([[SWSchemes alloc] init]);
    handler1->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    handler1->resources.set("sw://host2/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw://host2/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler1.get() forURLScheme:@"sw"];

    auto *processPool = configuration.get().processPool;
    [processPool _setUseSeparateServiceWorkerProcess: useSeparateServiceWorkerProcess];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];

    [webView loadRequest:request];

    waitUntilServiceWorkerProcessCount(processPool, 1);
    return webView.get().configuration.processPool._webProcessCountIgnoringPrewarmed;
}

TEST(ServiceWorkers, OutOfAndInProcessServiceWorker)
{
    bool useSeparateServiceWorkerProcess = false;
    EXPECT_EQ(1u, launchServiceWorkerProcess(useSeparateServiceWorkerProcess));

    useSeparateServiceWorkerProcess = true;
    EXPECT_EQ(2u, launchServiceWorkerProcess(useSeparateServiceWorkerProcess));
}

TEST(ServiceWorkers, ThrottleCrash)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw1");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);

    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw1://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw1://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];
#if PLATFORM(MAC)
    [[configuration preferences] _setAppNapEnabled:YES];
#endif
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"sw1"];

    auto *processPool = configuration.get().processPool;

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow: YES]);
    [webView1 setNavigationDelegate:navigationDelegate.get()];

    // Let's make it so that webView1 be app nappable after loading is completed.
    [webView1.get().window resignKeyWindow];
#if PLATFORM(MAC)
    [webView1.get().window orderOut:nil];
#endif

    auto *request1 = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw1://host/main.html"]];
    [webView1 loadRequest:request1];

    didFinishNavigation = false;
    TestWebKitAPI::Util::run(&didFinishNavigation);

    auto webView2Configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
#if PLATFORM(MAC)
    [[webView2Configuration preferences] _setAppNapEnabled:NO];
#endif
    [webView2Configuration setProcessPool:processPool];
    [webView2Configuration setWebsiteDataStore:dataStore];
    [[webView2Configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [webView2Configuration setURLSchemeHandler:handler.get() forURLScheme:@"sw1"];
    webView2Configuration.get()._relatedWebView = webView1.get();

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webView2Configuration.get()]);
    [webView2 setNavigationDelegate:navigationDelegate.get()];

    [webView2 loadRequest:request1];

    didFinishNavigation = false;
    TestWebKitAPI::Util::run(&didFinishNavigation);
}

TEST(ServiceWorkers, LoadData)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");

    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestSWAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    done = false;

    // Normal load to get SW registered.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    // Now try a data load.
    NSData *data = [NSData dataWithBytes:mainBytes length:strlen(mainBytes)];
    [webView loadData:data MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:[NSURL URLWithString:@"sw://host/main.html"]];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, RestoreFromDiskNonDefaultStore)
{
    ASSERT(mainRegisteringWorkerBytes);
    ASSERT(scriptBytes);
    ASSERT(mainRegisteringAlreadyExistingWorkerBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    NSURL *swDBPath = [NSURL fileURLWithPath:[@"~/Library/WebKit/TestWebKitAPI/CustomWebsiteData/ServiceWorkers/" stringByExpandingTildeInPath]];
    [[NSFileManager defaultManager] removeItemAtURL:swDBPath error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);
    [[NSFileManager defaultManager] createDirectoryAtURL:swDBPath withIntermediateDirectories:YES attributes:nil error:nil];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    // We protect the process pool so that it outlives the WebsiteDataStore.
    RetainPtr<WKProcessPool> protectedProcessPool;

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swDBPath;
        [websiteDataStoreConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw"];
        auto nonDefaultDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        configuration.get().websiteDataStore = nonDefaultDataStore.get();

        auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration was successful and service worker was activated"]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

        auto handler = adoptNS([[SWSchemes alloc] init]);
        handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringWorkerBytes });
        handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
        [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        protectedProcessPool = webView.get().configuration.processPool;

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]]];

        TestWebKitAPI::Util::run(&done);
        done = false;

        [webView.get().configuration.processPool _terminateServiceWorkerProcesses];
    }

    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
        websiteDataStoreConfiguration.get()._serviceWorkerRegistrationDirectory = swDBPath;
        [websiteDataStoreConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw"];
        auto nonDefaultDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        configuration.get().websiteDataStore = nonDefaultDataStore.get();

        auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

        auto handler = adoptNS([[SWSchemes alloc] init]);
        handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringAlreadyExistingWorkerBytes });
        handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
        [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]]];

        TestWebKitAPI::Util::run(&done);
        done = false;
    }
}

TEST(ServiceWorkers, SuspendNetworkProcess)
{
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr<SWMessageHandler> messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestSWAsyncNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    done = false;

    // Normal load to get SW registered.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto store = [configuration websiteDataStore];
    auto path = store._serviceWorkerRegistrationDirectory;

    NSURL* directory = [NSURL fileURLWithPath:path isDirectory:YES];
    NSURL *swDBPath = [directory URLByAppendingPathComponent:@"ServiceWorkerRegistrations-4.sqlite3"];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [ webView.get().configuration.processPool _sendNetworkProcessWillSuspendImminently];

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);

    [ webView.get().configuration.processPool _sendNetworkProcessDidResume];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:swDBPath.path]);
}

TEST(WebKit, ServiceWorkerDatabaseWithRecordsTableButUnexpectedSchema)
{
    // Copy the baked database files to the database directory
    NSURL *url1 = [[NSBundle mainBundle] URLForResource:@"BadServiceWorkerRegistrations-4" withExtension:@"sqlite3" subdirectory:@"TestWebKitAPI.resources"];

    NSURL *swPath = [NSURL fileURLWithPath:[@"~/Library/Caches/TestWebKitAPI/WebKit/ServiceWorkers/" stringByExpandingTildeInPath]];
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

    auto* dataStore = dataStoreWithRegisteredServiceWorkerScheme(@"sw1");
    
    // Start with a clean slate data store
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore];

    auto messageHandler = adoptNS([[SWMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    auto handler1 = adoptNS([[SWSchemes alloc] init]);
    handler1->resources.set("sw1://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    handler1->resources.set("sw1://host2/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host2/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler1.get() forURLScheme:@"sw1"];

    WKProcessPool *processPool = configuration.get().processPool;

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw1://host/main.html"]];

    configuration.get().websiteDataStore = dataStore;

    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    _WKWebsiteDataStoreConfiguration *nonPersistentConfiguration = [[[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration] autorelease];
    [nonPersistentConfiguration registerURLSchemeServiceWorkersCanHandleForTesting:@"sw1"];
    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:nonPersistentConfiguration] autorelease];

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    EXPECT_EQ(2U, processPool._serviceWorkerProcessCount);
}
