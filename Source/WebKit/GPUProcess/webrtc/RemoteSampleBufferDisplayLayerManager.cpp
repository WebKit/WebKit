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
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSampleBufferDisplayLayerManager);

RemoteSampleBufferDisplayLayerManager::RemoteSampleBufferDisplayLayerManager(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_connectionToWebProcess(gpuConnectionToWebProcess)
    , m_connection(gpuConnectionToWebProcess.connection())
    , m_queue(gpuConnectionToWebProcess.gpuProcess().videoMediaStreamTrackRendererQueue())
{
}

void RemoteSampleBufferDisplayLayerManager::startListeningForIPC()
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    Ref ipcConnection = connection->protectedConnection();
    ipcConnection->addWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayer::messageReceiverName(), m_queue, *this);
    ipcConnection->addWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName(), m_queue, *this);
}

RemoteSampleBufferDisplayLayerManager::~RemoteSampleBufferDisplayLayerManager() = default;

void RemoteSampleBufferDisplayLayerManager::close()
{
    auto connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    Ref ipcConnection = connection->protectedConnection();
    ipcConnection->removeWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayer::messageReceiverName());
    ipcConnection->removeWorkQueueMessageReceiver(Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName());
    protectedQueue()->dispatch([this, protectedThis = Ref { *this }] {
        Locker lock(m_layersLock);
        callOnMainRunLoop([layers = WTFMove(m_layers)] { });
    });
}

bool RemoteSampleBufferDisplayLayerManager::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (!ObjectIdentifier<SampleBufferDisplayLayerIdentifierType>::isValidIdentifier(decoder.destinationID()))
        return false;

    auto identifier = ObjectIdentifier<SampleBufferDisplayLayerIdentifierType>(decoder.destinationID());
    Locker lock(m_layersLock);
    if (RefPtr layer = m_layers.get(identifier))
        layer->didReceiveMessage(connection, decoder);
    return true;
}

void RemoteSampleBufferDisplayLayerManager::createLayer(SampleBufferDisplayLayerIdentifier identifier, bool hideRootLayer, WebCore::IntSize size, bool shouldMaintainAspectRatio, bool canShowWhileLocked, LayerCreationCallback&& callback)
{
    callOnMainRunLoop([this, protectedThis = Ref { *this }, identifier, hideRootLayer, size, shouldMaintainAspectRatio, canShowWhileLocked, callback = WTFMove(callback)]() mutable {
        auto connection = m_connectionToWebProcess.get();
        if (!connection)
            return callback({ });
        auto layer = RemoteSampleBufferDisplayLayer::create(*connection, identifier, m_connection.copyRef());
        if (!layer) {
            callback({ });
            return;
        }
        layer->initialize(hideRootLayer, size, shouldMaintainAspectRatio, canShowWhileLocked, [this, protectedThis = Ref { *this }, callback = WTFMove(callback), identifier, layer = Ref { *layer }](auto layerId) mutable {
            protectedQueue()->dispatch([this, protectedThis = WTFMove(protectedThis), callback = WTFMove(callback), identifier, layer = WTFMove(layer), layerId = WTFMove(layerId)]() mutable {
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
        protectedQueue()->dispatch([this, protectedThis = WTFMove(protectedThis), identifier] {
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
    if (RefPtr layer = m_layers.get(identifier))
        layer->updateBoundsAndPosition(bounds, WTFMove(sendRight));
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
