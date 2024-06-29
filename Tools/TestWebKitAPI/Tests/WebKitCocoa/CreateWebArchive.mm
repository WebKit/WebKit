
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
#import <WebKit/_WKArchiveConfiguration.h>
#import <WebKit/_WKArchiveExclusionRule.h>

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
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @""
            }
        };
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nullptr error:nullptr];
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;

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

static const char* htmlDataBytesForIframe = R"TESTRESOURCE(
<script>
count = 0;
function onFrameLoaded() {
    if (++count == 3) {
        frame = document.getElementById("iframe1_id");
        if (frame && !frame.contentDocument.body.innerHTML)
            frame.contentDocument.body.innerHTML = "<p>iframe1</p><img src='image.png'>";
        window.webkit.messageHandlers.testHandler.postMessage("done");
    }
}
</script>
<iframe onload="onFrameLoaded();" id="iframe1_id"></iframe>
<iframe onload="onFrameLoaded();" src="iframe.html"></iframe>
<iframe onload="onFrameLoaded();" srcdoc="<p>iframe3</p><img src='image.png'>"></iframe>
)TESTRESOURCE";
static const char* iframeHTMLDataBytes = R"TESTRESOURCE(
<head>
<link rel="stylesheet" href="style.css">
</head>
<p>iframe2</p>
<img src='image.png'>
)TESTRESOURCE";
static const char* cssDataBytesForIframe = R"TESTRESOURCE(
div {
    height: 50%;
    width: 50%;
}
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesIframe)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForIframe length:strlen(htmlDataBytesForIframe)];
    NSData *iframeHTMLData = [NSData dataWithBytes:iframeHTMLDataBytes length:strlen(iframeHTMLDataBytes)];
    NSData *cssData = [NSData dataWithBytes:cssDataBytesForIframe length:strlen(cssDataBytesForIframe)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/iframe.html"]) {
            mimeType = @"text/html";
            data = iframeHTMLData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/style.css"]) {
            mimeType = @"text/css";
            data = cssData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResourceString = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *savedResourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(5llu, savedResourceFileNames.count);

        unsigned frameFileCount = 0;
        unsigned imageFileCount = 0;
        unsigned cssFileCount = 0;
        NSMutableSet *frameResourceContentsToFind = [NSMutableSet set];
        [frameResourceContentsToFind addObjectsFromArray:[NSArray arrayWithObjects:@"iframe1", @"iframe2", @"iframe3", nil]];
        for (NSString *fileName in savedResourceFileNames) {
            if ([fileName containsString:@"frame_"]) {
                // Ensure urls are replaced with file path.
                NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
                EXPECT_TRUE([savedMainResourceString containsString:replacementPath]);
                // Ensure all iframes are saved by looking at content.
                NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
                NSString* savedSubframeResourceString = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
                NSRange range = [savedSubframeResourceString rangeOfString:@"iframe"];
                EXPECT_NE(NSNotFound, (long)range.location);
                NSString *iframeContent = [savedSubframeResourceString substringWithRange:NSMakeRange(range.location, range.length + 1)];
                [frameResourceContentsToFind removeObject:iframeContent];
                ++frameFileCount;
            }

            if ([fileName isEqualToString:@"image.png"])
                ++imageFileCount;
            if ([fileName isEqualToString:@"style.css"])
                ++cssFileCount;
        }
        EXPECT_EQ(3u, frameFileCount);
        EXPECT_EQ(1u, imageFileCount);
        EXPECT_EQ(1u, cssFileCount);
        EXPECT_EQ(0u, frameResourceContentsToFind.count);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForFrame = R"TESTRESOURCE(
<head>
<script>
count = 0;
function onFramesLoaded() {
    frame = document.getElementById("frame1_id");
    if (frame && !frame.contentDocument.body.innerHTML)
        frame.contentDocument.body.innerHTML = "<p>thisisframe1</p><img src='image.png'>";
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
</script>
</head>
<frameset cols="50%, 50%" onload="onFramesLoaded()">
<frame id="frame1_id">
<frame src="frame.html">
</frameset>
)TESTRESOURCE";
static const char* frameHTMLDataBytes = R"TESTRESOURCE(<p>thisisframe2</p><img src='image.png'>)TESTRESOURCE";

TEST(WebArchive, SaveResourcesFrame)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForFrame length:strlen(htmlDataBytesForFrame)];
    NSData *frameHTMLData = [NSData dataWithBytes:frameHTMLDataBytes length:strlen(frameHTMLDataBytes)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/frame.html"]) {
            mimeType = @"text/html";
            data = frameHTMLData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(3llu, resourceFileNames.count);

        unsigned frameFileCount = 0;
        unsigned imageFileCount = 0;
        NSMutableSet *frameResourceContentsToFind = [NSMutableSet set];
        [frameResourceContentsToFind addObjectsFromArray:[NSArray arrayWithObjects:@"thisisframe1", @"thisisframe2", nil]];
        for (NSString *fileName in resourceFileNames) {
            if ([fileName containsString:@"frame_"]) {
                NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
                EXPECT_TRUE([savedMainResource containsString:replacementPath]);
                NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
                NSString* savedSubframeResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
                NSRange range = [savedSubframeResource rangeOfString:@"thisisframe"];
                EXPECT_NE(NSNotFound, (long)range.location);
                NSString *frameContent = [savedSubframeResource substringWithRange:NSMakeRange(range.location, range.length + 1)];
                [frameResourceContentsToFind removeObject:frameContent];
                ++frameFileCount;
            }

            if ([fileName isEqualToString:@"image.png"])
                ++imageFileCount;
        }
        EXPECT_EQ(2u, frameFileCount);
        EXPECT_EQ(1u, imageFileCount);
        EXPECT_EQ(0u, frameResourceContentsToFind.count);
        saved = true;
    }];
    Util::run(&saved);
}

