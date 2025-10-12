/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

class RemoteImageBufferSetProxyFlushFence : public ThreadSafeRefCounted<RemoteImageBufferSetProxyFlushFence> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferSetProxyFlushFence);
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RemoteImageBufferSetProxyFlushFence);
public:
    static Ref<RemoteImageBufferSetProxyFlushFence> create(RenderingUpdateID renderingUpdateID, Seconds timeoutDuration)
    {
        return adoptRef(*new RemoteImageBufferSetProxyFlushFence { renderingUpdateID, timeoutDuration });
    }

    bool wait()
    {
        Locker locker { m_lock };
        if (!m_handles)
            m_condition.waitFor(m_lock, m_timeoutDuration);
        tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 1u);
        return !!m_handles;
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

    RenderingUpdateID renderingUpdateID() const { return m_renderingUpdateID; }

private:
    RemoteImageBufferSetProxyFlushFence(RenderingUpdateID renderingUpdateID, Seconds timeoutDuration)
        : m_renderingUpdateID(renderingUpdateID)
        , m_timeoutDuration(timeoutDuration)
    {
        tracePoint(FlushRemoteImageBufferStart, reinterpret_cast<uintptr_t>(this));
    }
    Lock m_lock;
    Condition m_condition;
    std::optional<BufferSetBackendHandle> m_handles WTF_GUARDED_BY_LOCK(m_lock);
    RenderingUpdateID m_renderingUpdateID;
    const Seconds m_timeoutDuration;
};

namespace {

class RemoteImageBufferSetProxyFlusher final : public ThreadSafeImageBufferSetFlusher {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RemoteImageBufferSetProxyFlusher);
public:
    RemoteImageBufferSetProxyFlusher(ImageBufferSetIdentifier identifier, Ref<RemoteImageBufferSetProxyFlushFence> flushState, unsigned)
        : m_identifier(identifier)
        , m_flushState(WTFMove(flushState))
    { }

    bool flushAndCollectHandles(HashMap<ImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>& handlesMap) final
    {
        if (m_flushState->wait()) {
            handlesMap.add(m_identifier, makeUnique<BufferSetBackendHandle>(*m_flushState->takeHandles()));
            return true;
        }
        RELEASE_LOG(RemoteLayerBuffers, "RemoteImageBufferSetProxyFlusher::flushAndCollectHandlers - failed");
        return false;
    }

private:
    ImageBufferSetIdentifier m_identifier;
    const Ref<RemoteImageBufferSetProxyFlushFence> m_flushState;
};

}

template<typename T>
ALWAYS_INLINE auto RemoteImageBufferSetProxy::send(T&& message)
{
    RefPtr connection = this->connection();
    if (!connection) [[unlikely]]
        return IPC::Error::InvalidConnection;

    auto result = connection->send(std::forward<T>(message), identifier());
    if (result != IPC::Error::NoError) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "RemoteImageBufferSetProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
    return result;
}

template<typename T>
ALWAYS_INLINE auto RemoteImageBufferSetProxy::sendSync(T&& message)
{
    RefPtr connection = this->connection();
    if (!connection) [[unlikely]]
        return IPC::StreamClientConnection::SendSyncResult<T> { IPC::Error::InvalidConnection };

    auto result = connection->sendSync(std::forward<T>(message), identifier());
    if (result.succeeded()) [[likely]]
        return result;

    RefPtr remoteRenderingBackendProxy = m_remoteRenderingBackendProxy.get();
    if (!remoteRenderingBackendProxy) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend was deleted] Proxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            IPC::description(T::name()).characters(), IPC::errorAsString(result.error()).characters());
        return result;
    }

    RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] Proxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
        remoteRenderingBackendProxy->renderingBackendIdentifier().toUInt64(), IPC::description(T::name()).characters(), IPC::errorAsString(result.error()).characters());
    didBecomeUnresponsive();

    return result;
}

ALWAYS_INLINE RefPtr<IPC::StreamClientConnection> RemoteImageBufferSetProxy::connection() const
{
    RefPtr backend = m_remoteRenderingBackendProxy.get();
    if (!backend) [[unlikely]]
        return nullptr;
    return backend->connection();
}

void RemoteImageBufferSetProxy::didBecomeUnresponsive() const
{
    RefPtr backend = m_remoteRenderingBackendProxy.get();
    if (!backend) [[unlikely]]
        return;
    backend->didBecomeUnresponsive();
}

Ref<RemoteImageBufferSetProxy> RemoteImageBufferSetProxy::create(RemoteRenderingBackendProxy& renderingBackend, ImageBufferSetClient& client)
{
    return adoptRef(*new RemoteImageBufferSetProxy(renderingBackend, client));
}

RemoteImageBufferSetProxy::RemoteImageBufferSetProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy, ImageBufferSetClient& client)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
    , m_client(&client)
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

void RemoteImageBufferSetProxy::setConfirmedVolatility(OptionSet<BufferInSetType> types)
{
    m_confirmedVolatility.add(types);
}

void RemoteImageBufferSetProxy::clearVolatility()
{
    m_requestedVolatility = { };
    m_confirmedVolatility = { };
}

#if PLATFORM(COCOA)
void RemoteImageBufferSetProxy::prepareToDisplay(const WebCore::Region& dirtyRegion, bool supportsPartialRepaint, bool hasEmptyDirtyRegion, bool drawingRequiresClearedPixels)
{
    if (RefPtr remoteRenderingBackendProxy = m_remoteRenderingBackendProxy.get())
        remoteRenderingBackendProxy->prepareImageBufferSetForDisplay({ *this, dirtyRegion, supportsPartialRepaint, hasEmptyDirtyRegion, drawingRequiresClearedPixels });
}

