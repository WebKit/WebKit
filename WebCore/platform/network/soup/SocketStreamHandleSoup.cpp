/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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

#include "CString.h"
#include "GOwnPtr.h"
#include "KURL.h"
#include "Logging.h"
#include "NotFound.h"
#include "NotImplemented.h"
#include "SocketStreamError.h"
#include "SocketStreamHandleClient.h"
#include "Vector.h"
#include <gio/gio.h>
#include <glib.h>

#define READ_BUFFER_SIZE 1024

namespace WebCore {

// These functions immediately call the similarly named SocketStreamHandle methods.
static void connectedCallback(GSocketClient*, GAsyncResult*, SocketStreamHandle*);
static void readReadyCallback(GInputStream*, GAsyncResult*, SocketStreamHandle*);
static gboolean writeReadyCallback(GSocket*, GIOCondition, SocketStreamHandle*);

// Having a list of active handles means that we do not have to worry about WebCore
// reference counting in GLib callbacks. Once the handle is off the active handles list
// we just ignore it in the callback. We avoid a lot of extra checks and tricky
// situations this way.
static Vector<SocketStreamHandle*> gActiveHandles;
bool isActiveHandle(SocketStreamHandle* handle)
{
    return gActiveHandles.find(handle) != notFound;
}

void deactivateHandle(SocketStreamHandle* handle)
{
    size_t handleIndex = gActiveHandles.find(handle);
    if (handleIndex != notFound)
        gActiveHandles.remove(handleIndex);
}

SocketStreamHandle::SocketStreamHandle(const KURL& url, SocketStreamHandleClient* client)
    : SocketStreamHandleBase(url, client)
    , m_readBuffer(0)
{
    // No support for SSL sockets yet.
    if (url.protocolIs("wss"))
        return;
    unsigned int port = url.hasPort() ? url.port() : 80;

    gActiveHandles.append(this);
    PlatformRefPtr<GSocketClient> socketClient = adoptPlatformRef(g_socket_client_new());
    g_socket_client_connect_to_host_async(socketClient.get(), url.host().utf8().data(), port, 0,
        reinterpret_cast<GAsyncReadyCallback>(connectedCallback), this);
}

SocketStreamHandle::~SocketStreamHandle()
{
    // If for some reason we were destroyed without closing, ensure that we are deactivated.
    deactivateHandle(this);
    setClient(0);
}

void SocketStreamHandle::connected(GSocketConnection* socketConnection, GError* error)
{
    if (error) {
        m_client->didFail(this, SocketStreamError(error->code));
        return;
    }

    m_socketConnection = adoptPlatformRef(socketConnection);
    m_outputStream = g_io_stream_get_output_stream(G_IO_STREAM(m_socketConnection.get()));
    m_inputStream = g_io_stream_get_input_stream(G_IO_STREAM(m_socketConnection.get()));

    m_readBuffer = new char[READ_BUFFER_SIZE];
    g_input_stream_read_async(m_inputStream.get(), m_readBuffer, READ_BUFFER_SIZE, G_PRIORITY_DEFAULT, 0,
        reinterpret_cast<GAsyncReadyCallback>(readReadyCallback), this);

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this); 
    m_state = Open;
    m_client->didOpen(this);
    if (!m_socketConnection) // Client closed the connection.
        return;
}

void SocketStreamHandle::readBytes(signed long bytesRead, GError* error)
{
    if (error) {
        m_client->didFail(this, SocketStreamError(error->code));
        return;
    }

    if (!bytesRead) {
        close();
        return;
    }

    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this); 
    m_client->didReceiveData(this, m_readBuffer, bytesRead);
    if (m_inputStream) // The client may have closed the connection.
        g_input_stream_read_async(m_inputStream.get(), m_readBuffer, READ_BUFFER_SIZE, G_PRIORITY_DEFAULT, 0,
            reinterpret_cast<GAsyncReadyCallback>(readReadyCallback), this);
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
    if (!g_socket_condition_check(g_socket_connection_get_socket(m_socketConnection.get()), G_IO_OUT)) {
        beginWaitingForSocketWritability();
        return 0;
    }

    GOwnPtr<GError> error;
    gssize written = g_output_stream_write(m_outputStream.get(), data, length, 0, &error.outPtr());
    if (error) {
        m_client->didFail(this, SocketStreamError(error->code)); // FIXME: Provide a sensible error.
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
    // We remove this handle from the active handles list first, to disable all callbacks.
    deactivateHandle(this);
    stopWaitingForSocketWritability();

    if (m_socketConnection) {
        GOwnPtr<GError> error;
        g_io_stream_close(G_IO_STREAM(m_socketConnection.get()), 0, &error.outPtr());
        if (error)
            m_client->didFail(this, SocketStreamError(error->code)); // FIXME: Provide a sensible error.
        m_socketConnection = 0;
    }

    m_outputStream = 0;
    m_inputStream = 0;
    delete m_readBuffer;

    m_client->didClose(this);
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::beginWaitingForSocketWritability()
{
    if (m_writeReadySource) // Already waiting.
        return;

    m_writeReadySource = adoptPlatformRef(g_socket_create_source(
        g_socket_connection_get_socket(m_socketConnection.get()), static_cast<GIOCondition>(G_IO_OUT), 0));
    g_source_set_callback(m_writeReadySource.get(), reinterpret_cast<GSourceFunc>(writeReadyCallback), this, 0);
    g_source_attach(m_writeReadySource.get(), 0);
}

void SocketStreamHandle::stopWaitingForSocketWritability()
{
    if (!m_writeReadySource) // Not waiting.
        return;

    g_source_remove(g_source_get_id(m_writeReadySource.get()));
    m_writeReadySource = 0;
}

static void connectedCallback(GSocketClient* client, GAsyncResult* result, SocketStreamHandle* handle)
{
    // Always finish the connection, even if this SocketStreamHandle was deactivated earlier.
    GOwnPtr<GError> error;
    GSocketConnection* socketConnection = g_socket_client_connect_to_host_finish(client, result, &error.outPtr());

    // The SocketStreamHandle has been deactivated, so just close the connection, ignoring errors.
    if (!isActiveHandle(handle)) {
        g_io_stream_close(G_IO_STREAM(socketConnection), 0, &error.outPtr());
        return;
    }

    handle->connected(socketConnection, error.get());
}

static void readReadyCallback(GInputStream* stream, GAsyncResult* result, SocketStreamHandle* handle)
{
    // Always finish the read, even if this SocketStreamHandle was deactivated earlier.
    GOwnPtr<GError> error;
    gssize bytesRead = g_input_stream_read_finish(stream, result, &error.outPtr());

    if (!isActiveHandle(handle))
        return;
    handle->readBytes(bytesRead, error.get());
}

static gboolean writeReadyCallback(GSocket*, GIOCondition condition, SocketStreamHandle* handle)
{
    if (!isActiveHandle(handle))
        return FALSE;

    // G_IO_HUP and G_IO_ERR are are always active. See:
    // http://library.gnome.org/devel/gio/stable/GSocket.html#g-socket-create-source
    if (condition & G_IO_HUP) {
        handle->close();
        return FALSE;
    }
    if (condition & G_IO_ERR) {
        handle->client()->didFail(handle, SocketStreamError(0)); // FIXME: Provide a sensible error.
        return FALSE;
    }
    if (condition & G_IO_OUT)
        handle->writeReady();
    return TRUE;
}

}  // namespace WebCore