TEST(WebArchive, SaveResourcesValidFileName)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];

    NSMutableString *mutableFileName = [NSMutableString stringWithCapacity:256];
    for (unsigned i = 0; i < 256; ++i)
        [mutableFileName appendString:@"x"];
    NSArray *tests = [NSArray arrayWithObjects:[NSString stringWithString:mutableFileName], @"a/image.png", @"b/image.png", @"c/image.png", @"d/image.png", @"image.png(1)", @"webarchivetest://host/file:image.png", @"image1/", @"image2///", @"image3.png/./", @"image4/content/../", @"image5%20file.png", @"image 6.png", nil];
    NSMutableString *mutableHTMLString = [NSMutableString string];
    NSString *styleString = @"<link rel='stylesheet' href='imagestyle'><link rel='stylesheet' href='style.css'><link rel='stylesheet' href='style'>";
    NSString *scriptString = [NSString stringWithFormat:@"<script>count = 0; function onImageLoad() { if (++count == %d) window.webkit.messageHandlers.testHandler.postMessage('done'); }</script>", (int)tests.count];
    [mutableHTMLString appendString:styleString];
    [mutableHTMLString appendString:scriptString];
    for (NSString *item in tests)
        [mutableHTMLString appendString:[NSString stringWithFormat:@"<img src='%@' onload='onImageLoad()'>", item]];
    NSData *htmlData = [[NSString stringWithString:mutableHTMLString] dataUsingEncoding:NSUTF8StringEncoding];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    NSString *cssString = @"img { width: 10px; }";
    NSData *cssData = [cssString dataUsingEncoding:NSUTF8StringEncoding];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString containsString:@"style"]) {
            mimeType = @"text/css";
            data = cssData;
        } else {
            mimeType = @"image/png";
            data = imageData;
        }
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

    NSSet *expectedFileNames = [NSSet setWithArray:[NSArray arrayWithObjects:[mutableFileName substringFromIndex:1], @"image.png", @"image.png-1", @"image.png-2", @"image.png-3", @"image.png-1-", @"file-image.png", @"image1", @"file", @"image3.png", @"image4", @"image5-file.png", @"image-6.png", @"imagestyle.css", @"style.css", @"style-1.css", nil]];
    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *resourceDirectoryName = @"host_files";
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:[directoryURL URLByAppendingPathComponent:resourceDirectoryName].path error:nil];
        NSSet *savedFileNames = [NSSet setWithArray:resourceFileNames];
        EXPECT_TRUE([savedFileNames isEqualToSet:expectedFileNames]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForBlobURL = R"TESTRESOURCE(
<body>
<script>
function onImageLoad() {
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
async function fetchImage() {
    response = await fetch("image.png");
    blob = await response.blob();
    url = URL.createObjectURL(blob);

    frame = document.createElement("iframe");
    frame.src = url;
    document.body.appendChild(frame);

    img = document.createElement('img');
    img.onload = onImageLoad;
    img.src = url;
    document.body.appendChild(img);
}

function createIframe() {
    htmlString = "<p>thisisframe</p>";
    blob = new Blob([htmlString], { type: 'text/html' });
    url = URL.createObjectURL(blob);
    frame = document.createElement("iframe");
    frame.onload = fetchImage;
    frame.src = url;
    document.body.appendChild(frame);
}
createIframe();
</script>
</body>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesBlobURL)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];

    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForBlobURL length:strlen(htmlDataBytesForBlobURL)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(2llu, resourceFileNames.count);

        for (NSString *fileName in resourceFileNames) {
            NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
            if ([fileName containsString:@"frame_"]) {
                // HTML file from the first iframe element.
                EXPECT_TRUE([savedMainResource containsString:replacementPath]);
                NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
                NSString* savedSubframeResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
                EXPECT_TRUE([savedSubframeResource containsString:@"thisisframe"]);
            } else {
                // Image file from img and the second iframe element.
                unsigned replacementPathCount = 0;
                NSString *savedMainResourceCopy = [NSString stringWithString:savedMainResource];
                NSRange range = [savedMainResourceCopy rangeOfString:replacementPath];
                while (range.location != NSNotFound) {
                    ++replacementPathCount;
                    savedMainResourceCopy = [savedMainResourceCopy substringFromIndex:range.location + 1];
                    range = [savedMainResourceCopy rangeOfString:replacementPath];
                }
                EXPECT_EQ(2u, replacementPathCount);
            }
        }
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForResponsiveImages = R"TESTRESOURCE(
<head>
<script>
const dpr = window.devicePixelRatio;
// Use image-set for div.
style = document.createElement('style');
style.innerHTML = "div { background-image:image-set('div1.png' " + dpr + "x, 'div2.png' " + (dpr + 1) + "x)}";
document.getElementsByTagName("head")[0].appendChild(style);
</script>
</head>
<body>
<div id="div">Hello</div>
<script>
count = 0;
function addSrcsetToImage() {
    image = document.getElementById("image");
    image.removeAttribute("src");
    image.srcset = "image2.png " + dpr + "x, image1.png " + (dpr + 1) + "x";
}
function onImageLoad() {
    ++count;
    if (count == 2) // image1.png and picture2.png are loaded.
        addSrcsetToImage();
    if (count == 3) // image2.png is loaded.
        window.webkit.messageHandlers.testHandler.postMessage("done");
}
</script>
<img id="image" src="image1.png" onload="onImageLoad()">
<picture id="picture">
    <source srcset="picture1.png" media="(min-width: 3000px)">
    <img src="picture2.png" onload="onImageLoad()">
