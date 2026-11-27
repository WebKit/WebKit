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
#include "RemoteDeviceProxy.h"
#include "RemoteGPUProxy.h"
#include "RemoteNativeImageProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteSharedResourceCacheProxy.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteCompositorIntegrationProxy);

RemoteCompositorIntegrationProxy::RemoteCompositorIntegrationProxy(RemoteGPUProxy& parent, WebGPUCompositorIntegrationIdentifier identifier)
    : m_backing(identifier)
    , m_parent(parent)
{
}

RemoteCompositorIntegrationProxy::~RemoteCompositorIntegrationProxy()
{
    auto sendResult = send(Messages::RemoteCompositorIntegration::Destruct());
    UNUSED_VARIABLE(sendResult);
}

#if PLATFORM(COCOA)
uint32_t RemoteCompositorIntegrationProxy::recreateRenderBuffers(int width, int height, WebCore::ColorSpace&& destinationColorSpace, WebCore::AlphaPremultiplication alphaMode, ::WebGPU::TextureFormat textureFormat, ::WebGPU::Device& device)
{
    auto& proxyDevice = downcast<RemoteDeviceProxy>(device);
    m_renderBufferSize = renderBufferSize(width, height);
    m_renderBufferColorSpace = destinationColorSpace;
    m_renderBufferHasAlpha = alphaMode == WebCore::AlphaPremultiplication::Premultiplied;
    auto sendResult = sendSync(Messages::RemoteCompositorIntegration::RecreateRenderBuffers(width, height, WTF::move(destinationColorSpace), alphaMode, textureFormat, proxyDevice.backing()));
    auto [renderBufferCount] = sendResult.takeReplyOr(0u);
    m_renderBufferCount = renderBufferCount;
    return renderBufferCount;
}

void RemoteCompositorIntegrationProxy::prepareForDisplay(uint32_t frameIndex, CompletionHandler<void(MachSendRight&&, std::optional<uint32_t>)>&& completionHandler)
{
    auto sendResult = sendSync(Messages::RemoteCompositorIntegration::PrepareForDisplay(frameIndex));
    auto [gpuFrameCost, presentStall, displayBuffer, replacedRenderBufferIndex] = sendResult.takeReplyOr(Seconds { 0 }, Seconds { 0 }, MachSendRight { }, std::nullopt);
    m_lastFrameGPUCost = gpuFrameCost;
    m_lastFramePresentStall = presentStall;

    completionHandler(WTF::move(displayBuffer), replacedRenderBufferIndex);
}
#endif

RefPtr<WebCore::NativeImage> RemoteCompositorIntegrationProxy::copyNativeImage(uint32_t bufferIndex)
{
    if (bufferIndex >= m_renderBufferCount) [[unlikely]]
        return nullptr;
    RefPtr sharedResourceCache = m_parent->sharedResourceCache();
    if (!sharedResourceCache) [[unlikely]]
        return nullptr;
    // The image contents will be published in the shared resource cache by the GPU process. Adopt it
    // into the rendering backend's cache right away, so that drawing it needs no further set up.
    auto headroom = m_contentsHeadroom ? WebCore::Headroom(std::max<float>(*m_contentsHeadroom, WebCore::Headroom::None)) : WebCore::Headroom::None;
    Ref nativeImage = RemoteNativeImageProxy::create(m_renderBufferSize, m_renderBufferColorSpace.platformColorSpace(), m_renderBufferHasAlpha, sharedResourceCache.releaseNonNull(), headroom);
    protect(m_parent->renderingBackend())->cacheNativeImageFromSharedNativeImage(nativeImage);
    auto sendResult = send(Messages::RemoteCompositorIntegration::CopyNativeImage(bufferIndex, nativeImage->reference()));
    if (sendResult != IPC::Error::NoError) [[unlikely]]
        return nullptr;
    return RefPtr<WebCore::NativeImage> { WTF::move(nativeImage) };
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<WebCore::VideoFrame> RemoteCompositorIntegrationProxy::copyVideoFrame(uint32_t bufferIndex)
{
    // The video frame object heap is reached through the GPU process connection, which is used on the
    // main run loop. Elsewhere the caller reads the contents some other way.
    if (!isMainRunLoop() || bufferIndex >= m_renderBufferCount) [[unlikely]]
        return nullptr;
    RefPtr gpuProcessConnection = m_parent->gpuProcessConnection();
    if (!gpuProcessConnection) [[unlikely]]
        return nullptr;
    auto reference = RemoteVideoFrameReference::generateForAdd();
    auto sendResult = sendSync(Messages::RemoteCompositorIntegration::CopyVideoFrame(bufferIndex, reference));
    if (!sendResult.succeeded()) [[unlikely]]
        return nullptr;
    auto [properties] = sendResult.takeReply();
    if (!properties)
        return nullptr;
    return RemoteVideoFrameProxy::create(protect(m_parent->gpuProcessIPCConnection()), protect(gpuProcessConnection->videoFrameObjectHeapProxy()), reference, WTF::move(*properties));
}
#endif

void RemoteCompositorIntegrationProxy::updateContentsHeadroom(float headroom)
{
    m_contentsHeadroom = headroom;
    auto result = send(Messages::RemoteCompositorIntegration::UpdateContentsHeadroom(headroom));
    UNUSED_VARIABLE(result);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
