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
#include "WebSocketServer.h"

#if PLATFORM(COCOA) && ENABLE(WEBDRIVER_BIDI)

#include "Logging.h"
#include "LoopbackCocoa.h"
#include "WebDriverNetworkSPI.h"
#include <wtf/OSObjectPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/darwin/DispatchExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

// The BiDi WebSocket server is an nw_listener with the WebSocket protocol, which does the
// handshake and the framing, so each receive delivers a whole message. It listens on 127.0.0.1
// only, rejects handshakes from pages that aren't on this machine, and answers each handshake
// with a handler on that connection's own WebSocket options, which is the only way to know which
// resource a connection requested.
//
// The requested resource is only available through nw_ws_request_copy_path(), which is new in
// macOS 26. Before macOS 26, every handshake is rejected.

namespace WebDriver {

// Network.framework takes the response of a handshake handler at +0, as code built with ARC returns
// it autoreleased, so the handlers keep the responses they return.
static OSObjectPtr<nw_ws_response_t> handshakeResponse(nw_ws_response_status_t status)
{
    return adoptOSObject(nw_ws_response_create(status, nullptr));
}

static String originOfHandshake(nw_ws_request_t request)
{
    __block String origin;
    nw_ws_request_enumerate_additional_headers(request, ^bool(const char* name, const char* value) {
        if (!equalLettersIgnoringASCIICase(StringView::fromLatin1(name), "origin"_s))
            return true;
        origin = String::fromUTF8(value);
        return false;
    });
    return origin;
}

static nw_ws_opcode_t webSocketOpcode(nw_content_context_t context)
{
    OSObjectPtr definition = adoptOSObject(nw_protocol_copy_ws_definition());
    OSObjectPtr metadata = adoptOSObject(nw_content_context_copy_protocol_metadata(context, definition.get()));
    return metadata ? nw_ws_metadata_get_opcode(metadata.get()) : nw_ws_opcode_invalid;
}

static void sendText(nw_connection_t connection, std::span<const char8_t> text)
{
    OSObjectPtr metadata = adoptOSObject(nw_ws_create_metadata(nw_ws_opcode_text));
    OSObjectPtr context = adoptOSObject(nw_content_context_create("send"));
    nw_content_context_set_metadata_for_protocol(context.get(), metadata.get());
    OSObjectPtr data = adoptOSObject(dispatch_data_create(text.data(), text.size(), mainDispatchQueueSingleton(), DISPATCH_DATA_DESTRUCTOR_DEFAULT));
    nw_connection_send(connection, data.get(), context.get(), true, ^(nw_error_t error) {
        if (error)
            RELEASE_LOG_ERROR(WebDriverBiDi, "Sending a WebSocket message failed with error %d", nw_error_get_error_code(error));
    });
}

static void closeConnection(const WebSocketMessageHandler::Connection& connection)
{
    OSObjectPtr metadata = adoptOSObject(nw_ws_create_metadata(nw_ws_opcode_close));
    nw_ws_metadata_set_close_code(metadata.get(), nw_ws_close_code_normal_closure);
    OSObjectPtr context = adoptOSObject(nw_content_context_create("close"));
    nw_content_context_set_metadata_for_protocol(context.get(), metadata.get());
    WebSocketMessageHandler::Connection closedConnection = connection;
    nw_connection_send(connection.get(), nullptr, context.get(), true, ^(nw_error_t) {
        nw_connection_cancel(closedConnection.get());
    });
}

std::optional<String> WebSocketServer::listen(const String& host, unsigned port)
{
    if (!validateListeningHost(host))
        return std::nullopt;

    OSObjectPtr parameters = adoptOSObject(nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    nw_parameters_set_local_endpoint(parameters.get(), adoptOSObject(nw_endpoint_create_host("127.0.0.1", String::number(port).utf8().legacyCStringPointer())).get());

    OSObjectPtr webSocketOptions = adoptOSObject(nw_ws_create_options(nw_ws_version_13));
    nw_ws_options_set_auto_reply_ping(webSocketOptions.get(), true);
    // Each connection gets its own handshake handler in startConnection(), so a handshake that
    // reaches this one belongs to no known connection.
    OSObjectPtr rejection = handshakeResponse(nw_ws_response_status_reject);
    nw_ws_options_set_client_request_handler(webSocketOptions.get(), mainDispatchQueueSingleton(), ^nw_ws_response_t(nw_ws_request_t) {
        return rejection.get();
    });
    OSObjectPtr protocolStack = adoptOSObject(nw_parameters_copy_default_protocol_stack(parameters.get()));
    nw_protocol_stack_prepend_application_protocol(protocolStack.get(), webSocketOptions.get());

    OSObjectPtr listener = adoptOSObject(nw_listener_create(parameters.get()));
    if (!listener) {
        RELEASE_LOG(WebDriverBiDi, "Failed to start WebSocket server at port %u", port);
        return std::nullopt;
    }

    auto weakThis = WeakPtr { *this };
    nw_listener_set_new_connection_handler(listener.get(), ^(nw_connection_t connection) {
        WebSocketMessageHandler::Connection newConnection = connection;
        dispatch_async(mainDispatchQueueSingleton(), ^{
            if (RefPtr protectedThis = weakThis)
                protectedThis->startConnection(WebSocketMessageHandler::Connection { newConnection });
            else
                nw_connection_cancel(newConnection.get());
        });
    });

    // The listener's handlers run on a queue of their own, so that this can wait for the listener to
    // be ready before the run loop runs.
    OSObjectPtr queue = adoptOSObject(dispatch_queue_create("org.webkit.minidriver.WebSocketServer", serialQueueWithAutoreleasePoolAttrSingleton()));
    nw_listener_set_queue(listener.get(), queue.get());

    OSObjectPtr semaphore = adoptOSObject(dispatch_semaphore_create(0));
    __block bool isReady = false;
    __block bool didReport = false;
    nw_listener_set_state_changed_handler(listener.get(), ^(nw_listener_state_t state, nw_error_t error) {
        if (didReport) {
            if (state == nw_listener_state_failed)
                RELEASE_LOG_ERROR(WebDriverBiDi, "WebSocket listener failed with error %d", nw_error_get_error_code(error));
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
        RELEASE_LOG(WebDriverBiDi, "Failed to start WebSocket server at port %u", port);
        nw_listener_cancel(listener.get());
        return std::nullopt;
    }

    unsigned boundPort = nw_listener_get_port(listener.get());
    m_networkListener = WTF::move(listener);

    // "/session" is where clients start BiDi-only sessions, with session.new. startListening() adds the
    // resource of each session. https://w3c.github.io/webdriver-bidi/#establishing
    m_listener = WebSocketListener::create("127.0.0.1"_s, boundPort, false, { getResourceName(nullString()) });
    return getWebSocketURL(m_listener, nullString());
}

// The listener gives every connection a copy of its parameters, so a handshake handler set on these
// options only answers this connection's handshake.
static OSObjectPtr<nw_protocol_options_t> webSocketOptions(nw_connection_t connection)
{
    OSObjectPtr parameters = adoptOSObject(nw_connection_copy_parameters(connection));
    OSObjectPtr protocolStack = adoptOSObject(nw_parameters_copy_default_protocol_stack(parameters.get()));
    OSObjectPtr webSocketDefinition = adoptOSObject(nw_protocol_copy_ws_definition());
    __block OSObjectPtr<nw_protocol_options_t> result;
    nw_protocol_stack_iterate_application_protocols(protocolStack.get(), ^(nw_protocol_options_t options) {
        OSObjectPtr definition = adoptOSObject(nw_protocol_options_copy_definition(options));
        if (nw_protocol_definition_is_equal(definition.get(), webSocketDefinition.get()))
            result = options;
    });
    return result;
}

void WebSocketServer::startConnection(WebSocketMessageHandler::Connection&& connection)
{
    WebSocketMessageHandler::Connection protectedConnection = WTF::move(connection);
    nw_connection_t networkConnection = protectedConnection.get();
    OSObjectPtr options = webSocketOptions(networkConnection);
    if (!options) {
        nw_connection_cancel(networkConnection);
        return;
    }

    __block String resourcePath;
    auto weakThis = WeakPtr { *this };
    OSObjectPtr acceptance = handshakeResponse(nw_ws_response_status_accept);
    OSObjectPtr rejection = handshakeResponse(nw_ws_response_status_reject);
    nw_ws_options_set_client_request_handler(options.get(), mainDispatchQueueSingleton(), ^nw_ws_response_t(nw_ws_request_t request) {
        RefPtr protectedThis = weakThis;
        if (!protectedThis)
            return rejection.get();

        // Browsers send the origin of the page that opens a WebSocket. Only accept pages from this
        // machine. Clients that aren't browsers usually send no origin.
        auto origin = originOfHandshake(request);
        if (!origin.isNull() && !isLoopbackURL(origin)) {
            RELEASE_LOG(WebDriverBiDi, "Rejecting a WebSocket handshake from origin %s", origin.utf8());
            return rejection.get();
        }

        if (__builtin_available(macOS 26.0, *)) {
            String path;
            if (char* rawPath = nw_ws_request_copy_path(request)) {
                path = String::fromUTF8(rawPath);
                free(rawPath);
            }
            if (auto queryStart = path.find('?'); queryStart != notFound)
                path = path.left(queryStart);
            if (!protectedThis->m_messageHandler.acceptHandshake({ "GET"_s, path, nullptr, 0 }))
                return rejection.get();
            resourcePath = WTF::move(path);
            return acceptance.get();
        }
        return rejection.get();
    });

    nw_connection_set_state_changed_handler(networkConnection, ^(nw_connection_state_t state, nw_error_t) {
        RefPtr protectedThis = weakThis;
        switch (state) {
        case nw_connection_state_ready:
            if (protectedThis)
                protectedThis->didOpenConnection(protectedConnection, resourcePath);
            else
                nw_connection_cancel(protectedConnection.get());
            return;
        case nw_connection_state_failed:
            nw_connection_cancel(protectedConnection.get());
            return;
        case nw_connection_state_cancelled:
            // This is the last state. Forget the handler, which keeps the connection alive.
            nw_connection_set_state_changed_handler(protectedConnection.get(), nullptr);
            if (protectedThis)
                protectedThis->didCloseConnection(protectedConnection);
            return;
        default:
            return;
        }
    });

    nw_connection_set_queue(networkConnection, mainDispatchQueueSingleton());
    nw_connection_start(networkConnection);
}

void WebSocketServer::didOpenConnection(const WebSocketMessageHandler::Connection& connection, const String& resourcePath)
{
    // A connection becomes ready even when its handshake was rejected.
    if (resourcePath.isNull()) {
        nw_connection_cancel(connection.get());
        return;
    }

    // Check the resource again, in case the session ended during the handshake.
    if (!m_messageHandler.acceptHandshake({ "GET"_s, resourcePath, nullptr, 0 })) {
        closeConnection(connection);
        return;
    }

    m_openConnections.add(connection);

    // Delayed steps from the end of the handshake, as now we have a connection object.
    if (resourcePath == "/session"_s)
        addStaticConnection(WebSocketMessageHandler::Connection { connection });
    else
        addConnection(WebSocketMessageHandler::Connection { connection }, getSessionID(resourcePath));

    receiveMessage(connection);
}

void WebSocketServer::receiveMessage(const WebSocketMessageHandler::Connection& connection)
{
    auto weakThis = WeakPtr { *this };
    WebSocketMessageHandler::Connection protectedConnection = connection;
    nw_connection_receive_message(connection.get(), ^(dispatch_data_t content, nw_content_context_t context, bool, nw_error_t error) {
        RefPtr protectedThis = weakThis;
        if (!protectedThis || !protectedThis->m_openConnections.contains(protectedConnection))
            return;

        auto opcode = context ? webSocketOpcode(context) : nw_ws_opcode_invalid;
        if (content && (opcode == nw_ws_opcode_text || opcode == nw_ws_opcode_binary))
            protectedThis->didReceiveMessage(protectedConnection, content, opcode);

        // The connection ends with a close frame, which needs one in reply (RFC 6455, section 5.5.1), an
        // error, or the final context when the client goes away without a close frame.
        if (opcode == nw_ws_opcode_close) {
            closeConnection(protectedConnection);
            return;
        }
        if (error || (context && nw_content_context_get_is_final(context))) {
            nw_connection_cancel(protectedConnection.get());
            return;
        }

        if (protectedThis->m_openConnections.contains(protectedConnection))
            protectedThis->receiveMessage(protectedConnection);
    });
}

void WebSocketServer::didReceiveMessage(const WebSocketMessageHandler::Connection& connection, dispatch_data_t content, nw_ws_opcode_t opcode)
{
    // https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message
    if (opcode != nw_ws_opcode_text) {
        RELEASE_LOG(WebDriverBiDi, "websocket message handler received non-text message. error return");
        auto errorReply = WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidArgument, std::nullopt, { "Non-text message received"_s });
        sendText(connection.get(), errorReply.payload.span());
        return;
    }

    const void* bytes = nullptr;
    size_t size = 0;
    OSObjectPtr contiguousContent = adoptOSObject(dispatch_data_create_map(content, &bytes, &size));
    UTF8CString payload { unsafeMakeSpan(static_cast<const char8_t*>(bytes), size) };

    m_messageHandler.handleMessage({ connection, WTF::move(payload) }, [](WebSocketMessageHandler::Message&& message) {
        if (!message.connection) {
            RELEASE_LOG(WebDriverBiDi, "No connection found when trying to send message: %s", message.payload);
            return;
        }
        sendText(message.connection.get(), message.payload.span());
    });
}

void WebSocketServer::didCloseConnection(const WebSocketMessageHandler::Connection& connection)
{
    // Connections that this side closed aren't reported, as with soup.
    if (m_openConnections.remove(connection))
        m_messageHandler.clientDisconnected(connection);
}

void WebSocketServer::sendMessage(WebSocketMessageHandler::Connection connection, const String& message)
{
    ASSERT(connection);
    RELEASE_LOG(WebDriverBiDi, "Sending message: %s", message.utf8());
    sendText(connection.get(), message.utf8().span());
}

void WebSocketServer::disconnect()
{
    if (!m_networkListener)
        return;

    nw_listener_cancel(m_networkListener.get());
    m_networkListener = nullptr;

    for (auto& connection : std::exchange(m_openConnections, { }))
        closeConnection(connection);
}

void WebSocketServer::disconnectSession(const String& sessionId)
{
    // Close every connection of the session. As with soup, they aren't reported as closed by the
    // client.
    for (auto& [connection, connectionSessionID] : m_connectionToSession) {
        if (connectionSessionID == sessionId && m_openConnections.remove(connection))
            closeConnection(connection);
    }
}

} // namespace WebDriver

#endif // PLATFORM(COCOA) && ENABLE(WEBDRIVER_BIDI)
