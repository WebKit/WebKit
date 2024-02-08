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

#include "config.h"
#include "WebGPUPresentationContextImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUCanvasConfiguration.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDeviceImpl.h"
#include "WebGPUTextureDescriptor.h"
#include "WebGPUTextureImpl.h"
#include <WebGPU/WebGPUExt.h>

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#endif

namespace WebCore::WebGPU {

PresentationContextImpl::PresentationContextImpl(WebGPUPtr<WGPUSurface>&& surface, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTFMove(surface))
    , m_convertToBackingContext(convertToBackingContext)
{
}

PresentationContextImpl::~PresentationContextImpl() = default;

void PresentationContextImpl::configure(const CanvasConfiguration& canvasConfiguration)
{
    m_swapChain = nullptr;

    m_format = canvasConfiguration.format;

    WGPUSwapChainDescriptor backingDescriptor {
        nullptr,
        nullptr,
        m_convertToBackingContext->convertTextureUsageFlagsToBacking(canvasConfiguration.usage),
        m_convertToBackingContext->convertToBacking(canvasConfiguration.format),
        m_width,
        m_height,
        WGPUPresentMode_Immediate,
        canvasConfiguration.viewFormats.map([&convertToBackingContext = m_convertToBackingContext.get()](auto colorFormat) {
            return convertToBackingContext.convertToBacking(colorFormat);
        })
    };

    m_swapChain = adoptWebGPU(wgpuDeviceCreateSwapChain(m_convertToBackingContext->convertToBacking(canvasConfiguration.device), m_backing.get(), &backingDescriptor));
}

void PresentationContextImpl::unconfigure()
{
    if (!m_swapChain)
        return;

    m_swapChain = nullptr;
    
    m_format = TextureFormat::Bgra8unorm;
    m_width = 0;
    m_height = 0;
    m_swapChain = nullptr;
    m_currentTexture = nullptr;
}

RefPtr<Texture> PresentationContextImpl::getCurrentTexture()
{
    if (!m_swapChain)
        return nullptr; // FIXME: This should return an invalid texture instead.

    if (!m_currentTexture) {
        ASSERT(m_swapChain);
        m_currentTexture = TextureImpl::create(WebGPUPtr<WGPUTexture> { wgpuSwapChainGetCurrentTexture(m_swapChain.get()) }, m_format, TextureDimension::_2d, m_convertToBackingContext);
    }

    return m_currentTexture;
}

void PresentationContextImpl::present()
{
    if (auto* surface = m_swapChain.get())
        wgpuSwapChainPresent(surface);
    m_currentTexture = nullptr;
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
