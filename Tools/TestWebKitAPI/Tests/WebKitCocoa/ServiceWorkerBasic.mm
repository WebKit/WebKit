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
#import <WebKit/WebKit.h>
#import <WebKit/_WKExperimentalFeature.h>
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
    reg.installing.postMessage("Hello from the web page");
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

    // Test that a regular page does not trigger a service worker connection to storage process if there is no registered service worker.
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

TEST(ServiceWorkers, StorageProcessConnectionCreation)
{
    ASSERT(mainBytes);
    ASSERT(scriptBytes);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    done = false;
    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setConfigurationInjectedBundlePath(configuration.get());

    RetainPtr<RegularPageMessageHandler> regularPageMessageHandler = adoptNS([[RegularPageMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:regularPageMessageHandler.get() name:@"regularPage"];

    RetainPtr<SWSchemes> handler = adoptNS([[SWSchemes alloc] init]);
    handler->resources.set("sw://host/regularPageWithoutConnection.html", ResourceInfo { @"text/html", regularPageWithoutConnectionBytes });
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"SW"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    EXPECT_EQ(0, webView.get().configuration.processPool._storageProcessIdentifier);

    // Test that a regular page does not trigger a storage process if there is no registered service worker.
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"sw://host/regularPageWithoutConnection.html"]];

    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Make sure than loading the simple page did not start a storage process.
    EXPECT_EQ(0, webView.get().configuration.processPool._storageProcessIdentifier);
}

#endif // WK_HAVE_C_SPI

#endif // WK_API_ENABLED
