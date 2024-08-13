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
#include "RemoteSampleBufferDisplayLayer.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "GPUConnectionToWebProcess.h"
#include "IPCUtilities.h"
#include "MessageSenderInlines.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "SampleBufferDisplayLayerMessages.h"
#include <WebCore/ImageTransferSessionVT.h>
#include <WebCore/LocalSampleBufferDisplayLayer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSampleBufferDisplayLayer);

RefPtr<RemoteSampleBufferDisplayLayer> RemoteSampleBufferDisplayLayer::create(GPUConnectionToWebProcess& gpuConnection, SampleBufferDisplayLayerIdentifier identifier, Ref<IPC::Connection>&& connection)
{
    RefPtr layer = adoptRef(*new RemoteSampleBufferDisplayLayer(gpuConnection, identifier, WTFMove(connection)));
    return layer->m_sampleBufferDisplayLayer ? WTFMove(layer) : nullptr;
}

RemoteSampleBufferDisplayLayer::RemoteSampleBufferDisplayLayer(GPUConnectionToWebProcess& gpuConnection, SampleBufferDisplayLayerIdentifier identifier, Ref<IPC::Connection>&& connection)
    : m_gpuConnection(gpuConnection)
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_sampleBufferDisplayLayer(LocalSampleBufferDisplayLayer::create(*this))
    , m_sharedVideoFrameReader(Ref { gpuConnection.videoFrameObjectHeap() }, gpuConnection.webProcessIdentity())
{
    ASSERT(m_sampleBufferDisplayLayer);
}

void RemoteSampleBufferDisplayLayer::initialize(bool hideRootLayer, IntSize size, bool shouldMaintainAspectRatio, bool canShowWhileLocked, LayerInitializationCallback&& callback)
{
    LayerHostingContextOptions contextOptions;
#if PLATFORM(IOS_FAMILY)
    contextOptions.canShowWhileLocked = canShowWhileLocked;
#else
    UNUSED_PARAM(canShowWhileLocked);
#endif
    m_sampleBufferDisplayLayer->initialize(hideRootLayer, size, shouldMaintainAspectRatio, [this, weakThis = WeakPtr { *this }, contextOptions, callback = WTFMove(callback)](bool didSucceed) mutable {
        if (!weakThis || !didSucceed)
            return callback({ });

        m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess(contextOptions);
        m_layerHostingContext->setRootLayer(m_sampleBufferDisplayLayer->rootLayer());
        callback(m_layerHostingContext->contextID());
    });
}

#if !RELEASE_LOG_DISABLED
void RemoteSampleBufferDisplayLayer::setLogIdentifier(String&& identifier)
{
    m_sampleBufferDisplayLayer->setLogIdentifier(WTFMove(identifier));
}
#endif

RemoteSampleBufferDisplayLayer::~RemoteSampleBufferDisplayLayer()
{
}

CGRect RemoteSampleBufferDisplayLayer::bounds() const
{
    return m_sampleBufferDisplayLayer->bounds();
}

void RemoteSampleBufferDisplayLayer::updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer)
{
    m_sampleBufferDisplayLayer->updateDisplayMode(hideDisplayLayer, hideRootLayer);
}

void RemoteSampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, std::optional<WTF::MachSendRight>&& fence)
{
    if (fence && fence->sendRight())
        m_layerHostingContext->setFencePort(fence->sendRight());

    m_sampleBufferDisplayLayer->updateBoundsAndPosition(bounds, { });
}

void RemoteSampleBufferDisplayLayer::flush()
{
    m_sampleBufferDisplayLayer->flush();
}

void RemoteSampleBufferDisplayLayer::flushAndRemoveImage()
{
    m_sampleBufferDisplayLayer->flushAndRemoveImage();
}

void RemoteSampleBufferDisplayLayer::play()
{
    m_sampleBufferDisplayLayer->play();
}

void RemoteSampleBufferDisplayLayer::pause()
{
    m_sampleBufferDisplayLayer->pause();
}

void RemoteSampleBufferDisplayLayer::enqueueVideoFrame(SharedVideoFrame&& frame)
{
    if (auto videoFrame = m_sharedVideoFrameReader.read(WTFMove(frame)))
        m_sampleBufferDisplayLayer->enqueueVideoFrame(*videoFrame);
}

void RemoteSampleBufferDisplayLayer::clearVideoFrames()
{
    m_sampleBufferDisplayLayer->clearVideoFrames();
}

IPC::Connection* RemoteSampleBufferDisplayLayer::messageSenderConnection() const
{
    return m_connection.ptr();
}

void RemoteSampleBufferDisplayLayer::sampleBufferDisplayLayerStatusDidFail()
{
    send(Messages::SampleBufferDisplayLayer::SetDidFail { m_sampleBufferDisplayLayer->didFail() });
}

void RemoteSampleBufferDisplayLayer::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteSampleBufferDisplayLayer::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTFMove(handle));
}

void RemoteSampleBufferDisplayLayer::setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio)
{
    m_sampleBufferDisplayLayer->setShouldMaintainAspectRatio(shouldMaintainAspectRatio);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
