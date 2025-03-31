/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if HAVE(WEB_TRANSPORT)

#import "config.h"

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WebTransportServer.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKInternalDebugFeature.h>

namespace TestWebKitAPI {

static void enableWebTransport(WKWebViewConfiguration *configuration)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"WebTransportEnabled"]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

static void validateChallenge(NSURLAuthenticationChallenge *challenge, uint16_t port)
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    EXPECT_NOT_NULL(challenge.protectionSpace.serverTrust);
    EXPECT_EQ(challenge.protectionSpace.port, port);
    EXPECT_WK_STREQ(challenge.protectionSpace.host, "127.0.0.1");
    verifyCertificateAndPublicKey(challenge.protectionSpace.serverTrust);
}

// FIXME: Re-enable these tests once rdar://148050136 is fixed.
#if PLATFORM(MAC)
TEST(WebTransport, ClientBidirectional)
#else
TEST(WebTransport, DISABLED_ClientBidirectional)
#endif
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        co_await connection.awaitableSend(WTFMove(request));
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool challenged { false };
    __block uint16_t port = echoServer.port();
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        validateChallenge(challenge, port);
        challenged = true;
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    NSString *html = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let s = await t.createBidirectionalStream();"
        "    let w = s.writable.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    await w.close();"
        "    let r = s.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    await r.cancel();"
        "    t.close();"
        "    alert('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");
    EXPECT_TRUE(challenged);
}

// FIXME: Re-enable these tests once rdar://148050136 is fixed.
#if PLATFORM(MAC)
TEST(WebTransport, Datagram)
#else
TEST(WebTransport, DISABLED_Datagram)
#endif
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto datagramConnection = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Datagram);
        auto request = co_await datagramConnection.awaitableReceiveBytes();
        co_await datagramConnection.awaitableSend(WTFMove(request));
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool challenged { false };
    __block uint16_t port = echoServer.port();
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        validateChallenge(challenge, port);
        challenged = true;
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    NSString *html = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let w = t.datagrams.writable.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    await w.close();"
        "    let r = t.datagrams.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    await r.cancel();"
        "    t.close();"
        "    alert('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");
    EXPECT_TRUE(challenged);
}

// FIXME: Re-enable these tests once rdar://148050136 is fixed.
#if PLATFORM(MAC)
TEST(WebTransport, Unidirectional)
#else
TEST(WebTransport, DISABLED_Unidirectional)
#endif
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        auto serverUnidirectionalStream = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Unidirectional);
        co_await serverUnidirectionalStream.awaitableSend(WTFMove(request));
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool challenged { false };
    __block uint16_t port = echoServer.port();
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        validateChallenge(challenge, port);
        challenged = true;
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    NSString *html = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let c = await t.createUnidirectionalStream();"
        "    let w = c.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    await w.close();"
        "    let sr = t.incomingUnidirectionalStreams.getReader();"
        "    let {value: s, d} = await sr.read();"
        "    let r = s.getReader();"
        "    const { value, done } = await r.read();"
        "    await r.cancel();"
        "    t.close();"
        "    alert('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");
    EXPECT_TRUE(challenged);
}

// FIXME: Fix and enable this test.
TEST(WebTransport, DISABLED_ServerBidirectional)
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        auto serverBidirectionalStream = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Bidirectional);
        co_await serverBidirectionalStream.awaitableSend(WTFMove(request));
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool challenged { false };
    __block uint16_t port = echoServer.port();
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        validateChallenge(challenge, port);
        challenged = true;
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    NSString *html = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let c = await t.createBidirectionalStream();"
        "    let w = c.writable.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    await w.close();"
        "    await c.readable.getReader().cancel();"
        "    let sr = t.incomingBidirectionalStreams.getReader();"
        "    let {value: s, d} = await sr.read();"
        "    let r = s.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    await r.cancel();"
        "    await s.writable.getWriter().close();"
        "    t.close();"
        "    alert('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");
    EXPECT_TRUE(challenged);
}

