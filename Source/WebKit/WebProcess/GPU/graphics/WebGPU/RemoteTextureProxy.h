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

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

// The values a texture was created with, which the getters of the C++ API return. Outside the
// class, so that a default argument of RemoteTextureProxy can use its default member initializers.
// The format is std::nullopt when the Web Process does not know the values.
struct RemoteTextureProxyProperties {
    ::WebGPU::Extent3D size;
    uint32_t mipLevelCount { 1 };
    uint32_t sampleCount { 1 };
    ::WebGPU::TextureDimension dimension { ::WebGPU::TextureDimension::_2d };
    std::optional<::WebGPU::TextureFormat> format;
    OptionSet<::WebGPU::TextureUsage> usage;
};

class RemoteTextureProxy final : public ::WebGPU::Texture {
    WTF_MAKE_TZONE_ALLOCATED(RemoteTextureProxy);
public:
    using Properties = RemoteTextureProxyProperties;

    static Ref<RemoteTextureProxy> create(RemoteDeviceProxy& device, WebGPUTextureIdentifier identifier, const Properties& properties = { }, bool isCanvasBacking = false)
    {
        return adoptRef(*new RemoteTextureProxy(device, identifier, properties, isCanvasBacking));
    }

    virtual ~RemoteTextureProxy();

    RemoteGPUProxy& root() const { return m_root; }
    WebGPUTextureIdentifier backing() const { return m_backing; }

    RemoteDeviceProxy& device() const final { return m_device; }
    RefPtr<::WebGPU::TextureView> createView(const std::optional<::WebGPU::TextureViewDescriptor>&) final;
    RefPtr<::WebGPU::TextureView> createInvalidView() final;

    void destroy() final;
    void setLabel(String&&) final;
    bool isValid() const final;
    void undestroy() final;

    uint32_t width() const final { return m_properties.size.width; }
    uint32_t height() const final { return m_properties.size.height; }
    uint32_t depthOrArrayLayers() const final { return m_properties.size.depthOrArrayLayers; }
    uint32_t mipLevelCount() const final { return m_properties.mipLevelCount; }
    uint32_t sampleCount() const final { return m_properties.sampleCount; }
    ::WebGPU::TextureDimension dimension() const final { return m_properties.dimension; }
    std::optional<::WebGPU::TextureFormat> optionalFormat() const final { return m_properties.format; }
    OptionSet<::WebGPU::TextureUsage> usage() const final { return m_properties.usage; }

private:
    RemoteTextureProxy(RemoteDeviceProxy&, WebGPUTextureIdentifier, const Properties&, bool isCanvasBacking);

    RemoteTextureProxy(const RemoteTextureProxy&) = delete;
    RemoteTextureProxy(RemoteTextureProxy&&) = delete;
    RemoteTextureProxy& operator=(const RemoteTextureProxy&) = delete;
    RemoteTextureProxy& operator=(RemoteTextureProxy&&) = delete;

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }

    WebGPUTextureIdentifier m_backing;
    const Ref<RemoteGPUProxy> m_root;
    const Ref<RemoteDeviceProxy> m_device;

    RefPtr<::WebGPU::TextureView> m_lastCreatedView;
    std::optional<::WebGPU::TextureViewDescriptor> m_lastCreatedViewDescriptor;
    const Properties m_properties;
    bool m_isCanvasBacking { false };
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteTextureProxy)
    // In the Web Process, every WebGPU::Texture is a RemoteTextureProxy.
    static bool isType(const ::WebGPU::Texture&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
