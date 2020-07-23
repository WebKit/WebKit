/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "TestURLSchemeHandler.h"
#import <WebKit/WebKit.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>

static bool readyToContinue;
static bool receivedScriptMessage;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface IndexedDBMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation IndexedDBMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

TEST(IndexedDB, IndexedDBPersistence)
{
    RetainPtr<IndexedDBMessageHandler> handler = adoptNS([[IndexedDBMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBPersistence-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string1 = (NSString *)[lastScriptMessage body];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];

    // Ditch this web view (ditching its web process)
    webView = nil;

    // Terminate the network process
    [configuration.get().processPool _terminateNetworkProcess];

    // Make a new web view to finish the test
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBPersistence-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string3 = (NSString *)[lastScriptMessage body];

    EXPECT_WK_STREQ(@"UpgradeNeeded", string1.get());
    EXPECT_WK_STREQ(@"Success", string2.get());
    EXPECT_WK_STREQ(@"2 TestObjectStore", string3.get());
}

TEST(IndexedDB, IndexedDBPersistencePrivate)
{
    RetainPtr<IndexedDBMessageHandler> handler = adoptNS([[IndexedDBMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto ephemeralStore = [WKWebsiteDataStore nonPersistentDataStore];
    configuration.get().websiteDataStore = ephemeralStore;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBPersistence-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string1 = (NSString *)[lastScriptMessage body];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];

    auto webViewPid1 = [webView _webProcessIdentifier];
    // Ditch this web view (ditching its web process)
    webView = nil;

    // Make a new web view to finish the test
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBPersistence-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string3 = (NSString *)[lastScriptMessage body];

    auto webViewPid2 = [webView _webProcessIdentifier];
    EXPECT_NE(webViewPid1, webViewPid2);

    EXPECT_WK_STREQ(@"UpgradeNeeded", string1.get());
    EXPECT_WK_STREQ(@"Success", string2.get());
    EXPECT_WK_STREQ(@"2 TestObjectStore", string3.get());
}

TEST(IndexedDB, IndexedDBDataRemoval)
{
    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeIndexedDBDatabases]]);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:websiteDataTypes.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        readyToContinue = true;
        ASSERT_EQ(0u, dataRecords.count);
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    receivedScriptMessage = false;
    auto handler = adoptNS([[IndexedDBMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IndexedDBPersistence-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        ASSERT_EQ(1u, dataRecords.count);
        [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:websiteDataTypes.get() forDataRecords:dataRecords completionHandler:^() {
            readyToContinue = true;
        }];
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        readyToContinue = true;
        ASSERT_EQ(0u, dataRecords.count);
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

static NSString *mainFrameString = @"<script> \
    function postResult(event) { \
        window.webkit.messageHandlers.testHandler.postMessage(event.data); \
    } \
    addEventListener('message', postResult, false); \
    </script> \
    <iframe src='iframe://'>";

static const char* iframeBytes = R"TESTRESOURCE(
<script>
function postResult(result) {
    if (window.parent != window.top) {
        parent.postMessage(result, '*');
    } else {
        window.webkit.messageHandlers.testHandler.postMessage(result);
    }
}

try {
    var request = window.indexedDB.open('IndexedDBThirdPartyFrameHasAccess');
    var db = null;
    request.onupgradeneeded = function(event) {
        db = event.target.result;
        var objectStore = db.createObjectStore('TestObjectStore');
        var putRequest = objectStore.put('TestValue', 'TestKey');
        putRequest.onsuccess = function(event) {
            postResult('database is created - put item success');
        }
        putRequest.onerror = function(event) {
            postResult('database is created - put item error: ' + event.target.error.name + ' - ' + event.target.error.message);
        }
    }
    request.onsuccess = function(event) {
        if (db)
            return;
        db = event.target.result;
        var objectStore = db.transaction(['TestObjectStore']).objectStore('TestObjectStore');
        var getRequest = objectStore.get('TestKey');
        getRequest.onsuccess = function(event) {
            postResult('database exists - get item success: ' + event.target.result);
        }
        getRequest.onerror = function(event) {
            postResult('database exists - get item error: ' + event.target.error.name + ' - ' + event.target.error.message);
        }
    }
    request.onerror = function(event) {
        if (!db) {
            postResult('database error: ' + event.target.error.name + ' - ' + event.target.error.message);
        }
    }
} catch(err) {
    postResult('database error: ' + err.name + ' - ' + err.message);
}
</script>
)TESTRESOURCE";

static void loadTestPageInWebView(WKWebView *webView, NSString *expectedResult)
{
    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(expectedResult, (NSString *)[lastScriptMessage body]);
}

TEST(IndexedDB, IndexedDBThirdPartyFrameHasAccess)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[IndexedDBMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:iframeBytes length:strlen(iframeBytes)]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"iframe"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(webView.get(), @"database is created - put item success");

    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(secondWebView.get(), @"database exists - get item success: TestValue");

    webView = nil;
    secondWebView = nil;
    [configuration.get().processPool _terminateNetworkProcess];

    // Third-party IDB storage is stored in the memory of network process.
    auto thirdWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(thirdWebView.get(), @"database is created - put item success");
}

TEST(IndexedDB, IndexedDBThirdPartyDataRemoval)
{
    auto websiteDataTypes = adoptNS([[NSSet alloc] initWithArray:@[WKWebsiteDataTypeIndexedDBDatabases]]);
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:websiteDataTypes.get() modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[IndexedDBMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:iframeBytes length:strlen(iframeBytes)]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"iframe"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(webView.get(), @"database is created - put item success");

    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [secondWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"iframe://"]]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ( @"database is created - put item success", (NSString *)[lastScriptMessage body]);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:websiteDataTypes.get() completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(1u, dataRecords.count);
        EXPECT_WK_STREQ("iframe", [[dataRecords firstObject] displayName]);
        [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:websiteDataTypes.get() forDataRecords:dataRecords completionHandler:^() {
            readyToContinue = true;
        }];
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    auto thirdWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(thirdWebView.get(), @"database is created - put item success");
}

static const char* workerBytes = R"TESTRESOURCE(
try {
    var request = indexedDB.open('IndexedDBThirdPartyWorkerHasAccess');
    var db = null;
    request.onupgradeneeded = function(event) {
        db = event.target.result;
        self.postMessage('database is created');
    }
    request.onsuccess = function(event) {
        if (db)
            return;
        self.postMessage('database exists');
    }
    request.onerror = function(event) {
        if (!db) {
            self.postMessage('database error: ' + event.target.error.name + ' - ' + event.target.error.message);
        }
    }
} catch(err) {
    self.postMessage('database error: ' + err.name + ' - ' + err.message);
}
)TESTRESOURCE";

static const char* workerFrameBytes = R"TESTRESOURCE(
<script>
    var worker = new Worker('worker.js');
    worker.onmessage = function(event) {
        parent.postMessage(event.data, '*');
    };
</script>
)TESTRESOURCE";

TEST(IndexedDB, IndexedDBThirdPartyWorkerHasAccess)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[IndexedDBMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr<NSURLResponse> response;
        RetainPtr<NSData> data;
        NSURL *requestURL = task.request.URL;
        if ([requestURL.absoluteString isEqualToString:@"iframe://"]) {
            response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
            data = [NSData dataWithBytes:workerFrameBytes length:strlen(workerFrameBytes)];
        } else {
            EXPECT_WK_STREQ("iframe://worker.js", requestURL.absoluteString);
            response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/javascript" expectedContentLength:0 textEncodingName:nil]);
            data = [NSData dataWithBytes:workerBytes length:strlen(workerBytes)];
        }
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data.get()];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"iframe"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(webView.get(), @"database is created");

    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(webView.get(), @"database exists");

    webView = nil;
    secondWebView = nil;
    [configuration.get().processPool _terminateNetworkProcess];

    auto thirdWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    loadTestPageInWebView(thirdWebView.get(), @"database is created");
}

