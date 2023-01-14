/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK)

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>

@interface FileSystemAccessMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation FileSystemAccessMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

static NSString *workerFrameString = @"<script> \
    function start() { \
        var worker = new Worker('worker.js'); \
        worker.onmessage = function(event) { \
            window.webkit.messageHandlers.testHandler.postMessage(event.data); \
        }; \
    } \
    window.webkit.messageHandlers.testHandler.postMessage('page is loaded'); \
    </script>";

static const char* workerBytes = R"TESTRESOURCE(
var position = 0;
var accessHandle;
async function test()
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        var fileHandle = await rootHandle.getFileHandle('file-system-access.txt', { 'create' : true });
        accessHandle = await fileHandle.createSyncAccessHandle();
        var buffer = new ArrayBuffer(10);
        var writeSize = accessHandle.write(buffer, { "at" : 0 });
        self.postMessage('success: write ' + writeSize + ' bytes');
        keepAccessHandleActive();
    } catch(err) {
        self.postMessage('error: ' + err.name + ' - ' + err.message);
        close();
    }
}
function keepAccessHandleActive()
{
    try {
        var buffer = new ArrayBuffer(1);
        var writeSize = accessHandle.write(buffer, { "at" : position });
        position += writeSize;
        setTimeout(keepAccessHandleActive, 100);
    } catch (err) {
        self.postMessage('error: ' + err.name + ' - ' + err.message);
        close();
    }
}
test();
)TESTRESOURCE";

TEST(FileSystemAccess, WebProcessCrashDuringWrite)
{
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._accessHandleEnabled = YES;
    preferences._storageAPIEnabled = YES;
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr<NSURLResponse> response;
        RetainPtr<NSData> data;
        NSURL *requestURL = task.request.URL;
        EXPECT_WK_STREQ("webkit://webkit.org/worker.js", requestURL.absoluteString);
        response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/javascript" expectedContentLength:0 textEncodingName:nil]);
        data = [NSData dataWithBytes:workerBytes length:strlen(workerBytes)];
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data.get()];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webkit"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:workerFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"page is loaded", [lastScriptMessage body]);

    [webView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);

    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [secondWebView loadHTMLString:workerFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"page is loaded", [lastScriptMessage body]);

    // Access handle cannot be created when there is an open one.
    [secondWebView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"error: InvalidStateError - The object is in an invalid state.", [lastScriptMessage body]);

    // Open access handle should be closed when web process crashes.
    [webView _killWebContentProcess];

    [secondWebView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);
}

TEST(FileSystemAccess, NetworkProcessCrashDuringWrite)
{
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._accessHandleEnabled = YES;
    preferences._storageAPIEnabled = YES;
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RetainPtr<NSData> data;
        NSURL *requestURL = task.request.URL;
        EXPECT_WK_STREQ("webkit://webkit.org/worker.js", requestURL.absoluteString);
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/javascript" expectedContentLength:0 textEncodingName:nil]);
        data = [NSData dataWithBytes:workerBytes length:strlen(workerBytes)];
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data.get()];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webkit"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:workerFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"page is loaded", [lastScriptMessage body]);

    [webView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);

    // Kill network process.
    [[configuration websiteDataStore] _terminateNetworkProcess];

    // Open access handle should be closed when network process crashes.
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"error: InvalidStateError - AccessHandle is closed", [lastScriptMessage body]);

    // Access handle can be created after network process is relaunched.
    [webView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);
}

TEST(FileSystemAccess, DeleteDataDuringWrite)
{
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._accessHandleEnabled = YES;
    preferences._storageAPIEnabled = YES;
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSURL *requestURL = task.request.URL;
        EXPECT_WK_STREQ("webkit://webkit.org/worker.js", requestURL.absoluteString);
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/javascript" expectedContentLength:0 textEncodingName:nil]);
        RetainPtr<NSData> data = [NSData dataWithBytes:workerBytes length:strlen(workerBytes)];
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data.get()];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webkit"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:workerFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"page is loaded", [lastScriptMessage body]);

    [webView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);

    done = false;
    auto types = [NSSet setWithObject:WKWebsiteDataTypeFileSystem];
    [[configuration websiteDataStore] removeDataOfTypes:types modifiedSince:[NSDate distantPast] completionHandler:^ {
        [[configuration websiteDataStore] fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
            EXPECT_EQ(records.count, 0u);
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);

    // Open access handle should be when website data is removed.
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"error: InvalidStateError - AccessHandle is closed", [lastScriptMessage body]);
}