void RemoteImageBufferSetProxy::didPrepareForDisplay(ImageBufferSetPrepareBufferForDisplayOutputData outputData, RenderingUpdateID renderingUpdateID)
{
    ASSERT(!isMainRunLoop());
    Locker locker { m_lock };
    RefPtr pendingFlush = m_pendingFlush;

    if (pendingFlush && pendingFlush->renderingUpdateID() == renderingUpdateID) {
        BufferSetBackendHandle handle;

        handle.bufferHandle = WTFMove(outputData.backendHandle);

        auto createBufferAndBackendInfo = [&](const std::optional<WebCore::RenderingResourceIdentifier>& bufferIdentifier) {
            if (bufferIdentifier)
                return std::optional { BufferAndBackendInfo { *bufferIdentifier, m_generation } };
            return std::optional<BufferAndBackendInfo>();
        };

        handle.frontBufferInfo = createBufferAndBackendInfo(outputData.bufferCacheIdentifiers.front);
        handle.backBufferInfo = createBufferAndBackendInfo(outputData.bufferCacheIdentifiers.back);
        handle.secondaryBackBufferInfo = createBufferAndBackendInfo(outputData.bufferCacheIdentifiers.secondaryBack);

        pendingFlush->setHandles(WTFMove(handle));
        m_prepareForDisplayIsPending = false;

        if (RefPtr streamConnection = m_streamConnection; m_closed && streamConnection) {
            streamConnection->removeWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), identifier().toUInt64());
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
    m_client = nullptr;

    if (RefPtr streamConnection = m_streamConnection; !m_prepareForDisplayIsPending && streamConnection) {
        streamConnection->removeWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), identifier().toUInt64());
        m_streamConnection = nullptr;
    }
    if (RefPtr remoteRenderingBackendProxy = m_remoteRenderingBackendProxy.get())
        remoteRenderingBackendProxy->releaseImageBufferSet(*this);
}

void RemoteImageBufferSetProxy::setConfiguration(RemoteImageBufferSetConfiguration&& configuration)
{
    m_configuration = WTFMove(configuration);
    m_remoteNeedsConfigurationUpdate = true;
}

std::unique_ptr<ThreadSafeImageBufferSetFlusher> RemoteImageBufferSetProxy::flushFrontBufferAsync(ThreadSafeImageBufferSetFlusher::FlushType flushType)
{
    RefPtr connection = this->connection();
    if (!connection)
        return nullptr;
    Ref pendingFlush = RemoteImageBufferSetProxyFlushFence::create(m_remoteRenderingBackendProxy->renderingUpdateID(), connection->defaultTimeoutDuration());
    {
        Locker locker { m_lock };
        m_pendingFlush = pendingFlush.ptr();
    }
    auto result = send(Messages::RemoteImageBufferSet::EndPrepareForDisplay(m_remoteRenderingBackendProxy->renderingUpdateID()));
    if (result != IPC::Error::NoError)
        return nullptr;
    return makeUnique<RemoteImageBufferSetProxyFlusher>(identifier(), WTFMove(pendingFlush), m_generation);
}

void RemoteImageBufferSetProxy::willPrepareForDisplay()
{
    RefPtr connection = this->connection();
    if (!connection)
        return;

    RefPtr renderingBackend = m_remoteRenderingBackendProxy.get();
    if (!renderingBackend)
        return;

    if (m_remoteNeedsConfigurationUpdate) {
        send(Messages::RemoteImageBufferSet::UpdateConfiguration(m_configuration));
        ImageBufferParameters parameters { m_configuration.logicalSize, m_configuration.resolutionScale, m_configuration.colorSpace, m_configuration.bufferFormat, m_configuration.renderingPurpose };
        auto transform = ImageBufferBackend::calculateBaseTransform(ImageBuffer::backendParameters(parameters));
        m_context.emplace(m_configuration.colorSpace, m_configuration.contentsFormat, m_configuration.renderingMode, FloatRect { { }, m_configuration.logicalSize }, transform, m_contextIdentifier, *renderingBackend);
    }
    m_remoteNeedsConfigurationUpdate = false;

    Locker locker { m_lock };

    if (!m_streamConnection) {
        m_streamConnection = connection;
        RefPtr { m_streamConnection }->addWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), m_remoteRenderingBackendProxy->workQueue(), *this, identifier().toUInt64());
    }
    m_prepareForDisplayIsPending = true;
}

void RemoteImageBufferSetProxy::setNeedsDisplay()
{
    if (CheckedPtr client = m_client)
        client->setNeedsDisplay();
}

void RemoteImageBufferSetProxy::disconnect()
{
    Locker locker { m_lock };
    if (RefPtr pendingFlush = m_pendingFlush) {
        pendingFlush->setHandles(BufferSetBackendHandle { });
        m_pendingFlush = nullptr;
    }
    if (RefPtr streamConnection = m_streamConnection) {
        streamConnection->removeWorkQueueMessageReceiver(Messages::RemoteImageBufferSetProxy::messageReceiverName(), identifier().toUInt64());
        m_streamConnection = nullptr;
    }
    m_prepareForDisplayIsPending = false;
    m_generation++;
    m_remoteNeedsConfigurationUpdate = true;
    m_context = std::nullopt;
}

GraphicsContext& RemoteImageBufferSetProxy::context()
{
    RELEASE_ASSERT(m_context);
    return *m_context;
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<WebCore::DynamicContentScalingDisplayList> RemoteImageBufferSetProxy::dynamicContentScalingDisplayList()
{
    if (!m_remoteRenderingBackendProxy) [[unlikely]]
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
