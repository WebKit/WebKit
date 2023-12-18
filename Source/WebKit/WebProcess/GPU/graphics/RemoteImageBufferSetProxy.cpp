/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "RemoteImageBufferSetProxy.h"

#include "RemoteImageBufferSetMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include <wtf/SystemTracing.h>

#if ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

class RemoteImageBufferSetProxyFlushFence : public ThreadSafeRefCounted<RemoteImageBufferSetProxyFlushFence> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferSetProxyFlushFence);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteImageBufferSetProxyFlushFence> create(IPC::Event event)
    {
        return adoptRef(*new RemoteImageBufferSetProxyFlushFence { WTFMove(event) });
    }

    ~RemoteImageBufferSetProxyFlushFence()
    {
        if (!m_signaled)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 1u);
    }

    bool waitFor(Seconds timeout)
    {
        Locker locker { m_lock };
        if (m_signaled)
            return true;
        m_signaled = m_event.waitFor(timeout);
        if (m_signaled)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 0u);
        return m_signaled;
    }

    std::optional<IPC::Event> tryTakeEvent()
    {
        if (!m_signaled)
            return std::nullopt;
        Locker locker { m_lock };
        return WTFMove(m_event);
    }

private:
    RemoteImageBufferSetProxyFlushFence(IPC::Event event)
        : m_event(WTFMove(event))
    {
        tracePoint(FlushRemoteImageBufferStart, reinterpret_cast<uintptr_t>(this));
    }
    Lock m_lock;
    std::atomic<bool> m_signaled { false };
    IPC::Event WTF_GUARDED_BY_LOCK(m_lock) m_event;
};

namespace {

class RemoteImageBufferSetProxyFlusher final : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteImageBufferSetProxyFlusher(Ref<RemoteImageBufferSetProxyFlushFence> flushState)
        : m_flushState(WTFMove(flushState))
    { }

    void flush() final
    {
        m_flushState->waitFor(RemoteRenderingBackendProxy::defaultTimeout);
    }

private:
    Ref<RemoteImageBufferSetProxyFlushFence> m_flushState;
};

}

template<typename T>
ALWAYS_INLINE void RemoteImageBufferSetProxy::send(T&& message)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;

    auto result = m_remoteRenderingBackendProxy->streamConnection().send(std::forward<T>(message), m_identifier, RemoteRenderingBackendProxy::defaultTimeout);
#if !RELEASE_LOG_DISABLED
    if (UNLIKELY(result != IPC::Error::NoError)) {
        auto& parameters = m_remoteRenderingBackendProxy->parameters();
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] Proxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result));
    }
#else
    UNUSED_VARIABLE(result);
#endif
}

template<typename T>
ALWAYS_INLINE auto RemoteImageBufferSetProxy::sendSync(T&& message)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return IPC::StreamClientConnection::SendSyncResult<T> { IPC::Error::InvalidConnection };

    auto result = m_remoteRenderingBackendProxy->streamConnection().sendSync(std::forward<T>(message), m_identifier, RemoteRenderingBackendProxy::defaultTimeout);
#if !RELEASE_LOG_DISABLED
    if (UNLIKELY(!result.succeeded())) {
        auto& parameters = m_remoteRenderingBackendProxy->parameters();
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] Proxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result.error));
    }
#endif
    return result;
}

RemoteImageBufferSetProxy::RemoteImageBufferSetProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
    , m_identifier(RemoteImageBufferSetIdentifier::generate())
    , m_displayListIdentifier(RenderingResourceIdentifier::generate())
{
}

RemoteImageBufferSetProxy::~RemoteImageBufferSetProxy()
{
    if (m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->releaseRemoteImageBufferSet(*this);
}

void RemoteImageBufferSetProxy::addRequestedVolatility(OptionSet<BufferInSetType> request)
{
    m_requestedVolatility.add(request);
}

void RemoteImageBufferSetProxy::setConfirmedVolatility(MarkSurfacesAsVolatileRequestIdentifier identifier, OptionSet<BufferInSetType> types)
{
    if (identifier > m_minimumVolatilityRequest)
        m_confirmedVolatility.add(types);
}

void RemoteImageBufferSetProxy::clearVolatilityUntilAfter(MarkSurfacesAsVolatileRequestIdentifier previousVolatilityRequest)
{
    m_requestedVolatility = { };
    m_confirmedVolatility = { };
    m_minimumVolatilityRequest = previousVolatilityRequest;
}


void RemoteImageBufferSetProxy::setConfiguration(WebCore::FloatSize size, float scale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, WebCore::RenderingMode renderingMode, WebCore::RenderingPurpose renderingPurpose)
{
    m_size = size;
    m_scale = scale;
    m_colorSpace = colorSpace;
    m_pixelFormat = pixelFormat;
    m_renderingMode = renderingMode;
    m_renderingPurpose = renderingPurpose;
    m_remoteNeedsConfigurationUpdate = true;
}

std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> RemoteImageBufferSetProxy::flushFrontBufferAsync()
{
    if (!m_remoteRenderingBackendProxy)
        return nullptr;

    std::optional<IPC::Event> event;
    if (m_pendingFlush)
        event = m_pendingFlush->tryTakeEvent();
    if (!event) {
        auto pair = IPC::createEventSignalPair();
        if (!pair)
            return nullptr;

        event = WTFMove(pair->event);
        send(Messages::RemoteImageBufferSet::SetFlushSignal(WTFMove(pair->signal)));
    }

    send(Messages::RemoteImageBufferSet::Flush());
    m_pendingFlush = RemoteImageBufferSetProxyFlushFence::create(WTFMove(*event));

    return makeUnique<RemoteImageBufferSetProxyFlusher>(Ref { *m_pendingFlush });
}

void RemoteImageBufferSetProxy::willPrepareForDisplay()
{
    if (m_remoteNeedsConfigurationUpdate && m_remoteRenderingBackendProxy) {
        send(Messages::RemoteImageBufferSet::UpdateConfiguration(m_size, m_renderingMode, m_scale, m_colorSpace, m_pixelFormat));

        OptionSet<WebCore::ImageBufferOptions> options;
        if (m_renderingMode == RenderingMode::Accelerated)
            options.add(WebCore::ImageBufferOptions::Accelerated);

        m_displayListRecorder = m_remoteRenderingBackendProxy->createDisplayListRecorder(m_displayListIdentifier, m_size, m_renderingPurpose, m_scale, m_colorSpace, m_pixelFormat, options);
    }
    m_remoteNeedsConfigurationUpdate = false;
}

void RemoteImageBufferSetProxy::remoteBufferSetWasDestroyed()
{
    m_generation++;
    m_remoteNeedsConfigurationUpdate = true;
    m_pendingFlush = nullptr;
}

GraphicsContext& RemoteImageBufferSetProxy::context()
{
    RELEASE_ASSERT(m_displayListRecorder);
    return *m_displayListRecorder;
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<WebCore::DynamicContentScalingDisplayList> RemoteImageBufferSetProxy::dynamicContentScalingDisplayList()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return std::nullopt;
    auto sendResult = sendSync(Messages::RemoteImageBufferSet::DynamicContentScalingDisplayList());
    if (!sendResult.succeeded())
        return std::nullopt;
    auto [handle] = sendResult.takeReply();
    if (!handle)
        return std::nullopt;
    return WTFMove(handle);
}
#endif


} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