</picture>
</body>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesResponsiveImages)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForResponsiveImages length:strlen(htmlDataBytesForResponsiveImages)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString containsString:@"png"]) {
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);
        NSString *savedMainResourceString = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        NSSet *expectedFileNames = [NSSet setWithArray:[NSArray arrayWithObjects:@"image1.png", @"image2.png", @"picture2.png", @"div1.png", nil]];
        NSSet *savedFileNames = [NSSet setWithArray:resourceFileNames];
        EXPECT_TRUE([savedFileNames isEqualToSet:expectedFileNames]);

        for (NSString *fileName in resourceFileNames) {
            NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
            EXPECT_TRUE([savedMainResourceString containsString:replacementPath]);
        }

        saved = true;
    }];
    Util::run(&saved);
}

static const char* hTMLDataBytesForDataURL = R"TESTRESOURCE(
<script>
count = 0;
function onLoad() {
    if (++count == 3)
        window.webkit.messageHandlers.testHandler.postMessage("done");
}
</script>
<img onload="onLoad()" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAZAAAAGQCAIAAAAP3aGbAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QcOEyQ6arfsmQAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAD5ElEQVR42u3UMQ0AAAgEsQflSEcFA0kr4YarTABeaAkAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIMSwLAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC+DSAp/KA6AWm53xAAAAAElFTkSuQmCC">
<iframe onload="onLoad()" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAZAAAAGQCAIAAAAP3aGbAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QcOEyQ6arfsmQAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAD5ElEQVR42u3UMQ0AAAgEsQflSEcFA0kr4YarTABeaAkAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIMSwLAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC8CwAAwLMCwAwwIwLMCwAAwLwLAAwwIwLADDAgwLwLAADAswLADDAgwLwLAADAswLADDAjAswLAADAvAsADDAjAsAMMCDAvAsAAMCzAsAMMCMCzAsAAMC8CwAMMCMCwAwwIMC+DSAp/KA6AWm53xAAAAAElFTkSuQmCC"></iframe>
<iframe onload="onLoad()" src="data:text/html,<img src='webarchivetest://host/image.png'>"></iframe>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesDataURL)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];

    NSData *htmlData = [NSData dataWithBytes:hTMLDataBytesForDataURL length:strlen(hTMLDataBytesForDataURL)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:nil];
        EXPECT_EQ(3llu, resourceFileNames.count);

        unsigned frameFileCount = 0;
        unsigned imageFileCount = 0;
        unsigned frameWithDataURLCount = 0;
        for (NSString *fileName in resourceFileNames) {
            if ([fileName containsString:@"frame_"]) {
                NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
                EXPECT_TRUE([savedMainResource containsString:replacementPath]);
                NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
                NSString* savedSubframeResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
                if ([savedSubframeResource containsString:@"data:image/png"])
                    ++frameWithDataURLCount;
                ++frameFileCount;
            }

            if ([fileName containsString:@".png"])
                ++imageFileCount;
        }
        EXPECT_EQ(2u, frameFileCount);
        EXPECT_EQ(1u, imageFileCount);
        EXPECT_EQ(1u, frameWithDataURLCount);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForIframeInIframe = R"TESTRESOURCE(
<script>
count = 0;
function frameLoaded() {
    if (++count == 2)
        window.webkit.messageHandlers.testHandler.postMessage("done");
}
window.addEventListener('message', function(event) {
    if (event.data == 'loaded')
        frameLoaded();
});
</script>
<iframe onload="frameLoaded();" src="iframe1.html"></iframe>
)TESTRESOURCE";
static const char* iframe1HTMLDataBytes = R"TESTRESOURCE(
<script>
function frameLoaded() {
    window.parent.postMessage("loaded", "*");
}
</script>
<p>thisisiframe1</p>
<iframe onload="frameLoaded();" src="iframe2.html"></iframe>
)TESTRESOURCE";
static const char* iframe2HTMLDataBytes = R"TESTRESOURCE(<p>thisisiframe2</p>)TESTRESOURCE";