static NSString *basicString = @"<script> \
    async function open() \
    { \
        try { \
            var rootHandle = await navigator.storage.getDirectory(); \
            var fileHandle = await rootHandle.getFileHandle('file-system-access.txt', { 'create' : false }); \
            window.webkit.messageHandlers.testHandler.postMessage('file is opened'); \
        } catch (err) { \
            window.webkit.messageHandlers.testHandler.postMessage('error: ' + err.name + ' - ' + err.message); \
        } \
    } \
    open(); \
    </script>";

TEST(FileSystemAccess, MigrateToNewStorageDirectory)
{
    NSString *hashedOrigin = @"Rpva_lVGHjojRmxI7eh92UpdZVvdH0OCis2MNCM-nDo";
    NSString *storageType = @"FileSystem";
    NSString *fileName = @"file-system-access.txt";
    
    NSFileManager *fileManager = [NSFileManager defaultManager];

    // This is old value returned by WebsiteDataStore::defaultGeneralStorageDirectory().
    NSString *oldStorageDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches/com.apple.WebKit.TestWebKitAPI/WebKit/Storage/"];
    [fileManager removeItemAtPath:oldStorageDirectory error:nil];
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:oldStorageDirectory]);
    
    // Copy baked files to old directory.
    NSString *oldFileSystemDirectory = [NSString pathWithComponents:@[oldStorageDirectory, hashedOrigin, hashedOrigin, storageType]];
    [fileManager createDirectoryAtURL:[NSURL fileURLWithPath:oldFileSystemDirectory] withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *oldFilePath = [oldFileSystemDirectory stringByAppendingPathComponent:fileName];
    [fileManager createFileAtPath:oldFilePath contents:nil attributes:nil];
    EXPECT_TRUE([fileManager fileExistsAtPath:oldFilePath]);

    NSString *resourceSaltPath = [[NSBundle mainBundle] URLForResource:@"file-system-access" withExtension:@"salt" subdirectory:@"TestWebKitAPI.resources"].path;
    NSString *oldSaltPath = [oldStorageDirectory stringByAppendingPathComponent:@"salt"];
    [fileManager copyItemAtPath:resourceSaltPath toPath:oldSaltPath error:nil];
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:oldSaltPath]);

    // This is current value returned by WebsiteDataStore::defaultGeneralStorageDirectory().
    NSString *newStorageDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/WebKit/com.apple.WebKit.TestWebKitAPI/WebsiteData/Default/"];
    [fileManager removeItemAtPath:newStorageDirectory error:nil];
    NSString *newFilePath = [NSString pathWithComponents:@[newStorageDirectory, hashedOrigin, hashedOrigin, storageType, fileName]];
    EXPECT_FALSE([fileManager fileExistsAtPath:newFilePath]);

    // Invoke WebsiteDataStore::defaultGeneralStorageDirectory() to trigger migration.
    NSString *currentStorageDirectory = [[[WKWebsiteDataStore defaultDataStore] _configuration] generalStorageDirectory].path;
    EXPECT_WK_STREQ(newStorageDirectory, currentStorageDirectory);
    EXPECT_FALSE([[NSFileManager defaultManager] fileExistsAtPath:oldFilePath]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:newFilePath]);

    // Ensure file can be opened after migration: test page only opens the file if it exists.
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._storageAPIEnabled = YES;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:basicString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"file is opened", [lastScriptMessage body]);
}

static NSString *testString = @"<script> \
    async function open(shouldCreateFile) \
    { \
        try { \
            var rootHandle = await navigator.storage.getDirectory(); \
            var fileHandle = await rootHandle.getFileHandle('file-system-access.txt', { 'create' : shouldCreateFile }); \
            window.webkit.messageHandlers.testHandler.postMessage('file is opened'); \
        } catch(err) { \
            window.webkit.messageHandlers.testHandler.postMessage('error: ' + err.name + ' - ' + err.message); \
        } \
    } \
    open(true); \
    </script>";

