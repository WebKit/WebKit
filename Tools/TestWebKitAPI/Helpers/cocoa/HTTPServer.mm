/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/HTTPServer.h"

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer/HTTPServerBridging.h"
#import "Helpers/cocoa/NetworkSPI.h"
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static HashMap<String, HTTPResponse> makeRequestMap(std::initializer_list<std::pair<String, HTTPResponse>> responses)
{
    HashMap<String, HTTPResponse> map;
    for (auto& pair : responses)
        map.add(pair.first, pair.second);
    return map;
}

static RetainPtr<NSDictionary<NSString *, NSString *>> toNSDictionary(const HashMap<String, String>& map)
{
    RetainPtr result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
    for (auto& pair : map)
        [result setObject:pair.value.createNSString() forKey:pair.key.createNSString()];
    return result;
}

static RetainPtr<NSArray<NSArray<NSString *> *>> toNSHeaderFields(const Vector<WTF::KeyValuePair<String, String>>& headerFields)
{
    RetainPtr result = adoptNS([[NSMutableArray alloc] initWithCapacity:headerFields.size()]);
    for (auto& field : headerFields)
        [result addObject:@[field.key.createNSString().get(), field.value.createNSString().get()]];
    return result;
}

static RetainPtr<HTTPResponseDataBridge> toHTTPResponseDataBridge(const HTTPResponse& response)
{
    return adoptNS([[HTTPResponseDataBridge alloc] initWithStatusCode:response.statusCode headerFields:toNSHeaderFields(response.headerFields) body:toNSData(response.body.span()) behavior:static_cast<HTTPResponseBehaviorBridge>(response.behavior) shouldRespondWith304:response.shouldRespondWith304ToConditionalRequests headerFieldsFor304:toNSDictionary(response.headerFieldsFor304)]);
}

void HTTPServer::startListening(CompletionHandler<void()>&& completionHandler)
{
    [m_serverBridge startListeningWithCompletionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](NSError *error) mutable {
        RELEASE_ASSERT(!error);
        completionHandler();
    }).get()];
}

void HTTPServer::cancel(CompletionHandler<void()>&& completionHandler)
{
    // The Swift implementation terminates its own connections as part of cancelling.
    [m_serverBridge cancelWithCompletionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler();
    }).get()];
}

void HTTPServer::cancel() {
    bool done = false;
    cancel([&] {
        done = true;
    });
    Util::run(&done);
}

void HTTPServer::terminateAllConnections(CompletionHandler<void()>&& completionHandler)
{
    [m_serverBridge terminateAllConnectionsWithCompletionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler();
    }).get()];
}

HTTPServer::HTTPServer(
    std::initializer_list<std::pair<String, HTTPResponse>> responses,
    Protocol protocol,
    CertificateVerifier&& verifier,
    SecIdentityRef identity,
    std::optional<uint16_t> port,
    DeferListening deferListening
)
    : HTTPServer(makeRequestMap(responses), protocol, WTF::move(verifier), identity, port, deferListening)
{
}

HTTPServer::HTTPServer(
    ResponseMap&& responses,
    Protocol protocol,
    CertificateVerifier&& verifier,
    SecIdentityRef identity,
    std::optional<uint16_t> port,
    DeferListening deferListening
)
    : m_protocol(protocol)
{
    RetainPtr<NSMutableDictionary<NSString *, HTTPResponseDataBridge *>> routes = adoptNS([[NSMutableDictionary alloc] initWithCapacity:responses.size()]);
    for (auto& pair : responses)
        [routes setObject:toHTTPResponseDataBridge(pair.value) forKey:pair.key.createNSString()];

    auto verifierBlock = verifier ? makeBlockPtr([verifier = WTF::move(verifier)](sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t complete) mutable {
        verifier(metadata, trust, complete);
    }) : nil;
    m_serverBridge = adoptNS([[HTTPServerBridge alloc] initWithRoutes:routes protocol:static_cast<HTTPServerProtocolBridge>(protocol) port:port.value_or(0) identity:identity certificateVerifier:verifierBlock.get()]);

    if (deferListening == DeferListening::No) {
        bool done = false;
        startListening([&] {
            done = true;
        });
        Util::run(&done);
    }
}