TEST(WebArchive, SaveResourcesIframeInIframe)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForIframeInIframe length:strlen(htmlDataBytesForIframeInIframe)];
    NSData *iframe1HTMLData = [NSData dataWithBytes:iframe1HTMLDataBytes length:strlen(iframe1HTMLDataBytes)];
    NSData *iframe2HTMLData = [NSData dataWithBytes:iframe2HTMLDataBytes length:strlen(iframe2HTMLDataBytes)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/iframe1.html"]) {
            mimeType = @"text/html";
            data = iframe1HTMLData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/iframe2.html"]) {
            mimeType = @"text/html";
            data = iframe2HTMLData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(2llu, resourceFileNames.count);

        NSMutableSet *frameResourceContentsToFind = [NSMutableSet set];
        [frameResourceContentsToFind addObjectsFromArray:[NSArray arrayWithObjects:@"thisisiframe1", @"thisisiframe2", nil]];
        for (NSString *fileName in resourceFileNames) {
            EXPECT_TRUE([fileName containsString:@"frame_"]);
            NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
            NSString* savedSubframeResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
            NSRange range = [savedSubframeResource rangeOfString:@"thisisiframe"];
            EXPECT_NE(NSNotFound, (long)range.location);
            NSString *iframeContent = [savedSubframeResource substringWithRange:NSMakeRange(range.location, range.length + 1)];
            if ([iframeContent isEqualToString:@"thisisiframe1"]) {
                // Main resources of iframes are stored in the same directory, so replaced url should not contain the directory name.
                EXPECT_FALSE([savedSubframeResource containsString:resourceDirectoryName]);
            }
            [frameResourceContentsToFind removeObject:iframeContent];
        }
        EXPECT_EQ(0u, frameResourceContentsToFind.count);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForIframesWithSameURL = R"TESTRESOURCE(
<script>
count = 0;
messageCount = 0;
function frameLoaded() {
    if (++count == 3) {
        const iframes = document.getElementsByTagName("iframe");
        for (let i = 0; i < iframes.length; ++i)
            iframes[i].contentWindow.postMessage("thisisiframe" + (i + 1), "*");
    }
}
window.addEventListener("message", function(event) {
    if (++messageCount == 3)
        window.webkit.messageHandlers.testHandler.postMessage("done");
});
</script>
<iframe onload="frameLoaded();" src="iframe.html"></iframe>
<iframe onload="frameLoaded();" src="iframe.html"></iframe>
<iframe onload="frameLoaded();" src="iframe.html"></iframe>
)TESTRESOURCE";
static const char* iframeHTMLDataBytesForIframesWithSameURL = R"TESTRESOURCE(
<head>
<script>
window.addEventListener("message", function(event) {
    if (!document.body.innerHTML)
        document.body.innerHTML = "<p>" + event.data + "</p>";
    window.parent.postMessage("done", "*");
});
</script>
</head>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesIframesWithSameURL)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForIframesWithSameURL length:strlen(htmlDataBytesForIframesWithSameURL)];
    NSData *iframeHTMLData = [NSData dataWithBytes:iframeHTMLDataBytesForIframesWithSameURL length:strlen(iframeHTMLDataBytesForIframesWithSameURL)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/iframe.html"]) {
            mimeType = @"text/html";
            data = iframeHTMLData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(3llu, resourceFileNames.count);

        NSMutableSet *frameResourceContentsToFind = [NSMutableSet set];
        [frameResourceContentsToFind addObjectsFromArray:[NSArray arrayWithObjects:@"thisisiframe1", @"thisisiframe2", @"thisisiframe3", nil]];
        for (NSString *fileName in resourceFileNames) {
            EXPECT_TRUE([fileName containsString:@"frame_"]);
            NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
            NSString* savedSubframeResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
            NSRange range = [savedSubframeResource rangeOfString:@"thisisiframe"];
            EXPECT_NE(NSNotFound, (long)range.location);
            NSString *iframeContent = [savedSubframeResource substringWithRange:NSMakeRange(range.location, range.length + 1)];
            [frameResourceContentsToFind removeObject:iframeContent];
        }
        EXPECT_EQ(0u, frameResourceContentsToFind.count);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForShadowDOM = R"TESTRESOURCE(
<button id="button" onclick="buttonClicked()"></button>
<script>
function buttonClicked() {
    div1 = document.createElement("div");
    div1.id = "div1";
    root1 = div1.attachShadow({ mode: "open" });
    root1.innerHTML = "ShadowRoot1";
    div2 = document.createElement("div");
    div2.id = "div2";
    root2 = div2.attachShadow({ mode: "closed" });
    root2.innerHTML = "ShadowRoot2<slot name='text'></slot>";
    span = document.createElement("span");
    span.slot= "text";
    span.innerHTML = "Text";
    root2.appendChild(span);
    root1.appendChild(div2);
    var button = document.getElementById("button");
    button.parentNode.removeChild(button);
    document.body.appendChild(div1);
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
window.webkit.messageHandlers.testHandler.postMessage("start");
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesShadowDOM)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForShadowDOM length:strlen(htmlDataBytesForShadowDOM)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else
            FAIL();

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    static bool messageReceived = false;
    [webView performAfterReceivingMessage:@"start" action:[&] {
        messageReceived = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];
    Util::run(&messageReceived);

    messageReceived = false;
    [webView performAfterReceivingMessage:@"done" action:[&] {
        messageReceived = true;
    }];
    [webView evaluateJavaScript:@"document.getElementById('button').click();" completionHandler:nil];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);
        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedMainResource containsString:@"<div id=\"div1\"><template shadowrootmode=\"open\">ShadowRoot1<div id=\"div2\"><template shadowrootmode=\"closed\">ShadowRoot2<slot name=\"text\"></slot><span slot=\"text\">Text</span></template></div></template></div>"]);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForDeclarativeShadowDOM = R"TESTRESOURCE(
