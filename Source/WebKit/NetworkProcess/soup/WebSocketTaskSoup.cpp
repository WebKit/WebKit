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
#include "NetworkSocketChannel.h"
#include <WebCore/HTTPParsers.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

WebSocketTask::WebSocketTask(NetworkSocketChannel& channel, SoupSession* session, SoupMessage* msg, const String& protocol)
    : m_channel(channel)
    , m_cancellable(adoptGRef(g_cancellable_new()))
{
    auto protocolList = protocol.split(',');
    GUniquePtr<char*> protocols;
    if (!protocolList.isEmpty()) {
        protocols.reset(static_cast<char**>(g_new0(char*, protocolList.size() + 1)));
        unsigned i = 0;
        for (auto& subprotocol : protocolList)
            protocols.get()[i++] = g_strdup(WebCore::stripLeadingAndTrailingHTTPSpaces(subprotocol).utf8().data());
    }
    soup_session_websocket_connect_async(session, msg, nullptr, protocols.get(), m_cancellable.get(),
        [] (GObject* session, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<SoupWebsocketConnection> connection = adoptGRef(soup_session_websocket_connect_finish(SOUP_SESSION(session), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;
            auto* task = static_cast<WebSocketTask*>(userData);
            if (connection)
                task->didConnect(WTFMove(connection));
            else
                task->didFail(String::fromUTF8(error->message));
        }, this);
}

WebSocketTask::~WebSocketTask()
{
    cancel();
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

    m_channel.didConnect(soup_websocket_connection_get_protocol(m_connection.get()));
}

void WebSocketTask::didReceiveMessageCallback(WebSocketTask* task, SoupWebsocketDataType dataType, GBytes* message)
{
    if (g_cancellable_is_cancelled(task->m_cancellable.get()))
        return;

    switch (dataType) {
    case SOUP_WEBSOCKET_DATA_TEXT:
        task->m_channel.didReceiveText(String::fromUTF8(static_cast<const char*>(g_bytes_get_data(message, nullptr))));
        break;
    case SOUP_WEBSOCKET_DATA_BINARY: {
        gsize dataSize;
        auto data = g_bytes_get_data(message, &dataSize);
        task->m_channel.didReceiveBinaryData(static_cast<const uint8_t*>(data), dataSize);
        break;
    }
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
    m_channel.didReceiveMessageError(errorMessage);
    if (!m_connection) {
        didClose(SOUP_WEBSOCKET_CLOSE_ABNORMAL, { });
        return;
    }

    if (soup_websocket_connection_get_state(m_connection.get()) == SOUP_WEBSOCKET_STATE_OPEN)
        didClose(0, { });
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

void WebSocketTask::sendString(const String& text, CompletionHandler<void()>&& callback)
{
    if (m_connection && soup_websocket_connection_get_state(m_connection.get()) == SOUP_WEBSOCKET_STATE_OPEN)
        soup_websocket_connection_send_text(m_connection.get(), text.utf8().data());
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

} // namespace WebKit
