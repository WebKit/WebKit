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
#include "RemoteCompositorIntegrationProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteCompositorIntegrationMessages.h"
#include "RemoteGPUProxy.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/WebGPUTextureFormat.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCompositorIntegrationProxy);

RemoteCompositorIntegrationProxy::RemoteCompositorIntegrationProxy(RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteCompositorIntegrationProxy::~RemoteCompositorIntegrationProxy()
{
    auto sendResult = send(Messages::RemoteCompositorIntegration::Destruct());
    UNUSED_VARIABLE(sendResult);
}

#if PLATFORM(COCOA)
Vector<MachSendRight> RemoteCompositorIntegrationProxy::recreateRenderBuffers(int width, int height, WebCore::DestinationColorSpace&& destinationColorSpace, WebCore::AlphaPremultiplication alphaMode, WebCore::WebGPU::TextureFormat textureFormat, WebCore::WebGPU::Device& device)
{
    RemoteDeviceProxy& proxyDevice = static_cast<RemoteDeviceProxy&>(device);
    auto sendResult = sendSync(Messages::RemoteCompositorIntegration::RecreateRenderBuffers(width, height, WTFMove(destinationColorSpace), alphaMode, textureFormat, proxyDevice.backing()));
    if (!sendResult.succeeded())
        return { };

    auto [renderBuffers] = sendResult.takeReply();
    return WTFMove(renderBuffers);
}
#endif

void RemoteCompositorIntegrationProxy::prepareForDisplay(uint32_t frameIndex, CompletionHandler<void()>&& completionHandler)
{
    auto sendResult = sendSync(Messages::RemoteCompositorIntegration::PrepareForDisplay(frameIndex));
    UNUSED_VARIABLE(sendResult);
    RefPtr { m_presentationContext }->present(frameIndex);

    completionHandler();
}

void RemoteCompositorIntegrationProxy::paintCompositedResultsToCanvas(WebCore::ImageBuffer& buffer, uint32_t bufferIndex)
{
    buffer.flushDrawingContext();
    auto sendResult = sendSync(Messages::RemoteCompositorIntegration::PaintCompositedResultsToCanvas(buffer.renderingResourceIdentifier(), bufferIndex));
    UNUSED_VARIABLE(sendResult);
}

void RemoteCompositorIntegrationProxy::withDisplayBufferAsNativeImage(uint32_t, Function<void(WebCore::NativeImage*)>)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
