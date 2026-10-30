/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/DragAndDropSimulator.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

@interface UploadDelegate : NSObject <WKUIDelegate>
- (instancetype)initWithDirectory:(NSURL *)directory;
- (BOOL)sentDirectory;
@end

@implementation UploadDelegate {
    RetainPtr<NSURL> _directory;
    BOOL _sentDirectory;
}

- (instancetype)initWithDirectory:(NSURL *)directory
{
    if (!(self = [super init]))
        return nil;
    _directory = directory;
    return self;
}

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *URLs))completionHandler
{
    completionHandler(@[_directory.get()]);
    _sentDirectory = YES;
}

- (BOOL)sentDirectory
{
    return _sentDirectory;
}

@end

#if PLATFORM(MAC)

static bool waitForChosenFile(TestWKWebView *webView)
{
    return TestWebKitAPI::Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.querySelector('input').files.length"] isEqualToString:@"1"];
    });
}

TEST(WebKit, UploadDirectory)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;
    NSURL *directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"/UploadDirectory"] isDirectory:YES];
    [fileManager removeItemAtPath:directory.path error:nil];
    EXPECT_TRUE([fileManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    EXPECT_FALSE(error);
    NSData *testData = [@"testdata" dataUsingEncoding:NSUTF8StringEncoding];
    EXPECT_TRUE([fileManager createFileAtPath:[directory.path stringByAppendingPathComponent:@"testfile"] contents:testData attributes:nil]);

    {
        using namespace TestWebKitAPI;
        HTTPServer server([] (Connection connection) {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                constexpr auto response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 123\r\n\r\n"
                "<form id='form' action='/upload.php' method='post' enctype='multipart/form-data'><input type='file' name='testname'></form>"_s;
                connection.send(response, [=] {
                    connection.receiveHTTPRequest([=](Vector<char>&& request) {
                        EXPECT_TRUE(contains(request.span(), "Content-Length: 543\r\n"_span));
                        size_t headerEnd = find(request.span(), "\r\n\r\n"_span);
                        EXPECT_TRUE(headerEnd != notFound);
                        EXPECT_EQ(request.size() - (headerEnd + strlen("\r\n\r\n")), 543u);
                        constexpr auto secondResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: 0\r\n\r\n"_s;
                        connection.send(secondResponse);
                    });
                });
            });
        });

        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
        RetainPtr delegate = adoptNS([[UploadDelegate alloc] initWithDirectory:directory]);
        [webView setUIDelegate:delegate.get()];

        [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

        auto chooseFileButtonLocation = NSMakePoint(10, 590);
        [webView sendClickAtPoint:chooseFileButtonLocation];
        while (![delegate sentDirectory])
            TestWebKitAPI::Util::spinRunLoop();
        EXPECT_TRUE(waitForChosenFile(webView.get()));
        [webView evaluateJavaScript:@"document.getElementById('form').submit()" completionHandler:nil];
        [webView _test_waitForDidFinishNavigation];
    }

    EXPECT_TRUE([fileManager removeItemAtPath:directory.path error:&error]);
    EXPECT_FALSE(error);
}

