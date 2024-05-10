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
#import <wtf/WeakPtr.h>

struct WGPUTextureViewImpl {
};

namespace WebGPU {

class CommandEncoder;
class Device;
class Texture;

// https://gpuweb.github.io/gpuweb/#gputextureview
class TextureView : public WGPUTextureViewImpl, public RefCounted<TextureView>, public CanMakeWeakPtr<TextureView> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<TextureView> create(id<MTLTexture> texture, const WGPUTextureViewDescriptor& descriptor, const std::optional<WGPUExtent3D>& renderExtent, Texture& parentTexture, Device& device)
    {
        return adoptRef(*new TextureView(texture, descriptor, renderExtent, parentTexture, device));
    }
    static Ref<TextureView> createInvalid(Texture& texture, Device& device)
    {
        return adoptRef(*new TextureView(texture, device));
    }

    ~TextureView();

    void setLabel(String&&);

    bool isValid() const;

    id<MTLTexture> texture() const;
    id<MTLTexture> parentTexture() const;
    const WGPUTextureViewDescriptor& descriptor() const { return m_descriptor; }
    const std::optional<WGPUExtent3D>& renderExtent() const { return m_renderExtent; }

    Device& device() const { return m_device; }
    bool previouslyCleared() const;
    void setPreviouslyCleared(uint32_t mipLevel = 0, uint32_t slice = 0);
    uint32_t width() const;
    uint32_t height() const;
    uint32_t depthOrArrayLayers() const;
    WGPUTextureUsageFlags usage() const;
    uint32_t sampleCount() const;
    WGPUTextureFormat parentFormat() const;
    WGPUTextureFormat format() const;
    uint32_t parentMipLevelCount() const;
    uint32_t mipLevelCount() const;
    uint32_t baseMipLevel() const;
    WGPUTextureAspect aspect() const;
    uint32_t arrayLayerCount() const;
    uint32_t baseArrayLayer() const;
    WGPUTextureViewDimension dimension() const;
    bool isDestroyed() const;
    void destroy();
    void setCommandEncoder(CommandEncoder&) const;
    const Texture& apiParentTexture() const { return m_parentTexture; }
    Texture& apiParentTexture() { return m_parentTexture; }
    uint32_t parentRelativeSlice() const;
    uint32_t parentRelativeMipLevel() const;

private:
    TextureView(id<MTLTexture>, const WGPUTextureViewDescriptor&, const std::optional<WGPUExtent3D>&, Texture&, Device&);
    TextureView(Texture&, Device&);

    id<MTLTexture> m_texture { nil };

    const WGPUTextureViewDescriptor m_descriptor;
    const std::optional<WGPUExtent3D> m_renderExtent;

    const Ref<Device> m_device;
    Ref<Texture> m_parentTexture;
    mutable WeakPtr<CommandEncoder> m_commandEncoder;
};

} // namespace WebGPU
