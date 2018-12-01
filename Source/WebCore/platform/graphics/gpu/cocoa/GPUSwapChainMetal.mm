/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GPUSwapChain.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "GPUTexture.h"
#import "GPUTextureFormatEnum.h"
#import "Logging.h"

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

RefPtr<GPUSwapChain> GPUSwapChain::create()
{
    PlatformSwapLayerSmartPtr platformLayer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    platformLayer = adoptNS([[CAMetalLayer alloc] init]);

    [platformLayer setOpaque:0];
    [platformLayer setName:@"WebGPU Layer"];

    // FIXME: For now, default to this usage flag.
    [platformLayer setFramebufferOnly:YES];

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!platformLayer) {
        LOG(WebGPU, "GPUSwapChain::create(): Unable to create CAMetalLayer!");
        return nullptr;
    }

    return adoptRef(new GPUSwapChain(WTFMove(platformLayer)));
}

GPUSwapChain::GPUSwapChain(PlatformSwapLayerSmartPtr&& platformLayer)
    : m_platformSwapLayer(WTFMove(platformLayer))
{
}

void GPUSwapChain::setDevice(const GPUDevice& device)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUSwapChain::setDevice(): MTLDevice does not exist!");
        return;
    }

    [m_platformSwapLayer setDevice:device.platformDevice()];
}

static std::optional<PlatformTextureFormat> platformTextureFormatForGPUTextureFormat(GPUTextureFormatEnum format)
{
    switch (format) {
    case GPUTextureFormatEnum::R8G8B8A8Unorm:
        return MTLPixelFormatRGBA8Unorm;
    case GPUTextureFormatEnum::R8G8B8A8Uint:
        return MTLPixelFormatRGBA8Uint;
    case GPUTextureFormatEnum::B8G8R8A8Unorm:
        return MTLPixelFormatBGRA8Unorm;
    case GPUTextureFormatEnum::D32FloatS8Uint:
        return MTLPixelFormatDepth32Float_Stencil8;
    default:
        LOG(WebGPU, "GPUSwapChain::setFormat(): Invalid texture format specified!");
        return std::nullopt;
    }
}

void GPUSwapChain::setFormat(GPUTextureFormatEnum format)
{
    auto result = platformTextureFormatForGPUTextureFormat(format);
    if (!result)
        return;

    auto mtlResult = static_cast<MTLPixelFormat>(result.value());

    switch (mtlResult) {
    case MTLPixelFormatBGRA8Unorm:
    // FIXME: Add the other supported swap layer formats as they are added to GPU spec.
    //  MTLPixelFormatBGRA8Unorm_sRGB, MTLPixelFormatRGBA16Float, MTLPixelFormatBGRA10_XR, and MTLPixelFormatBGRA10_XR_sRGB.
        [m_platformSwapLayer setPixelFormat:mtlResult];
        return;
    default:
        LOG(WebGPU, "GPUSwapChain::setFormat(): Unsupported MTLPixelFormat!");
    }
}

void GPUSwapChain::reshape(int width, int height)
{
    [m_platformSwapLayer setBounds:CGRectMake(0, 0, width, height)];
    [m_platformSwapLayer setDrawableSize:CGSizeMake(width, height)];
}

RefPtr<GPUTexture> GPUSwapChain::getNextTexture()
{
    RetainPtr<MTLTexture> mtlTexture;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    m_currentDrawable = retainPtr([m_platformSwapLayer nextDrawable]);
    mtlTexture = retainPtr([m_currentDrawable texture]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlTexture) {
        LOG(WebGPU, "GPUSwapChain::getNextTexture(): Unable to get next MTLTexture!");
        return nullptr;
    }

    return GPUTexture::create(WTFMove(mtlTexture));
}

void GPUSwapChain::present()
{
    [m_currentDrawable present];
    m_currentDrawable = nullptr;
}

PlatformLayer* GPUSwapChain::platformLayer() const
{
    return m_platformSwapLayer.get();
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
