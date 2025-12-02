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
#import <CommonCrypto/CommonDigest.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>

SOFT_LINK_FRAMEWORK(Network)

// FIXME: Replace this soft linking with a HAVE macro once rdar://158191390 is available on all tested OS builds.
SOFT_LINK_MAY_FAIL(Network, nw_webtransport_options_set_allow_joining_before_ready, void, (nw_protocol_options_t options, bool allow), (options, allow))

// FIXME: Replace this soft linking with a HAVE macro once rdar://164265337 is available on all tested OS builds.
SOFT_LINK_MAY_FAIL(Network, nw_webtransport_metadata_set_local_draining, void, (nw_protocol_metadata_t metadata), (metadata))

// FIXME: Replace this soft linking with a HAVE macro once rdar://164514830 is available on all tested OS builds.
SOFT_LINK_MAY_FAIL(Network, nw_webtransport_metadata_get_session_closed, bool, (nw_protocol_metadata_t metadata), (metadata))

// FIXME: Replace this soft linking with a HAVE macro once rdar://164917448 is available on all tested OS builds.
SOFT_LINK_MAY_FAIL(Network, nw_webtransport_metadata_get_transport_mode, nw_webtransport_transport_mode_t, (nw_protocol_metadata_t metadata), (metadata))

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

TEST(WebTransport, ClientBidirectional)
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        request.append('d');
        request.append('e');
        request.append('f');
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
        "    let initialReadStats = await s.readable.getStats();"
        "    let initialWriteStats = await s.writable.getStats();"
        "    let w = s.writable.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    let finalWriteStats = await s.writable.getStats();"
        "    let r = s.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    let finalReadStats = await s.readable.getStats();"
        "    await w.close();"
        "    await r.cancel();"
        "    t.close();"
        "    let writableThrew = false;"
        "    let readableThrew = false;"
        "    try { await s.writable.getStats() } catch (e) { writableThrew = true }"
        "    try { await s.readable.getStats() } catch (e) { readableThrew = true }"
        "    alert('successfully read ' + new TextDecoder().decode(value)"
        "        + ', stats before: ' + initialReadStats.bytesReceived + ' ' + initialWriteStats.bytesSent"
        "        + ', stats after: ' + finalReadStats.bytesReceived + ' ' + finalWriteStats.bytesSent"
        "        + ', writable threw after closing: ' + writableThrew"
        "        + ', readable threw after closing: ' + readableThrew"
        "    );"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    const char* expected =
        "successfully read abcdef"
        ", stats before: 0 0"
        ", stats after: 6 3"
        ", writable threw after closing: true"
        ", readable threw after closing: true";
    EXPECT_WK_STREQ([webView _test_waitForAlert], expected);
    EXPECT_TRUE(challenged);
}

TEST(WebTransport, Datagram)
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
        "  var s = 'unexpected unset value';"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:1/');"
        "    await t.ready;"
        "    alert('unexpected success');"
        "  } catch (e) { s = 'abc' }"
        "  "
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    let g = t.createSendGroup();"
        "    await t.ready;"
        "    let w = t.datagrams.createWritable({ sendGroup : g }).getWriter();"
        "    await w.write(new TextEncoder().encode(s));"
        "    let r = t.datagrams.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    await r.cancel();"
        "    const groupStats = await g.getStats();"
        "    t.close();"
        "    await w.closed;"
        "    alert('successfully read ' + new TextDecoder().decode(value) + ', group sent ' + groupStats.bytesWritten + ' bytes, maxDatagramSize ' + t.datagrams.maxDatagramSize + ', reliability ' + t.reliability);"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>",
        port];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    if (!canLoadnw_webtransport_metadata_get_transport_mode())
        EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc, group sent 3 bytes, maxDatagramSize 65535, reliability pending");
    else
        EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc, group sent 3 bytes, maxDatagramSize 65535, reliability supports-unreliable");
    EXPECT_TRUE(challenged);
}

TEST(WebTransport, Unidirectional)
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

TEST(WebTransport, ServerBidirectional)
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

TEST(WebTransport, NetworkProcessCrash)
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
        "  let writer = session.datagrams.createWritable().getWriter();"
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
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await getIncomingUniStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await readFromBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await readFromIncomingBidiStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await readFromIncomingUniStream()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

    obj = [webView objectByCallingAsyncFunction:@"return await readDatagram()" withArguments:@{ } error:&error];
    EXPECT_EQ(obj, nil);
    EXPECT_NOT_NULL(error);
    error = nil;

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

TEST(WebTransport, Worker)
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
        "    %s"
        "    let c = await t.createBidirectionalStream();"
        "    let w = c.writable.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    let sr = t.incomingBidirectionalStreams.getReader();"
        "    let {value: s, d} = await sr.read();"
        "    let r = s.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    self.postMessage('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { self.postMessage('caught ' + e); }"
        "}; test();", transportServer.port(), canLoadnw_webtransport_options_set_allow_joining_before_ready() ? "" : "await t.ready;"];

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

