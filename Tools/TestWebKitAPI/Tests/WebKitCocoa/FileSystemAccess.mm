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
#import "PlatformUtilities.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

@interface FileSystemAccessMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation FileSystemAccessMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

static NSString *mainFrameString = @"<script> \
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
    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"page is loaded", [lastScriptMessage body]);

    [webView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);

    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [secondWebView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
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
    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"webkit://webkit.org"]];
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
    EXPECT_WK_STREQ(@"error: InvalidStateError - AccessHandle is closing or closed", [lastScriptMessage body]);

    // Access handle can be created after network process is relaunched.
    [webView evaluateJavaScript:@"start()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"success: write 10 bytes", [lastScriptMessage body]);
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

#endif // USE(APPLE_INTERNAL_SDK)
