/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(HTTP2Server, RoundTrip)
{
    HTTPServer server({
        { "/"_s, HTTPResponse("hello"_s) }
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("hello", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    EXPECT_EQ(server.totalRequests(), 1u);
    EXPECT_WK_STREQ("h2", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('navigation')[0].nextHopProtocol"]);
}

TEST(HTTP2Server, RequestMethodPathAndHeaders)
{
    RetainPtr<NSString> receivedMethod;
    RetainPtr<NSString> receivedPath;
    RetainPtr<NSString> receivedHeaderValue;

    HTTPServer server([&](Connection connection) {
        connection.receiveHTTPMessagingRequest([&, connection](HTTPRequestData&& request) {
            receivedMethod = request.method.createNSString();
            receivedPath = request.path.createNSString();
            receivedHeaderValue = request.headerFields.get("x-test-header"_s).createNSString();
            connection.sendHTTPMessagingResponse(HTTPResponse("hello"_s));
        });
    }, HTTPServer::Protocol::Http2);

    RetainPtr request = adoptNS([server.request("/test"_s) mutableCopy]);
    [request setValue:@"testvalue" forHTTPHeaderField:@"X-Test-Header"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:request.get()];

    EXPECT_WK_STREQ("GET", receivedMethod.get());
    EXPECT_WK_STREQ("/test", receivedPath.get());
    EXPECT_WK_STREQ("testvalue", receivedHeaderValue.get());
    EXPECT_WK_STREQ("hello", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    EXPECT_WK_STREQ("h2", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('navigation')[0].nextHopProtocol"]);
}

TEST(HTTP2Server, POSTBody)
{
    RetainPtr<NSData> receivedBody;
    RetainPtr<NSString> receivedMethod;

    HTTPServer server([&](Connection connection) {
        connection.receiveHTTPMessagingRequest([&, connection](HTTPRequestData&& request) {
            if (request.path == "/"_s) {
                connection.sendHTTPMessagingResponse(HTTPResponse("<script>fetch('/submit', { method: 'POST', body: 'hello world' }).then(() => alert('done'))</script>"_s));
                return;
            }
            receivedMethod = request.method.createNSString();
            receivedBody = adoptNS([[NSData alloc] initWithBytes:request.body.span().data() length:request.body.size()]);
            connection.sendHTTPMessagingResponse(HTTPResponse("ok"_s));
        });
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("done", [webView _test_waitForAlert]);

    RetainPtr<NSData> bodyData = [@"hello world" dataUsingEncoding:NSUTF8StringEncoding];
    EXPECT_WK_STREQ("POST", receivedMethod.get());
    EXPECT_TRUE([bodyData isEqualToData:receivedBody.get()]);
    EXPECT_WK_STREQ("h2", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('resource')[0].nextHopProtocol"]);
}

TEST(HTTP2Server, NonDefaultStatusCodeAndResponseHeader)
{
    HTTPServer server({
        { "/"_s, HTTPResponse("<script>fetch('/missing').then(response => alert(`${response.status}:${response.headers.get('X-Custom-Response-Header')}`))</script>"_s) },
        { "/missing"_s, HTTPResponse(404, { { "X-Custom-Response-Header"_s, "responsevalue"_s } }) }
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("404:responsevalue", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("h2", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('resource')[0].nextHopProtocol"]);
}

TEST(HTTP2Server, MultipleRequestsOverOneConnection)
{
    HTTPServer server({
        { "/"_s, HTTPResponse("<script>"
            "Promise.all([fetch('/a').then(response => response.text()), fetch('/b').then(response => response.text())])"
            ".then(([a, b]) => alert(`${a}:${b}`))"
            "</script>"_s) },
        { "/a"_s, HTTPResponse("a"_s) },
        { "/b"_s, HTTPResponse("b"_s) },
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("a:b", [webView _test_waitForAlert]);

    EXPECT_EQ(server.totalRequests(), 3u);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('resource').every(entry => entry.nextHopProtocol === 'h2').toString()"]);
}

} // namespace TestWebKitAPI

#endif // HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)
