/*
 * Copyright (C) 2009, 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SocketStreamHandle.h"

#if USE(SOUP)

#include "URL.h"
#include "Logging.h"
#include "SocketStreamError.h"
#include "SocketStreamHandleClient.h"
#include <gio/gio.h>
#include <glib.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#define READ_BUFFER_SIZE 1024

namespace WebCore {

SocketStreamHandle::SocketStreamHandle(const URL& url, SocketStreamHandleClient& client)
    : SocketStreamHandleBase(url, client)
    , m_cancellable(adoptGRef(g_cancellable_new()))
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, &m_client);
    unsigned port = url.hasPort() ? url.port() : (url.protocolIs("wss") ? 443 : 80);

    GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
    if (url.protocolIs("wss"))
        g_socket_client_set_tls(socketClient.get(), TRUE);
    relaxAdoptionRequirement();
    RefPtr<SocketStreamHandle> protectedThis(this);
    g_socket_client_connect_to_host_async(socketClient.get(), url.host().utf8().data(), port, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(connectedCallback), protectedThis.leakRef());
}

SocketStreamHandle::SocketStreamHandle(GSocketConnection* socketConnection, SocketStreamHandleClient& client)
    : SocketStreamHandleBase(URL(), client)
    , m_cancellable(adoptGRef(g_cancellable_new()))
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, &m_client);
    GRefPtr<GSocketConnection> connection = socketConnection;
    connected(WTFMove(connection));
}

SocketStreamHandle::~SocketStreamHandle()
{
    LOG(Network, "SocketStreamHandle %p delete", this);
}

void SocketStreamHandle::connected(GRefPtr<GSocketConnection>&& socketConnection)
{
    m_socketConnection = WTFMove(socketConnection);
    m_outputStream = G_POLLABLE_OUTPUT_STREAM(g_io_stream_get_output_stream(G_IO_STREAM(m_socketConnection.get())));
    m_inputStream = g_io_stream_get_input_stream(G_IO_STREAM(m_socketConnection.get()));
    m_readBuffer = std::make_unique<char[]>(READ_BUFFER_SIZE);

    RefPtr<SocketStreamHandle> protectedThis(this);
    g_input_stream_read_async(m_inputStream.get(), m_readBuffer.get(), READ_BUFFER_SIZE, G_PRIORITY_DEFAULT, m_cancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(readReadyCallback), protectedThis.leakRef());

    m_state = Open;
    m_client.didOpenSocketStream(*this);
}

void SocketStreamHandle::connectedCallback(GSocketClient* client, GAsyncResult* result, SocketStreamHandle* handle)
{
    RefPtr<SocketStreamHandle> protectedThis = adoptRef(handle);

    // Always finish the connection, even if this SocketStreamHandle was cancelled earlier.
    GUniqueOutPtr<GError> error;
    GRefPtr<GSocketConnection> socketConnection = adoptGRef(g_socket_client_connect_to_host_finish(client, result, &error.outPtr()));

    // The SocketStreamHandle has been cancelled, so just close the connection, ignoring errors.
    if (g_cancellable_is_cancelled(handle->m_cancellable.get())) {
        if (socketConnection)
            g_io_stream_close(G_IO_STREAM(socketConnection.get()), nullptr, nullptr);
        return;
    }

    if (error)
        handle->didFail(SocketStreamError(error->code, error->message));
    else
        handle->connected(WTFMove(socketConnection));
}

void SocketStreamHandle::readBytes(gssize bytesRead)
{
    if (!bytesRead) {
        close();
        return;
    }

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protectedThis(this);
    m_client.didReceiveSocketStreamData(*this, m_readBuffer.get(), bytesRead);
    if (m_inputStream) {
        g_input_stream_read_async(m_inputStream.get(), m_readBuffer.get(), READ_BUFFER_SIZE, G_PRIORITY_DEFAULT, m_cancellable.get(),
            reinterpret_cast<GAsyncReadyCallback>(readReadyCallback), protectedThis.leakRef());
    }
}

void SocketStreamHandle::readReadyCallback(GInputStream* stream, GAsyncResult* result, SocketStreamHandle* handle)
{
    RefPtr<SocketStreamHandle> protectedThis = adoptRef(handle);

    // Always finish the read, even if this SocketStreamHandle was cancelled earlier.
    GUniqueOutPtr<GError> error;
    gssize bytesRead = g_input_stream_read_finish(stream, result, &error.outPtr());

    if (g_cancellable_is_cancelled(handle->m_cancellable.get()))
        return;

    if (error)
        handle->didFail(SocketStreamError(error->code, error->message));
    else
        handle->readBytes(bytesRead);
}

void SocketStreamHandle::didFail(SocketStreamError&& error)
{
    m_client.didFailSocketStream(*this, WTFMove(error));
}

void SocketStreamHandle::writeReady()
{
    // We no longer have buffered data, so stop waiting for the socket to be writable.
    if (!bufferedAmount()) {
        stopWaitingForSocketWritability();
        return;
    }

    sendPendingData();
}

int SocketStreamHandle::platformSend(const char* data, int length)
{
    LOG(Network, "SocketStreamHandle %p platformSend", this);
    if (!m_outputStream || !data)
        return 0;

    GUniqueOutPtr<GError> error;
    gssize written = g_pollable_output_stream_write_nonblocking(m_outputStream.get(), data, length, m_cancellable.get(), &error.outPtr());
    if (error) {
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
            beginWaitingForSocketWritability();
        else
            didFail(SocketStreamError(error->code, error->message));
        return 0;
    }

    // If we did not send all the bytes we were given, we know that
    // SocketStreamHandleBase will need to send more in the future.
    if (written < length)
        beginWaitingForSocketWritability();

    return written;
}

void SocketStreamHandle::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);
    // We cancel this handle first to disable all callbacks.
    g_cancellable_cancel(m_cancellable.get());
    stopWaitingForSocketWritability();

    if (m_socketConnection) {
        GUniqueOutPtr<GError> error;
        g_io_stream_close(G_IO_STREAM(m_socketConnection.get()), nullptr, &error.outPtr());
        if (error)
            didFail(SocketStreamError(error->code, error->message));
        m_socketConnection = nullptr;
    }

    m_outputStream = nullptr;
    m_inputStream = nullptr;
    m_readBuffer = nullptr;

    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandle::beginWaitingForSocketWritability()
{
    if (m_writeReadySource) // Already waiting.
        return;

    m_writeReadySource = adoptGRef(g_pollable_output_stream_create_source(m_outputStream.get(), m_cancellable.get()));
    ref();
    g_source_set_callback(m_writeReadySource.get(), reinterpret_cast<GSourceFunc>(writeReadyCallback), this,
        [](gpointer handle) { static_cast<SocketStreamHandle*>(handle)->deref(); });
    g_source_attach(m_writeReadySource.get(), g_main_context_get_thread_default());
}

void SocketStreamHandle::stopWaitingForSocketWritability()
{
    if (!m_writeReadySource) // Not waiting.
        return;

    g_source_destroy(m_writeReadySource.get());
    m_writeReadySource = nullptr;
}

gboolean SocketStreamHandle::writeReadyCallback(GPollableOutputStream*, SocketStreamHandle* handle)
{
    if (g_cancellable_is_cancelled(handle->m_cancellable.get()))
        return G_SOURCE_REMOVE;

    handle->writeReady();
    return G_SOURCE_CONTINUE;
}

} // namespace WebCore

#endif
