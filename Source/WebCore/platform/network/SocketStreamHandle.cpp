/*
 * Copyright (C) 2009, 2011, 2012 Google Inc.  All rights reserved.
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

#include "SocketStreamHandleClient.h"

namespace WebCore {

const unsigned bufferSize = 100 * 1024 * 1024;

SocketStreamHandle::SocketStreamHandle(const URL& url, SocketStreamHandleClient& client)
    : m_url(url)
    , m_client(client)
    , m_state(Connecting)
{
}

SocketStreamHandle::SocketStreamState SocketStreamHandle::state() const
{
    return m_state;
}

bool SocketStreamHandle::send(const char* data, size_t length)
{
    if (m_state == Connecting || m_state == Closing)
        return false;
    if (!m_buffer.isEmpty()) {
        if (m_buffer.size() + length > bufferSize) {
            // FIXME: report error to indicate that buffer has no more space.
            return false;
        }
        m_buffer.append(data, length);
        m_client.didUpdateBufferedAmount(static_cast<SocketStreamHandle&>(*this), bufferedAmount());
        return true;
    }
    size_t bytesWritten = 0;
    if (m_state == Open) {
        if (auto result = platformSend(data, length))
            bytesWritten = result.value();
        else
            return false;
    }
    if (m_buffer.size() + length - bytesWritten > bufferSize) {
        // FIXME: report error to indicate that buffer has no more space.
        return false;
    }
    if (bytesWritten < length) {
        m_buffer.append(data + bytesWritten, length - bytesWritten);
        m_client.didUpdateBufferedAmount(static_cast<SocketStreamHandle&>(*this), bufferedAmount());
    }
    return true;
}

void SocketStreamHandle::close()
{
    if (m_state == Closed)
        return;
    m_state = Closing;
    if (!m_buffer.isEmpty())
        return;
    disconnect();
}

void SocketStreamHandle::disconnect()
{
    auto protect = makeRef(static_cast<SocketStreamHandle&>(*this)); // platformClose calls the client, which may make the handle get deallocated immediately.

    platformClose();
    m_state = Closed;
}

bool SocketStreamHandle::sendPendingData()
{
    if (m_state != Open && m_state != Closing)
        return false;
    if (m_buffer.isEmpty()) {
        if (m_state == Open)
            return false;
        if (m_state == Closing) {
            disconnect();
            return false;
        }
    }
    bool pending;
    do {
        auto result = platformSend(m_buffer.firstBlockData(), m_buffer.firstBlockSize());
        if (!result)
            return false;
        size_t bytesWritten = result.value();
        if (!bytesWritten)
            return false;
        pending = bytesWritten != m_buffer.firstBlockSize();
        ASSERT(m_buffer.size() - bytesWritten <= bufferSize);
        m_buffer.consume(bytesWritten);
    } while (!pending && !m_buffer.isEmpty());
    m_client.didUpdateBufferedAmount(static_cast<SocketStreamHandle&>(*this), bufferedAmount());
    return true;
}

} // namespace WebCore
