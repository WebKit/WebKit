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
#include "RemotePrintBackendProxy.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "Decoder.h"
#include "Logging.h"
#include "PrintConnectionToWebProcessMessages.h"
#include "PrintProcessConnection.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemotePrintBackendMessages.h"
#include "RemoteSnapshotRecorderProxy.h"
#include "StreamClientConnection.h"
#include "WebProcess.h"
#include <WebCore/DisplayList.h>
#include <WebCore/Filter.h>
#include <WebCore/FontBase.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/FontPlatformData.h>
#include <WebCore/Gradient.h>
#include <WebCore/ImageBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePrintBackendProxy);

Ref<RemotePrintBackendProxy> RemotePrintBackendProxy::create(PrintProcessConnection& printProcessConnection)
{
    return adoptRef(*new RemotePrintBackendProxy(printProcessConnection));
}

RemotePrintBackendProxy::RemotePrintBackendProxy(PrintProcessConnection& printProcessConnection)
    : m_printProcessConnection(&printProcessConnection.connection())
    , m_remoteResourceCacheProxy(RemoteResourceCacheProxy::create(*this))
{
}

RemotePrintBackendProxy::~RemotePrintBackendProxy()
{
    if (!m_connection)
        return;
    if (RefPtr printProcessConnection = m_printProcessConnection)
        printProcessConnection->send(Messages::PrintConnectionToWebProcess::ReleasePrintBackend(m_identifier), 0);
    disconnect();
}

void RemotePrintBackendProxy::ensureBackendCreated()
{
    if (m_connection || !m_printProcessConnection)
        return;
    constexpr unsigned connectionBufferSizeLog2 = 21u;
    auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2, WebProcess::singleton().gpuProcessTimeoutDuration());
    if (!connectionPair) {
        disconnect();
        return;
    }
    Ref streamConnection = WTF::move(connectionPair->streamConnection);
    m_connection = streamConnection.copyRef();
    // The print backend sends nothing back, so the connection is only watched for closing.
    streamConnection->open(*this, RunLoop::mainSingleton());
    protect(m_printProcessConnection)->send(Messages::PrintConnectionToWebProcess::CreatePrintBackend(m_identifier, WTF::move(connectionPair->connectionHandle)), 0);
}

RefPtr<IPC::StreamClientConnection> RemotePrintBackendProxy::connection()
{
    ensureBackendCreated();
    if (!m_isResponsive)
        return nullptr;
    return m_connection;
}

template<typename T>
void RemotePrintBackendProxy::send(T&& message)
{
    RefPtr connection = this->connection();
    if (!connection) [[unlikely]]
        return;
    auto result = connection->send(std::forward<T>(message), m_identifier);
    if (result != IPC::Error::NoError) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "[printBackend=%" PRIu64 "] RemotePrintBackendProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING, m_identifier.toUInt64(), IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
}

void RemotePrintBackendProxy::didBecomeUnresponsive()
{
    if (!m_isResponsive)
        return;
    RELEASE_LOG(RemoteLayerBuffers, "[printBackend=%" PRIu64 "] RemotePrintBackendProxy::didBecomeUnresponsive", m_identifier.toUInt64());
    // Nothing waits for the print process to recover: the print snapshots fail instead.
    m_isResponsive = false;
}

void RemotePrintBackendProxy::disconnect()
{
    // Invalidating the stream cancels the replies that are pending, so that the recorders waiting for them
    // fail, and the recorders that are still drawing send to nothing.
    if (RefPtr connection = std::exchange(m_connection, nullptr))
        connection->invalidate();
    m_printProcessConnection = nullptr;
    m_isResponsive = false;
    m_remoteResourceCacheProxy->disconnect();
}

void RemotePrintBackendProxy::didClose(IPC::Connection&)
{
    disconnect();
}

void RemotePrintBackendProxy::didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder)
{
    // The print backend sends nothing to the web process.
    decoder.markInvalid();
}

void RemotePrintBackendProxy::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>&)
{
    decoder.markInvalid();
}

void RemotePrintBackendProxy::createSnapshot(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier rootFrameIdentifier, const FloatSize& size)
{
    // Waited for, so that the snapshot exists before any frame hosted elsewhere is asked to record
    // into it.
    RefPtr connection = this->connection();
    if (!connection)
        return;
    auto result = connection->sendSync(Messages::RemotePrintBackend::CreateSnapshot(snapshotIdentifier, rootFrameIdentifier, size), m_identifier);
    if (!result.succeeded()) [[unlikely]]
        didBecomeUnresponsive();
}

