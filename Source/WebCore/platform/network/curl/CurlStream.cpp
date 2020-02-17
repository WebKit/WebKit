/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "CurlStream.h"

#include "CurlStreamScheduler.h"
#include "SocketStreamError.h"

#if USE(CURL)

namespace WebCore {

CurlStream::CurlStream(CurlStreamScheduler& scheduler, CurlStreamID streamID, URL&& url)
    : m_scheduler(scheduler)
    , m_streamID(streamID)
{
    ASSERT(!isMainThread());

    m_curlHandle = WTF::makeUnique<CurlHandle>();

    // Libcurl is not responsible for the protocol handling. It just handles connection.
    // Only scheme, host and port is required.
    URL urlForConnection;
    urlForConnection.setProtocol(url.protocolIs("wss") ? "https" : "http");
    urlForConnection.setHostAndPort(url.hostAndPort());
    m_curlHandle->setUrl(urlForConnection);

    m_curlHandle->enableConnectionOnly();

    auto errorCode = m_curlHandle->perform();
    if (errorCode != CURLE_OK) {
        notifyFailure(errorCode);
        return;
    }

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID](Client& client) {
        client.didOpen(streamID);
    });
}

CurlStream::~CurlStream()
{
    ASSERT(!isMainThread());
    destroyHandle();
}

void CurlStream::destroyHandle()
{
    if (!m_curlHandle)
        return;

    m_curlHandle = nullptr;
}

void CurlStream::send(UniqueArray<uint8_t>&& buffer, size_t length)
{
    ASSERT(!isMainThread());

    if (!m_curlHandle)
        return;

    m_sendBuffers.append(std::make_pair(WTFMove(buffer), length));
}

void CurlStream::appendMonitoringFd(fd_set& readfds, fd_set& writefds, fd_set& exceptfds, int& maxfd)
{
    ASSERT(!isMainThread());

    if (!m_curlHandle)
        return;

    auto socket = m_curlHandle->getActiveSocket();
    if (!socket.has_value()) {
        notifyFailure(socket.error());
        return;
    }

    FD_SET(*socket, &readfds);
    FD_SET(*socket, &exceptfds);

    if (m_sendBuffers.size())
        FD_SET(*socket, &writefds);

    if (maxfd < *socket)
        maxfd = *socket;
}

void CurlStream::tryToTransfer(const fd_set& readfds, const fd_set& writefds, const fd_set& exceptfds)
{
    ASSERT(!isMainThread());

    if (!m_curlHandle)
        return;

    auto socket = m_curlHandle->getActiveSocket();
    if (!socket.has_value()) {
        notifyFailure(socket.error());
        return;
    }

    if (FD_ISSET(*socket, &readfds) || FD_ISSET(*socket, &exceptfds))
        tryToReceive();

    if (FD_ISSET(*socket, &writefds))
        tryToSend();
}

void CurlStream::tryToReceive()
{
    if (!m_curlHandle)
        return;

    auto receiveBuffer = makeUniqueArray<uint8_t>(kReceiveBufferSize);
    size_t bytesReceived = 0;

    auto errorCode = m_curlHandle->receive(receiveBuffer.get(), kReceiveBufferSize, bytesReceived);
    if (errorCode != CURLE_OK) {
        if (errorCode != CURLE_AGAIN)
            notifyFailure(errorCode);
        return;
    }

    // 0 bytes indicates a closed connection.
    if (!bytesReceived)
        destroyHandle();

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID, buffer = WTFMove(receiveBuffer), length = bytesReceived](Client& client) mutable {
        client.didReceiveData(streamID, reinterpret_cast<const char*>(buffer.get()), length);
    });
}

void CurlStream::tryToSend()
{
    if (!m_curlHandle || !m_sendBuffers.size())
        return;

    auto& [buffer, length] = m_sendBuffers.first();
    size_t bytesSent = 0;

    auto errorCode = m_curlHandle->send(buffer.get() + m_sendBufferOffset, length - m_sendBufferOffset, bytesSent);
    if (errorCode != CURLE_OK) {
        if (errorCode != CURLE_AGAIN)
            notifyFailure(errorCode);
        return;
    }

    m_sendBufferOffset += bytesSent;

    if (m_sendBufferOffset >= length) {
        m_sendBuffers.remove(0);
        m_sendBufferOffset = 0;
    }

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID, length = bytesSent](Client& client) {
        client.didSendData(streamID, length);
    });
}

void CurlStream::notifyFailure(CURLcode errorCode)
{
    destroyHandle();

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID, errorCode](Client& client) mutable {
        client.didFail(streamID, errorCode);
    });
}

}

#endif
