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

#import "config.h"
#import "RemoteSampleBufferDisplayLayer.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#import "GPUConnectionToWebProcess.h"
#import "IPCUtilities.h"
#import "MessageSenderInlines.h"
#import "RemoteVideoFrameObjectHeap.h"
#import "SampleBufferDisplayLayerMessages.h"
#import "SharedPreferencesForWebProcess.h"
#import <QuartzCore/CALayer.h>
#import <WebCore/ImageTransferSessionVT.h>
#import <WebCore/LocalSampleBufferDisplayLayer.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSampleBufferDisplayLayer);

RefPtr<RemoteSampleBufferDisplayLayer> RemoteSampleBufferDisplayLayer::create(GPUConnectionToWebProcess& gpuConnection, SampleBufferDisplayLayerIdentifier identifier, Ref<IPC::Connection>&& connection, RemoteSampleBufferDisplayLayerManager& remoteSampleBufferDisplayLayerManager)
{
    RefPtr layer = adoptRef(*new RemoteSampleBufferDisplayLayer(gpuConnection, identifier, WTFMove(connection), remoteSampleBufferDisplayLayerManager));
    return layer->m_sampleBufferDisplayLayer ? WTFMove(layer) : nullptr;
}

RemoteSampleBufferDisplayLayer::RemoteSampleBufferDisplayLayer(GPUConnectionToWebProcess& gpuConnection, SampleBufferDisplayLayerIdentifier identifier, Ref<IPC::Connection>&& connection, RemoteSampleBufferDisplayLayerManager& remoteSampleBufferDisplayLayerManager)
    : m_gpuConnection(gpuConnection)
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_sampleBufferDisplayLayer(LocalSampleBufferDisplayLayer::create(*this))
    , m_sharedVideoFrameReader(Ref { gpuConnection.videoFrameObjectHeap() }, gpuConnection.webProcessIdentity())
    , m_remoteSampleBufferDisplayLayerManager(remoteSampleBufferDisplayLayerManager)
{
    ASSERT(m_sampleBufferDisplayLayer);
}

RefPtr<WebCore::LocalSampleBufferDisplayLayer> RemoteSampleBufferDisplayLayer::protectedSampleBufferDisplayLayer() const
{
    return m_sampleBufferDisplayLayer;
}

void RemoteSampleBufferDisplayLayer::initialize(bool hideRootLayer, IntSize size, bool shouldMaintainAspectRatio, bool canShowWhileLocked, LayerInitializationCallback&& callback)
{
    LayerHostingContextOptions contextOptions;
#if PLATFORM(IOS_FAMILY)
    contextOptions.canShowWhileLocked = canShowWhileLocked;
#if USE(EXTENSIONKIT)
    contextOptions.useHostable = true;
#endif
#else
    UNUSED_PARAM(canShowWhileLocked);
#endif
    protectedSampleBufferDisplayLayer()->initialize(hideRootLayer, size, shouldMaintainAspectRatio, [weakThis = WeakPtr { *this }, contextOptions, callback = WTFMove(callback)](bool didSucceed) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !didSucceed)
            return callback({ });

        protectedThis->m_layerHostingContext = LayerHostingContext::create(contextOptions);
        protectedThis->m_layerHostingContext->setRootLayer(protectedThis->protectedSampleBufferDisplayLayer()->protectedRootLayer().get());
        callback(protectedThis->m_layerHostingContext->hostingContext());
    });
}

#if !RELEASE_LOG_DISABLED
void RemoteSampleBufferDisplayLayer::setLogIdentifier(uint64_t identifier)
{
    m_sampleBufferDisplayLayer->setLogIdentifier(identifier);
}
#endif

RemoteSampleBufferDisplayLayer::~RemoteSampleBufferDisplayLayer()
{
}

CGRect RemoteSampleBufferDisplayLayer::bounds() const
{
    return protectedSampleBufferDisplayLayer()->bounds();
}

void RemoteSampleBufferDisplayLayer::updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer)
{
    protectedSampleBufferDisplayLayer()->updateDisplayMode(hideDisplayLayer, hideRootLayer);
}

void RemoteSampleBufferDisplayLayer::updateBoundsAndPosition(CGRect bounds, std::optional<WTF::MachSendRightAnnotated>&& fence)
{
#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHostingTransactionCoordinator> hostingUpdateCoordinator;
    if (fence && fence->sendRight.sendRight()) {
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(WTFMove(*fence));
#else
        hostingUpdateCoordinator = LayerHostingContext::createHostingUpdateCoordinator(fence->sendRight.sendRight());
#endif // ENABLE(MACH_PORT_LAYER_HOSTING)
        if (hostingUpdateCoordinator)
            [hostingUpdateCoordinator addLayerHierarchy:m_layerHostingContext->hostable().get()];
        else
            RELEASE_LOG_ERROR(Media, "Could not create hosting update coordinator");
    }
    protectedSampleBufferDisplayLayer()->updateBoundsAndPosition(bounds, { });
    if (hostingUpdateCoordinator)
        [hostingUpdateCoordinator commit];
#else
    if (fence && fence->sendRight.sendRight())
        m_layerHostingContext->setFencePort(fence->sendRight.sendRight());

    protectedSampleBufferDisplayLayer()->updateBoundsAndPosition(bounds, { });
#endif // USE(EXTENSIONKIT)
}

void RemoteSampleBufferDisplayLayer::flush()
{
    protectedSampleBufferDisplayLayer()->flush();
}

void RemoteSampleBufferDisplayLayer::flushAndRemoveImage()
{
    protectedSampleBufferDisplayLayer()->flushAndRemoveImage();
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
        protectedSampleBufferDisplayLayer()->enqueueVideoFrame(*videoFrame);
}

void RemoteSampleBufferDisplayLayer::clearVideoFrames()
{
    protectedSampleBufferDisplayLayer()->clearVideoFrames();
}

IPC::Connection* RemoteSampleBufferDisplayLayer::messageSenderConnection() const
{
    return m_connection.ptr();
}

void RemoteSampleBufferDisplayLayer::sampleBufferDisplayLayerStatusDidFail()
{
    send(Messages::SampleBufferDisplayLayer::SetDidFail { protectedSampleBufferDisplayLayer()->didFail() });
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
    protectedSampleBufferDisplayLayer()->setShouldMaintainAspectRatio(shouldMaintainAspectRatio);
}

std::optional<SharedPreferencesForWebProcess> RemoteSampleBufferDisplayLayer::sharedPreferencesForWebProcess() const
{
    if (RefPtr manager = m_remoteSampleBufferDisplayLayerManager.get())
        return manager->sharedPreferencesForWebProcess();

    return std::nullopt;
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
