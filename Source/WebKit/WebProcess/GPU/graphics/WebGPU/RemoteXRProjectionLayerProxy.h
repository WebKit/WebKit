/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteGPUProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUXRProjectionLayer.h>

namespace PlatformXR {
struct FrameData;
}

namespace WebCore {
class ImageBuffer;
class NativeImage;
namespace WebGPU {
class Device;
}
}

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteXRProjectionLayerProxy final : public WebCore::WebGPU::XRProjectionLayer {
    WTF_MAKE_TZONE_ALLOCATED(RemoteXRProjectionLayerProxy);
public:
    static Ref<RemoteXRProjectionLayerProxy> create(Ref<RemoteGPUProxy>&& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteXRProjectionLayerProxy(WTFMove(parent), convertToBackingContext, identifier));
    }

    virtual ~RemoteXRProjectionLayerProxy();

    RemoteGPUProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent; }
    WebGPUIdentifier backing() const { return m_backing; }

private:
    friend class DowncastConvertToBackingContext;

    RemoteXRProjectionLayerProxy(Ref<RemoteGPUProxy>&&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteXRProjectionLayerProxy(const RemoteXRProjectionLayerProxy&) = delete;
    RemoteXRProjectionLayerProxy(RemoteXRProjectionLayerProxy&&) = delete;
    RemoteXRProjectionLayerProxy& operator=(const RemoteXRProjectionLayerProxy&) = delete;
    RemoteXRProjectionLayerProxy& operator=(RemoteXRProjectionLayerProxy&&) = delete;

    uint32_t textureWidth() const final;
    uint32_t textureHeight() const final;
    uint32_t textureArrayLength() const final;

    bool ignoreDepthValues() const final;
    std::optional<float> fixedFoveation() const final;
    void setFixedFoveation(std::optional<float>) final;
    WebCore::WebXRRigidTransform* deltaPose() const final;
    void setDeltaPose(WebCore::WebXRRigidTransform*) final;

    // WebXRLayer
#if PLATFORM(COCOA)
    void startFrame(size_t frameIndex, MachSendRight&&, MachSendRight&&, MachSendRight&&, size_t reusableTextureIndex, PlatformXR::RateMapDescription&&) final;
#endif
    void endFrame() final;

    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTFMove(message), backing());
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().protectedStreamClientConnection()->sendSync(WTFMove(message), backing());
    }

    bool isRemoteXRProjectionLayerProxy() const final { return true; }

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_parent;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteXRProjectionLayerProxy)
    static bool isType(const WebCore::WebGPU::XRProjectionLayer& projectionLayer) { return projectionLayer.isRemoteXRProjectionLayerProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
