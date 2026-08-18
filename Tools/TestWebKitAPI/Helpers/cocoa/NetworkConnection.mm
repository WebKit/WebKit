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
#import "Helpers/cocoa/NetworkConnection.h"

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/NetworkSPI.h"
#import <Security/Security.h>
#import <wtf/BlockPtr.h>
#import <wtf/SHA1.h>
#import <wtf/StdLibExtras.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/Base64.h>
#import <wtf/text/MakeString.h>
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

static OSObjectPtr<dispatch_data_t> dataFromString(String&& s)
{
    auto impl = s.releaseImpl();
    ASSERT(impl->is8Bit());
    auto characters = impl->span8();
    return adoptOSObject(dispatch_data_create(characters.data(), characters.size(), mainDispatchQueueSingleton(), ^{
        (void)impl;
    }));
}

void Connection::receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler, size_t minimumSize) const
{
    nw_connection_receive(m_connection.get(), minimumSize, std::numeric_limits<uint32_t>::max(), makeBlockPtr([connection = *this, completionHandler = WTF::move(completionHandler)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
        if (error || !content)
            return completionHandler({ });
        completionHandler(vectorFromData(content));
    }).get());
}

void Connection::receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&& completionHandler, Vector<char>&& buffer) const
{
    receiveBytes([connection = *this, completionHandler = WTF::move(completionHandler), buffer = WTF::move(buffer)](Vector<uint8_t>&& bytes) mutable {
        buffer.appendVector(WTF::move(bytes));
        if (size_t doubleNewlineIndex = find(buffer.span(), "\r\n\r\n"_span); doubleNewlineIndex != notFound) {
            if (size_t contentLengthBeginIndex = find(buffer.span(), "Content-Length"_span); contentLengthBeginIndex != notFound) {
                size_t contentLength = parseIntegerAllowingTrailingJunk<int>(buffer.span().subspan(contentLengthBeginIndex + strlen("Content-Length: "))).value_or(0);
                size_t headerLength = doubleNewlineIndex + strlen("\r\n\r\n");
                if (buffer.size() - headerLength < contentLength)
                    return connection.receiveHTTPRequest(WTF::move(completionHandler), WTF::move(buffer));
            }
            completionHandler(WTF::move(buffer));
        } else
            connection.receiveHTTPRequest(WTF::move(completionHandler), WTF::move(buffer));
    });
}

ReceiveHTTPRequestOperation Connection::awaitableReceiveHTTPRequest() const
{
    return { *this };
}

void ReceiveHTTPRequestOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.receiveHTTPRequest([this, handle](Vector<char>&& result) mutable {
        m_result = WTF::move(result);
        handle();
    });
}

#if HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)

void Connection::receiveHTTPMessagingRequest(CompletionHandler<void(HTTPRequestData&&)>&& completionHandler, HTTPRequestData&& partial) const
{
    nw_connection_receive_message(m_connection.get(), makeBlockPtr([connection = *this, completionHandler = WTF::move(completionHandler), partial = WTF::move(partial)](dispatch_data_t content, nw_content_context_t context, bool isComplete, nw_error_t error) mutable {
        __block HTTPRequestData blockPartial = WTF::move(partial);
        // Discard any headers/body already accumulated from earlier chunks: an error mid-stream (e.g. the
        // client resets the stream after sending headers but before finishing the body) must not be mistaken
        // by callers for a successfully-received request just because path happens to already be set.
        if (error)
            return completionHandler({ });

        if (context) {
            if (RetainPtr metadata = adoptNS(nw_content_context_copy_protocol_metadata(context, adoptNS(nw_protocol_copy_http_definition()).get()))) {
                if (nw_http_metadata_get_type(metadata.get()) == nw_http_metadata_type_request) {
                    if (RetainPtr request = adoptNS(nw_http_metadata_copy_request(metadata.get()))) {
                        nw_http_request_access_method(request.get(), ^(const char* method) {
                            blockPartial.method = String::fromUTF8(method);
                        });
                        nw_http_request_access_path(request.get(), ^(const char* path) {
                            if (path)
                                blockPartial.path = String::fromUTF8(path);
                        });
                        if (RetainPtr fields = adoptNS(nw_http_request_copy_header_fields(request.get()))) {
                            nw_http_fields_enumerate(fields.get(), ^bool(const char* name, size_t nameLength, const char* value, size_t valueLength) {
                                String fieldValue = String::fromUTF8(std::span(value, valueLength));
                                auto addResult = blockPartial.headerFields.add(String::fromUTF8(std::span(name, nameLength)), fieldValue);
                                // RFC 7230 3.2.2: repeated fields with the same name are combined by joining with a comma.
                                if (!addResult.isNewEntry)
                                    addResult.iterator->value = makeString(addResult.iterator->value, ", "_s, fieldValue);
                                return true;
                            });
                        }
                    }
                }
            }
        }

        if (content)
            blockPartial.body.appendVector(vectorFromData(content));

        if (isComplete)
            return completionHandler(WTF::move(blockPartial));

        connection.receiveHTTPMessagingRequest(WTF::move(completionHandler), WTF::move(blockPartial));
    }).get());
}

