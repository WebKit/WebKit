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

#pragma once

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUIntegralTypes.h"
#include "WebGPUSwapChainWrapper.h"
#include "WebGPUTexture.h"
#include "WebGPUTextureDimension.h"
#include "WebGPUTextureFormat.h"
#include <WebGPU/WebGPU.h>

namespace PAL::WebGPU {

class ConvertToBackingContext;

class TextureImpl final : public Texture {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<TextureImpl> create(WGPUTexture texture, TextureFormat format, TextureDimension dimension, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new TextureImpl(texture, format, dimension, convertToBackingContext));
    }
    static Ref<TextureImpl> create(WGPUTexture texture, TextureFormat format, TextureDimension dimension, ConvertToBackingContext& convertToBackingContext, Ref<SwapChainWrapper>&& swapChainWrapper)
    {
        return adoptRef(*new TextureImpl(texture, format, dimension, convertToBackingContext, WTFMove(swapChainWrapper)));
    }

    virtual ~TextureImpl();

private:
    friend class DowncastConvertToBackingContext;

    TextureImpl(WGPUTexture, TextureFormat, TextureDimension, ConvertToBackingContext&);
    TextureImpl(WGPUTexture, TextureFormat, TextureDimension, ConvertToBackingContext&, Ref<SwapChainWrapper>&&);

    TextureImpl(const TextureImpl&) = delete;
    TextureImpl(TextureImpl&&) = delete;
    TextureImpl& operator=(const TextureImpl&) = delete;
    TextureImpl& operator=(TextureImpl&&) = delete;

    WGPUTexture backing() const { return m_backing; }

    Ref<TextureView> createView(const std::optional<TextureViewDescriptor>&) final;

    void destroy() final;

    void setLabelInternal(const String&) final;

    TextureFormat m_format { TextureFormat::Rgba8unorm };
    TextureDimension m_dimension { TextureDimension::_2d };

    WGPUTexture m_backing { nullptr };
    Ref<ConvertToBackingContext> m_convertToBackingContext;

    // Some textures are internally owned by their WGPUSwapChain, and wgpuSwapChainGetCurrentTexture() is
    // supposed to return the same object each time it's called within the same frame. This means that both PresentationContextImpl and TextureImpl need to
    // have strong references to the same WGPUSwapChain. However, WGPUSwapChains aren't reference counted, so we use a reference
    // counted wrapper around it.
    RefPtr<SwapChainWrapper> m_swapChainWrapper;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
