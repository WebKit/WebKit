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

#include "config.h"
#include "HTTPServer.h"

#if PLATFORM(COCOA)

#include "Logging.h"
#include "LoopbackCocoa.h"
#include <CFNetwork/CFNetwork.h>
#include <Network/Network.h>
#include <wtf/JSONValues.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/darwin/DispatchExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

// minidriver's HTTP server. Network.framework accepts the connections, and CFHTTPMessage parses
// requests and serializes responses. It listens on 127.0.0.1 and ::1, keeps connections alive and
// answers pipelined requests in order, and drops connections whose header is too large or whose
// request doesn't arrive in time. Before reading a body, it rejects requests whose Host or Origin
// isn't on this machine, and bodies that are too large.

namespace WebDriver {

// Room for 100 header lines of up to 8190 bytes. A connection whose request header is larger is
// dropped without a response.
static constexpr size_t maximumHeaderSize = 100 * 8190;

// The largest WebDriver requests carry script arguments or text to type. A larger body gets a 413.
static constexpr size_t maximumBodySize = 64 * MB;

// A connection that doesn't send a whole request in this time is dropped, including an idle one.
static constexpr Seconds requestTimeout = 30_s;

static String headerValue(CFHTTPMessageRef message, CFStringRef name)
{
    return adoptCF(CFHTTPMessageCopyHeaderFieldValue(message, name)).get();
}

static bool hasVersion(CFHTTPMessageRef message, CFStringRef version)
{
    RetainPtr messageVersion = adoptCF(CFHTTPMessageCopyVersion(message));
    return messageVersion && CFEqual(messageVersion.get(), version);
}

class HTTPConnection : public RefCountedAndCanMakeWeakPtr<HTTPConnection> {
public:
    static Ref<HTTPConnection> create(nw_connection_t connection, HTTPRequestHandler& requestHandler)
    {
        return adoptRef(*new HTTPConnection(connection, requestHandler));
    }

    void start();

private:
    HTTPConnection(nw_connection_t, HTTPRequestHandler&);

    enum class AfterSending : uint8_t { Continue, StartNextRequest, Close };

    void startRequest();
    void receive();
    void didReceiveData(std::span<const uint8_t>);
    bool didReceiveHeader();
    void didReceiveRequest(std::span<const uint8_t> body);
    bool isFromThisMachine() const;
    bool shouldCloseAfterResponse() const;
    void respond(HTTPRequestHandler::Response&&, bool closeAfterSending);
    void respondWithStatus(unsigned statusCode, bool closeAfterSending) { respond({ statusCode, { }, { } }, closeAfterSending); }
    void send(CFHTTPMessageRef, UTF8CString body, AfterSending);
    void close();

