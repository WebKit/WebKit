/*
 * Copyright (C) 2009 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "SocketStreamHandleImpl.h"

#if USE(CURL)

#include "DeprecatedGlobalSettings.h"
#include "Logging.h"
#include "SocketStreamError.h"
#include "SocketStreamHandleClient.h"
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

namespace WebCore {

SocketStreamHandleImpl::SocketStreamHandleImpl(const URL& url, SocketStreamHandleClient& client, const StorageSessionProvider* provider)
    : SocketStreamHandle(url, client)
    , m_storageSessionProvider(provider)
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, &m_client);
    ASSERT(isMainThread());

    // FIXME: Using DeprecatedGlobalSettings from here is a layering violation.
    if (m_url.protocolIs("wss") && DeprecatedGlobalSettings::allowsAnySSLCertificate())
        CurlContext::singleton().sslHandle().setIgnoreSSLErrors(true);

    m_workerThread = Thread::create("WebSocket thread", [this, protectedThis = makeRef(*this)] {
        threadEntryPoint();
    });
}

SocketStreamHandleImpl::~SocketStreamHandleImpl()
{
    LOG(Network, "SocketStreamHandle %p delete", this);
    stopThread();
}

Optional<size_t> SocketStreamHandleImpl::platformSendInternal(const uint8_t* data, size_t length)
{
    LOG(Network, "SocketStreamHandle %p platformSend", this);
    ASSERT(isMainThread());

    // If there's data waiting, return zero to indicate whole data should be put in a m_buffer.
    // This is thread-safe because state is read in atomic. Also even if the state is cleared by
    // worker thread between load() and evaluation of size, it is okay because invocation of
    // sendPendingData() is serialized in the main thread, so that another call will be happen
    // immediately.
    if (m_writeBufferSize.load())
        return 0;

    if (length > kWriteBufferSize)
        length = kWriteBufferSize;

    // We copy the buffer and then set the state atomically to say there are bytes available.
    // The worker thread will skip reading the buffer if no bytes are available, so it won't
    // access the buffer prematurely
    m_writeBuffer = makeUniqueArray<uint8_t>(length);
    memcpy(m_writeBuffer.get(), data, length);
    m_writeBufferOffset = 0;
    m_writeBufferSize.store(length);

    return length;
}

void SocketStreamHandleImpl::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);
    ASSERT(isMainThread());

    if (m_state == Closed)
        return;

    stopThread();
    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandleImpl::threadEntryPoint()
{
    ASSERT(!isMainThread());

    CurlSocketHandle socket { m_url, [this](CURLcode errorCode) {
        handleError(errorCode);
    }};

    // Connect to host
    if (!socket.connect())
        return;

    callOnMainThread([this, protectedThis = makeRef(*this)] {
        if (m_state == Connecting) {
            m_state = Open;
            m_client.didOpenSocketStream(*this);
        }
    });

    while (m_running) {
        auto writeBufferSize = m_writeBufferSize.load();
        auto result = socket.wait(20_ms, writeBufferSize > 0);
        if (!result)
            continue;

        // These logic only run when there's data waiting. In that case, m_writeBufferSize won't
        // updated by `platformSendInternal()` running in main thread.
        if (result->writable && m_running) {
            auto offset = m_writeBufferOffset;
            auto bytesSent = socket.send(m_writeBuffer.get() + offset, writeBufferSize - offset);
            offset += bytesSent;

            if (writeBufferSize > offset)
                m_writeBufferOffset = offset;
            else {
                m_writeBuffer = nullptr;
                m_writeBufferOffset = 0;
                m_writeBufferSize.store(0);
                callOnMainThread([this, protectedThis = makeRef(*this)] {
                    sendPendingData();
                });
            }
        }

        if (result->readable && m_running) {
            auto readBuffer = makeUniqueArray<uint8_t>(kReadBufferSize);
            auto bytesRead = socket.receive(readBuffer.get(), kReadBufferSize);
            // `nullopt` result means nothing to handle at this moment.
            if (!bytesRead)
                continue;

            // 0 bytes indicates a closed connection.
            if (!*bytesRead) {
                m_running = false;
                callOnMainThread([this, protectedThis = makeRef(*this)] {
                    close();
                });
                break;
            }

            callOnMainThread([this, protectedThis = makeRef(*this), buffer = WTFMove(readBuffer), size = *bytesRead ] {
                if (m_state == Open)
                    m_client.didReceiveSocketStreamData(*this, reinterpret_cast<const char*>(buffer.get()), size);
            });
        }
    }
}

void SocketStreamHandleImpl::handleError(CURLcode errorCode)
{
    m_running = false;
    callOnMainThread([this, protectedThis = makeRef(*this), errorCode, localizedDescription = CurlHandle::errorDescription(errorCode)] {
        if (errorCode == CURLE_RECV_ERROR)
            m_client.didFailToReceiveSocketStreamData(*this);
        else
            m_client.didFailSocketStream(*this, SocketStreamError(static_cast<int>(errorCode), { }, localizedDescription));
    });
}

void SocketStreamHandleImpl::stopThread()
{
    ASSERT(isMainThread());

    m_running = false;

    if (m_workerThread) {
        m_workerThread->waitForCompletion();
        m_workerThread = nullptr;
    }
}

} // namespace WebCore

#endif