<script>
function loaded() {
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
</script>
<body onload="loaded()">
<div>
<template shadowroot="open"><slot></slot></template><p>ShadowDOM</p><button>Button</button>
</div>
</body>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesDeclarativeShadowDOM)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForDeclarativeShadowDOM length:strlen(htmlDataBytesForDeclarativeShadowDOM)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedMainResource containsString:@"<template shadowroot=\"open\"><slot></slot></template><p>ShadowDOM</p><button>Button</button>"]);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForStyle = R"TESTRESOURCE(
<style>
@font-face {
    font-family: "WebFont";
    src: url("Ahem-10000A.ttf");
}
div {
    height: 50%;
    width: 50%;
    font-family: "WebFont";
    background-image: url("image.png");
}
</style>
<div id="div">Hello</div>
<script>
    div = document.getElementById("div");
    imageURL = getComputedStyle(div).backgroundImage;
    document.fonts.ready.then(() => { window.webkit.messageHandlers.testHandler.postMessage("done"); });
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesStyle)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForStyle length:strlen(htmlDataBytesForStyle)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    NSData *fontData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"Ahem-10000A" withExtension:@"ttf" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/Ahem-10000A.ttf"]) {
            mimeType = @"font/ttf";
            data = fontData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(2llu, resourceFileNames.count);

        for (NSString *fileName in resourceFileNames) {
            NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
            EXPECT_TRUE([savedMainResource containsString:replacementPath]);
        }

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForInlineStyle = R"TESTRESOURCE(
<div id="div" style="background-image:url('image.png');width:50%;height:50%;color:red">Hello</div>
<script>
    div = document.getElementById("div");
    div.style.color = "fuchsia";
    imageURL = getComputedStyle(div).backgroundImage;
    window.webkit.messageHandlers.testHandler.postMessage("done");
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesInlineStyle)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForInlineStyle length:strlen(htmlDataBytesForInlineStyle)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedMainResource containsString:@"color: fuchsia"]);

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(1llu, resourceFileNames.count);

        for (NSString *fileName in resourceFileNames) {
            NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
            EXPECT_TRUE([savedMainResource containsString:replacementPath]);
        }

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForLink = R"TESTRESOURCE(
<head>
<link rel="manifest" href="manifest.json">
<link rel="preload" href="Ahem-10000A.ttf" as="font" type="font/ttf">
<link rel="stylesheet" href="style.css">
</head>
<div><p id="console">Hello</p></div>
<script>
img = null;
function onImageLoad() {
    img = null;
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
document.styleSheets[0].insertRule("p { color: fuchsia; }");
console = document.getElementById("console");
computedStyle = getComputedStyle(console);
var img = document.createElement("img");
img.src = "files/image.png";
img.onload = onImageLoad;
</script>
)TESTRESOURCE";
static const char* cssDataBytesForLink = R"TESTRESOURCE(
div {
    height: 50%;
    width: 50%;
    background-image: url("files/image.png");
}
)TESTRESOURCE";
static const char* manifestDataBytesForLink = R"TESTRESOURCE(
{
    "name": "webarchivetest",
    "short_name": "wat",
    "description": "This is a test app."
}
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesLink)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForLink length:strlen(htmlDataBytesForLink)];
    NSData *cssData = [NSData dataWithBytes:cssDataBytesForLink length:strlen(cssDataBytesForLink)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    NSData *manifestData = [NSData dataWithBytes:manifestDataBytesForLink length:strlen(manifestDataBytesForLink)];
    NSData *fontData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"Ahem-10000A" withExtension:@"ttf" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/files/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/style.css"]) {
            mimeType = @"text/css";
            data = cssData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/manifest.json"]) {
            mimeType = @"application/json";
            data = manifestData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/Ahem-10000A.ttf"]) {
            mimeType = @"font/ttf";
            data = fontData;
        }

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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);
        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath =[directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:nil];
        NSSet *savedFileNames = [NSSet setWithArray:resourceFileNames];
        NSSet *expectedFileNames = [NSSet setWithArray:[NSArray arrayWithObjects:@"image.png", @"style.css", @"manifest.json", @"Ahem-10000A.ttf", nil]];
        EXPECT_TRUE([savedFileNames isEqualToSet:expectedFileNames]);
        for (NSString *savedFileName in resourceFileNames) {
            // image.png is not subresource in main resource file.
            if ([savedFileName isEqualToString:@"image.png"])
                continue;

            NSString *savedFileRelativePath = [resourceDirectoryName stringByAppendingPathComponent:savedFileName];
            EXPECT_TRUE([savedMainResource containsString:savedFileRelativePath]);
        }

        NSString *styleResourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:@"style.css"];
        NSString *savedStyleResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:styleResourceFilePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedStyleResource containsString:@"url(\"image.png\")"]);
        EXPECT_TRUE([savedStyleResource containsString:@"color: fuchsia"]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForEmptyStyleSheet = R"TESTRESOURCE(
<head>
<link rel="stylesheet" href="empty.css">
</head>
<script>
window.webkit.messageHandlers.testHandler.postMessage("done");
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesEmptyStyleSheet)
{
    RetainPtr directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    RetainPtr htmlData = [NSData dataWithBytes:htmlDataBytesForEmptyStyleSheet length:strlen(htmlDataBytesForEmptyStyleSheet)];
    RetainPtr cssData = adoptNS([[NSData alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData.get();
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/empty.css"]) {
            mimeType = @"text/css";
            data = cssData.get();
        }

        RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    static bool messageReceived = false;
    [webView performAfterReceivingMessage:@"done" action:[&] {
        messageReceived = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);
        RetainPtr savedMainResource = adoptNS([[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding]);

        NSString *resourceDirectoryName = @"host_files";
        NSString *styleReourceName = @"empty.css";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSString *styleResourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:styleReourceName];
        EXPECT_TRUE([fileManager fileExistsAtPath:styleResourceFilePath]);
        RetainPtr savedStyleResource = adoptNS([[NSString alloc] initWithData:[NSData dataWithContentsOfFile:styleResourceFilePath] encoding:NSUTF8StringEncoding]);
        EXPECT_EQ([savedStyleResource length], 0u);

        NSString *styleResourceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:styleReourceName];
        EXPECT_TRUE([savedMainResource.get() containsString:styleResourceRelativePath]);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForLinksWithSameURL = R"TESTRESOURCE(
<head>
<link href="style.css" rel="stylesheet">
<link href="style.css" rel="stylesheet">
</head>
<div>Hello</div>
<p id="console">World</p>
<script>
document.styleSheets[0].deleteRule(0);
document.styleSheets[0].insertRule("p { color: gray; }");
console = document.getElementById("console");
computedStyle = getComputedStyle(console);
window.webkit.messageHandlers.testHandler.postMessage("done");
</script>
)TESTRESOURCE";
static const char* cssDataBytesForLinksWithSameURL = R"TESTRESOURCE(
div {
    color: blue;
}
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesLinksWithSameURL)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForLinksWithSameURL length:strlen(htmlDataBytesForLinksWithSameURL)];
    NSData *cssData = [NSData dataWithBytes:cssDataBytesForLinksWithSameURL length:strlen(cssDataBytesForLinksWithSameURL)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/style.css"]) {
            mimeType = @"text/css";
            data = cssData;
        }

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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);
        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath =[directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:nil];
        EXPECT_EQ(2llu, resourceFileNames.count);

        NSMutableSet *styleResourceContentsToFind = [NSMutableSet set];
        [styleResourceContentsToFind addObjectsFromArray:[NSArray arrayWithObjects:@"color: blue", @"color: fuchsia", nil]];
        for (NSString *fileName in resourceFileNames) {
            NSString *replacementPath = [resourceDirectoryName stringByAppendingPathComponent:fileName];
            EXPECT_TRUE([savedMainResource containsString:replacementPath]);
            NSString *resourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:fileName];
            NSString* savedStyleResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:resourceFilePath] encoding:NSUTF8StringEncoding];
            NSRange range = [savedStyleResource rangeOfString:@"color: "];
            EXPECT_NE(NSNotFound, (long)range.location);
            NSString *styleResourceContent = [savedStyleResource substringWithRange:NSMakeRange(range.location, range.length + 4)];
            [styleResourceContentsToFind removeObject:styleResourceContent];
        }

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForCSSImportRule = R"TESTRESOURCE(
<head>
<style>
@import url("style.css");
</style>
</head>
<div><p id="console">Hello</p></div>
<script>
img = null;
function onImageLoad() {
    img = null;
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
document.styleSheets[0].cssRules[0].styleSheet.insertRule("p { color: fuchsia; }");
console = document.getElementById("console");
computedStyle = getComputedStyle(console);
var img = document.createElement("img");
img.src = "files/image.png";
img.onload = onImageLoad;
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesCSSImportRule)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForCSSImportRule length:strlen(htmlDataBytesForCSSImportRule)];
    NSData *cssData = [NSData dataWithBytes:cssDataBytesForLink length:strlen(cssDataBytesForLink)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/files/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/style.css"]) {
            mimeType = @"text/css";
            data = cssData;
        }

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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);
        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath =[directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:nil];
        NSSet *savedFileNames = [NSSet setWithArray:resourceFileNames];
        NSSet *expectedFileNames = [NSSet setWithArray:[NSArray arrayWithObjects:@"image.png", @"style.css", nil]];
        EXPECT_TRUE([savedFileNames isEqualToSet:expectedFileNames]);

        NSString *styleResourceFileName = [resourceDirectoryName stringByAppendingPathComponent:@"style.css"];
        EXPECT_TRUE([savedMainResource containsString:styleResourceFileName]);
        NSString *styleResourceFilePath = [resourceDirectoryPath stringByAppendingPathComponent:@"style.css"];
        NSString *savedStyleResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:styleResourceFilePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedStyleResource containsString:@"url(\"image.png\")"]);
        EXPECT_TRUE([savedStyleResource containsString:@"color: fuchsia"]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForCSSSupportsRule = R"TESTRESOURCE(
<head>
<style>
@supports (background-image: url()) {
    div {
        background-image: url("image.png");
    }
}
</style>
</head>
<div>Hello</div>
<script>
img = null;
function onImageLoad() {
    img = null;
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
var img = document.createElement("img");
img.src = "image.png";
img.onload = onImageLoad;
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesCSSSupportsRule)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForCSSSupportsRule length:strlen(htmlDataBytesForCSSSupportsRule)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        }

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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *imageFile = @"image.png";
        NSString *resourceDirectoryName = @"host_files";
        NSString *imageResouceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:imageFile];
        EXPECT_TRUE([savedMainResource containsString:imageResouceRelativePath]);

        NSString *imageResourcePath = [directoryURL URLByAppendingPathComponent:imageResouceRelativePath].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:imageResourcePath]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForCSSMediaRule = R"TESTRESOURCE(