TEST(WebKit, AllowUploadFromTempDirectory)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;
    NSURL *directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"/com.apple.WebKit.Networking+com.apple.WebKit.TestWebKitAPI/UploadDirectory"] isDirectory:YES];
    [fileManager removeItemAtPath:directory.path error:nil];
    EXPECT_TRUE([fileManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    EXPECT_FALSE(error);
    NSData *testData = [@"testdata" dataUsingEncoding:NSUTF8StringEncoding];
    EXPECT_TRUE([fileManager createFileAtPath:[directory.path stringByAppendingPathComponent:@"testfile"] contents:testData attributes:nil]);

    {
        using namespace TestWebKitAPI;
        HTTPServer server([] (Connection connection) {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                constexpr auto response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 123\r\n\r\n"
                "<form id='form' action='/upload.php' method='post' enctype='multipart/form-data'><input type='file' name='testname'></form>"_s;
                connection.send(response, [=] {
                    connection.receiveHTTPRequest([=](Vector<char>&& request) {
                        size_t headerEnd = find(request.span(), "\r\n\r\n"_span);
                        EXPECT_TRUE(headerEnd != notFound);
                        EXPECT_GT(request.size() - (headerEnd + strlen("\r\n\r\n")), 0u);
                        constexpr auto secondResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: 0\r\n\r\n"_s;
                        connection.send(secondResponse);
                    });
                });
            });
        });

        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
        RetainPtr delegate = adoptNS([[UploadDelegate alloc] initWithDirectory:directory]);
        [webView setUIDelegate:delegate.get()];

        [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

        auto chooseFileButtonLocation = NSMakePoint(10, 590);
        [webView sendClickAtPoint:chooseFileButtonLocation];
        while (![delegate sentDirectory])
            TestWebKitAPI::Util::spinRunLoop();
        EXPECT_TRUE(waitForChosenFile(webView.get()));
        [webView evaluateJavaScript:@"document.getElementById('form').submit()" completionHandler:nil];
        [webView _test_waitForDidFinishNavigation];
    }

    EXPECT_TRUE([fileManager removeItemAtPath:directory.path error:&error]);
    EXPECT_FALSE(error);
}

#endif // PLATFORM(MAC)

TEST(WebKit, AllowTempUploadDirectory)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;
    NSURL *directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"/AllowTempUploadDirectory"] isDirectory:YES];
    [fileManager removeItemAtPath:directory.path error:nil];
    EXPECT_TRUE([fileManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    EXPECT_FALSE(error);
    NSData *testData = [@"testdata" dataUsingEncoding:NSUTF8StringEncoding];
    EXPECT_TRUE([fileManager createFileAtPath:[directory.path stringByAppendingPathComponent:@"testfile"] contents:testData attributes:nil]);

    {
        using namespace TestWebKitAPI;
        HTTPServer server([] (Connection connection) {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                constexpr auto response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 133\r\n\r\n"
                "<form id='form' action='/upload.php' method='post' enctype='multipart/form-data'><input id='file' type='file' name='testname'></form>"_s;
                connection.send(response, [=] {
                    connection.receiveHTTPRequest([=](Vector<char>&& request) {
                        EXPECT_TRUE(contains(request.span(), "Content-Length: 192\r\n"_span));
                        size_t headerEnd = find(request.span(), "\r\n\r\n"_span);
                        EXPECT_TRUE(headerEnd != notFound);
                        EXPECT_EQ(request.size() - (headerEnd + strlen("\r\n\r\n")), 192u);
                        constexpr auto secondResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: 0\r\n\r\n"_s;
                        connection.send(secondResponse);
                    });
                });
            });
        });

        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
        RetainPtr delegate = adoptNS([[UploadDelegate alloc] initWithDirectory:directory]);
        [webView setUIDelegate:delegate.get()];

        [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

        [webView clickOnElementID:@"file"];
        while (![delegate sentDirectory])
            TestWebKitAPI::Util::spinRunLoop();
        [webView evaluateJavaScript:@"document.getElementById('form').submit()" completionHandler:nil];
        [webView _test_waitForDidFinishNavigation];
    }

    EXPECT_TRUE([fileManager removeItemAtPath:directory.path error:&error]);
    EXPECT_FALSE(error);
}

