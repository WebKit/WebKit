/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/RefPtr.h>

struct WGPUTextureImpl {
};

namespace WebGPU {

class Device;
class TextureView;

class Texture : public WGPUTextureImpl, public RefCounted<Texture> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Texture> create(id<MTLTexture> texture, const WGPUTextureDescriptor& descriptor, Device& device)
    {
        return adoptRef(*new Texture(texture, descriptor, device));
    }

    ~Texture();

    RefPtr<TextureView> createView(const WGPUTextureViewDescriptor&);
    void destroy();
    void setLabel(String&&);

    static uint32_t texelBlockWidth(WGPUTextureFormat); // Texels
    static uint32_t texelBlockHeight(WGPUTextureFormat); // Texels
    static bool containsDepthAspect(WGPUTextureFormat);
    static bool containsStencilAspect(WGPUTextureFormat);
    static bool isDepthOrStencilFormat(WGPUTextureFormat);

    id<MTLTexture> texture() const { return m_texture; }
    const WGPUTextureDescriptor& descriptor() const { return m_descriptor; }

private:
    Texture(id<MTLTexture>, const WGPUTextureDescriptor&, Device&);

    std::optional<WGPUTextureViewDescriptor> resolveTextureViewDescriptorDefaults(const WGPUTextureViewDescriptor&) const;
    uint32_t arrayLayerCount() const;
    bool validateCreateView(const WGPUTextureViewDescriptor&) const;

    const id<MTLTexture> m_texture { nil };

    const WGPUTextureDescriptor m_descriptor { }; // "The GPUTextureDescriptor describing this texture."

    const Ref<Device> m_device;
};

} // namespace WebGPU