UniqueRef<RemoteSnapshotRecorderProxy> RemotePrintBackendProxy::createSnapshotRecorder(const FloatRect& initialClip, RemoteSnapshotIdentifier snapshotIdentifier, RenderingMode renderingMode)
{
    auto recorder = makeUniqueRef<RemoteSnapshotRecorderProxy>(initialClip, renderingMode, *this);
    send(Messages::RemotePrintBackend::CreateSnapshotRecorder(recorder->identifier(), snapshotIdentifier));
    return recorder;
}

void RemotePrintBackendProxy::sinkSnapshotRecorderIntoSnapshotFrame(UniqueRef<RemoteSnapshotRecorderProxy>&& recorder, FrameIdentifier frameIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr connection = this->connection();
    if (!connection) {
        completionHandler(false);
        return;
    }
    // Without a reply, the completion handler is called with false.
    if (!connection->sendWithAsyncReply(Messages::RemotePrintBackend::SinkSnapshotRecorderIntoSnapshotFrame(recorder->identifier(), frameIdentifier), WTF::move(completionHandler), m_identifier)) [[unlikely]]
        didBecomeUnresponsive();
}

void RemotePrintBackendProxy::releaseMemory()
{
    m_remoteResourceCacheProxy->releaseMemory();
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseMemory());
}

RefPtr<ImageBuffer> RemotePrintBackendProxy::createImageBufferForContext(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const ColorSpace& colorSpace, ImageBufferFormat bufferFormat)
{
    // The print process has no image buffers, so the buffers that contexts create while recording,
    // for filters, masks and transparency layers, stay here. They are drawn into the recorder as the
    // image buffers of any other backend are. Recorders of DisplayList roots record into DisplayList
    // buffers, and the others draw without IOSurfaces.
    auto localRenderingMode = renderingMode == RenderingMode::DisplayList ? RenderingMode::DisplayList : RenderingMode::Unaccelerated;
    return ImageBuffer::create(size, localRenderingMode, purpose, resolutionScale, colorSpace, bufferFormat);
}

void RemotePrintBackendProxy::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemotePrintBackend::CacheNativeImage(WTF::move(handle), renderingResourceIdentifier));
}

void RemotePrintBackendProxy::releaseNativeImage(RenderingResourceIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseNativeImage(identifier));
}

void RemotePrintBackendProxy::cachePathImpl(Ref<PathImpl>&& path, RemotePathImplIdentifier identifier)
{
    send(Messages::RemotePrintBackend::CachePathImpl(WTF::move(path), identifier));
}

void RemotePrintBackendProxy::releasePathImpl(RemotePathImplIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleasePathImpl(identifier));
}

void RemotePrintBackendProxy::cacheFont(const FontInternalAttributes& fontAttributes, const FontPlatformDataAttributes& platformData, std::optional<RenderingResourceIdentifier> identifier)
{
    send(Messages::RemotePrintBackend::CacheFont(fontAttributes, platformData, identifier));
}

void RemotePrintBackendProxy::releaseFont(RenderingResourceIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseFont(identifier));
}

void RemotePrintBackendProxy::cacheFontCustomPlatformData(Ref<const FontCustomPlatformData>&& customPlatformData)
{
    send(Messages::RemotePrintBackend::CacheFontCustomPlatformData(customPlatformData->serializedData()));
}

void RemotePrintBackendProxy::releaseFontCustomPlatformData(RenderingResourceIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseFontCustomPlatformData(identifier));
}

void RemotePrintBackendProxy::cacheGradient(Ref<Gradient>&& gradient, RemoteGradientIdentifier identifier)
{
    send(Messages::RemotePrintBackend::CacheGradient(WTF::move(gradient), identifier));
}

void RemotePrintBackendProxy::releaseGradient(RemoteGradientIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseGradient(identifier));
}

void RemotePrintBackendProxy::cacheFilter(Ref<Filter>&& filter)
{
    send(Messages::RemotePrintBackend::CacheFilter(WTF::move(filter)));
}

void RemotePrintBackendProxy::releaseFilter(RenderingResourceIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseFilter(identifier));
}

void RemotePrintBackendProxy::cacheDisplayList(RemoteDisplayListIdentifier identifier, const DisplayList::DisplayList& displayList)
{
    RemoteDisplayListRecorderProxy recorder(*this);
    auto recorderIdentifier = recorder.identifier();
    send(Messages::RemotePrintBackend::CreateDisplayListRecorder(recorderIdentifier));
    recorder.appendDisplayList(displayList);
    send(Messages::RemotePrintBackend::SinkDisplayListRecorderIntoDisplayList(recorderIdentifier, identifier));
}

void RemotePrintBackendProxy::releaseDisplayList(RemoteDisplayListIdentifier identifier)
{
    if (!m_connection)
        return;
    send(Messages::RemotePrintBackend::ReleaseDisplayList(identifier));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
