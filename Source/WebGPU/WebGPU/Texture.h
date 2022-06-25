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
#import <wtf/Vector.h>

struct WGPUTextureImpl {
};

namespace WebGPU {

class Device;
class TextureView;

// https://gpuweb.github.io/gpuweb/#gputexture
class Texture : public WGPUTextureImpl, public RefCounted<Texture> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Texture> create(id<MTLTexture> texture, const WGPUTextureDescriptor& descriptor, Vector<WGPUTextureFormat>&& viewFormats, Device& device)
    {
        return adoptRef(*new Texture(texture, descriptor, WTFMove(viewFormats), device));
    }
    static Ref<Texture> createInvalid(Device& device)
    {
        return adoptRef(*new Texture(device));
    }

    ~Texture();

    Ref<TextureView> createView(const WGPUTextureViewDescriptor&);
    void destroy();
    void setLabel(String&&);

    bool isValid() const { return m_texture; }

    static uint32_t texelBlockWidth(WGPUTextureFormat); // Texels
    static uint32_t texelBlockHeight(WGPUTextureFormat); // Texels
    // For depth-stencil textures, the input value to texelBlockSize()
    // needs to be the output of aspectSpecificFormat().
    static uint32_t texelBlockSize(WGPUTextureFormat); // Bytes
    static bool containsDepthAspect(WGPUTextureFormat);
    static bool containsStencilAspect(WGPUTextureFormat);
    static bool isDepthOrStencilFormat(WGPUTextureFormat);
    static WGPUTextureFormat aspectSpecificFormat(WGPUTextureFormat, WGPUTextureAspect);
    static bool validateImageCopyTexture(const WGPUImageCopyTexture&, const WGPUExtent3D&);
    static bool validateTextureCopyRange(const WGPUImageCopyTexture&, const WGPUExtent3D&);
    static bool refersToSingleAspect(WGPUTextureFormat, WGPUTextureAspect);
    static bool isValidImageCopySource(WGPUTextureFormat, WGPUTextureAspect);
    static bool isValidImageCopyDestination(WGPUTextureFormat, WGPUTextureAspect);
    static bool validateLinearTextureData(const WGPUTextureDataLayout&, uint64_t, WGPUTextureFormat, WGPUExtent3D);
    static WGPUTextureFormat removeSRGBSuffix(WGPUTextureFormat);

    WGPUExtent3D logicalMiplevelSpecificTextureExtent(uint32_t mipLevel);
    WGPUExtent3D physicalMiplevelSpecificTextureExtent(uint32_t mipLevel);

    id<MTLTexture> texture() const { return m_texture; }
    const WGPUTextureDescriptor& descriptor() const { return m_descriptor; }

    Device& device() const { return m_device; }

private:
    Texture(id<MTLTexture>, const WGPUTextureDescriptor&, Vector<WGPUTextureFormat>&& viewFormats, Device&);
    Texture(Device&);

    std::optional<WGPUTextureViewDescriptor> resolveTextureViewDescriptorDefaults(const WGPUTextureViewDescriptor&) const;
    uint32_t arrayLayerCount() const;
    bool validateCreateView(const WGPUTextureViewDescriptor&) const;

    id<MTLTexture> m_texture { nil };

    const WGPUTextureDescriptor m_descriptor { };
    const Vector<WGPUTextureFormat> m_viewFormats;

    const Ref<Device> m_device;
};

} // namespace WebGPU