    const OSObjectPtr<nw_connection_t> m_connection;
    HTTPRequestHandler& m_requestHandler;
    RunLoop::Timer m_requestTimer;
    RetainPtr<CFHTTPMessageRef> m_request;
    size_t m_headerSize { 0 };
    std::optional<size_t> m_contentLength;
    Vector<uint8_t> m_nextRequestData;
    bool m_isHandlingRequest { false };
    bool m_clientDidClose { false };
    bool m_isClosed { false };
};

HTTPConnection::HTTPConnection(nw_connection_t connection, HTTPRequestHandler& requestHandler)
    : m_connection(connection)
    , m_requestHandler(requestHandler)
    , m_requestTimer(RunLoop::mainSingleton(), "HTTPConnection::requestTimer"_s, this, &HTTPConnection::close)
{
}

void HTTPConnection::start()
{
    nw_connection_set_queue(m_connection.get(), mainDispatchQueueSingleton());
    nw_connection_start(m_connection.get());
    startRequest();
}

void HTTPConnection::startRequest()
{
    m_request = adoptCF(CFHTTPMessageCreateEmpty(kCFAllocatorDefault, true));
    m_headerSize = 0;
    m_contentLength = std::nullopt;
    m_isHandlingRequest = false;
    // Receiving more data doesn't restart the timer, so that a client can't hold the connection
    // by trickling a request.
    m_requestTimer.startOneShot(requestTimeout);

    // What the client sent after the previous request is the start of this one.
    if (auto data = std::exchange(m_nextRequestData, { }); !data.isEmpty()) {
        didReceiveData(data.span());
        if (m_isHandlingRequest || m_isClosed)
            return;
    }

    if (m_clientDidClose) {
        close();
        return;
    }

    receive();
}

void HTTPConnection::receive()
{
    RefPtr protectedThis = this;
    nw_connection_receive(m_connection.get(), 1, std::numeric_limits<uint32_t>::max(), ^(dispatch_data_t content, nw_content_context_t, bool isComplete, nw_error_t error) {
        if (protectedThis->m_isClosed)
            return;

        if (error) {
            protectedThis->close();
            return;
        }

        protectedThis->m_clientDidClose = isComplete;
        if (content) {
            const void* bytes = nullptr;
            size_t size = 0;
            OSObjectPtr contiguousContent = adoptOSObject(dispatch_data_create_map(content, &bytes, &size));
            protectedThis->didReceiveData(unsafeMakeSpan(static_cast<const uint8_t*>(bytes), size));
        }

        // Receiving resumes once the response has been sent.
        if (protectedThis->m_isClosed || protectedThis->m_isHandlingRequest)
            return;

        // The client went away before sending a whole request.
        if (protectedThis->m_clientDidClose) {
            protectedThis->close();
            return;
        }

        protectedThis->receive();
    });
}

void HTTPConnection::didReceiveData(std::span<const uint8_t> data)
{
    if (!CFHTTPMessageAppendBytes(m_request.get(), data.data(), data.size())) {
        respondWithStatus(400, true);
        return;
    }

    if (!m_contentLength) {
        if (!CFHTTPMessageIsHeaderComplete(m_request.get())) {
            m_headerSize += data.size();
            if (m_headerSize > maximumHeaderSize)
                close();
            return;
        }

        if (!didReceiveHeader())
            return;
    }

    // Everything after the header is in the body, including the start of any next request.
    RetainPtr body = adoptCF(CFHTTPMessageCopyBody(m_request.get()));
    auto bodyBytes = body ? span(body.get()) : std::span<const uint8_t> { };
    if (bodyBytes.size() < *m_contentLength)
        return;

    didReceiveRequest(bodyBytes);
}

bool HTTPConnection::didReceiveHeader()
{
    // Only serve clients on this machine, so that web pages can't create sessions. This is checked
    // before the body is read, so that a page can't make this side buffer one.
    if (!isFromThisMachine()) {
        auto error = JSON::Object::create();
        error->setString("error"_s, "invalid argument"_s);
        error->setString("message"_s, "'Host' header or 'Origin' header is specified and is not localhost."_s);
        error->setString("stacktrace"_s, emptyString());
        auto responseBody = JSON::Object::create();
        responseBody->setObject("value"_s, WTF::move(error));
        respond({ 400, responseBody->toJSONString().utf8(), "application/json; charset=utf-8"_s }, true);
        return false;
    }

    // A request line that CFHTTPMessage can't make sense of has no method.
    if (RetainPtr method = adoptCF(CFHTTPMessageCopyRequestMethod(m_request.get())); !method) {
        respondWithStatus(405, true);
        return false;
    }

    // WebDriver clients send bodies with a Content-Length, so chunked bodies aren't supported.
    if (!headerValue(m_request.get(), CFSTR("Transfer-Encoding")).isNull()) {
        respondWithStatus(501, true);
        return false;
    }

    auto contentLength = headerValue(m_request.get(), CFSTR("Content-Length"));
    if (contentLength.isNull())
        m_contentLength = 0;
    else {
        m_contentLength = parseInteger<size_t>(contentLength);
        if (!m_contentLength) {
            respondWithStatus(400, true);
            return false;
        }
        if (*m_contentLength > maximumBodySize) {
            respondWithStatus(413, true);
            return false;
        }
    }

    if (!contentLength.isNull() && hasVersion(m_request.get(), kCFHTTPVersion1_1)) {
        auto expect = headerValue(m_request.get(), CFSTR("Expect"));
        for (auto token : StringView { expect }.split(',')) {
            if (equalLettersIgnoringASCIICase(token.trim(isASCIIWhitespace<char16_t>), "100-continue"_s)) {
                send(adoptCF(CFHTTPMessageCreateResponse(kCFAllocatorDefault, 100, nullptr, kCFHTTPVersion1_1)).get(), { }, AfterSending::Continue);
                break;
            }
        }
    }

    return true;
}

bool HTTPConnection::isFromThisMachine() const
{
    auto host = headerValue(m_request.get(), CFSTR("Host"));
    if (!host.isNull() && !isLoopbackURL(makeString("http://"_s, host)))
        return false;
    auto origin = headerValue(m_request.get(), CFSTR("Origin"));
    return origin.isNull() || isLoopbackURL(origin);
}

void HTTPConnection::didReceiveRequest(std::span<const uint8_t> body)
{
    // Handling a request can take much longer than receiving it.
    m_requestTimer.stop();

    m_nextRequestData = body.subspan(*m_contentLength);
    body = body.first(*m_contentLength);

    if (!hasVersion(m_request.get(), kCFHTTPVersion1_0) && !hasVersion(m_request.get(), kCFHTTPVersion1_1)) {
        respondWithStatus(505, shouldCloseAfterResponse());
        return;
    }

    RetainPtr method = adoptCF(CFHTTPMessageCopyRequestMethod(m_request.get()));
    RetainPtr url = adoptCF(CFHTTPMessageCopyRequestURL(m_request.get()));
    if (!url) {
        respondWithStatus(400, true);
        return;
    }

    RetainPtr path = adoptCF(CFURLCopyPath(url.get()));
    RetainPtr decodedPath = adoptCF(CFURLCreateStringByReplacingPercentEscapes(kCFAllocatorDefault, path.get(), CFSTR("")));

    HTTPRequestHandler::Request request;
    request.method = method.get();
    request.path = decodedPath ? decodedPath.get() : path.get();
    if (!body.empty()) {
        request.data = byteCast<char>(body.data());
        request.dataLength = body.size();
    }

    // The request handler reads the body before it returns.
    m_isHandlingRequest = true;
    m_requestHandler.handleRequest(WTF::move(request), [protectedThis = Ref { *this }, closeAfterSending = shouldCloseAfterResponse()](HTTPRequestHandler::Response&& response) {
        protectedThis->respond(WTF::move(response), closeAfterSending);
    });
}

bool HTTPConnection::shouldCloseAfterResponse() const
{
    auto connection = headerValue(m_request.get(), CFSTR("Connection"));
    if (hasVersion(m_request.get(), kCFHTTPVersion1_0))
        return !equalLettersIgnoringASCIICase(connection, "keep-alive"_s);
    return equalLettersIgnoringASCIICase(connection, "close"_s);
}

void HTTPConnection::respond(HTTPRequestHandler::Response&& response, bool closeAfterSending)
{
    if (m_isClosed)
        return;

    // Don't read the next request until this response has been sent.
    m_isHandlingRequest = true;

    RetainPtr message = adoptCF(CFHTTPMessageCreateResponse(kCFAllocatorDefault, response.statusCode, nullptr, kCFHTTPVersion1_1));
    if (!response.contentType.isEmpty())
        CFHTTPMessageSetHeaderFieldValue(message.get(), CFSTR("Content-Type"), response.contentType.createCFString().get());
    CFHTTPMessageSetHeaderFieldValue(message.get(), CFSTR("Content-Length"), String::number(response.data.length()).createCFString().get());
    CFHTTPMessageSetHeaderFieldValue(message.get(), CFSTR("Cache-Control"), CFSTR("no-cache"));
    CFHTTPMessageSetHeaderFieldValue(message.get(), CFSTR("Connection"), closeAfterSending ? CFSTR("close") : CFSTR("keep-alive"));
    send(message.get(), WTF::move(response.data), closeAfterSending ? AfterSending::Close : AfterSending::StartNextRequest);
}

void HTTPConnection::send(CFHTTPMessageRef message, UTF8CString body, AfterSending afterSending)
{
    RetainPtr header = adoptCF(CFHTTPMessageCopySerializedMessage(message));
    if (!header) {
        close();
        return;
    }

    // Send the body without copying it. The blocks keep the header and body alive.
    auto headerBytes = span(header.get());
    OSObjectPtr data = adoptOSObject(dispatch_data_create(headerBytes.data(), headerBytes.size(), mainDispatchQueueSingleton(), ^{
        (void)header;
    }));
    if (body.length()) {
        auto bodyBytes = body.span();
        OSObjectPtr bodyData = adoptOSObject(dispatch_data_create(bodyBytes.data(), bodyBytes.size(), mainDispatchQueueSingleton(), ^{
            (void)body;
        }));
        data = adoptOSObject(dispatch_data_create_concat(data.get(), bodyData.get()));
    }

    RefPtr protectedThis = this;
    nw_connection_send(m_connection.get(), data.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, ^(nw_error_t error) {
        if (error) {
            RELEASE_LOG_ERROR(WebDriverClassic, "Sending an HTTP response failed with error %d", nw_error_get_error_code(error));
            protectedThis->close();
            return;
        }
        switch (afterSending) {
        case AfterSending::Continue:
            return;
        case AfterSending::StartNextRequest:
            protectedThis->startRequest();
            return;
        case AfterSending::Close:
            protectedThis->close();
            return;
        }
    });
}

void HTTPConnection::close()
{
    if (m_isClosed)
        return;
    m_isClosed = true;
    m_requestTimer.stop();
    nw_connection_cancel(m_connection.get());
}

bool HTTPServer::listen(const std::optional<String>& host, unsigned port)
{
    if (!validateListeningHost(host.value_or(String())))
        return false;

    auto boundPort = startListener("127.0.0.1"_s, port);
    if (!boundPort || !startListener("::1"_s, *boundPort)) {
        disconnect();
        return false;
    }

    // The WebSocket server only listens on 127.0.0.1, so that's the host in webSocketUrl.
    m_visibleHost = "127.0.0.1"_s;
    return true;
}

std::optional<uint16_t> HTTPServer::startListener(ASCIILiteral address, uint16_t port)
{
    OSObjectPtr parameters = adoptOSObject(nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    nw_parameters_set_reuse_local_address(parameters.get(), true);
    nw_parameters_set_local_endpoint(parameters.get(), adoptOSObject(nw_endpoint_create_host(address.characters(), String::number(port).utf8().legacyCStringPointer())).get());

    OSObjectPtr listener = adoptOSObject(nw_listener_create(parameters.get()));
    if (!listener)
        return std::nullopt;

    auto& requestHandler = m_requestHandler;
    nw_listener_set_new_connection_handler(listener.get(), ^(nw_connection_t connection) {
        OSObjectPtr<nw_connection_t> newConnection = connection;
        dispatch_async(mainDispatchQueueSingleton(), ^{
            HTTPConnection::create(newConnection.get(), requestHandler)->start();
        });
    });

    // The listener's handlers run on a queue of their own, so that this can wait for the listener to
    // be ready before the run loop runs.
    OSObjectPtr queue = adoptOSObject(dispatch_queue_create("org.webkit.minidriver.HTTPServer", serialQueueWithAutoreleasePoolAttrSingleton()));
    nw_listener_set_queue(listener.get(), queue.get());

    OSObjectPtr semaphore = adoptOSObject(dispatch_semaphore_create(0));
    __block bool isReady = false;
    __block bool didReport = false;
    nw_listener_set_state_changed_handler(listener.get(), ^(nw_listener_state_t state, nw_error_t error) {
        if (didReport) {
            if (state == nw_listener_state_failed)
                RELEASE_LOG_ERROR(WebDriverClassic, "HTTP listener failed with error %d", nw_error_get_error_code(error));
            return;
        }
        if (state != nw_listener_state_ready && state != nw_listener_state_failed && state != nw_listener_state_waiting && state != nw_listener_state_cancelled)
            return;
        isReady = state == nw_listener_state_ready;
        didReport = true;
        dispatch_semaphore_signal(semaphore.get());
    });
    nw_listener_start(listener.get());

    dispatch_semaphore_wait(semaphore.get(), DISPATCH_TIME_FOREVER);
    if (!isReady) {
        nw_listener_cancel(listener.get());
        return std::nullopt;
    }

    uint16_t boundPort = nw_listener_get_port(listener.get());
    m_listeners.append(WTF::move(listener));
    return boundPort;
}

void HTTPServer::disconnect()
{
    for (auto& listener : m_listeners)
        nw_listener_cancel(listener.get());
    m_listeners.clear();
}

} // namespace WebDriver

#endif // PLATFORM(COCOA)
