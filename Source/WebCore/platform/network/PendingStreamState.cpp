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
#include "PendingStreamState.h"

#include "Logging.h"
#include "SharedBuffer.h"
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class PendingStreamState::DataAvailableHandler : public ThreadSafeRefCounted<DataAvailableHandler> {
public:
    static Ref<DataAvailableHandler> create(Function<void()>&& function) { return adoptRef(*new DataAvailableHandler(WTF::move(function))); }

    void invoke()
    {
        if (m_function)
            m_function();
    }

private:
    explicit DataAvailableHandler(Function<void()>&& function)
        : m_function(WTF::move(function))
    {
    }

    Function<void()> m_function;
};

Ref<PendingStreamState> PendingStreamState::create()
{
    return adoptRef(*new PendingStreamState());
}

PendingStreamState::PendingStreamState() = default;

PendingStreamState::~PendingStreamState() = default;

void PendingStreamState::setHTTPVersionProbe(HTTPVersionProbe&& probe)
{
    Locker locker { m_lock };
    ASSERT(!m_httpVersionProbe);
    m_httpVersionProbe = WTF::move(probe);
}

void PendingStreamState::invokeHandlerIfNeeded(NOESCAPE const std::function<RefPtr<DataAvailableHandler>()>& function)
{
    if (RefPtr handler = function())
        handler->invoke();
}

void PendingStreamState::appendData(Ref<SharedBuffer>&& buffer)
{
    invokeHandlerIfNeeded([&] -> RefPtr<DataAvailableHandler> {
        Locker locker { m_lock };
        if (m_ended || m_errorCode)
            return nullptr;
        m_chunks.append(WTF::move(buffer));
        return m_dataAvailableHandler;
    });
}

void PendingStreamState::endStream()
{
    invokeHandlerIfNeeded([&] -> RefPtr<DataAvailableHandler> {
        Locker locker { m_lock };
        if (m_ended || m_errorCode)
            return nullptr;
        m_ended = true;
        return m_dataAvailableHandler;
    });
}

void PendingStreamState::errorStream(int errorCode)
{
    invokeHandlerIfNeeded([&] -> RefPtr<DataAvailableHandler> {
        Locker locker { m_lock };
        if (m_ended || m_errorCode)
            return nullptr;
        m_errorCode = errorCode ? errorCode : -1;
        return m_dataAvailableHandler;
    });
}

void PendingStreamState::setDataAvailableHandler(Function<void()>&& handler)
{
    if (!handler) {
        Locker locker { m_lock };
        m_dataAvailableHandler = nullptr;
        return;
    }

    invokeHandlerIfNeeded([&] -> RefPtr<DataAvailableHandler> {
        Locker locker { m_lock };
        ASSERT(!m_dataAvailableHandler);
        RELEASE_LOG_ERROR_IF(m_dataAvailableHandler, Network, "Trying to get upload stream data twice");
        m_dataAvailableHandler = DataAvailableHandler::create(WTF::move(handler));
        if (!m_chunks.isEmpty() || m_ended || m_errorCode)
            return m_dataAvailableHandler;
        return nullptr;
    });
}

void PendingStreamState::clearDataAvailableHandler()
{
    Locker locker { m_lock };
    m_dataAvailableHandler = nullptr;
}

PendingStreamState::HTTPVersion PendingStreamState::currentHTTPVersion()
{
    Locker locker { m_lock };
    if (!m_httpVersionProbe)
        return HTTPVersion::Unknown;
    return m_httpVersionProbe();
}

size_t PendingStreamState::read(std::span<uint8_t> outBuffer, bool& atEOF, int& errorCode)
{
    Locker locker { m_lock };
    errorCode = m_errorCode;
    atEOF = false;

    if (errorCode)
        return 0;

    size_t written = 0;
    while (written < outBuffer.size() && !m_chunks.isEmpty()) {
        auto chunkSpan = protect(m_chunks.first())->span();
        size_t available = chunkSpan.size() - m_frontChunkOffset;
        size_t remaining = outBuffer.size() - written;
        size_t toCopy = std::min(available, remaining);
        memcpySpan(outBuffer.subspan(written, toCopy), chunkSpan.subspan(m_frontChunkOffset, toCopy));
        written += toCopy;
        m_frontChunkOffset += toCopy;
        if (m_frontChunkOffset >= chunkSpan.size()) {
            m_chunks.removeFirst();
            m_frontChunkOffset = 0;
        }
    }

    if (m_chunks.isEmpty() && m_ended)
        atEOF = true;

    return written;
}

RefPtr<SharedBuffer> PendingStreamState::takeNextChunk(bool& atEOF, int& errorCode)
{
    Locker locker { m_lock };
    errorCode = m_errorCode;
    atEOF = false;

    if (errorCode)
        return nullptr;

    if (m_chunks.isEmpty()) {
        if (m_ended)
            atEOF = true;
        return nullptr;
    }

    RefPtr<SharedBuffer> chunk = m_chunks.takeFirst();
    if (m_frontChunkOffset) {
        chunk = SharedBuffer::create(chunk->span().subspan(m_frontChunkOffset));
        m_frontChunkOffset = 0;
    }

    if (m_chunks.isEmpty() && m_ended)
        atEOF = true;

    return chunk;
}

bool PendingStreamState::hasReadyData()
{
    Locker locker { m_lock };
    return !m_chunks.isEmpty() || m_ended || m_errorCode;
}

} // namespace WebCore
