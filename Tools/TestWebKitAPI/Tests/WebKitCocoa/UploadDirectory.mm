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

#if PLATFORM(MAC)

#import "DragAndDropSimulator.h"
#import "HTTPServer.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
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

TEST(WebKit, UploadDirectory)
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSError *error = nil;
    NSURL *directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"UploadDirectory"] isDirectory:YES];
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

        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
        auto delegate = adoptNS([[UploadDelegate alloc] initWithDirectory:directory]);
        [webView setUIDelegate:delegate.get()];

        [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];

        auto chooseFileButtonLocation = NSMakePoint(10, 590);
        [webView sendClickAtPoint:chooseFileButtonLocation];
        while (![delegate sentDirectory])
            TestWebKitAPI::Util::spinRunLoop();
        Util::runFor(50_ms);
        [webView evaluateJavaScript:@"document.getElementById('form').submit()" completionHandler:nil];
        [webView _test_waitForDidFinishNavigation];
    }

    EXPECT_TRUE([fileManager removeItemAtPath:directory.path error:&error]);
    EXPECT_FALSE(error);
}

#endif
