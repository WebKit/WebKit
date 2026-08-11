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
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

namespace WebCore {

class PendingStreamState::CallbackHandler : public ThreadSafeRefCounted<CallbackHandler> {
public:
    static Ref<CallbackHandler> create(Function<void()>&& function) { return adoptRef(*new CallbackHandler(WTF::move(function))); }

    void invoke()
    {
        if (m_function)
            m_function();
    }

private:
    explicit CallbackHandler(Function<void()>&& function)
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

void PendingStreamState::invokeHandlerIfNeeded(NOESCAPE const std::function<RefPtr<CallbackHandler>()>& function)
{
    if (RefPtr handler = function())
        handler->invoke();
}

template<typename Result>
Result PendingStreamState::invokeDrainedHandlerIfNeeded(NOESCAPE const std::function<Result(RefPtr<CallbackHandler>&)>& function)
{
    RefPtr<CallbackHandler> handler;
    auto result = function(handler);
    if (handler) {
#if ASSERT_ENABLED
        m_invokingQueueDrainedHandlerThreadUID = Thread::currentSingleton().uid();
        auto scope = makeScopeExit([&] {
            m_invokingQueueDrainedHandlerThreadUID = 0;
        });
#endif
        handler->invoke();
    }
    return result;
}

#if ASSERT_ENABLED
bool PendingStreamState::isInvokingQueueDrainedHandlerInSameThread() const
{
    return m_invokingQueueDrainedHandlerThreadUID.load() == Thread::currentSingleton().uid();
}
#endif

void PendingStreamState::appendData(Ref<SharedBuffer>&& buffer)
{
    invokeHandlerIfNeeded([&] -> RefPtr<CallbackHandler> {
        Locker locker { m_lock };
        ASSERT(!isInvokingQueueDrainedHandlerInSameThread());
        if (m_ended || m_errorCode)
            return nullptr;
        m_chunks.append(WTF::move(buffer));
        return m_dataAvailableHandler;
    });
}

void PendingStreamState::endStream()
{
    invokeHandlerIfNeeded([&] -> RefPtr<CallbackHandler> {
        Locker locker { m_lock };
        ASSERT(!isInvokingQueueDrainedHandlerInSameThread());
        if (m_ended || m_errorCode)
            return nullptr;
        m_ended = true;
        return m_dataAvailableHandler;
    });
}

void PendingStreamState::errorStream(int errorCode)
{
    invokeHandlerIfNeeded([&] -> RefPtr<CallbackHandler> {
        Locker locker { m_lock };
        ASSERT(!isInvokingQueueDrainedHandlerInSameThread());
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

    invokeHandlerIfNeeded([&] -> RefPtr<CallbackHandler> {
        Locker locker { m_lock };
        ASSERT(!m_dataAvailableHandler);
        ASSERT(!isInvokingQueueDrainedHandlerInSameThread());
        RELEASE_LOG_ERROR_IF(m_dataAvailableHandler, Network, "Trying to get upload stream data twice");
        m_dataAvailableHandler = CallbackHandler::create(WTF::move(handler));
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

void PendingStreamState::setQueueDrainedHandler(Function<void()>&& handler)
{
    Locker locker { m_lock };
    if (!handler) {
        m_queueDrainedHandler = nullptr;
        return;
    }
    ASSERT(!m_queueDrainedHandler);
    m_queueDrainedHandler = CallbackHandler::create(WTF::move(handler));
}

PendingStreamState::HTTPVersion PendingStreamState::currentHTTPVersion()
{
    Locker locker { m_lock };
    if (!m_httpVersionProbe)
        return HTTPVersion::Unknown;
    return m_httpVersionProbe();
}

PendingStreamState::ReadResult PendingStreamState::readInto(std::span<uint8_t> outBuffer)
{
    return invokeDrainedHandlerIfNeeded<ReadResult>([&](auto& drainedHandler) -> ReadResult {
        Locker locker { m_lock };
        ASSERT(!isInvokingQueueDrainedHandlerInSameThread());

        if (m_errorCode)
            return makeUnexpected(m_errorCode);

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

        if (m_chunks.isEmpty())
            drainedHandler = m_queueDrainedHandler;
        return std::make_pair(written, m_chunks.isEmpty() && m_ended);
    });
}

PendingStreamState::TakeResult PendingStreamState::takeAvailableChunks()
{
    return invokeDrainedHandlerIfNeeded<TakeResult>([&](auto& drainedHandler) -> TakeResult {
        Locker locker { m_lock };
        ASSERT(!m_frontChunkOffset);
        ASSERT(!isInvokingQueueDrainedHandlerInSameThread());

        if (m_errorCode)
            return makeUnexpected(m_errorCode);

        drainedHandler = m_queueDrainedHandler;
        return std::make_pair(std::exchange(m_chunks, { }), m_ended);
    });
}

bool PendingStreamState::hasReadyData()
{
    Locker locker { m_lock };
    return !m_chunks.isEmpty() || m_ended || m_errorCode;
}

void PendingStreamState::setCancelCallback(Function<void()>&& handler)
{
    if (!handler) {
        Locker locker { m_lock };
        m_cancelHandler = nullptr;
        return;
    }
    {
        Locker locker { m_lock };
        if (m_ended || m_errorCode)
            return;
        m_cancelHandler = CallbackHandler::create(WTF::move(handler));
    }
}

void PendingStreamState::cancel()
{
    RefPtr<CallbackHandler> cancelHandler;
    {
        Locker locker { m_lock };
        if (m_ended || m_errorCode)
            return;

        m_errorCode = -1;
        cancelHandler = m_cancelHandler;
    }

    if (cancelHandler)
        cancelHandler->invoke();
}


} // namespace WebCore
