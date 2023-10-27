
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
static const char* iframeHTMLDataBytes = R"TESTRESOURCE(<p>iframe2</p><img src='image.png'>)TESTRESOURCE";

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
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:resourceDirectoryPath error:0];
        EXPECT_EQ(4llu, resourceFileNames.count);
        unsigned frameFileCount = 0;
        unsigned imageFileCount = 0;
        NSMutableSet *frameResourceContentsToFind = [NSMutableSet set];
        [frameResourceContentsToFind addObjectsFromArray:[NSArray arrayWithObjects:@"iframe1", @"iframe2", @"iframe3", nil]];
        for (NSString *fileName in resourceFileNames) {
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
        }
        EXPECT_EQ(3u, frameFileCount);
        EXPECT_EQ(1u, imageFileCount);
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
    NSArray *tests = [NSArray arrayWithObjects:[NSString stringWithString:mutableFileName], @"a/image.png", @"b/image.png", @"image.png(1)", @"webarchivetest://host/file:image.png", @"image1/", @"image2///", @"image3.png/./", @"image4/content/../", nil];
    NSMutableString *mutableHTMLString = [NSMutableString string];
    NSString *scriptString = [NSString stringWithFormat:@"<script>count = 0; function onImageLoad() { if (++count == %d) window.webkit.messageHandlers.testHandler.postMessage('done'); }</script>", (int)tests.count];
    [mutableHTMLString appendString:scriptString];
    for (NSString *item in tests)
        [mutableHTMLString appendString:[NSString stringWithFormat:@"<img src='%@' onload='onImageLoad()'>", item]];
    NSData *htmlData = [[NSString stringWithString:mutableHTMLString] dataUsingEncoding:NSUTF8StringEncoding];
    NSData *imageData = [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"400x400-green" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSData *data = nil;
        NSString *mimeType = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"webarchivetest://host/main.html"]) {
            mimeType = @"text/html";
            data = htmlData;
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

    NSSet *expectedFileNames = [NSSet setWithArray:[NSArray arrayWithObjects:[mutableFileName substringFromIndex:1], @"image.png", @"image.png(1)", @"image.png(1)(1)", @"file%3Aimage.png", @"image1", @"file", @"image3.png", @"image4", nil]];
    static bool saved = false;
    [webView _saveResources:directoryURL.get() suggestedFileName:@"host" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:[directoryURL URLByAppendingPathComponent:@"host_files"].path error:nil];
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

        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:[directoryURL URLByAppendingPathComponent:@"host_files"].path error:nil];
        EXPECT_EQ(2llu, resourceFileNames.count);

        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
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
<body>
<script>
count = 0;
function replaceImg() {
    img = document.getElementById("img_id");
    const ratio = window.devicePixelRatio;
    img.removeAttribute("src");
    img.srcset = 'image2.png ' + ratio + 'x,' + ' image1.png ' + (ratio + 1) + 'x';
    img2 = document.getElementById("img_id2");
    document.body.removeChild(img2);
}
function onImageLoad() {
    ++count;
    if (count == 3)
        replaceImg();
    if (count == 4)
        window.webkit.messageHandlers.testHandler.postMessage("done");
}
</script>
<img id="img_id" src="image1.png" onload="onImageLoad()">
<img id="img_id2" src="image3.png" onload="onImageLoad()">
<picture>
    <source srcset="image3.png" media="(min-width: 3000px)">
    <img src="image4.png" onload="onImageLoad()">
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
        } else if ([task.request.URL.absoluteString containsString:@"image"]) {
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
        NSSet *expectedFileNames = [NSSet setWithArray:[NSArray arrayWithObjects:@"image1.png", @"image2.png", @"image3.png", @"image4.png", nil]];
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

        NSArray *resourceFileNames = [fileManager contentsOfDirectoryAtPath:[directoryURL URLByAppendingPathComponent:@"host_files"].path error:nil];
        EXPECT_EQ(3llu, resourceFileNames.count);
        unsigned frameFileCount = 0;
        unsigned imageFileCount = 0;
        unsigned frameWithDataURLCount = 0;
        NSString *resourceDirectoryName = @"host_files";
        NSString *resourceDirectoryPath = [directoryURL URLByAppendingPathComponent:resourceDirectoryName].path;
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

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
