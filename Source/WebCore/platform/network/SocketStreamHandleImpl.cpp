/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "SocketStreamHandleImpl.h"

#include "SocketStreamHandleClient.h"
#include <wtf/Function.h>

namespace WebCore {

void SocketStreamHandleImpl::platformSend(const char* data, size_t length, Function<void(bool)>&& completionHandler)
{
    if (!m_buffer.isEmpty()) {
        if (m_buffer.size() + length > maxBufferSize) {
            // FIXME: report error to indicate that buffer has no more space.
            return completionHandler(false);
        }
        m_buffer.append(data, length);
        m_client.didUpdateBufferedAmount(*this, bufferedAmount());
        return completionHandler(true);
    }
    size_t bytesWritten = 0;
    if (m_state == Open) {
        if (auto result = platformSendInternal(data, length))
            bytesWritten = result.value();
        else
            return completionHandler(false);
    }
    if (m_buffer.size() + length - bytesWritten > maxBufferSize) {
        // FIXME: report error to indicate that buffer has no more space.
        return completionHandler(false);
    }
    if (bytesWritten < length) {
        m_buffer.append(data + bytesWritten, length - bytesWritten);
        m_client.didUpdateBufferedAmount(static_cast<SocketStreamHandle&>(*this), bufferedAmount());
    }
    return completionHandler(true);
}

bool SocketStreamHandleImpl::sendPendingData()
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
        auto result = platformSendInternal(m_buffer.firstBlockData(), m_buffer.firstBlockSize());
        if (!result)
            return false;
        size_t bytesWritten = result.value();
        if (!bytesWritten)
            return false;
        pending = bytesWritten != m_buffer.firstBlockSize();
        ASSERT(m_buffer.size() - bytesWritten <= maxBufferSize);
        m_buffer.consume(bytesWritten);
    } while (!pending && !m_buffer.isEmpty());
    m_client.didUpdateBufferedAmount(static_cast<SocketStreamHandle&>(*this), bufferedAmount());
    return true;
}

size_t SocketStreamHandleImpl::bufferedAmount()
{
    return m_buffer.size();
}

} // namespace WebCore