TEST(WebTransport, WorkerAfterNetworkProcessCrash)
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
        "};"
        "addEventListener('message', test);"
        "self.postMessage('started worker');", transportServer.port()];

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
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "message from worker: started worker");
    kill([configuration.get().websiteDataStore _networkProcessIdentifier], SIGKILL);
    while ([[configuration websiteDataStore] _networkProcessExists])
        TestWebKitAPI::Util::spinRunLoop();
    [webView objectByEvaluatingJavaScript:@"'wait for web process to be informed of network process termination'"];
    [webView evaluateJavaScript:@"worker.postMessage('start')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "message from worker: successfully read abc");
}

TEST(WebTransport, CreateStreamsBeforeReady)
{
    if (!canLoadnw_webtransport_options_set_allow_joining_before_ready())
        return;

    WebTransportServer datagramServer([](ConnectionGroup group) -> ConnectionTask {
        auto datagramConnection = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Datagram);
        auto request = co_await datagramConnection.awaitableReceiveBytes();
        co_await datagramConnection.awaitableSend(WTFMove(request));
    });

    WebTransportServer streamServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        co_await connection.awaitableSend(WTFMove(request));
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];

    NSString *datagramHTML = [NSString stringWithFormat:@"<script>"
    "async function test() {"
    "  try {"
    "    const w = new WebTransport('https://127.0.0.1:%d/');"
    "    const writer = w.datagrams.createWritable().getWriter();"
    "    const reader = w.datagrams.readable.getReader();"
    "    await writer.write(new TextEncoder().encode('abc'));"
    "    const { value, done } = await reader.read();"
    "    alert('successfully read ' + new TextDecoder().decode(value));"
    "  } catch (e) { alert('caught ' + e); }"
    "}; test()"
    "</script>", datagramServer.port()];
    [webView loadHTMLString:datagramHTML baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");

    NSString *streamHTML = [NSString stringWithFormat:@"<script>"
    "async function test() {"
    "  try {"
    "    const w = new WebTransport('https://127.0.0.1:%d/');"
    "    let c = await w.createBidirectionalStream();"
    "    let writer = c.writable.getWriter();"
    "    await writer.write(new TextEncoder().encode('abc'));"
    "    let reader = await c.readable.getReader();"
    "    const { value, done } = await reader.read();"
    "    alert('successfully read ' + new TextDecoder().decode(value));"
    "  } catch (e) { alert('caught ' + e); }"
    "}; test()"
    "</script>", streamServer.port()];
    [webView loadHTMLString:streamHTML baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");
}

// FIXME: Re-enable this test on iOS when rdar://161858543 is resolved.
#if PLATFORM(MAC)
TEST(WebTransport, CSP)
#else
TEST(WebTransport, DISABLED_CSP)
#endif
{
    WebTransportServer server([](ConnectionGroup group) -> ConnectionTask {
        co_return;
    });

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];

    auto runTest = [&] (const char* allowedDestination) {
        NSString *html = [NSString stringWithFormat:@"<script>"
            "function setCSP(destination) {"
            "  let meta = document.createElement('meta');"
            "  meta.httpEquiv = 'Content-Security-Policy';"
            "  meta.content = 'connect-src ' + destination;"
            "  document.head.appendChild(meta);"
            "};"
            "async function test() {"
            "  try {"
            "    setCSP('%s');"
            "    const w = new WebTransport('https://localhost:%d/');"
            "    await w.ready;"
            "    alert('ready');"
            "  } catch (e) { alert('caught ' + e.name + ' ' + e.source + ' ' + e.streamErrorCode + ' ' + (e instanceof WebTransportError)); }"
            "}; test()"
            "</script>", allowedDestination, server.port()];
        [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        return [webView _test_waitForAlert];
    };
    EXPECT_WK_STREQ(runTest("none"), "caught WebTransportError session null true");
    EXPECT_WK_STREQ(runTest([NSString stringWithFormat:@"https://localhost:%d", server.port()].UTF8String), "ready");
}

TEST(WebTransport, ServerCertificateHashes)
{
    auto runTest = [] (uint64_t certLifetime, bool matchHash = true) {
        NSDictionary* options = @{
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
            (id)kSecAttrKeySizeInBits: @256,
        };
        CFErrorRef error = nullptr;
        RetainPtr privateKey = adoptCF(SecKeyCreateRandomKey((__bridge CFDictionaryRef)options, &error));
        EXPECT_NULL(error);

        NSArray *subject = @[];
        NSDictionary *parameters = @{
            (__bridge NSString*)kSecCertificateLifetime: @(certLifetime)
        };
        RetainPtr certificate = adoptCF(SecGenerateSelfSignedCertificate((__bridge CFArrayRef)subject, (__bridge CFDictionaryRef)parameters, nullptr, privateKey.get()));
        RetainPtr identity = adoptCF(SecIdentityCreate(kCFAllocatorDefault, certificate.get(), privateKey.get()));
        RetainPtr certificateDER = adoptNS((__bridge NSData *)SecCertificateCopyData(certificate.get()));

        WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
            auto connection = co_await group.receiveIncomingConnection();
            auto request = co_await connection.awaitableReceiveBytes();
            auto serverBidirectionalStream = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Bidirectional);
            co_await serverBidirectionalStream.awaitableSend(WTFMove(request));
        }, adoptNS(sec_identity_create(identity.get())).get());

        std::array<uint8_t, CC_SHA256_DIGEST_LENGTH> sha2 { };
        if (matchHash)
            CC_SHA256([certificateDER bytes], [certificateDER length], sha2.data());

        StringBuilder certificateBytes;
        for (NSUInteger i = 0; i < CC_SHA256_DIGEST_LENGTH; i++) {
            if (i)
                certificateBytes.append(", "_s);
            certificateBytes.append(makeString((unsigned)sha2[i]));
        }

        NSString *html = [NSString stringWithFormat:@""
            "<script>async function test() {"
            "  try {"
            "    const hashValue = new Uint8Array([%s]);"
            "    let t = new WebTransport('https://127.0.0.1:%d/',{serverCertificateHashes: [{algorithm: 'sha-256',value: hashValue}]});"
            "    try { await t.ready } catch (e) { alert('did not become ready') };"
            "    let c = await t.createBidirectionalStream();"
            "    let w = c.writable.getWriter();"
            "    await w.write(new TextEncoder().encode('abc'));"
            "    let sr = t.incomingBidirectionalStreams.getReader();"
            "    let {value: s, d} = await sr.read();"
            "    let r = s.readable.getReader();"
            "    const { value, done } = await r.read();"
            "    alert('successfully read ' + new TextDecoder().decode(value));"
            "  } catch (e) { alert('caught ' + e); }"
            "}; test();"
            "</script>", certificateBytes.toString().utf8().data(), echoServer.port()];

        auto configuration = adoptNS([WKWebViewConfiguration new]);
        enableWebTransport(configuration.get());
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto delegate = adoptNS([TestNavigationDelegate new]);
        [webView setNavigationDelegate:delegate.get()];

        __block bool challenged { false };
        delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            challenged = true;
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        };

        [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        NSString *result = [webView _test_waitForAlert];
        EXPECT_FALSE(challenged);
        return result;
    };

    constexpr uint64_t oneWeekValidity = 7 * 24 * 60 * 60;
    EXPECT_WK_STREQ(runTest(oneWeekValidity), "successfully read abc");
    // FIXME: Add negative tests once rdar://161855525 is fixed.
}

