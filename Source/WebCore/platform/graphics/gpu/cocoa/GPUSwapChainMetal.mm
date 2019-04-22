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
#import "GPUSwapChainDescriptor.h"
#import "GPUTextureFormat.h"
#import "GPUUtils.h"
#import "Logging.h"
#import "WebGPULayer.h"
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>

namespace WebCore {

static Optional<MTLPixelFormat> tryGetSupportedPixelFormat(GPUTextureFormat format)
{
    auto mtlFormat = static_cast<MTLPixelFormat>(platformTextureFormatForGPUTextureFormat(format));

    switch (mtlFormat) {
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatBGRA8Unorm_sRGB:
    case MTLPixelFormatRGBA16Float:
        return mtlFormat;
    default: {
        LOG(WebGPU, "GPUSwapChain::tryCreate(): Unsupported MTLPixelFormat!");
        return WTF::nullopt;
    }
    }
}

static void setLayerShape(WebGPULayer *layer, int width, int height)
{
    [layer setBounds:CGRectMake(0, 0, width, height)];
    [layer setDrawableSize:CGSizeMake(width, height)];
}

static RetainPtr<WebGPULayer> tryCreateWebGPULayer(MTLDevice *device, MTLPixelFormat format, OptionSet<GPUTextureUsage::Flags> usage)
{

    RetainPtr<WebGPULayer> layer;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    layer = adoptNS([WebGPULayer new]);

    [layer setName:@"Web GPU Layer"];
    [layer setOpaque:0];

    [layer setDevice:device];
    [layer setFramebufferOnly:(usage == GPUTextureUsage::Flags::OutputAttachment)];
    [layer setPixelFormat:format];
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!layer)
        LOG(WebGPU, "GPUSwapChain::tryCreate(): Unable to create CAMetalLayer!");

    return layer;
}

RefPtr<GPUSwapChain> GPUSwapChain::tryCreate(const GPUSwapChainDescriptor& descriptor, int width, int height)
{
    if (!descriptor.device->platformDevice()) {
        LOG(WebGPU, "GPUSwapChain::tryCreate(): Invalid GPUDevice!");
        return nullptr;
    }

    auto pixelFormat = tryGetSupportedPixelFormat(descriptor.format);
    if (!pixelFormat)
        return nullptr;

    auto usageOptions = OptionSet<GPUTextureUsage::Flags>::fromRaw(descriptor.usage);
    if (!usageOptions.contains(GPUTextureUsage::Flags::OutputAttachment)) {
        LOG(WebGPU, "GPUSwapChain::tryCreate(): Swap chain usage must include OUTPUT_ATTACHMENT!");
        return nullptr;
    }

    auto layer = tryCreateWebGPULayer(descriptor.device->platformDevice(), *pixelFormat, usageOptions);
    if (!layer)
        return nullptr;

    setLayerShape(layer.get(), width, height);

    auto swapChain = adoptRef(new GPUSwapChain(WTFMove(layer), usageOptions));
    descriptor.device->setSwapChain(swapChain.copyRef());
    return swapChain;
}

GPUSwapChain::GPUSwapChain(RetainPtr<WebGPULayer>&& platformLayer, OptionSet<GPUTextureUsage::Flags> usageOptions)
    : m_platformSwapLayer(WTFMove(platformLayer))
    , m_usage(usageOptions)
{
    [m_platformSwapLayer setSwapChain:this];
}

RefPtr<GPUTexture> GPUSwapChain::tryGetCurrentTexture()
{
    RetainPtr<MTLTexture> mtlTexture;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (!m_currentDrawable)
        m_currentDrawable = retainPtr([m_platformSwapLayer nextDrawable]);
    mtlTexture = [m_currentDrawable texture];

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlTexture) {
        LOG(WebGPU, "GPUSwapChain::getCurrentTexture(): Unable to get current MTLTexture!");
        return nullptr;
    }

    return GPUTexture::create(WTFMove(mtlTexture), m_usage);
}

void GPUSwapChain::present()
{
    if (!m_currentDrawable)
        return;

    [m_currentDrawable present];
    m_currentDrawable = nullptr;
}

void GPUSwapChain::reshape(int width, int height)
{
    setLayerShape(m_platformSwapLayer.get(), width, height);
}

PlatformLayer* GPUSwapChain::platformLayer() const
{
    return m_platformSwapLayer.get();
}

#if USE(METAL)
RetainPtr<CAMetalDrawable> GPUSwapChain::takeDrawable()
{
    RetainPtr<CAMetalDrawable> ptr;
    ptr.swap(m_currentDrawable);
    return ptr;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGPU)
