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

static Deque<String> expectedMessages;

@interface SWMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SWMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    EXPECT_TRUE([[message body] isEqualToString:@"Message from worker: ServiceWorker received: Hello from the web page"]);
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

#endif // WK_API_ENABLED
