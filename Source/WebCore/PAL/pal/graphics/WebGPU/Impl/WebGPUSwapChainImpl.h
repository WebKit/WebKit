/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebGPUSwapChain.h"
#include <IOSurface/IOSurfaceRef.h>
#include <WebGPU/WebGPU.h>

namespace PAL::WebGPU {

class SwapChainImpl final : public SwapChain {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<SwapChainImpl> create(WGPUSurface surface, WGPUSwapChain swapChain)
    {
        return adoptRef(*new SwapChainImpl(surface, swapChain));
    }

    virtual ~SwapChainImpl();

private:
    friend class DowncastConvertToBackingContext;

    SwapChainImpl(WGPUSurface, WGPUSwapChain);

    SwapChainImpl(const SwapChainImpl&) = delete;
    SwapChainImpl(SwapChainImpl&&) = delete;
    SwapChainImpl& operator=(const SwapChainImpl&) = delete;
    SwapChainImpl& operator=(SwapChainImpl&&) = delete;

    WGPUSwapChain backing() const { return m_backing; }

    void destroy() final;
#if PLATFORM(COCOA)
    void prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&&) final;
#endif

    void setLabelInternal(const String&) final;

    WGPUSwapChain m_backing { nullptr };
    WGPUSurface m_surface { nullptr };
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
