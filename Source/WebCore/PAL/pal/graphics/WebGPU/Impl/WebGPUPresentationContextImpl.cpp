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

namespace PAL::WebGPU {

PresentationContextImpl::PresentationContextImpl(WGPUSurface surface, ConvertToBackingContext& convertToBackingContext)
    : m_backing(surface)
    , m_convertToBackingContext(convertToBackingContext)
{
}

PresentationContextImpl::~PresentationContextImpl()
{
    ASSERT(m_backing);
    wgpuSurfaceRelease(m_backing);
}

void PresentationContextImpl::configure(const CanvasConfiguration& canvasConfiguration)
{
    if (m_swapChain)
        m_swapChainWrapper = nullptr;

    m_format = canvasConfiguration.format;

    WGPUSwapChainDescriptor backingDescriptor {
        nullptr,
        nullptr,
        m_convertToBackingContext->convertTextureUsageFlagsToBacking(canvasConfiguration.usage),
        m_convertToBackingContext->convertToBacking(canvasConfiguration.format),
        m_width,
        m_height,
        WGPUPresentMode_Immediate,
    };

    m_swapChainWrapper = SwapChainWrapper::create(wgpuDeviceCreateSwapChain(m_convertToBackingContext->convertToBacking(canvasConfiguration.device), m_backing, &backingDescriptor));
    m_swapChain = m_swapChainWrapper->backing();
}

void PresentationContextImpl::unconfigure()
{
    if (!m_swapChain)
        return;

    m_swapChainWrapper = nullptr;
    
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
        ASSERT(m_swapChainWrapper);
        m_currentTexture = TextureImpl::create(wgpuSwapChainGetCurrentTexture(m_swapChain), m_format, TextureDimension::_2d, m_convertToBackingContext, *m_swapChainWrapper.copyRef()).ptr();
    }

    return m_currentTexture;
}

void PresentationContextImpl::present()
{
    wgpuSwapChainPresent(m_swapChain);
    m_currentTexture = nullptr;
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