TEST(WebTransport, ServerConnectionTermination)
{
    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        EXPECT_EQ(request.size(), 3u);
        group.cancel();
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];

    NSString *html = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let c = await t.createUnidirectionalStream();"
        "    let w = c.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    let closeInfo = await t.closed;"
        "    alert('successfully read closeInfo (' + closeInfo.closeCode + ', ' + closeInfo.reason + ')');"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>", echoServer.port()];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], canLoadnw_webtransport_metadata_get_session_closed() ? "successfully read closeInfo (0, )" : "caught WebTransportError");
}

TEST(WebTransport, BackForwardCache)
{
    bool serverConnectionTerminatedByClient { false };
    WebTransportServer echoServer([&](ConnectionGroup group) -> ConnectionTask {
        auto datagramConnection = group.createWebTransportConnection(ConnectionGroup::ConnectionType::Datagram);
        auto request = co_await datagramConnection.awaitableReceiveBytes();
        co_await datagramConnection.awaitableSend(WTFMove(request));
        co_await group.awaitableFailure();
        serverConnectionTerminatedByClient = true;
    });

    NSString *mainHTML = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let w = t.datagrams.createWritable().getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    let r = t.datagrams.readable.getReader();"
        "    const { value, done } = await r.read();"
        "    alert('successfully read ' + new TextDecoder().decode(value));"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>", echoServer.port()];

    HTTPServer loadingServer({
        { "/"_s, { mainHTML } },
        { "/other"_s, { @"<script>alert('loaded')</script>" } }
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:loadingServer.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully read abc");
    [webView loadRequest:loadingServer.request("/other"_s)];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");
    Util::run(&serverConnectionTerminatedByClient);
}

TEST(WebTransport, ServerDrain)
{
    if (!canLoadnw_webtransport_metadata_set_local_draining())
        return;

    WebTransportServer echoServer([](ConnectionGroup group) -> ConnectionTask {
        auto connection = co_await group.receiveIncomingConnection();
        auto request = co_await connection.awaitableReceiveBytes();
        EXPECT_EQ(request.size(), 3u);
        group.drainWebTransportSession();
    });

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableWebTransport(configuration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];

    NSString *html = [NSString stringWithFormat:@""
        "<script>async function test() {"
        "  try {"
        "    let t = new WebTransport('https://127.0.0.1:%d/');"
        "    await t.ready;"
        "    let c = await t.createUnidirectionalStream();"
        "    let w = c.getWriter();"
        "    await w.write(new TextEncoder().encode('abc'));"
        "    await t.draining;"
        "    alert('successfully receieved draining');"
        "  } catch (e) { alert('caught ' + e); }"
        "}; test();"
        "</script>", echoServer.port()];
    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "successfully receieved draining");
}

} // namespace TestWebKitAPI

#endif // HAVE(WEB_TRANSPORT)
