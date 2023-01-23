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

#include "WebGPUSwapChain.h"
#include "WebGPUTextureFormat.h"
#include <WebGPU/WebGPU.h>

namespace PAL::WebGPU {

class ConvertToBackingContext;
class TextureImpl;
class TextureViewImpl;

class SwapChainImpl final : public SwapChain {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<SwapChainImpl> create(WGPUSurface surface, WGPUSwapChain swapChain, TextureFormat format)
    {
        return adoptRef(*new SwapChainImpl(surface, swapChain, format));
    }

    virtual ~SwapChainImpl();

private:
    friend class DowncastConvertToBackingContext;

    SwapChainImpl(WGPUSurface, WGPUSwapChain, TextureFormat);

    SwapChainImpl(const SwapChainImpl&) = delete;
    SwapChainImpl(SwapChainImpl&&) = delete;
    SwapChainImpl& operator=(const SwapChainImpl&) = delete;
    SwapChainImpl& operator=(SwapChainImpl&&) = delete;

    void clearCurrentTextureAndView();
    void ensureCurrentTextureAndView();

    WGPUSwapChain backing() const { return m_backing; }

    Texture& getCurrentTexture() final;
    TextureView& getCurrentTextureView() final;
    void present() final;

    void destroy() final;
#if PLATFORM(COCOA)
    void prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&&) final;
#endif

    void setLabelInternal(const String&) final;

    TextureFormat m_format { TextureFormat::Rgba8unorm };

    WGPUSwapChain m_backing { nullptr };
    WGPUSurface m_surface { nullptr };
    RefPtr<TextureImpl> m_currentTexture;
    RefPtr<TextureViewImpl> m_currentTextureView;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
