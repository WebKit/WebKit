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

#import "config.h"
#import "NetworkConnection.h"

#import "HTTPServer.h"
#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/SHA1.h>
#import <wtf/StdLibExtras.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringToIntegerConversion.h>

namespace TestWebKitAPI {

static Vector<uint8_t> vectorFromData(dispatch_data_t content)
{
    ASSERT(content);
    __block Vector<uint8_t> request;
    dispatch_data_apply(content, ^bool(dispatch_data_t, size_t, const void* buffer, size_t size) {
        request.append(std::span { static_cast<const uint8_t*>(buffer), size });
        return true;
    });
    return request;
}

static RetainPtr<dispatch_data_t> dataFromString(String&& s)
{
    auto impl = s.releaseImpl();
    ASSERT(impl->is8Bit());
    auto characters = impl->span8();
    return adoptNS(dispatch_data_create(characters.data(), characters.size(), mainDispatchQueueSingleton(), ^{
        (void)impl;
    }));
}

void Connection::receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler, size_t minimumSize) const
{
    nw_connection_receive(m_connection.get(), minimumSize, std::numeric_limits<uint32_t>::max(), makeBlockPtr([connection = *this, completionHandler = WTFMove(completionHandler)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
        if (error || !content)
            return completionHandler({ });
        completionHandler(vectorFromData(content));
    }).get());
}

void Connection::receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&& completionHandler, Vector<char>&& buffer) const
{
    receiveBytes([connection = *this, completionHandler = WTFMove(completionHandler), buffer = WTFMove(buffer)](Vector<uint8_t>&& bytes) mutable {
        buffer.appendVector(WTFMove(bytes));
        if (size_t doubleNewlineIndex = find(buffer.span(), "\r\n\r\n"_span); doubleNewlineIndex != notFound) {
            if (size_t contentLengthBeginIndex = find(buffer.span(), "Content-Length"_span); contentLengthBeginIndex != notFound) {
                size_t contentLength = parseIntegerAllowingTrailingJunk<int>(buffer.span().subspan(contentLengthBeginIndex + strlen("Content-Length: "))).value_or(0);
                size_t headerLength = doubleNewlineIndex + strlen("\r\n\r\n");
                if (buffer.size() - headerLength < contentLength)
                    return connection.receiveHTTPRequest(WTFMove(completionHandler), WTFMove(buffer));
            }
            completionHandler(WTFMove(buffer));
        } else
            connection.receiveHTTPRequest(WTFMove(completionHandler), WTFMove(buffer));
    });
}

ReceiveHTTPRequestOperation Connection::awaitableReceiveHTTPRequest() const
{
    return { *this };
}

void ReceiveHTTPRequestOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.receiveHTTPRequest([this, handle](Vector<char>&& result) mutable {
        m_result = WTFMove(result);
        handle();
    });
}

ReceiveBytesOperation Connection::awaitableReceiveBytes() const
{
    return { *this };
}

void ReceiveBytesOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.receiveBytes([this, handle](Vector<uint8_t>&& result) mutable {
        m_result = WTFMove(result);
        handle();
    });
}

void SendOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.send(WTFMove(m_data), [handle] (bool) mutable {
        handle();
    });
}

SendOperation Connection::awaitableSend(Vector<uint8_t>&& message)
{
    return { makeDispatchData(WTFMove(message)), *this };
}

SendOperation Connection::awaitableSend(String&& message)
{
    return { dataFromString(WTFMove(message)), *this };
}

SendOperation Connection::awaitableSend(RetainPtr<dispatch_data_t>&& data)
{
    return { WTFMove(data), *this };
}

void Connection::send(String&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(dataFromString(WTFMove(message)), [completionHandler = WTFMove(completionHandler)] (bool) mutable {
        if (completionHandler)
            completionHandler();
    });
}

void Connection::send(Vector<uint8_t>&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(makeDispatchData(WTFMove(message)), [completionHandler = WTFMove(completionHandler)] (bool) mutable {
        if (completionHandler)
            completionHandler();
    });
}

void Connection::sendAndReportError(Vector<uint8_t>&& message, CompletionHandler<void(bool)>&& completionHandler) const
{
    send(makeDispatchData(WTFMove(message)), WTFMove(completionHandler));
}

void Connection::send(RetainPtr<dispatch_data_t>&& message, CompletionHandler<void(bool)>&& completionHandler) const
{
    nw_connection_send(m_connection.get(), message.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([completionHandler = WTFMove(completionHandler)](nw_error_t error) mutable {
        if (completionHandler)
            completionHandler(!!error);
    }).get());
}

