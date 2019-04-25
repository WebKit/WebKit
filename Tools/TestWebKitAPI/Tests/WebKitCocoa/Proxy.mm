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

#import "TCPServer.h"
#import "Utilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

static bool receivedAlert;

@interface ProxyDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation ProxyDelegate

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_TRUE([message isEqualToString:@"success!"]);
    receivedAlert = true;
    completionHandler();
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * _Nullable credential))completionHandler
{
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}

@end

namespace TestWebKitAPI {

TEST(WebKit, HTTPSProxy)
{
    TCPServer server(TCPServer::Protocol::HTTPSProxy, [] (SSL* ssl) {
        char requestBuffer[1000];
        auto readResult = SSL_read(ssl, requestBuffer, sizeof(requestBuffer));
        ASSERT_UNUSED(readResult, readResult > 0);

        const char* reply = ""
        "HTTP/1.1 200 OK\r\n"
        "Strict-Transport-Security: max-age=16070400; includeSubDomains\r\n"
        "Content-Length: 34\r\n\r\n"
        "<script>alert('success!')</script>";
        auto writeResult = SSL_write(ssl, reply, strlen(reply));
        ASSERT_UNUSED(writeResult, writeResult == static_cast<int>(strlen(reply)));
    });

    RetainPtr<NSString> tempDir = NSTemporaryDirectory();
    WTFLogAlways("ORIGINAL TEMP DIR %@", tempDir.get());
    if ([tempDir hasSuffix:@"/"])
        tempDir = [tempDir substringWithRange:NSMakeRange(0, [tempDir length] - 1)];
    
    WTFLogAlways("TEMP DIR %@", tempDir.get());
    auto poolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
    [poolConfiguration setHSTSStorageDirectory:tempDir.get()];
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()] autorelease]];
    [viewConfiguration setProcessPool:[[[WKProcessPool alloc] _initWithConfiguration:poolConfiguration.get()] autorelease]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    TestWebKitAPI::Util::run(&receivedAlert);
    
    RetainPtr<NSString> filePath = [tempDir stringByAppendingPathComponent:@"HSTS.plist"];
    WTFLogAlways("WAITING FOR FILE TO BE WRITTEN TO %@", filePath.get());
    while (![[NSFileManager defaultManager] fileExistsAtPath:filePath.get()])
        TestWebKitAPI::Util::spinRunLoop();
    [[NSFileManager defaultManager] removeItemAtPath:filePath.get() error:nil];
    WTFLogAlways("DONE");
}

} // namespace TestWebKitAPI