void Connection::sendHTTPMessagingResponse(const HTTPResponse& response, CompletionHandler<void()>&& completionHandler) const
{
    RetainPtr httpResponse = adoptNS(nw_http_response_create(response.statusCode, nullptr));
    RetainPtr fields = adoptNS(nw_http_fields_create());
    for (auto& pair : response.headerFields)
        nw_http_fields_append(fields.get(), pair.key.utf8().data(), pair.value.utf8().data());
    nw_http_response_set_header_fields(httpResponse.get(), fields.get());

    RetainPtr metadata = adoptNS(nw_http_create_metadata_for_response(httpResponse.get()));
    RetainPtr context = adoptNS(nw_content_context_create("response"));
    nw_content_context_set_metadata_for_protocol(context.get(), metadata.get());

    nw_connection_send(m_connection.get(), makeDispatchData(Vector<uint8_t>(response.body)).get(), context.get(), true, makeBlockPtr([completionHandler = WTF::move(completionHandler)](nw_error_t error) mutable {
        ASSERT_UNUSED(error, !error);
        if (completionHandler)
            completionHandler();
    }).get());
}

#endif // HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)

ReceiveBytesOperation Connection::awaitableReceiveBytes() const
{
    return { *this };
}

void ReceiveBytesOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.receiveBytes([this, handle](Vector<uint8_t>&& result) mutable {
        m_result = WTF::move(result);
        handle();
    });
}

void SendOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.send(WTF::move(m_data), [handle] (bool) mutable {
        handle();
    }, m_isComplete);
}

SendOperation Connection::awaitableSend(Vector<uint8_t>&& message, bool isComplete)
{
    return { makeDispatchData(WTF::move(message)), *this, isComplete };
}

SendOperation Connection::awaitableSend(String&& message, bool isComplete)
{
    return { dataFromString(WTF::move(message)), *this, isComplete };
}

SendOperation Connection::awaitableSend(OSObjectPtr<dispatch_data_t>&& data, bool isComplete)
{
    return { WTF::move(data), *this, isComplete };
}

void Connection::send(String&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(dataFromString(WTF::move(message)), [completionHandler = WTF::move(completionHandler)] (bool) mutable {
        if (completionHandler)
            completionHandler();
    });
}

void Connection::send(Vector<uint8_t>&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(makeDispatchData(WTF::move(message)), [completionHandler = WTF::move(completionHandler)] (bool) mutable {
        if (completionHandler)
            completionHandler();
    });
}

void Connection::sendAndReportError(Vector<uint8_t>&& message, CompletionHandler<void(bool)>&& completionHandler) const
{
    send(makeDispatchData(WTF::move(message)), WTF::move(completionHandler));
}

void Connection::send(OSObjectPtr<dispatch_data_t>&& message, CompletionHandler<void(bool)>&& completionHandler, bool isComplete) const
{
    nw_connection_send(m_connection.get(), message.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, isComplete, makeBlockPtr([completionHandler = WTF::move(completionHandler)](nw_error_t error) mutable {
        if (completionHandler)
            completionHandler(!!error);
    }).get());
}

void Connection::webSocketHandshake(CompletionHandler<void()>&& connectionHandler)
{
    receiveHTTPRequest([connection = Connection(*this), connectionHandler = WTF::move(connectionHandler)] (Vector<char>&& request) mutable {

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
        }).serialize(HTTPResponse::IncludeContentLength::No), WTF::move(connectionHandler));
    });
}

void Connection::terminate(CompletionHandler<void()>&& completionHandler)
{
    nw_connection_set_state_changed_handler(m_connection.get(), makeBlockPtr([completionHandler = WTF::move(completionHandler)] (nw_connection_state_t state, nw_error_t error) mutable {
        ASSERT_UNUSED(error, !error);
        if (state == nw_connection_state_cancelled && completionHandler)
            completionHandler();
    }).get());
    nw_connection_cancel(m_connection.get());
}

#if HAVE(WEBTRANSPORT)

void Connection::abortReads(uint64_t errorCode)
{
    nw_connection_abort_reads(m_connection.get(), errorCode);
}

void Connection::abortWrites(uint64_t errorCode)
{
    nw_connection_abort_writes(m_connection.get(), errorCode);
}

