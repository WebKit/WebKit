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

#include "BufferAndBackendInfo.h"
#include "Logging.h"
#include "RemoteImageBufferSetMessages.h"
#include "RemoteImageBufferSetProxyMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include <wtf/SystemTracing.h>

#if ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

class RemoteImageBufferSetProxyFlushFence : public ThreadSafeRefCounted<RemoteImageBufferSetProxyFlushFence> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferSetProxyFlushFence);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteImageBufferSetProxyFlushFence> create(IPC::Event event, RenderingUpdateID renderingUpdateID)
    {
        return adoptRef(*new RemoteImageBufferSetProxyFlushFence { WTFMove(event), renderingUpdateID });
    }

    ~RemoteImageBufferSetProxyFlushFence()
    {
        if (m_signalIsPending)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 1u);
    }

    bool waitFor(Seconds relativeTimeout)
    {
        IPC::Timeout timeout(relativeTimeout);
        Locker locker { m_lock };
        if (!m_handles)
            m_condition.waitFor(m_lock, timeout.secondsUntilDeadline());
        ASSERT(m_event);
        if (m_signalIsPending && m_event->waitFor(timeout))
            m_signalIsPending = false;
        if (!m_signalIsPending)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 0u);
        return !m_signalIsPending && m_handles;
    }

    std::optional<IPC::Event> tryTakeEvent()
    {
        if (m_signalIsPending)
            return std::nullopt;
        Locker locker { m_lock };
        return std::exchange(m_event, std::nullopt);
    }

    void setHandles(BufferSetBackendHandle&& handles)
    {
        Locker locker { m_lock };
        m_handles = WTFMove(handles);
        m_condition.notifyOne();
    }

    std::optional<BufferSetBackendHandle> takeHandles()
    {
        Locker locker { m_lock };
        return std::exchange(m_handles, std::nullopt);
    }

    void setWaitingForSignal()
    {
        m_signalIsPending = true;
    }

    RenderingUpdateID renderingUpdateID() const { return m_renderingUpdateID; }

private:
    RemoteImageBufferSetProxyFlushFence(std::optional<IPC::Event> event, RenderingUpdateID renderingUpdateID)
        : m_event(WTFMove(event))
        , m_renderingUpdateID(renderingUpdateID)
    {
        tracePoint(FlushRemoteImageBufferStart, reinterpret_cast<uintptr_t>(this));
    }
    Lock m_lock;
    Condition m_condition;
    std::atomic<bool> m_signalIsPending { false };
    std::optional<IPC::Event> WTF_GUARDED_BY_LOCK(m_lock) m_event;
    std::optional<BufferSetBackendHandle> m_handles WTF_GUARDED_BY_LOCK(m_lock);
    RenderingUpdateID m_renderingUpdateID;
};

namespace {

class RemoteImageBufferSetProxyFlusher final : public ThreadSafeImageBufferSetFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteImageBufferSetProxyFlusher(RemoteImageBufferSetIdentifier identifier, Ref<RemoteImageBufferSetProxyFlushFence> flushState, unsigned generation)
        : m_identifier(identifier)
        , m_flushState(WTFMove(flushState))
        , m_generation(generation)
    { }

    void flushAndCollectHandles(HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>& handlesMap) final
    {
        if (m_flushState->waitFor(RemoteRenderingBackendProxy::defaultTimeout))
            handlesMap.add(m_identifier, makeUnique<BufferSetBackendHandle>(*m_flushState->takeHandles()));

    }

private:
    RemoteImageBufferSetIdentifier m_identifier;
    Ref<RemoteImageBufferSetProxyFlushFence> m_flushState;
    unsigned m_generation;
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
            parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result.error()));
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
    Locker locker { m_lock };
    ASSERT(m_closed);
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

#if PLATFORM(COCOA)
void RemoteImageBufferSetProxy::didPrepareForDisplay(ImageBufferSetPrepareBufferForDisplayOutputData outputData, RenderingUpdateID renderingUpdateID)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    if (m_pendingFlush && m_pendingFlush->renderingUpdateID() == renderingUpdateID) {
        BufferSetBackendHandle handle;

        handle.bufferHandle = WTFMove(outputData.backendHandle);

        auto createBufferAndBackendInfo = [&](const std::optional<WebCore::RenderingResourceIdentifier>& bufferIdentifier) {
            if (bufferIdentifier)
                return std::optional { BufferAndBackendInfo { *bufferIdentifier, m_generation }    };
            return std::optional<BufferAndBackendInfo>();
        };

        handle.frontBufferInfo = createBufferAndBackendInfo(outputData.bufferCacheIdentifiers.front);
        handle.backBufferInfo = createBufferAndBackendInfo(outputData.bufferCacheIdentifiers.back);
        handle.secondaryBackBufferInfo = createBufferAndBackendInfo(outputData.bufferCacheIdentifiers.secondaryBack);

        m_pendingFlush->setHandles(WTFMove(handle));

        m_prepareForDisplayIsPending = false;
        if (m_closed && m_streamConnection) {
            m_streamConnection->removeWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), m_identifier.toUInt64());
            m_streamConnection = nullptr;
        }
    }
}
#endif

