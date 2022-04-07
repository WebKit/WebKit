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

struct WGPUTextureViewImpl {
};

namespace WebGPU {

class Device;
class Texture;

// https://gpuweb.github.io/gpuweb/#gputextureview
class TextureView : public WGPUTextureViewImpl, public RefCounted<TextureView> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<TextureView> create(id<MTLTexture> texture, const WGPUTextureViewDescriptor& descriptor, const std::optional<WGPUExtent3D>& renderExtent, Device& device)
    {
        return adoptRef(*new TextureView(texture, descriptor, renderExtent, device));
    }
    static Ref<TextureView> createInvalid(Device& device)
    {
        return adoptRef(*new TextureView(device));
    }

    ~TextureView();

    void setLabel(String&&);

    bool isValid() const { return m_texture; }

    id<MTLTexture> texture() const { return m_texture; }
    const WGPUTextureViewDescriptor& descriptor() const { return m_descriptor; }
    const std::optional<WGPUExtent3D>& renderExtent() const { return m_renderExtent; }

    Device& device() const { return m_device; }

private:
    TextureView(id<MTLTexture>, const WGPUTextureViewDescriptor&, const std::optional<WGPUExtent3D>&, Device&);
    TextureView(Device&);

    const id<MTLTexture> m_texture { nil };

    const WGPUTextureViewDescriptor m_descriptor;
    const std::optional<WGPUExtent3D> m_renderExtent;

    const Ref<Device> m_device;
};

} // namespace WebGPU