HTTPServer::HTTPServer(Function<void(Connection)>&& connectionHandler, Protocol protocol)
    : m_protocol(protocol)
{
    m_serverBridge = adoptNS([[HTTPServerBridge alloc] initWithProtocol:static_cast<HTTPServerProtocolBridge>(protocol) connectionHandler:makeBlockPtr([connectionHandler = WTF::move(connectionHandler)](nw_connection_t connection) mutable {
        connectionHandler(Connection(connection));
    }).get()]);

    bool done = false;
    startListening([&] {
        done = true;
    });
    Util::run(&done);
}

HTTPServer::HTTPServer(UseCoroutines, Function<ConnectionTask(Connection)>&& connectionHandler, Protocol protocol)
    : m_protocol(protocol)
{
    m_serverBridge = adoptNS([[HTTPServerBridge alloc] initWithProtocol:static_cast<HTTPServerProtocolBridge>(protocol) connectionHandler:makeBlockPtr([coroutineHandles = Vector<CoroutineHandle<ConnectionTask::promise_type>> { }, connectionHandler = WTF::move(connectionHandler)](nw_connection_t connection) mutable {
        // The coroutine handle has to outlive this call, so it stays on the C++ side.
        coroutineHandles.append(connectionHandler(Connection(connection)).handle);
    }).get()]);

    bool done = false;
    startListening([&] {
        done = true;
    });
    Util::run(&done);
}

void HTTPServer::addResponse(String&& path, HTTPResponse&& response)
{
    [m_serverBridge addResponse:toHTTPResponseDataBridge(response).get() forPath:path.createNSString()];
}

void HTTPServer::setResponse(String&& path, HTTPResponse&& response)
{
    [m_serverBridge setResponse:toHTTPResponseDataBridge(response).get() forPath:path.createNSString()];
}

void HTTPServer::respondWithChallengeThenOK(Connection connection)
{
    connection.receiveHTTPRequest([connection] (Vector<char>&&) {
        constexpr auto challengeHeader =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
        "Content-Length: 0\r\n"
        "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n"_s;
        connection.send(challengeHeader, [connection] {
            connection.receiveHTTPRequest([connection] (Vector<char>&&) {
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 34\r\n\r\n"
                    "<script>alert('success!')</script>"_s, [connection] {
                        respondWithChallengeThenOK(connection);
                    }
                );
            });
        });
    });
}

void HTTPServer::respondWithOK(Connection connection)
{
    connection.receiveHTTPRequest([connection] (Vector<char>&&) {
        connection.send(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 34\r\n\r\n"
            "<script>alert('success!')</script>"_s
        );
    });
}

size_t HTTPServer::totalConnections() const
{
    return [m_serverBridge totalConnections];
}

size_t HTTPServer::totalRequests() const
{
    return [m_serverBridge totalRequests];
}

String HTTPServer::lastRequestCookies() const
{
    return [m_serverBridge lastRequestCookies];
}

bool HTTPServer::sawAuthorizationHeader() const
{
    return [m_serverBridge sawAuthorizationHeader];
}

static ASCIILiteral statusText(unsigned statusCode)
{
    switch (statusCode) {
    case 101:
        return "Switching Protocols"_s;
    case 200:
        return "OK"_s;
    case 204:
        return "No Content"_s;
    case 301:
        return "Moved Permanently"_s;
    case 302:
        return "Found"_s;
    case 303:
        return "See Other"_s;
    case 304:
        return "Not Modified"_s;
    case 401:
        return "Unauthorized"_s;
    case 403:
        return "Forbidden"_s;
    case 404:
        return "Not Found"_s;
    case 503:
        return "Service Unavailable"_s;
    }
    ASSERT_NOT_REACHED();
    return "Unknown Status Code"_s;
}

