/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "WebSocketTaskSoup.h"

#include "DataReference.h"
#include "NetworkProcess.h"
#include "NetworkSocketChannel.h"
#include <WebCore/HTTPParsers.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/WebSocketChannel.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

WebSocketTask::WebSocketTask(NetworkSocketChannel& channel, SoupSession* session, SoupMessage* msg, const String& protocol)
    : m_channel(channel)
    , m_handshakeMessage(msg)
    , m_cancellable(adoptGRef(g_cancellable_new()))
    , m_delayFailTimer(RunLoop::main(), this, &WebSocketTask::delayFailTimerFired)
{
    auto protocolList = protocol.split(',');
    GUniquePtr<char*> protocols;
    if (!protocolList.isEmpty()) {
        protocols.reset(static_cast<char**>(g_new0(char*, protocolList.size() + 1)));
        unsigned i = 0;
        for (auto& subprotocol : protocolList)
            protocols.get()[i++] = g_strdup(WebCore::stripLeadingAndTrailingHTTPSpaces(subprotocol).utf8().data());
    }

    // Ensure a new connection is used for WebSockets.
    // FIXME: this is done by libsoup since 2.69.1 and 2.68.4, so it can be removed when bumping the libsoup requirement.
    // See https://bugs.webkit.org/show_bug.cgi?id=203404
    soup_message_set_flags(msg, static_cast<SoupMessageFlags>(soup_message_get_flags(msg) | SOUP_MESSAGE_NEW_CONNECTION));

    soup_session_websocket_connect_async(session, msg, nullptr, protocols.get(), m_cancellable.get(),
        [] (GObject* session, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<SoupWebsocketConnection> connection = adoptGRef(soup_session_websocket_connect_finish(SOUP_SESSION(session), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;
            auto* task = static_cast<WebSocketTask*>(userData);
            if (g_error_matches(error.get(), SOUP_WEBSOCKET_ERROR, SOUP_WEBSOCKET_ERROR_NOT_WEBSOCKET)
                && task->m_handshakeMessage
                && (task->m_handshakeMessage->status_code == SOUP_STATUS_CANT_CONNECT
                    || task->m_handshakeMessage->status_code == SOUP_STATUS_CANT_CONNECT_PROXY)) {
                task->m_delayErrorMessage = String::fromUTF8(error->message);
                task->m_delayFailTimer.startOneShot(NetworkProcess::randomClosedPortDelay());
                return;
            }
            if (connection)
                task->didConnect(WTFMove(connection));
            else
                task->didFail(String::fromUTF8(error->message));
        }, this);

    WebCore::ResourceRequest request;
    request.updateFromSoupMessage(msg);
    m_channel.didSendHandshakeRequest(WTFMove(request));
}

WebSocketTask::~WebSocketTask()
{
    cancel();
}

String WebSocketTask::acceptedExtensions() const
{
#if SOUP_CHECK_VERSION(2, 67, 90)
    StringBuilder result;
    GList* extensions = soup_websocket_connection_get_extensions(m_connection.get());
    for (auto* it = extensions; it; it = g_list_next(it)) {
        auto* extension = SOUP_WEBSOCKET_EXTENSION(it->data);

        if (!result.isEmpty())
            result.appendLiteral(", ");
        result.append(String::fromUTF8(SOUP_WEBSOCKET_EXTENSION_GET_CLASS(extension)->name));

        GUniquePtr<char> params(soup_websocket_extension_get_response_params(extension));
        if (params)
            result.append(String::fromUTF8(params.get()));
    }
    return result.toStringPreserveCapacity();
#else
    return { };
#endif
}

void WebSocketTask::didConnect(GRefPtr<SoupWebsocketConnection>&& connection)
{
    m_connection = WTFMove(connection);

#if SOUP_CHECK_VERSION(2, 56, 0)
    // Use the same maximum payload length as WebKit internal implementation for backwards compatibility.
    static const uint64_t maxPayloadLength = UINT64_C(0x7FFFFFFFFFFFFFFF);
    soup_websocket_connection_set_max_incoming_payload_size(m_connection.get(), maxPayloadLength);
#endif

    g_signal_connect_swapped(m_connection.get(), "message", reinterpret_cast<GCallback>(didReceiveMessageCallback), this);
    g_signal_connect_swapped(m_connection.get(), "error", reinterpret_cast<GCallback>(didReceiveErrorCallback), this);
    g_signal_connect_swapped(m_connection.get(), "closed", reinterpret_cast<GCallback>(didCloseCallback), this);

    m_channel.didConnect(soup_websocket_connection_get_protocol(m_connection.get()), acceptedExtensions());

    WebCore::ResourceResponse response;
    response.updateFromSoupMessage(m_handshakeMessage.get());
    m_channel.didReceiveHandshakeResponse(WTFMove(response));
    m_handshakeMessage = nullptr;
}

void WebSocketTask::didReceiveMessageCallback(WebSocketTask* task, SoupWebsocketDataType dataType, GBytes* message)
{
    if (g_cancellable_is_cancelled(task->m_cancellable.get()))
        return;

    gsize dataSize;
    const auto* data = g_bytes_get_data(message, &dataSize);

    switch (dataType) {
    case SOUP_WEBSOCKET_DATA_TEXT:
        task->m_channel.didReceiveText(String::fromUTF8(static_cast<const char*>(data), dataSize));
        break;
    case SOUP_WEBSOCKET_DATA_BINARY:
        task->m_channel.didReceiveBinaryData(static_cast<const uint8_t*>(data), dataSize);
        break;
    }
}

void WebSocketTask::didReceiveErrorCallback(WebSocketTask* task, GError* error)
{
    if (g_cancellable_is_cancelled(task->m_cancellable.get()))
        return;

    task->didFail(String::fromUTF8(error->message));
}

void WebSocketTask::didFail(const String& errorMessage)
{
    if (m_receivedDidFail)
        return;

    m_receivedDidFail = true;
    if (m_handshakeMessage) {
        WebCore::ResourceResponse response;
        response.updateFromSoupMessage(m_handshakeMessage.get());
        m_channel.didReceiveHandshakeResponse(WTFMove(response));
        m_handshakeMessage = nullptr;
    }
    m_channel.didReceiveMessageError(errorMessage);
    if (!m_connection) {
        didClose(SOUP_WEBSOCKET_CLOSE_ABNORMAL, { });
        return;
    }

    if (soup_websocket_connection_get_state(m_connection.get()) == SOUP_WEBSOCKET_STATE_OPEN)
        didClose(WebCore::WebSocketChannel::CloseEventCodeAbnormalClosure, { });
}

void WebSocketTask::didCloseCallback(WebSocketTask* task)
{
    task->didClose(soup_websocket_connection_get_close_code(task->m_connection.get()), String::fromUTF8(soup_websocket_connection_get_close_data(task->m_connection.get())));
}

void WebSocketTask::didClose(unsigned short code, const String& reason)
{
    if (m_receivedDidClose)
        return;

    m_receivedDidClose = true;
    m_channel.didClose(code, reason);
}

void WebSocketTask::sendString(const IPC::DataReference& utf8, CompletionHandler<void()>&& callback)
{
    if (m_connection && soup_websocket_connection_get_state(m_connection.get()) == SOUP_WEBSOCKET_STATE_OPEN) {
#if SOUP_CHECK_VERSION(2, 67, 3)
        // Soup is going to copy the data immediately, so we can use g_bytes_new_static() here to avoid more data copies.
        GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_static(utf8.data(), utf8.size()));
        soup_websocket_connection_send_message(m_connection.get(), SOUP_WEBSOCKET_DATA_TEXT, bytes.get());
#else
        soup_websocket_connection_send_text(m_connection.get(), reinterpret_cast<const char*>(utf8.data()));
#endif
    }
    callback();
}

void WebSocketTask::sendData(const IPC::DataReference& data, CompletionHandler<void()>&& callback)
{
    if (m_connection && soup_websocket_connection_get_state(m_connection.get()) == SOUP_WEBSOCKET_STATE_OPEN)
        soup_websocket_connection_send_binary(m_connection.get(), data.data(), data.size());
    callback();
}

void WebSocketTask::close(int32_t code, const String& reason)
{
    if (m_receivedDidClose)
        return;

    if (!m_connection) {
        g_cancellable_cancel(m_cancellable.get());
        didClose(code ? code : SOUP_WEBSOCKET_CLOSE_ABNORMAL, reason);
        return;
    }

#if SOUP_CHECK_VERSION(2, 67, 90)
    if (code == WebCore::WebSocketChannel::CloseEventCodeNotSpecified)
        code = SOUP_WEBSOCKET_CLOSE_NO_STATUS;
#endif

    if (soup_websocket_connection_get_state(m_connection.get()) == SOUP_WEBSOCKET_STATE_OPEN)
        soup_websocket_connection_close(m_connection.get(), code, reason.utf8().data());
}

void WebSocketTask::cancel()
{
    g_cancellable_cancel(m_cancellable.get());

    if (m_connection) {
        g_signal_handlers_disconnect_matched(m_connection.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        m_connection = nullptr;
    }
}

void WebSocketTask::resume()
{
}

void WebSocketTask::delayFailTimerFired()
{
    didFail(m_delayErrorMessage);
}

} // namespace WebKit
