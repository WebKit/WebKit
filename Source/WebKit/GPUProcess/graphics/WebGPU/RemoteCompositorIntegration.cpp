/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "RemoteCompositorIntegration.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteCompositorIntegrationMessages.h"
#include "RemoteGPU.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUCompositorIntegration.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_streamConnection, completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCompositorIntegration);

RemoteCompositorIntegration::RemoteCompositorIntegration(WebCore::WebGPU::CompositorIntegration& compositorIntegration, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(compositorIntegration)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteCompositorIntegration::messageReceiverName(), m_identifier.toUInt64());
}

RemoteCompositorIntegration::~RemoteCompositorIntegration() = default;

void RemoteCompositorIntegration::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteCompositorIntegration::paintCompositedResultsToCanvas(WebCore::RenderingResourceIdentifier imageBufferIdentifier, uint32_t bufferIndex, CompletionHandler<void()>&& completionHandler)
{
    UNUSED_PARAM(imageBufferIdentifier);
    protect(m_backing)->withDisplayBufferAsNativeImage(bufferIndex, [gpu = m_gpu, imageBufferIdentifier, completionHandler = WTF::move(completionHandler)] (WebCore::NativeImage* image) mutable {
        if (image && gpu.ptr())
            gpu->paintNativeImageToImageBuffer(*image, imageBufferIdentifier);
        completionHandler();
    });
}

void RemoteCompositorIntegration::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteCompositorIntegration::messageReceiverName(), m_identifier.toUInt64());
}

#if PLATFORM(COCOA)
void RemoteCompositorIntegration::recreateRenderBuffers(int width, int height, WebCore::DestinationColorSpace&& destinationColorSpace, WebCore::AlphaPremultiplication alphaMode, WebCore::WebGPU::TextureFormat textureFormat, unsigned bufferCount, WebKit::WebGPUIdentifier deviceIdentifier, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
    auto convertedDevice = protect(m_objectHeap)->convertDeviceFromBacking(deviceIdentifier);
    MESSAGE_CHECK_COMPLETION(convertedDevice, callback({ }));

    callback(protect(m_backing)->recreateRenderBuffers(width, height, WTF::move(destinationColorSpace), alphaMode, textureFormat, bufferCount, *convertedDevice));
}
#endif

void RemoteCompositorIntegration::prepareForDisplay(uint32_t frameIndex, CompletionHandler<void(bool)>&& completionHandler)
{
    protect(m_backing)->prepareForDisplay(frameIndex, [completionHandler = WTF::move(completionHandler)]() mutable {
        completionHandler(true);
    });
}

void RemoteCompositorIntegration::updateContentsHeadroom(float headroom)
{
    protect(m_backing)->updateContentsHeadroom(headroom);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
