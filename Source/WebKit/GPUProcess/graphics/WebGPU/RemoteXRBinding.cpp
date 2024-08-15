/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RemoteXRBinding.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "RemoteGPU.h"
#include "RemoteXRBindingMessages.h"
#include "RemoteXRProjectionLayer.h"
#include "RemoteXRSubImage.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUXRBinding.h>
#include <WebCore/WebGPUXRProjectionLayer.h>

namespace WebKit {

RemoteXRBinding::RemoteXRBinding(WebCore::WebGPU::XRBinding& xrBinding, WebGPU::ObjectHeap& objectHeap, RemoteGPU& gpu, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(xrBinding)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
    , m_gpu(gpu)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteXRBinding::messageReceiverName(), m_identifier.toUInt64());
}

RemoteXRBinding::~RemoteXRBinding() = default;

void RemoteXRBinding::destruct()
{
    m_objectHeap->removeObject(m_identifier);
}

void RemoteXRBinding::createProjectionLayer(WebCore::WebGPU::TextureFormat colorFormat, std::optional<WebCore::WebGPU::TextureFormat> depthStencilFormat, WebCore::WebGPU::TextureUsageFlags textureUsage, double scaleFactor, WebGPUIdentifier identifier)
{
    WebCore::WebGPU::XRProjectionLayerInit init {
        .colorFormat = colorFormat,
        .depthStencilFormat = WTFMove(depthStencilFormat),
        .textureUsage = textureUsage,
        .scaleFactor = scaleFactor
    };
    auto projectionLayer = m_backing->createProjectionLayer(WTFMove(init));
    if (!projectionLayer) {
        // FIXME: Add MESSAGE_CHECK call
        return;
    }

    auto remoteProjectionLayer = RemoteXRProjectionLayer::create(*projectionLayer, m_objectHeap, m_streamConnection.copyRef(), m_gpu, identifier);
    m_objectHeap->addObject(identifier, remoteProjectionLayer);
}

void RemoteXRBinding::getViewSubImage(WebGPUIdentifier projectionLayerIdentifier, WebCore::WebGPU::XREye eye, WebGPUIdentifier identifier)
{
    auto projectionLayer = m_objectHeap->convertXRProjectionLayerFromBacking(projectionLayerIdentifier);
    if (!projectionLayer) {
        // FIXME: Add MESSAGE_CHECK call
        return;
    }

    auto subImage = m_backing->getViewSubImage(*projectionLayer, eye);
    if (!subImage) {
        // FIXME: Add MESSAGE_CHECK call
        return;
    }

    auto remoteSubImage = RemoteXRSubImage::create(*subImage, m_objectHeap, m_streamConnection.copyRef(), m_gpu, identifier);
    m_objectHeap->addObject(identifier, remoteSubImage);
}

void RemoteXRBinding::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteXRBinding::messageReceiverName(), m_identifier.toUInt64());
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
