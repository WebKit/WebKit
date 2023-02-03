/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "WebGPUPresentationContext.h"
#include "WebGPUTextureFormat.h"
#include <IOSurface/IOSurfaceRef.h>
#include <WebGPU/WebGPU.h>
#include <wtf/MachSendRight.h>

namespace PAL::WebGPU {

class ConvertToBackingContext;
class TextureImpl;

class PresentationContextImpl final : public PresentationContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PresentationContextImpl> create(WGPUSurface surface, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new PresentationContextImpl(surface, convertToBackingContext));
    }

    virtual ~PresentationContextImpl();

    void setSize(uint32_t width, uint32_t height)
    {
        m_width = width;
        m_height = height;
    }

    IOSurfaceRef drawingBuffer() const;

    WGPUSurface backing() const { return m_backing; }

private:
    friend class DowncastConvertToBackingContext;

    PresentationContextImpl(WGPUSurface, ConvertToBackingContext&);

    PresentationContextImpl(const PresentationContextImpl&) = delete;
    PresentationContextImpl(PresentationContextImpl&&) = delete;
    PresentationContextImpl& operator=(const PresentationContextImpl&) = delete;
    PresentationContextImpl& operator=(PresentationContextImpl&&) = delete;

    void configure(const CanvasConfiguration&) final;
    void unconfigure() final;

    RefPtr<Texture> getCurrentTexture() final;

    void present() final;

#if PLATFORM(COCOA)
    void prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&&) final;
#endif

    TextureFormat m_format { TextureFormat::Bgra8unorm };
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };

    WGPUSurface m_backing { nullptr };
    WGPUSwapChain m_swapChain { nullptr };
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    RefPtr<TextureImpl> m_currentTexture;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
