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
#include "RemoteCompositorIntegrationMessages.h"
#include "RemoteGPU.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/WebGPUCompositorIntegration.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, *connection(), completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCompositorIntegration);

RemoteCompositorIntegration::RemoteCompositorIntegration(WebCore::WebGPU::CompositorIntegration& compositorIntegration, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(compositorIntegration)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteCompositorIntegration::messageReceiverName(), m_identifier.toUInt64());
}

RemoteCompositorIntegration::~RemoteCompositorIntegration() = default;

RefPtr<IPC::Connection> RemoteCompositorIntegration::connection() const
{
    RefPtr connection = protectedGPU()->gpuConnectionToWebProcess();
    if (!connection)
        return nullptr;
    return &connection->connection();
}

void RemoteCompositorIntegration::destruct()
{
    Ref { m_objectHeap.get() }->removeObject(m_identifier);
}

void RemoteCompositorIntegration::paintCompositedResultsToCanvas(WebCore::RenderingResourceIdentifier imageBufferIdentifier, uint32_t bufferIndex, CompletionHandler<void()>&& completionHandler)
{
    UNUSED_PARAM(imageBufferIdentifier);
    protectedBacking()->withDisplayBufferAsNativeImage(bufferIndex, [gpu = m_gpu, imageBufferIdentifier, completionHandler = WTFMove(completionHandler)] (WebCore::NativeImage* image) mutable {
        if (image && gpu.ptr())
            gpu->paintNativeImageToImageBuffer(*image, imageBufferIdentifier);
        completionHandler();
    });
}

void RemoteCompositorIntegration::stopListeningForIPC()
{
    protectedStreamConnection()->stopReceivingMessages(Messages::RemoteCompositorIntegration::messageReceiverName(), m_identifier.toUInt64());
}

#if PLATFORM(COCOA)
void RemoteCompositorIntegration::recreateRenderBuffers(int width, int height, WebCore::DestinationColorSpace&& destinationColorSpace, WebCore::AlphaPremultiplication alphaMode, WebCore::WebGPU::TextureFormat textureFormat, WebKit::WebGPUIdentifier deviceIdentifier, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
    auto convertedDevice = protectedObjectHeap()->convertDeviceFromBacking(deviceIdentifier);
    MESSAGE_CHECK_COMPLETION(convertedDevice, callback({ }));

    callback(protectedBacking()->recreateRenderBuffers(width, height, WTFMove(destinationColorSpace), alphaMode, textureFormat, *convertedDevice));
}
#endif

void RemoteCompositorIntegration::prepareForDisplay(CompletionHandler<void(bool)>&& completionHandler)
{
    protectedBacking()->prepareForDisplay([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(true);
    });
}

Ref<WebCore::WebGPU::CompositorIntegration> RemoteCompositorIntegration::protectedBacking()
{
    return m_backing;
}

Ref<IPC::StreamServerConnection> RemoteCompositorIntegration::protectedStreamConnection() const
{
    return m_streamConnection;
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
