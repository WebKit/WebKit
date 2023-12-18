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
#include "RemoteSampleBufferDisplayLayerManager.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "Decoder.h"
#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "RemoteSampleBufferDisplayLayer.h"
#include "RemoteSampleBufferDisplayLayerManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerMessages.h"
#include <WebCore/IntSize.h>

namespace WebKit {

RemoteSampleBufferDisplayLayerManager::RemoteSampleBufferDisplayLayerManager(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_connectionToWebProcess(gpuConnectionToWebProcess)
    , m_connection(gpuConnectionToWebProcess.connection())
    , m_queue(gpuConnectionToWebProcess.gpuProcess().videoMediaStreamTrackRendererQueue())
{
}

void RemoteSampleBufferDisplayLayerManager::startListeningForIPC()
{
    m_connectionToWebProcess.connection().addWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayer::messageReceiverName(), m_queue, *this);
    m_connectionToWebProcess.connection().addWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName(), m_queue, *this);
}

RemoteSampleBufferDisplayLayerManager::~RemoteSampleBufferDisplayLayerManager() = default;

void RemoteSampleBufferDisplayLayerManager::close()
{
    m_connectionToWebProcess.connection().removeWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayer::messageReceiverName());
    m_connectionToWebProcess.connection().removeWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName());
    m_queue->dispatch([this, protectedThis = Ref { *this }] {
        Locker lock(m_layersLock);
        callOnMainRunLoop([layers = WTFMove(m_layers)] { });
    });
}

bool RemoteSampleBufferDisplayLayerManager::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (!decoder.destinationID())
        return false;

    auto identifier = ObjectIdentifier<SampleBufferDisplayLayerIdentifierType>(decoder.destinationID());
    Locker lock(m_layersLock);
    if (auto* layer = m_layers.get(identifier))
        layer->didReceiveMessage(connection, decoder);
    return true;
}

void RemoteSampleBufferDisplayLayerManager::createLayer(SampleBufferDisplayLayerIdentifier identifier, bool hideRootLayer, WebCore::IntSize size, bool shouldMaintainAspectRatio, LayerCreationCallback&& callback)
{
    callOnMainRunLoop([this, protectedThis = Ref { *this }, identifier, hideRootLayer, size, shouldMaintainAspectRatio, callback = WTFMove(callback)]() mutable {
        auto layer = RemoteSampleBufferDisplayLayer::create(m_connectionToWebProcess, identifier, m_connection.copyRef());
        if (!layer) {
            callback({ });
            return;
        }
        auto& layerReference = *layer;
        layerReference.initialize(hideRootLayer, size, shouldMaintainAspectRatio, [this, protectedThis = Ref { *this }, callback = WTFMove(callback), identifier, layer = WTFMove(layer)](auto layerId) mutable {
            m_queue->dispatch([this, protectedThis = WTFMove(protectedThis), callback = WTFMove(callback), identifier, layer = WTFMove(layer), layerId = WTFMove(layerId)]() mutable {
                Locker lock(m_layersLock);
                ASSERT(!m_layers.contains(identifier));
                m_layers.add(identifier, WTFMove(layer));
                callback(WTFMove(layerId));
            });
        });
    });
}

void RemoteSampleBufferDisplayLayerManager::releaseLayer(SampleBufferDisplayLayerIdentifier identifier)
{
    callOnMainRunLoop([this, protectedThis = Ref { *this }, identifier]() mutable {
        m_queue->dispatch([this, protectedThis = WTFMove(protectedThis), identifier] {
            Locker lock(m_layersLock);
            ASSERT(m_layers.contains(identifier));
            callOnMainRunLoop([layer = m_layers.take(identifier)] { });
        });
    });
}

bool RemoteSampleBufferDisplayLayerManager::allowsExitUnderMemoryPressure() const
{
    Locker lock(m_layersLock);
    return m_layers.isEmpty();
}

void RemoteSampleBufferDisplayLayerManager::updateSampleBufferDisplayLayerBoundsAndPosition(SampleBufferDisplayLayerIdentifier identifier, WebCore::FloatRect bounds, std::optional<MachSendRight>&& sendRight)
{
    Locker lock(m_layersLock);
    if (auto* layer = m_layers.get(identifier))
        layer->updateBoundsAndPosition(bounds, WTFMove(sendRight));
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