// FIXME: Re-enable these tests once rdar://148050136 is fixed.
#if PLATFORM(MAC)
TEST(WebTransport, NetworkProcessCrash)
#else
TEST(WebTransport, DISABLED_NetworkProcessCrash)
#endif
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto datagramConnection = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Datagram);
        co_await datagramConnection.awaitableSend(@"abc");
        auto bidiConnection = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Bidirectional);
        co_await bidiConnection.awaitableSend(@"abc");
        auto uniConnection = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Unidirectional);
        co_await uniConnection.awaitableSend(@"abc");
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool challenged { false };
    __block uint16_t port = echoServer.port();
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        validateChallenge(challenge, port);
        challenged = true;
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    NSString *html = [NSString stringWithFormat:@""
        "<script>"
        "let session = new WebTransport('https://127.0.0.1:%d/');"
        "let bidiStream = null;"
        "let uniStream = null;"
        "let incomingBidiStream = null;"
        "let incomingUniStream = null;"
        "let data = new TextEncoder().encode('abc');"
        "async function setupSession() {"
        "  try {"
        "    await session.ready;"
        "    bidiStream = await session.createBidirectionalStream();"
        "    uniStream = await session.createUnidirectionalStream();"
        "    incomingBidiStream = await getIncomingBidiStream();"
        "    incomingUniStream = await getIncomingUniStream();"
        "    alert('successfully established');"
        "  } catch (e) { alert('caught ' + e); }"
        "}; setupSession();"
        "async function getIncomingBidiStream() {"
        "  let reader = session.incomingBidirectionalStreams.getReader();"
        "  let {value: s, d} = await reader.read();"
        "  reader.releaseLock();"
        "  return s;"
        "};"
        "async function getIncomingUniStream() {"
        "  let reader = session.incomingUnidirectionalStreams.getReader();"
        "  let {value: s, d} = await reader.read();"
        "  reader.releaseLock();"
        "  return s;"
        "};"
        "async function readFromBidiStream() {"
        "  let reader = bidiStream.readable.getReader();"
        "  let {value: c, d} = await reader.read();"
        "  reader.releaseLock();"
        "  return c;"
        "};"
        "async function readFromIncomingBidiStream() {"
        "  let reader = incomingBidiStream.readable.getReader();"
        "  let {value: c, d} = await reader.read();"
        "  reader.releaseLock();"
        "  return c;"
        "};"
        "async function readFromIncomingUniStream() {"
        "  let reader = incomingUniStream.getReader();"
        "  let {value: c, d} = await reader.read();"
        "  reader.releaseLock();"
        "  return c;"
        "};"
        "async function readDatagram() {"
        "  let reader = session.datagrams.readable.getReader();"
        "  let {value: c, d} = await reader.read();"
        "  reader.releaseLock();"
        "  return c;"
        "};"
        "async function writeOnBidiStream() {"
        "  let writer = bidiStream.writable.getWriter();"
        "  await writer.write(data);"
        "  writer.releaseLock();"
        "  return;"
        "};"
        "async function writeOnUniStream() {"
        "  let writer = uniStream.getWriter();"
        "  await writer.write(data);"
        "  writer.releaseLock();"
        "  return;"
        "};"
        "async function writeOnIncomingBidiStream() {"
        "  let writer = incomingBidiStream.writable.getWriter();"
        "  await writer.write(data);"
        "  writer.releaseLock();"
        "  return;"
        "};"
        "async function writeDatagram() {"
        "  let writer = session.datagrams.writable.getWriter();"
        "  await writer.write(data);"
        "  writer.releaseLock();"
        "  return;"
        "};"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully established");
    EXPECT_TRUE(challenged);

    pid_t networkProcessIdentifier = [configuration.get().websiteDataStore _networkProcessIdentifier];

    kill(networkProcessIdentifier, SIGKILL);

    NSError *error = nil;

    id obj = [webView objectByCallingAsyncFunction:@"return await session.createBidirectionalStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await session.createUnidirectionalStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await getIncomingBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NULL(error);

    obj = [webView objectByCallingAsyncFunction:@"return await getIncomingUniStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NULL(error);

    obj = [webView objectByCallingAsyncFunction:@"return await readFromBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NULL(error);

    obj = [webView objectByCallingAsyncFunction:@"return await readFromIncomingBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NULL(error);

    obj = [webView objectByCallingAsyncFunction:@"return await readFromIncomingUniStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NULL(error);

    obj = [webView objectByCallingAsyncFunction:@"return await readDatagram()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NULL(error);

    obj = [webView objectByCallingAsyncFunction:@"return await writeOnBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await writeOnUniStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await writeOnIncomingBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await writeDatagram()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByEvaluatingJavaScript:@"session.close()"];
    EXPECT_EQ(obj, nil);
}

// FIXME: Re-enable these tests once rdar://148050136 is fixed.
#if PLATFORM(MAC)
TEST(WebTransport, Worker)
#else
TEST(WebTransport, DISABLED_Worker)
#endif
{
    WebTransportServer transportServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        auto serverBidirectionalStream = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Bidirectional);
        co_await serverBidirectionalStream.awaitableSend(WTFMove(request));
    });

    auto mainHTML = "<script>"
    "const worker = new Worker('worker.js');"
    "worker.onmessage = (event) => {"
    "  alert('message from worker: ' + event.data);"
    "};"
    "</script>"_s;

    NSString *workerJS = [NSString stringWithFormat:@""
        "async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let c = await t.createBidirectionalStream();"
        "    let w = c.writable.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    let sr = t.incomingBidirectionalStreams.getReader();"
        "    let {value: s, d} = await sr.read();"
        "    let r = s.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    self.postMessage('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { self.postMessage('caught ' + e); }"
        "}; test();", transportServer.port()];

    HTTPServer loadingServer({
        { "/"_s, { mainHTML } },
        { "/worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, workerJS } }
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:loadingServer.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "message from worker: successfully read abc");
}

} // namespace TestWebKitAPI

#endif // HAVE(WEB_TRANSPORT)