void Connection::setRemoteReceiveErrorHandler(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    RetainPtr metadata = adoptNS(nw_connection_copy_protocol_metadata(m_connection.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get()));
    if (metadata) {
        nw_webtransport_metadata_set_remote_receive_error_handler(metadata.get(), makeBlockPtr([completionHandler = WTF::move(completionHandler)] (uint64_t errorCode) mutable {
            completionHandler(errorCode);
        }).get(), mainDispatchQueueSingleton());
    }
}

void Connection::setRemoteSendErrorHandler(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    RetainPtr metadata = adoptNS(nw_connection_copy_protocol_metadata(m_connection.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get()));
    if (metadata) {
        nw_webtransport_metadata_set_remote_send_error_handler(metadata.get(), makeBlockPtr([completionHandler = WTF::move(completionHandler)] (uint64_t errorCode) mutable {
            completionHandler(errorCode);
        }).get(), mainDispatchQueueSingleton());
    }
}

// FIXME: This shouldn't need to be thread safe.
// Make it non-thread-safe once rdar://161905206 is resolved.
struct ConnectionGroup::Data : public ThreadSafeRefCounted<ConnectionGroup::Data> {
    static Ref<Data> create(nw_connection_group_t group) { return adoptRef(*new Data(group)); }
    Data(nw_connection_group_t group)
        : group(group) { }

    RetainPtr<nw_connection_group_t> group;
    CompletionHandler<void(Connection)> connectionHandler;
    Vector<Connection> connections;
    CompletionHandler<void()> failureCompletionHandler;
};

ConnectionGroup::ConnectionGroup(nw_connection_group_t group)
    : m_data(Data::create(group)) { }

ConnectionGroup::~ConnectionGroup() = default;

ConnectionGroup::ConnectionGroup(const ConnectionGroup&) = default;

void ConnectionGroup::markAsFailed()
{
    if (auto handler = std::exchange(m_data->failureCompletionHandler, nullptr))
        handler();
}

Awaitable<void> ConnectionGroup::awaitableFailure()
{
    co_return co_await AwaitableFromCompletionHandler<void> { [data = m_data] (auto completionHandler) {
        data->failureCompletionHandler = WTF::move(completionHandler);
    } };
}

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
        m_result = WTF::move(result);
        handle();
    });
}

ReceiveIncomingConnectionOperation ConnectionGroup::receiveIncomingConnection() const
{
    return { *this };
}

void ConnectionGroup::receiveIncomingConnection(CompletionHandler<void(Connection)>&& connectionHandler)
{
    m_data->connectionHandler = WTF::move(connectionHandler);
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

void ConnectionGroup::drainWebTransportSession()
{
    RetainPtr metadata = nw_connection_group_copy_protocol_metadata(m_data->group.get(), adoptNS(nw_protocol_copy_webtransport_definition()).get());
    if (metadata)
        nw_webtransport_metadata_set_local_draining(metadata.get());
}

static RetainPtr<sec_protocol_metadata_t> securityMetadata(nw_connection_group_t group)
{
    RetainPtr sessionMetadata = nw_connection_group_copy_protocol_metadata(group, adoptNS(nw_protocol_copy_webtransport_definition()).get());
    if (!sessionMetadata)
        return nullptr;
    switch (nw_webtransport_metadata_get_transport_mode(sessionMetadata.get())) {
    case nw_webtransport_transport_mode_unknown:
        return nullptr;
    case nw_webtransport_transport_mode_http2: {
        RetainPtr tlsMetadata = nw_connection_group_copy_protocol_metadata(group, adoptNS(nw_protocol_copy_tls_definition()).get());
        return adoptNS(nw_tls_copy_sec_protocol_metadata(tlsMetadata.get()));
    }
    case nw_webtransport_transport_mode_http3: {
        RetainPtr quicMetadata = nw_connection_group_copy_protocol_metadata(group, adoptNS(nw_protocol_copy_quic_connection_definition()).get());
        return adoptNS(nw_quic_connection_copy_sec_protocol_metadata(quicMetadata.get()));
    }
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

Vector<uint8_t> ConnectionGroup::exportKeyingMaterial(std::span<const uint8_t> label, std::span<const uint8_t> context, uint32_t outputLength) const
{
    RetainPtr metadata = securityMetadata(m_data->group.get());
    if (!metadata)
        return { };
    RetainPtr secret = adoptNS(sec_protocol_metadata_create_secret_with_context(metadata.get(), label.size(), reinterpret_cast<const char*>(label.data()), context.size(), context.data(), outputLength));
    if (!secret)
        return { };
    return vectorFromData(secret.get());
}

#endif // HAVE(WEBTRANSPORT)

} // namespace TestWebKitAPI