void RemoteImageBufferSetProxy::close()
{
    assertIsMainRunLoop();
    Locker locker { m_lock };
    m_closed = true;
    if (!m_prepareForDisplayIsPending && m_streamConnection) {
        m_streamConnection->removeWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), m_identifier.toUInt64());
        m_streamConnection = nullptr;
    }
    if (m_remoteRenderingBackendProxy)
        m_remoteRenderingBackendProxy->releaseRemoteImageBufferSet(*this);
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

void RemoteImageBufferSetProxy::createFlushFence()
{
    ASSERT(m_remoteRenderingBackendProxy);

    std::optional<IPC::Event> event;
    if (m_pendingFlush)
        event = m_pendingFlush->tryTakeEvent();
    if (!event) {
        auto pair = IPC::createEventSignalPair();
        if (!pair)
            return;

        event = WTFMove(pair->event);
        send(Messages::RemoteImageBufferSet::SetFlushSignal(WTFMove(pair->signal)));
    }

    m_pendingFlush = RemoteImageBufferSetProxyFlushFence::create(WTFMove(*event), m_remoteRenderingBackendProxy->renderingUpdateID());
}

std::unique_ptr<ThreadSafeImageBufferSetFlusher> RemoteImageBufferSetProxy::flushFrontBufferAsync(ThreadSafeImageBufferSetFlusher::FlushType flushType)
{
    if (!m_remoteRenderingBackendProxy)
        return nullptr;

    Locker locker { m_lock };
    ASSERT(m_pendingFlush && m_pendingFlush->renderingUpdateID() == m_remoteRenderingBackendProxy->renderingUpdateID());
    if (!m_pendingFlush)
        return nullptr;

    if (flushType == ThreadSafeImageBufferSetFlusher::FlushType::BackendHandlesAndDrawing) {
        m_pendingFlush->setWaitingForSignal();
        send(Messages::RemoteImageBufferSet::Flush());
    }

    return makeUnique<RemoteImageBufferSetProxyFlusher>(m_identifier, Ref { *m_pendingFlush }, m_generation);
}

void RemoteImageBufferSetProxy::willPrepareForDisplay()
{
    if (!m_remoteRenderingBackendProxy)
        return;

    if (m_remoteNeedsConfigurationUpdate) {
        send(Messages::RemoteImageBufferSet::UpdateConfiguration(m_size, m_renderingMode, m_scale, m_colorSpace, m_pixelFormat));

        OptionSet<WebCore::ImageBufferOptions> options;
        if (m_renderingMode == RenderingMode::Accelerated)
            options.add(WebCore::ImageBufferOptions::Accelerated);

        m_displayListRecorder = m_remoteRenderingBackendProxy->createDisplayListRecorder(m_displayListIdentifier, m_size, m_renderingPurpose, m_scale, m_colorSpace, m_pixelFormat, options);
    }
    m_remoteNeedsConfigurationUpdate = false;


    Locker locker { m_lock };
    createFlushFence();

    if (!m_streamConnection) {
        m_streamConnection = &m_remoteRenderingBackendProxy->streamConnection();
        m_streamConnection->addWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), m_remoteRenderingBackendProxy->workQueue(), *this, m_identifier.toUInt64());
    }
    m_prepareForDisplayIsPending = true;
}

void RemoteImageBufferSetProxy::remoteBufferSetWasDestroyed()
{
    Locker locker { m_lock };
    if (m_pendingFlush) {
        m_pendingFlush->setHandles(BufferSetBackendHandle { });
        m_pendingFlush = nullptr;
    }
    if (m_streamConnection) {
        m_streamConnection->removeWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), m_identifier.toUInt64());
        m_streamConnection = nullptr;
    }
    m_prepareForDisplayIsPending = false;
    m_generation++;
    m_remoteNeedsConfigurationUpdate = true;
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