<head>
<style>
@media (min-width: 1px) {
    div {
        background-image: url("image.png");
    }
}
</style>
</head>
<div>Hello</div>
<script>
img = null;
function onImageLoad() {
    img = null;
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
var img = document.createElement("img");
img.src = "image.png";
img.onload = onImageLoad;
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesCSSMediaRule)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    RetainPtr htmlData = [NSData dataWithBytes:htmlDataBytesForCSSMediaRule length:strlen(htmlDataBytesForCSSMediaRule)];
    RetainPtr imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData.get();
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/image.png"]) {
            mimeType = @"image/png";
            data = imageData.get();
        }

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    static bool messageReceived = false;
    [webView performAfterReceivingMessage:@"done" action:[&] {
        messageReceived = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *imageFile = @"image.png";
        NSString *resourceDirectoryName = @"host_files";
        NSString *imageResouceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:imageFile];
        EXPECT_TRUE([savedMainResource containsString:imageResouceRelativePath]);

        NSString *imageResourcePath = [directoryURL URLByAppendingPathComponent:imageResouceRelativePath].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:imageResourcePath]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForCrossOriginLink = R"TESTRESOURCE(
<head>
<link href="webarchivetest://resource.com/style.css" rel="stylesheet">
</head>
<div id="console">Hello</div>
<script>
console = document.getElementById("console");
computedStyle = getComputedStyle(console);
window.webkit.messageHandlers.testHandler.postMessage("done");
</script>
)TESTRESOURCE";
static const char* cssDataBytesForCrossOriginLink = R"TESTRESOURCE(
div {
    height: 50%;
    width: 50%;
}
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesCrossOriginLink)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForCrossOriginLink length:strlen(htmlDataBytesForCrossOriginLink)];
    NSData *cssData = [NSData dataWithBytes:cssDataBytesForCrossOriginLink length:strlen(cssDataBytesForCrossOriginLink)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host.com/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://resource.com/style.css"]) {
            mimeType = @"text/css";
            data = cssData;
        }

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
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host.com/main.html"]]];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *cssFile = @"style.css";
        NSString *resourceDirectoryName = @"host_files";
        NSString *cssResourceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:cssFile];
        EXPECT_TRUE([savedMainResource containsString:cssResourceRelativePath]);

        NSString *cssResourcePath = [directoryURL URLByAppendingPathComponent:cssResourceRelativePath].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:cssResourcePath]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForExcludeBaseElement = R"TESTRESOURCE(