void Connection::webSocketHandshake(CompletionHandler<void()>&& connectionHandler)
{
    receiveHTTPRequest([connection = Connection(*this), connectionHandler = WTFMove(connectionHandler)] (Vector<char>&& request) mutable {

        auto webSocketAcceptValue = [] (const Vector<char>& request) {
            constexpr auto keyHeaderField = "Sec-WebSocket-Key: "_s;
            size_t keyBegin = find(request.span(), keyHeaderField.span());
            ASSERT(keyBegin != notFound);
            auto keySpan = request.subspan(keyBegin + keyHeaderField.length());
            size_t keyEnd = find(keySpan, "\r\n"_span);
            ASSERT(keyEnd != notFound);

            constexpr auto webSocketKeyGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"_s;
            SHA1 sha1;
            sha1.addBytes(byteCast<uint8_t>(keySpan.first(keyEnd)));
            sha1.addBytes(webSocketKeyGUID.span());
            SHA1::Digest hash;
            sha1.computeHash(hash);
            return base64EncodeToString(hash);
        };

        connection.send(HTTPResponse(101, {
            { "Upgrade"_s, "websocket"_s },
            { "Connection"_s, "Upgrade"_s },
            { "Sec-WebSocket-Accept"_s, webSocketAcceptValue(request) }
        }).serialize(HTTPResponse::IncludeContentLength::No), WTFMove(connectionHandler));
    });
}

void Connection::terminate(CompletionHandler<void()>&& completionHandler)
{
    nw_connection_set_state_changed_handler(m_connection.get(), makeBlockPtr([completionHandler = WTFMove(completionHandler)] (nw_connection_state_t state, nw_error_t error) mutable {
        ASSERT_UNUSED(error, !error);
        if (state == nw_connection_state_cancelled && completionHandler)
            completionHandler();
    }).get());
    nw_connection_cancel(m_connection.get());
}

#if HAVE(WEB_TRANSPORT)

// FIXME: This shouldn't need to be thread safe.
// Make it non-thread-safe once rdar://161905206 is resolved.
struct ConnectionGroup::Data : public ThreadSafeRefCounted<ConnectionGroup::Data> {
    static Ref<Data> create(nw_connection_group_t group) { return adoptRef(*new Data(group)); }
    Data(nw_connection_group_t group)
        : group(group) { }

    RetainPtr<nw_connection_group_t> group;
    CompletionHandler<void(Connection)> connectionHandler;
    Vector<Connection> connections;
};

ConnectionGroup::ConnectionGroup(nw_connection_group_t group)
    : m_data(Data::create(group)) { }

ConnectionGroup::~ConnectionGroup() = default;

ConnectionGroup::ConnectionGroup(const ConnectionGroup&) = default;

Connection ConnectionGroup::createWebTransportConnection(ConnectionType type) const
{
    RetainPtr webtransportOptions = adoptNS(nw_webtransport_create_options());
    ASSERT(webtransportOptions);
    nw_webtransport_options_set_is_unidirectional(webtransportOptions.get(), type == ConnectionType::Unidirectional);
    nw_webtransport_options_set_is_datagram(webtransportOptions.get(), type == ConnectionType::Datagram);

    RetainPtr connection = adoptNS(nw_connection_group_extract_connection(m_data->group.get(), nil, webtransportOptions.get()));
    ASSERT(connection);
    nw_connection_set_queue(connection.get(), mainDispatchQueueSingleton());
    nw_connection_start(connection.get());
    return Connection(connection.get());
}

void ConnectionGroup::cancel()
{
    nw_connection_group_cancel(m_data->group.get());
}

void ReceiveIncomingConnectionOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_group.receiveIncomingConnection([this, handle](Connection result) mutable {
        m_result = WTFMove(result);
        handle();
    });
}

ReceiveIncomingConnectionOperation ConnectionGroup::receiveIncomingConnection() const
{
    return { *this };
}

void ConnectionGroup::receiveIncomingConnection(CompletionHandler<void(Connection)>&& connectionHandler)
{
    m_data->connectionHandler = WTFMove(connectionHandler);
}

void ConnectionGroup::receiveIncomingConnection(Connection connection)
{
    m_data->connections.append(connection);
    nw_connection_set_state_changed_handler(connection.m_connection.get(), [connection, data = m_data] (nw_connection_state_t state, nw_error_t error) mutable {
        if (state != nw_connection_state_ready)
            return;
        data->connectionHandler(connection);
    });
    nw_connection_set_queue(connection.m_connection.get(), mainDispatchQueueSingleton());
    nw_connection_start(connection.m_connection.get());
}

#endif // HAVE(WEB_TRANSPORT)

} // namespace TestWebKitAPI
