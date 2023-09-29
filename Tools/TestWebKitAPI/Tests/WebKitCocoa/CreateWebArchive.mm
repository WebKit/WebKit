
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace TestWebKitAPI {

static const char* mainBytes = R"TESTRESOURCE(
<head>
<script src="webarchivetest://host/script.js"></script>
</head>
)TESTRESOURCE";

static const char* scriptBytes = R"TESTRESOURCE(
window.webkit.messageHandlers.testHandler.postMessage("done");
)TESTRESOURCE";

TEST(WebArchive, CreateCustomScheme)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        const char* bytes = nullptr;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"])
            bytes = mainBytes;
        else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/script.js"])
            bytes = scriptBytes;
        else
            FAIL();

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:bytes length:strlen(bytes)]];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    static bool done = false;
    [webView performAfterReceivingMessage:@"done" action:[&] { done = true; }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];

    Util::run(&done);
    done = false;

    static RetainPtr<NSData> archiveData;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE(!!result);
        archiveData = result;
        done = true;
    }];

    Util::run(&done);
    done = false;

    [webView performAfterReceivingMessage:@"done" action:[&] { done = true; }];
    [webView loadData:archiveData.get() MIMEType:(NSString *)kUTTypeArchive characterEncodingName:@"utf-8" baseURL:[NSURL URLWithString:@"about:blank"]];

    Util::run(&done);
    done = false;
}

static RetainPtr<NSData> webArchiveAccessingCookies()
{
    HTTPServer server({ { "/"_s, { "<script>document.cookie</script>"_s } } }, HTTPServer::Protocol::HttpsProxy);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:server.httpsProxyConfiguration()]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<NSData> webArchive;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        webArchive = result;
    }];
    while (!webArchive)
        Util::spinRunLoop();
    return webArchive;
}

TEST(WebArchive, CookieAccessAfterLoadRequest)
{
    auto webArchive = webArchiveAccessingCookies();

    NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"CookieAccessTest.webarchive"];
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    BOOL success = [webArchive writeToFile:path atomically:YES];
    EXPECT_TRUE(success);

    auto webView = adoptNS([WKWebView new]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL fileURLWithPath:path isDirectory:NO]]];
    [webView _test_waitForDidFinishNavigation];
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
}

TEST(WebArchive, CookieAccessAfterLoadData)
{
    auto webArchive = webArchiveAccessingCookies();
    auto webView = adoptNS([WKWebView new]);
    [webView loadData:webArchive.get() MIMEType:@"application/x-webarchive" characterEncodingName:@"utf-8" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    [webView _test_waitForDidFinishNavigation];
}

TEST(WebArchive, ApplicationXWebarchiveMIMETypeDoesNotLoadHTML)
{
    HTTPServer server({ { "/"_s, { { { "Content-Type"_s, "application/x-webarchive"_s } }, "Not web archive content, should not load"_s } } });

    auto webView = adoptNS([WKWebView new]);
    [webView loadRequest:server.request()];
    [webView _test_waitForDidFailProvisionalNavigation];
}

TEST(WebArchive, SaveResourcesBasic)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [@"<head><script src=\"script.js\"></script></head><img src=\"image.png\" onload=\"onImageLoad()\"><script>function onImageLoad() { notifyTestRunner(); }</script>" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *scriptData = [@"function notifyTestRunner() { window.webkit.messageHandlers.testHandler.postMessage(\"done\"); }" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/script.js"]) {
            mimeType = @"application/javascript";
            data = scriptData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        } else
            FAIL();

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    static bool messageReceived = false;
    [webView performAfterReceivingMessage:@"done" action:[&] {
        messageReceived = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        auto htmlPath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:htmlPath]);
        NSData *htmlDataWithReplacedURLs = [@"<html><head><script src=\"host_files/script.js\"></script></head><body><img src=\"host_files/image.png\" onload=\"onImageLoad()\"><script>function onImageLoad() { notifyTestRunner(); }</script></body></html>" dataUsingEncoding:NSUTF8StringEncoding];
        EXPECT_TRUE([[NSData dataWithContentsOfFile:htmlPath] isEqualToData:htmlDataWithReplacedURLs]);

        auto scriptPath = [directoryURL URLByAppendingPathComponent:@"host_files/script.js"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:scriptPath]);
        EXPECT_TRUE([[NSData dataWithContentsOfFile:scriptPath] isEqualToData:scriptData]);

        auto imagePath = [directoryURL URLByAppendingPathComponent:@"host_files/image.png"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:imagePath]);
        EXPECT_TRUE([[NSData dataWithContentsOfFile:imagePath] isEqualToData:imageData]);
        saved = true;
    }];
    Util::run(&saved);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
