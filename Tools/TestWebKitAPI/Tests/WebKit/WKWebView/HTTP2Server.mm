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
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
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
                connection.sendHTTPMessagingResponse(HTTPResponse("hello"_s));
                return;
            }
            receivedMethod = request.method.createNSString();
            receivedBody = adoptNS([[NSData alloc] initWithBytes:request.body.span().data() length:request.body.size()]);
            connection.sendHTTPMessagingResponse(HTTPResponse("ok"_s));
        });
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("ok", [webView objectByCallingAsyncFunction:@"const response = await fetch('/submit', { method: 'POST', body: 'hello world' }); return await response.text()" withArguments:@{ }]);

    RetainPtr<NSData> bodyData = [@"hello world" dataUsingEncoding:NSUTF8StringEncoding];
    EXPECT_WK_STREQ("POST", receivedMethod.get());
    EXPECT_TRUE([bodyData isEqualToData:receivedBody.get()]);
    EXPECT_WK_STREQ("h2", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('resource')[0].nextHopProtocol"]);
}

TEST(HTTP2Server, NonDefaultStatusCodeAndResponseHeader)
{
    HTTPServer server({
        { "/"_s, HTTPResponse("hello"_s) },
        { "/missing"_s, HTTPResponse(404, { { "X-Custom-Response-Header"_s, "responsevalue"_s } }) }
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("404:responsevalue", [webView objectByCallingAsyncFunction:@"const response = await fetch('/missing'); return `${response.status}:${response.headers.get('X-Custom-Response-Header')}`" withArguments:@{ }]);
    EXPECT_WK_STREQ("h2", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('resource')[0].nextHopProtocol"]);
}

TEST(HTTP2Server, MultipleRequestsOverOneConnection)
{
    HTTPServer server({
        { "/"_s, HTTPResponse("hello"_s) },
        { "/a"_s, HTTPResponse("a"_s) },
        { "/b"_s, HTTPResponse("b"_s) },
    }, HTTPServer::Protocol::Http2);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("a:b", [webView objectByCallingAsyncFunction:@"const [a, b] = await Promise.all([fetch('/a').then(response => response.text()), fetch('/b').then(response => response.text())]); return `${a}:${b}`" withArguments:@{ }]);

    EXPECT_EQ(server.totalRequests(), 3u);
    EXPECT_WK_STREQ("true", [webView stringByEvaluatingJavaScript:@"performance.getEntriesByType('resource').every(entry => entry.nextHopProtocol === 'h2').toString()"]);
}

TEST(HTTP2Server, MultipleCookiesSetBeforeLoad)
{
    // RFC 7540 8.1.2.5 "cookie crumbling": a client may split one logical Cookie header into
    // multiple header fields on the wire. These must be rejoined with "; ", not the "," used
    // for other repeated header fields (RFC 7230 3.2.2), to reconstruct the Cookie header value.
    RetainPtr<NSString> receivedCookieHeader;

    HTTPServer server([&](Connection connection) {
        connection.receiveHTTPMessagingRequest([&, connection](HTTPRequestData&& request) {
            receivedCookieHeader = request.headerFields.get("cookie"_s).createNSString();
            connection.sendHTTPMessagingResponse(HTTPResponse("hello"_s));
        });
    }, HTTPServer::Protocol::Http2);

    RetainPtr<NSHTTPCookie> firstCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"firstCookie",
        NSHTTPCookieValue: @"firstValue",
        NSHTTPCookieDomain: @"127.0.0.1",
    }];
    RetainPtr<NSHTTPCookie> secondCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookiePath: @"/",
        NSHTTPCookieName: @"secondCookie",
        NSHTTPCookieValue: @"secondValue",
        NSHTTPCookieDomain: @"127.0.0.1",
    }];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    WKHTTPCookieStore *cookieStore = webView.get().configuration.websiteDataStore.httpCookieStore;
    __block bool cookiesSet = false;
    [cookieStore setCookie:firstCookie.get() completionHandler:^{
        [cookieStore setCookie:secondCookie.get() completionHandler:^{
            cookiesSet = true;
        }];
    }];
    TestWebKitAPI::Util::run(&cookiesSet);

    [webView synchronouslyLoadRequestIgnoringSSLErrors:server.request()];
    EXPECT_WK_STREQ("firstCookie=firstValue; secondCookie=secondValue", receivedCookieHeader.get());
}

TEST(HTTP2Server, ThroughProxy)
{
    String receivedAuthority;
    bool done = false;

    HTTPServer server([&](Connection connection) {
        connection.receiveHTTPMessagingRequest([&, connection](HTTPRequestData&& request) {
            receivedAuthority = request.authority;
            connection.sendHTTPMessagingResponse(HTTPResponse("hello"_s));
            done = true;
        });
    }, HTTPServer::Protocol::Http2Proxy);

    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);

    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.example/"]]];
    Util::run(&done);

    EXPECT_WK_STREQ("a.example", receivedAuthority);
}

} // namespace TestWebKitAPI

#endif // HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)