<head>
<base href="webarchivetest://resource.com" target="_blank">
</head>
<img src="image.png" onload="onImageLoad()">
<a href="/link">Open Link</a>
<script>
function onImageLoad() {
    window.webkit.messageHandlers.testHandler.postMessage("done");
}
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesExcludeBaseElement)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForExcludeBaseElement length:strlen(htmlDataBytesForExcludeBaseElement)];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host.com/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://resource.com/image.png"]) {
            mimeType = @"image/png";
            data = imageData;
        }

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
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host.com/main.html"]]];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        NSString *imageFile = @"image.png";
        NSString *resourceDirectoryName = @"host_files";
        NSString *imageResourceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:imageFile];
        EXPECT_TRUE([savedMainResource containsString:imageResourceRelativePath]);
        EXPECT_FALSE([savedMainResource containsString:@"base"]);
        // The anchor element's url should be resolved.
        EXPECT_TRUE([savedMainResource containsString:@"webarchivetest://resource.com/link"]);

        NSString *imageResourcePath = [directoryURL URLByAppendingPathComponent:imageResourceRelativePath].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:imageResourcePath]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForExclusionRules = R"TESTRESOURCE(
<a href="foo.html" target="_blank">Blank</a>
<a href="#bar" target="_self">Self</a>
<textarea disabled hidden>Text</textarea>
<button disabled>Button</button>
<script>
window.webkit.messageHandlers.testHandler.postMessage("done");
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesExclusionRules)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForExclusionRules length:strlen(htmlDataBytesForExclusionRules)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        }

        EXPECT_TRUE(data);
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
    RetainPtr<_WKArchiveConfiguration> archiveConfiguration = adoptNS([[_WKArchiveConfiguration alloc] init]);
    archiveConfiguration.get().directory = directoryURL.get();
    archiveConfiguration.get().suggestedFileName = @"host";
    archiveConfiguration.get().exclusionRules = @[
        adoptNS([[_WKArchiveExclusionRule alloc] initWithElementLocalName:@"script" attributeLocalNames:nil attributeValues:nil]).get(),
        adoptNS([[_WKArchiveExclusionRule alloc] initWithElementLocalName:nil attributeLocalNames:@[@"disabled", @"hidden"] attributeValues:@[@"", @""]]).get(),
        adoptNS([[_WKArchiveExclusionRule alloc] initWithElementLocalName:@"a" attributeLocalNames:@[@"target"] attributeValues:@[@"_blank"]]).get(),
    ];
    [webView _archiveWithConfiguration:archiveConfiguration.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        EXPECT_FALSE([savedMainResource containsString:@"script"]);
        EXPECT_FALSE([savedMainResource containsString:@"hidden"]);
        EXPECT_FALSE([savedMainResource containsString:@"target=\"_blank\""]);
        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForExcludeCrossOriginAttribute = R"TESTRESOURCE(
<head>
<link href="webarchivetest://resource.com/style.css" rel="stylesheet" crossorigin="anonymous">
</head>
<div id="console"></div>
<script src="webarchivetest://resource.com/script.js" integrity="sha256-whXcIuErT+KLyiIBuDxrli97oBljDS1fLlyogfljnnM=" crossorigin="anonymous"></script>
<script>
div = document.getElementById("console");
div.innerHTML += getComputedStyle(div).width;
window.webkit.messageHandlers.testHandler.postMessage("done");
</script>
)TESTRESOURCE";

