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
#import "Test.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
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

#if WK_API_ENABLED

struct ResourceInfo {
    RetainPtr<NSString> mimeType;
    const char* data;
};

static bool done;

static String expectedMessage;
static String retrievedString;

@interface SWMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SWMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_TRUE([[message body] isEqualToString:@"Message from worker: ServiceWorker received: Hello from the web page"]);
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
    EXPECT_TRUE([[message body] isEqualToString:expectedMessage]);
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

TEST(ServiceWorkers, Basic)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

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

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = nullptr;

    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] completionHandler:^(NSArray<WKWebsiteDataRecord *> *websiteDataRecords) {
        EXPECT_EQ(1u, [websiteDataRecords count]);
        EXPECT_TRUE([websiteDataRecords[0].displayName isEqualToString:@"sw host"]);

        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, RestoreFromDisk)
{
    ASSERT(mainRegisteringWorkerBytes);
    ASSERT(scriptBytes);
    ASSERT(mainRegisteringAlreadyExistingWorkerBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerForRestoreFromDiskTest> messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration was successful and service worker was activated"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringWorkerBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;
    handler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringAlreadyExistingWorkerBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(ServiceWorkers, CacheStorageRestoreFromDisk)
{
    ASSERT(mainCacheStorageBytes);
    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    done = false;

    [WKWebsiteDataStore _deleteDefaultDataStoreForTesting];

    auto handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainCacheStorageBytes });

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"No cache storage data"]);

    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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
    messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"Some cache storage data: my cache"]);

    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerForFetchTest> messageHandler = adoptNS([[SWMessageHandlerForFetchTest alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFetchTestBytes });
    handler->resources.set("sw://host/test.html", ResourceInfo { @"text/html", testBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptHandlingFetchBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);

    webView = nullptr;
    configuration = nullptr;
    messageHandler = nullptr;
    handler = nullptr;

    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    messageHandler = adoptNS([[SWMessageHandlerForFetchTest alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFetchTestBytes });
    handler->resources.set("sw://host/test.html", ResourceInfo { @"text/html", testBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptHandlingFetchBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    expectedMessage = "Intercepted by worker";
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

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

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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
    messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainForFirstLoadInterceptTestBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptInterceptingFirstLoadBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    // Verify service worker is intercepting load.
    expectedMessage = "Intercepted by worker";
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];
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
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];
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
    WKRetainPtr<WKContextRef> context(AdoptWK, TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.processPool = (WKProcessPool *)context.get();
    auto pool = configuration.processPool;
    [pool _registerURLSchemeServiceWorkersCanHandle:@"sw"];
    [pool _setMaximumNumberOfProcesses:5];

    configuration.websiteDataStore = (WKWebsiteDataStore *)WKContextGetWebsiteDataStore(context.get());
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
    EXPECT_EQ(1u, webView.get().configuration.processPool._webProcessCount);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithConnection.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading the simple page did not start the service worker process.
    EXPECT_EQ(1u, webView.get().configuration.processPool._webProcessCount);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(2u, webView.get().configuration.processPool._webProcessCount);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading this page did start the service worker process.
    EXPECT_EQ(3u, webView.get().configuration.processPool._webProcessCount);

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

    [newConfiguration.get().processPool _setMaximumNumberOfProcesses:1];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithoutConnection.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Let's use the web site data store that has service worker and load a page.
    newConfiguration.get().websiteDataStore = [configuration websiteDataStore];

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:newConfiguration.get()]);
    EXPECT_EQ(1u, webView.get().configuration.processPool._webProcessCount);
    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithConnection.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure that loading the simple page did not start the service worker process.
    EXPECT_EQ(1u, webView.get().configuration.processPool._webProcessCount);

    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
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

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
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
    EXPECT_TRUE([[configuration websiteDataStore] _hasRegisteredServiceWorker]);

    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

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
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;
    EXPECT_TRUE([websiteDataStore _hasRegisteredServiceWorker]);

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

    configuration.get().websiteDataStore = [[[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()] autorelease];

    RetainPtr<SWMessageHandlerWithExpectedMessage> messageHandler = adoptNS([[SWMessageHandlerWithExpectedMessage alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytesForSessionIDTest });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytesForSessionIDTest });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    expectedMessage = "PASS: activation successful";
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

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

TEST(ServiceWorkers, ProcessPerOrigin)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

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

    RetainPtr<SWSchemes> handler1 = adoptNS([[SWSchemes alloc] init]);
    handler1->resources.set("sw1://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler1->resources.set("sw1://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler1.get() forURLScheme:@"sw1"];

    RetainPtr<SWSchemes> handler2 = adoptNS([[SWSchemes alloc] init]);
    handler2->resources.set("sw2://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler2->resources.set("sw2://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler2.get() forURLScheme:@"sw2"];

    WKProcessPool *processPool = configuration.get().processPool;
    [processPool _registerURLSchemeServiceWorkersCanHandle:@"sw1"];
    [processPool _registerURLSchemeServiceWorkersCanHandle:@"sw2"];

    // Normally, service workers get terminated several seconds after their clients are gone.
    // Disable this delay for the purpose of testing.
    [processPool _disableServiceWorkerProcessTerminationDelay];

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

    EXPECT_EQ(2U, processPool._serviceWorkerProcessCount);

    NSURLRequest *aboutBlankRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]];
    [webView3 loadRequest:aboutBlankRequest];

    waitUntilServiceWorkerProcessCount(processPool, 1);
    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    [webView2 loadRequest:aboutBlankRequest];
    TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_EQ(1U, processPool._serviceWorkerProcessCount);

    [webView1 loadRequest:aboutBlankRequest];
    waitUntilServiceWorkerProcessCount(processPool, 0);
    EXPECT_EQ(0U, processPool._serviceWorkerProcessCount);
}

TEST(ServiceWorkers, LoadData)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

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

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainBytes });
    handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

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
        auto nonDefaultDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        configuration.get().websiteDataStore = nonDefaultDataStore.get();

        auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration was successful and service worker was activated"]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

        auto handler = adoptNS([[SWSchemes alloc] init]);
        handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringWorkerBytes });
        handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
        [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];
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
        auto nonDefaultDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        configuration.get().websiteDataStore = nonDefaultDataStore.get();

        auto messageHandler = adoptNS([[SWMessageHandlerForRestoreFromDiskTest alloc] initWithExpectedMessage:@"PASS: Registration already has an active worker"]);
        [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"sw"];

        auto handler = adoptNS([[SWSchemes alloc] init]);
        handler->resources.set("sw://host/main.html", ResourceInfo { @"text/html", mainRegisteringAlreadyExistingWorkerBytes });
        handler->resources.set("sw://host/sw.js", ResourceInfo { @"application/javascript", scriptBytes });
        [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView.get().configuration.processPool _registerURLSchemeServiceWorkersCanHandle:@"sw"];

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/main.html"]]];

        TestWebKitAPI::Util::run(&done);
        done = false;
    }
}

#endif // WK_API_ENABLED
