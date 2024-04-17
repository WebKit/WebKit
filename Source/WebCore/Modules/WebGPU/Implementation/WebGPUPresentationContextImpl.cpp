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

#include "NativeImage.h"
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

void PresentationContextImpl::setSize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

static WGPUCompositeAlphaMode convertToAlphaMode(WebCore::WebGPU::CanvasAlphaMode compositingAlphaMode)
{
    switch (compositingAlphaMode) {
    case WebCore::WebGPU::CanvasAlphaMode::Opaque:
        return WGPUCompositeAlphaMode_Opaque;
    case WebCore::WebGPU::CanvasAlphaMode::Premultiplied:
        return WGPUCompositeAlphaMode_Premultiplied;
    }

    ASSERT_NOT_REACHED();
    return WGPUCompositeAlphaMode_Premultiplied;
}

bool PresentationContextImpl::configure(const CanvasConfiguration& canvasConfiguration)
{
    m_swapChain = nullptr;

    m_format = canvasConfiguration.format;

    WGPUSwapChainDescriptor backingDescriptor {
        .nextInChain = nullptr,
        .label = nullptr,
        .usage = m_convertToBackingContext->convertTextureUsageFlagsToBacking(canvasConfiguration.usage),
        .format = m_convertToBackingContext->convertToBacking(canvasConfiguration.format),
        .width = m_width,
        .height = m_height,
        .presentMode = WGPUPresentMode_Immediate,
        .viewFormats = canvasConfiguration.viewFormats.map([&convertToBackingContext = m_convertToBackingContext.get()](auto colorFormat) {
            return convertToBackingContext.convertToBacking(colorFormat);
        }),
        .colorSpace = canvasConfiguration.colorSpace == WebCore::WebGPU::PredefinedColorSpace::SRGB ? WGPUColorSpace::SRGB : WGPUColorSpace::DisplayP3,
        .compositeAlphaMode = convertToAlphaMode(canvasConfiguration.compositingAlphaMode),
        .reportValidationErrors = canvasConfiguration.reportValidationErrors
    };

    m_swapChain = adoptWebGPU(wgpuDeviceCreateSwapChain(m_convertToBackingContext->convertToBacking(canvasConfiguration.device), m_backing.get(), &backingDescriptor));
    return true;
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
        auto texturePtr = wgpuSwapChainGetCurrentTexture(m_swapChain.get());
        if (!texturePtr)
            return nullptr;

        m_currentTexture = TextureImpl::create(WebGPUPtr<WGPUTexture> { texturePtr }, m_format, TextureDimension::_2d, m_convertToBackingContext);
    }
    return m_currentTexture;
}

void PresentationContextImpl::present(bool)
{
    if (auto* surface = m_swapChain.get())
        wgpuSwapChainPresent(surface);
    m_currentTexture = nullptr;
}

RefPtr<WebCore::NativeImage> PresentationContextImpl::getMetalTextureAsNativeImage(uint32_t bufferIndex)
{
    if (auto* surface = m_swapChain.get())
        return WebCore::NativeImage::create(wgpuSwapChainGetTextureAsNativeImage(surface, bufferIndex));

    return nullptr;
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