static const char* scriptDataBytesForExcludeCrossOriginAttribute = R"TESTRESOURCE(document.getElementById("console").innerHTML = "ScriptRuns";)TESTRESOURCE";
static const char* cssDataBytesForExcludeCrossOriginAttribute = R"TESTRESOURCE(div { width: 10px; })TESTRESOURCE";

TEST(WebArchive, SaveResourcesExcludeCrossOriginAttribute)
{
    RetainPtr<NSURL> directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    NSData *htmlData = [NSData dataWithBytes:htmlDataBytesForExcludeCrossOriginAttribute length:strlen(htmlDataBytesForExcludeCrossOriginAttribute)];
    NSData *scriptData = [NSData dataWithBytes:scriptDataBytesForExcludeCrossOriginAttribute length:strlen(scriptDataBytesForExcludeCrossOriginAttribute)];
    NSData *cssData = [NSData dataWithBytes:cssDataBytesForExcludeCrossOriginAttribute length:strlen(cssDataBytesForExcludeCrossOriginAttribute)];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        bool shouldAddAccessControlHeader = false;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://resource.com/script.js"]) {
            mimeType = @"application/javascript";
            data = scriptData;
            shouldAddAccessControlHeader = true;
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://resource.com/style.css"]) {
            mimeType = @"text/css";
            data = cssData;
            shouldAddAccessControlHeader = true;
        }
        EXPECT_TRUE(data);

        RetainPtr<NSMutableDictionary> headerFields = adoptNS(@{
            @"Content-Length": [NSString stringWithFormat:@"%zu", (size_t)data.length],
            @"Content-Type": mimeType,
        }.mutableCopy);
        if (shouldAddAccessControlHeader)
            [headerFields setObject:@"*" forKey:@"Access-Control-Allow-Origin"];

        auto response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:200 HTTPVersion:nil headerFields:headerFields.get()]);
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
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedMainResource containsString:@"ScriptRuns"]);
        EXPECT_TRUE([savedMainResource containsString:@"10px"]);
        EXPECT_FALSE([savedMainResource containsString:@"integrity"]);
        EXPECT_FALSE([savedMainResource containsString:@"crossorigin"]);

        NSString *resourceDirectoryName = @"host_files";
        NSString *scriptFile = @"script.js";
        NSString *scriptResourceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:scriptFile];
        EXPECT_TRUE([savedMainResource containsString:scriptResourceRelativePath]);
        NSString *scriptResourcePath = [directoryURL URLByAppendingPathComponent:scriptResourceRelativePath].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:scriptResourcePath]);

        NSString *cssFile = @"style.css";
        NSString *cssResourceRelativePath = [resourceDirectoryName stringByAppendingPathComponent:cssFile];
        EXPECT_TRUE([savedMainResource containsString:cssResourceRelativePath]);
        NSString *cssResourcePath = [directoryURL URLByAppendingPathComponent:scriptResourceRelativePath].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:cssResourcePath]);

        saved = true;
    }];
    Util::run(&saved);
}

static const char* htmlDataBytesForUnsavedSubresources = R"TESTRESOURCE(
<style>
@font-face {
    font-family: "WebFont";
    src: url("Ahem-10000A.ttf") format("truetype"), url("Ahem-10000A-backup.ttf") format("truetype")
}
div {
    width: 100%;
    font-family: "WebFont";
}
</style>
<div id="div">Hello</div>
<script>
    div = document.getElementById("div");
    computedFontFamily = getComputedStyle(div).getPropertyValue("font-family");
    document.fonts.ready.then(() => { window.webkit.messageHandlers.testHandler.postMessage("done"); });
</script>
)TESTRESOURCE";

TEST(WebArchive, SaveResourcesStyleWithUnloadedResources)
{
    RetainPtr directoryURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SaveResourcesTest"] isDirectory:YES];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:directoryURL.get() error:nil];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"webarchivetest"];
    RetainPtr htmlData = [NSData dataWithBytes:htmlDataBytesForUnsavedSubresources length:strlen(htmlDataBytesForUnsavedSubresources)];
    RetainPtr fontData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"Ahem-10000A" withExtension:@"ttf" subdirectory:@"TestWebKitAPI.resources"]];
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData.get();
        } else if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/Ahem-10000A.ttf"]) {
            mimeType = @"font/ttf";
            data = fontData.get();
        }
        RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:mimeType expectedContentLength:data.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:data];
        [task didFinish];
    }];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    static bool messageReceived = false;
    [webView performAfterReceivingMessage:@"done" action:[&] {
        messageReceived = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"webarchivetest://host/main.html"]]];
    Util::run(&messageReceived);

    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSString *mainResourcePath = [directoryURL URLByAppendingPathComponent:@"host.html"].path;
        EXPECT_TRUE([fileManager fileExistsAtPath:mainResourcePath]);

        NSString *savedMainResource = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:mainResourcePath] encoding:NSUTF8StringEncoding];
        EXPECT_TRUE([savedMainResource containsString:@"host_files/Ahem-10000A.ttf"]);
        EXPECT_FALSE([savedMainResource containsString:@"host_files/Ahem-10000A-backup.ttf"]);
        EXPECT_TRUE([savedMainResource containsString:@"webarchivetest://host/Ahem-10000A-backup.ttf"]);

        saved = true;
    }];
    Util::run(&saved);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