static Vector<uint8_t> toUTF8Vector(const String& string)
{
    Vector<uint8_t> result;
    std::ignore = string.tryGetUTF8([&](std::span<const char8_t> utf8) {
        result.append(byteCast<uint8_t>(utf8));
        return true;
    });
    return result;
}

String HTTPServer::parsePath(const Vector<char>& request)
{
    if (!request.size())
        return { };
    auto getPathPrefix = "GET "_s;
    auto postPathPrefix = "POST "_s;
    size_t pathEnd = find(request.span(), " HTTP/1.1\r\n"_span);
    ASSERT_WITH_MESSAGE(pathEnd != notFound, "HTTPServer assumes request is HTTP 1.1");
    size_t pathPrefixLength = 0;
    if (spanHasPrefix(request.span(), getPathPrefix.span()))
        pathPrefixLength = getPathPrefix.length();
    else if (spanHasPrefix(request.span(), postPathPrefix.span()))
        pathPrefixLength = postPathPrefix.length();
    ASSERT_WITH_MESSAGE(pathPrefixLength, "HTTPServer assumes request is GET or POST");
    size_t pathLength = pathEnd - pathPrefixLength;
    return String::fromUTF8(request.subspan(pathPrefixLength, pathLength));
}

String HTTPServer::parseBody(const Vector<char>& request)
{
    auto headerEndBytes = "\r\n\r\n"_s;
    size_t headerEnd = find(request.span(), headerEndBytes.span()) + strlen(headerEndBytes);
    return String::fromUTF8(request.subspan(headerEnd));
}

uint16_t HTTPServer::port() const
{
    return [m_serverBridge port];
}

const char* HTTPServer::scheme() const
{
    const char* scheme = nullptr;
    switch (m_protocol) {
    case Protocol::Http:
        scheme = "http";
        break;
    case Protocol::Https:
    case Protocol::HttpsWithLegacyTLS:
    case Protocol::Http2Raw:
    case Protocol::Http2:
    case Protocol::Http3:
        scheme = "https";
        break;
    case Protocol::HttpsProxy:
    case Protocol::HttpsProxyWithAuthentication:
    case Protocol::Http2Proxy:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return scheme;
}

String HTTPServer::origin() const
{
    return [NSString stringWithFormat:@"%s://127.0.0.1:%d", scheme(), port()];
}

NSURLRequest *HTTPServer::request(StringView path) const
{
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"%s://127.0.0.1:%d%@", scheme(), port(), path.createNSString().get()]]];
}

NSURLRequest *HTTPServer::requestWithLocalhost(StringView path) const
{
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"%s://localhost:%d%@", scheme(), port(), path.createNSString().get()]]];
}

WKWebViewConfiguration *HTTPServer::httpsProxyConfiguration() const
{
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    return viewConfiguration.autorelease();
}

Vector<uint8_t> HTTPResponse::bodyFromString(const String& string)
{
    return toUTF8Vector(string);
}

Vector<uint8_t> HTTPResponse::serialize(IncludeContentLength includeContentLength) const
{
    StringBuilder responseBuilder;
    responseBuilder.append("HTTP/1.1 "_s, statusCode, ' ', statusText(statusCode), "\r\n"_s);
    if (includeContentLength == IncludeContentLength::Yes)
        responseBuilder.append("Content-Length: "_s, body.size(), "\r\n"_s);
    for (auto& pair : headerFields)
        responseBuilder.append(pair.key, ": "_s, pair.value, "\r\n"_s);
    responseBuilder.append("\r\n"_s);

    auto bytesToSend = toUTF8Vector(responseBuilder.toString());
    bytesToSend.appendVector(body);
    return bytesToSend;
}

} // namespace TestWebKitAPI
