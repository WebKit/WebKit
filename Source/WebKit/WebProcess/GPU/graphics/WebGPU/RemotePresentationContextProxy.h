/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "RemoteTextureProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <array>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class NativeImage;
}

namespace WebKit::WebGPU {

class RemoteTextureProxy;

class RemotePresentationContextProxy final : public ::WebGPU::PresentationContext {
    WTF_MAKE_TZONE_ALLOCATED(RemotePresentationContextProxy);
public:
    static Ref<RemotePresentationContextProxy> create(RemoteGPUProxy& parent, WebGPUPresentationContextIdentifier identifier)
    {
        return adoptRef(*new RemotePresentationContextProxy(parent, identifier));
    }

    virtual ~RemotePresentationContextProxy();

    RemoteGPUProxy& parent() const { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }
    WebGPUPresentationContextIdentifier backing() const { return m_backing; }

    void configure(const ::WebGPU::CanvasConfiguration&) final;
    void unconfigure() final;
    void present(uint32_t frameIndex) final;
    RefPtr<::WebGPU::Texture> getCurrentTexture(uint32_t) final;
    void renderBufferWasReplaced(uint32_t frameIndex) final;
    Seconds lastFrameGPUCost() const final;
    Seconds lastFramePresentStall() const final;
#if PLATFORM(COCOA)
    RetainPtr<CGImageRef> getTextureAsNativeImage(uint32_t, bool& isIOSurfaceSupportedFormat) final;
#endif
    void setLabel(String&&) final { }
    bool isValid() const final;

private:
    RemotePresentationContextProxy(RemoteGPUProxy&, WebGPUPresentationContextIdentifier);

    RemotePresentationContextProxy(const RemotePresentationContextProxy&) = delete;
    RemotePresentationContextProxy(RemotePresentationContextProxy&&) = delete;
    RemotePresentationContextProxy& operator=(const RemotePresentationContextProxy&) = delete;
    RemotePresentationContextProxy& operator=(RemotePresentationContextProxy&&) = delete;

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }

    WebGPUPresentationContextIdentifier m_backing;
    const Ref<RemoteGPUProxy> m_parent;
    static constexpr size_t textureCount = 3;
    std::array<RefPtr<RemoteTextureProxy>, textureCount> m_currentTexture;
    // The properties of the textures of the current configuration, as configured: the GPU Process
    // may clamp the size or replace an unsupported format.
    RemoteTextureProxy::Properties m_textureProperties;
    // The device of the current configuration, which the textures belong to.
    RefPtr<RemoteDeviceProxy> m_configuredDevice;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemotePresentationContextProxy)
    // In the Web Process, every WebGPU::PresentationContext is a RemotePresentationContextProxy.
    static bool isType(const ::WebGPU::PresentationContext&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
