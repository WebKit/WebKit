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

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDeviceImpl.h"
#include "WebGPUPresentationConfiguration.h"
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
    if (m_swapChain)
        wgpuSwapChainRelease(m_swapChain);

    ASSERT(m_backing);
    wgpuSurfaceRelease(m_backing);
}

IOSurfaceRef PresentationContextImpl::drawingBuffer() const
{
    return wgpuSurfaceCocoaCustomSurfaceGetDrawingBuffer(m_backing);
}

void PresentationContextImpl::configure(const PresentationConfiguration& presentationConfiguration)
{
    if (m_swapChain)
        wgpuSwapChainRelease(m_swapChain);

    m_format = presentationConfiguration.format;

    WGPUSwapChainDescriptor backingDescriptor {
        nullptr,
        nullptr,
        m_convertToBackingContext->convertTextureUsageFlagsToBacking(presentationConfiguration.usage),
        m_convertToBackingContext->convertToBacking(presentationConfiguration.format),
        presentationConfiguration.width,
        presentationConfiguration.height,
        WGPUPresentMode_Immediate,
    };

    m_swapChain = wgpuDeviceCreateSwapChain(m_convertToBackingContext->convertToBacking(presentationConfiguration.device), m_backing, &backingDescriptor);
}

void PresentationContextImpl::unconfigure()
{
    if (!m_swapChain)
        return;

    wgpuSwapChainRelease(m_swapChain);
    m_swapChain = nullptr;
}

RefPtr<Texture> PresentationContextImpl::getCurrentTexture()
{
    // FIXME: If m_swapChain is nullptr, return an invalid texture.

    if (!m_currentTexture) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250958 This is wrong; these objects are +0, but our Impl wrappers treat them as +1 objects.
        m_currentTexture = TextureImpl::create(wgpuSwapChainGetCurrentTexture(m_swapChain), m_format, TextureDimension::_2d, m_convertToBackingContext).ptr();
    }

    return m_currentTexture;
}

void PresentationContextImpl::present()
{
    wgpuSwapChainPresent(m_swapChain);
    m_currentTexture = nullptr;
}

#if PLATFORM(COCOA)
void PresentationContextImpl::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    if (!m_swapChain)
        return;

    wgpuSwapChainPresent(m_swapChain);
    auto ioSurface = wgpuSurfaceCocoaCustomSurfaceGetDisplayBuffer(m_backing);
    completionHandler(MachSendRight::adopt(IOSurfaceCreateMachPort(ioSurface)));
}
#endif

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
