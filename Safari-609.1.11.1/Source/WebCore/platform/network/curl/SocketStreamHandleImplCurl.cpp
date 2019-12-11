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
#include "StorageSessionProvider.h"
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

    m_workerThread = Thread::create("WebSocket thread", [this, protectedThis = makeRef(*this), url = url.isolatedCopy()] {
        threadEntryPoint(url);
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

    if (m_hasPendingWriteData)
        return 0;

    m_hasPendingWriteData = true;

    auto writeBuffer = makeUniqueArray<uint8_t>(length);
    memcpy(writeBuffer.get(), data, length);

    callOnWorkerThread([this, writeBuffer = WTFMove(writeBuffer), writeBufferSize = length]() mutable {
        ASSERT(!isMainThread());
        m_writeBuffer = WTFMove(writeBuffer);
        m_writeBufferSize = writeBufferSize;
        m_writeBufferOffset = 0;
    });

    return length;
}

void SocketStreamHandleImpl::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);
    ASSERT(isMainThread());

    if (m_state == Closed)
        return;
    m_state = Closed;

    stopThread();
    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandleImpl::threadEntryPoint(const URL& url)
{
    ASSERT(!isMainThread());

    CurlSocketHandle socket { url, [this](CURLcode errorCode) {
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
        executeTasks();

        auto result = socket.wait(20_ms, m_writeBuffer.get());
        if (!result)
            continue;

        // These logic only run when there's data waiting.
        if (result->writable && m_running) {
            auto bytesSent = socket.send(m_writeBuffer.get() + m_writeBufferOffset, m_writeBufferSize - m_writeBufferOffset);
            m_writeBufferOffset += bytesSent;

            if (m_writeBufferSize <= m_writeBufferOffset) {
                m_writeBuffer = nullptr;
                m_writeBufferSize = 0;
                m_writeBufferOffset = 0;

                callOnMainThread([this, protectedThis = makeRef(*this)] {
                    m_hasPendingWriteData = false;
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

    m_writeBuffer = nullptr;
}

void SocketStreamHandleImpl::handleError(CURLcode errorCode)
{
    m_running = false;
    callOnMainThread([this, protectedThis = makeRef(*this), errorCode, localizedDescription = CurlHandle::errorDescription(errorCode).isolatedCopy()] {
        if (m_state == Closed)
            return;

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

void SocketStreamHandleImpl::callOnWorkerThread(Function<void()>&& task)
{
    ASSERT(isMainThread());
    m_taskQueue.append(makeUnique<Function<void()>>(WTFMove(task)));
}

void SocketStreamHandleImpl::executeTasks()
{
    ASSERT(!isMainThread());

    auto tasks = m_taskQueue.takeAllMessages();
    for (auto& task : tasks)
        (*task)();
}

} // namespace WebCore

#endif
