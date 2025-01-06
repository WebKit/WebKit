/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SampleBufferDisplayLayer.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnection.h"
#include "LayerHostingContext.h"
#include "Logging.h"
#include "RemoteSampleBufferDisplayLayerManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerMessages.h"
#include "RemoteVideoFrameProxy.h"
#include "SampleBufferDisplayLayerManager.h"
#include "WebProcess.h"

namespace WebKit {
using namespace WebCore;

Ref<SampleBufferDisplayLayer> SampleBufferDisplayLayer::create(SampleBufferDisplayLayerManager& manager, WebCore::SampleBufferDisplayLayerClient& client)
{
    return adoptRef(*new SampleBufferDisplayLayer(manager, client));
}

SampleBufferDisplayLayer::SampleBufferDisplayLayer(SampleBufferDisplayLayerManager& manager, WebCore::SampleBufferDisplayLayerClient& client)
    : WebCore::SampleBufferDisplayLayer(client)
    , m_gpuProcessConnection(&WebProcess::singleton().ensureGPUProcessConnection())
    , m_manager(manager)
    , m_connection(m_gpuProcessConnection.get()->connection())
{
    manager.addLayer(*this);
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    gpuProcessConnection->addClient(*this);
}

void SampleBufferDisplayLayer::initialize(bool hideRootLayer, IntSize size, bool shouldMaintainAspectRatio, CompletionHandler<void(bool)>&& callback)
{
    m_connection->sendWithAsyncReply(Messages::RemoteSampleBufferDisplayLayerManager::CreateLayer { identifier(), hideRootLayer, size, shouldMaintainAspectRatio, canShowWhileLocked() }, [this, weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto contextId) mutable {
        if (!weakThis)
            return callback(false);
        m_hostingContextID = contextId;
        callback(!!contextId);
    });
}

#if !RELEASE_LOG_DISABLED
void SampleBufferDisplayLayer::setLogIdentifier(String&& logIdentifier)
{
    ASSERT(m_hostingContextID);
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::SetLogIdentifier { logIdentifier }, identifier());
}
#endif

SampleBufferDisplayLayer::~SampleBufferDisplayLayer()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayerManager::ReleaseLayer { identifier() }, 0);
    if (RefPtr manager = m_manager.get())
        manager->removeLayer(*this);
}

bool SampleBufferDisplayLayer::didFail() const
{
    return m_didFail;
}

void SampleBufferDisplayLayer::updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer)
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::UpdateDisplayMode { hideDisplayLayer, hideRootLayer }, identifier());
}

void SampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, std::optional<WTF::MachSendRight>&& fence)
{
    m_connection->send(Messages::GPUConnectionToWebProcess::UpdateSampleBufferDisplayLayerBoundsAndPosition { identifier(), bounds, WTFMove(fence) }, identifier());
}

void SampleBufferDisplayLayer::flush()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::Flush { }, identifier());
}

void SampleBufferDisplayLayer::flushAndRemoveImage()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::FlushAndRemoveImage { }, identifier());
}

void SampleBufferDisplayLayer::play()
{
    m_paused = false;
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::Play { }, identifier());
}

void SampleBufferDisplayLayer::pause()
{
    m_paused = true;
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::Pause { }, identifier());
}

void SampleBufferDisplayLayer::enqueueBlackFrameFrom(const VideoFrame& videoFrame)
{
    auto size = videoFrame.presentationSize();
    WebCore::IntSize blackFrameSize { static_cast<int>(size.width()), static_cast<int>(size.height()) };
    SharedVideoFrame sharedVideoFrame { videoFrame.presentationTime(), false, videoFrame.rotation(), blackFrameSize };
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::EnqueueVideoFrame { WTFMove(sharedVideoFrame) }, identifier());
}

void SampleBufferDisplayLayer::enqueueVideoFrame(VideoFrame& videoFrame)
{
    if (m_paused)
        return;

    auto sharedVideoFrame = m_sharedVideoFrameWriter.write(videoFrame,
        [this](auto& semaphore) { m_connection->send(Messages::RemoteSampleBufferDisplayLayer::SetSharedVideoFrameSemaphore { semaphore }, identifier()); },
        [this](SharedMemory::Handle&& handle) { m_connection->send(Messages::RemoteSampleBufferDisplayLayer::SetSharedVideoFrameMemory { WTFMove(handle) }, identifier()); }
    );
    if (!sharedVideoFrame)
        return;

    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::EnqueueVideoFrame { WTFMove(*sharedVideoFrame) }, identifier());
}

void SampleBufferDisplayLayer::clearVideoFrames()
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::ClearVideoFrames { }, identifier());
}

PlatformLayer* SampleBufferDisplayLayer::rootLayer()
{
    if (!m_videoLayer && m_hostingContextID)
        m_videoLayer = LayerHostingContext::createPlatformLayerForHostingContext(*m_hostingContextID);
    return m_videoLayer.get();
}

void SampleBufferDisplayLayer::setDidFail(bool value)
{
    m_didFail = value;
    RefPtr client = m_client.get();
    if (client && m_didFail)
        client->sampleBufferDisplayLayerStatusDidFail();
}

void SampleBufferDisplayLayer::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    m_sharedVideoFrameWriter.disable();
    setDidFail(true);
}

void SampleBufferDisplayLayer::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    m_connection->send(Messages::RemoteSampleBufferDisplayLayer::SetShouldMaintainAspectRatio { shouldMaintainAspectRatio }, identifier());
}

}

#endif