static constexpr auto uploadFileFromIndexedDBScript = R"UPLOADRESOURCE(
function openDatabase()
{
    return new Promise((resolve, reject) => {
        const request = indexedDB.open('upload-file-from-indexeddb', 1);
        request.onupgradeneeded = () => request.result.createObjectStore('files');
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
}

async function saveToIndexedDB(file)
{
    const db = await openDatabase();
    return new Promise((resolve, reject) => {
        const transaction = db.transaction('files', 'readwrite');
        transaction.objectStore('files').put(file, 'test-file');
        transaction.oncomplete = () => {
            db.close();
            resolve();
        };
        transaction.onerror = () => {
            db.close();
            reject(transaction.error);
        };
    });
}

async function loadFromIndexedDB()
{
    const db = await openDatabase();
    return new Promise((resolve, reject) => {
        const request = db.transaction('files', 'readonly').objectStore('files').get('test-file');
        request.onsuccess = () => {
            db.close();
            resolve(request.result);
        };
        request.onerror = () => {
            db.close();
            reject(request.error);
        };
    });
}

async function runTest()
{
    try {
        await saveToIndexedDB(new File(['IndexedDB file content'], 'test.txt', { type: 'text/plain' }));

        // The File loaded from IndexedDB is backed by a file in the origin's IndexedDB directory.
        const file = await loadFromIndexedDB();

        const formData = new FormData();
        formData.append('file', file, 'test.txt');
        const response = await fetch('/upload', { method: 'POST', body: formData });
        alert(await response.text());
    } catch (error) {
        alert('error: ' + error);
    }
}
)UPLOADRESOURCE"_s;

TEST(WebKit, UploadFileFromIndexedDB)
{
    using namespace TestWebKitAPI;
    auto mainBytes = makeString("<script>"_s, uploadFileFromIndexedDBScript, "runTest();</script>"_s);
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](auto connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/"_s) {
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "_s, mainBytes.length(), "\r\n\r\n"_s, mainBytes));
                continue;
            }
            if (path == "/upload"_s) {
                auto result = contains(request.span(), "IndexedDB file content"_span) ? "PASS"_s : "FAIL"_s;
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "_s, result.length(), "\r\n\r\n"_s, result));
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "PASS");
}

static constexpr auto uploadFileFromIndexedDBServiceWorkerBytes = R"SWRESOURCE(
self.addEventListener('install', event => event.waitUntil(self.skipWaiting()));
self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
self.addEventListener('fetch', event => event.respondWith(fetch(event.request)));
)SWRESOURCE"_s;

TEST(WebKit, UploadFileFromIndexedDBThroughServiceWorker)
{
    using namespace TestWebKitAPI;
    auto mainBytes = makeString("<script>"_s, uploadFileFromIndexedDBScript, R"SWRESOURCE(
async function registerServiceWorkerAndRunTest()
{
    try {
        await navigator.serviceWorker.register('/sw.js');
        await navigator.serviceWorker.ready;
        if (!navigator.serviceWorker.controller)
            await new Promise(resolve => navigator.serviceWorker.addEventListener('controllerchange', resolve, { once: true }));
    } catch (error) {
        alert('error: ' + error);
        return;
    }
    await runTest();
}
registerServiceWorkerAndRunTest();
</script>)SWRESOURCE"_s);

    bool uploadReceived = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](auto connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/"_s) {
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "_s, mainBytes.length(), "\r\n\r\n"_s, mainBytes));
                continue;
            }
            if (path == "/sw.js"_s) {
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\nContent-Length: "_s, uploadFileFromIndexedDBServiceWorkerBytes.length(), "\r\n\r\n"_s, uploadFileFromIndexedDBServiceWorkerBytes));
                continue;
            }
            if (path == "/upload"_s) {
                uploadReceived = true;
                auto result = contains(request.span(), "IndexedDB file content"_span) ? "PASS"_s : "FAIL"_s;
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "_s, result.length(), "\r\n\r\n"_s, result));
                continue;
            }
            EXPECT_FALSE(true);
        }
    });

    auto removeServiceWorkerRegistrations = [] {
        __block bool done = false;
        [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeServiceWorkerRegistrations] modifiedSince:[NSDate distantPast] completionHandler:^{
            done = true;
        }];
        Util::run(&done);
    };
    removeServiceWorkerRegistrations();

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "PASS");
    EXPECT_TRUE(uploadReceived);

    removeServiceWorkerRegistrations();
}