TEST(FileSystemAccess, FetchAndRemoveData)
{
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto websiteDataStore = [configuration websiteDataStore];
    auto types = [NSSet setWithObject:WKWebsiteDataTypeFileSystem];

    // Remove existing data.
    done = false;
    [websiteDataStore removeDataOfTypes:types modifiedSince:[NSDate distantPast] completionHandler:^ {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._storageAPIEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:testString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"file is opened", [lastScriptMessage body]);

    // Fetch data and remove it by origin.
    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 1u);
        auto record = [records objectAtIndex:0];
        EXPECT_STREQ("webkit.org", [record.displayName UTF8String]);

        // Remove data.
        [websiteDataStore removeDataOfTypes:types forDataRecords:records completionHandler:^{
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);

    // Fetch data after removal.
    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 0u);
        done = true;
    }];

    // File cannot be opened after data removal.
    [webView evaluateJavaScript:@"open(false)" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"error: NotFoundError - The object can not be found here.", [lastScriptMessage body]);
}

TEST(FileSystemAccess, RemoveDataByModificationTime)
{
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._storageAPIEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:testString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"file is opened", [lastScriptMessage body]);

    auto websiteDataStore = [configuration websiteDataStore];
    auto types = [NSSet setWithObject:WKWebsiteDataTypeFileSystem];
    done = false;
    __block NSUInteger recordsCount;
    [websiteDataStore fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        recordsCount = records.count;
        EXPECT_GT(recordsCount, 0u);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore removeDataOfTypes:types modifiedSince:[NSDate now] completionHandler:^ {
        [websiteDataStore fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
            recordsCount = records.count;
            EXPECT_EQ(records.count, recordsCount);
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore removeDataOfTypes:types modifiedSince:[NSDate distantPast] completionHandler:^ {
        [websiteDataStore fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
            EXPECT_EQ(records.count, 0u);
            done = true;
        }];
    }];
    TestWebKitAPI::Util::run(&done);
}

static NSString *mainFrameString = @"<script> \
    function postResult(event) \
    { \
        window.webkit.messageHandlers.testHandler.postMessage(event.data); \
    } \
    addEventListener('message', postResult, false); \
    </script> \
    <iframe src='https://127.0.0.1:9091/'>";

static constexpr auto frameBytes = R"TESTRESOURCE(
<script>
function postMessage(message)
{
    parent.postMessage(message, '*');
}
async function open()
{
    try {
        var rootHandle = await navigator.storage.getDirectory();
        var fileHandle = await rootHandle.getFileHandle('file-system-access.txt', { 'create' : true });
        postMessage('file is opened');
    } catch(err) {
        postMessage('error: ' + err.name + ' - ' + err.message);
    }
}
open();
</script>
)TESTRESOURCE"_s;

TEST(FileSystemAccess, FetchDataForThirdParty)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { frameBytes } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9091);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[FileSystemAccessMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._fileSystemAccessEnabled = YES;
    preferences._storageAPIEnabled = YES;

    auto websiteDataStore = [configuration websiteDataStore];
    auto types = [NSSet setWithObject:WKWebsiteDataTypeFileSystem];
    done = false;
    [websiteDataStore removeDataOfTypes:types modifiedSince:[NSDate distantPast] completionHandler:^ {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [navigationDelegate setDecidePolicyForNavigationAction:[&](WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"file is opened", [lastScriptMessage body]);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:types completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        // Should return both opening origin and top origin.
        EXPECT_EQ(records.count, 2u);
        auto sortFunction = ^(WKWebsiteDataRecord *record1, WKWebsiteDataRecord *record2){
            return [record1.displayName compare:record2.displayName];
        };
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"127.0.0.1", [sortedRecords objectAtIndex:0].displayName);
        EXPECT_WK_STREQ(@"webkit.org", [sortedRecords objectAtIndex:1].displayName);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#endif // USE(APPLE_INTERNAL_SDK)
