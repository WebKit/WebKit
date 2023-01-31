/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PresentationContextCoreAnimation.h"

#import "APIConversions.h"
#import "Texture.h"
#import "TextureView.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebGPU {

static CAMetalLayer *layerFromSurfaceDescriptor(const WGPUSurfaceDescriptor& descriptor)
{
    ASSERT(descriptor.nextInChain->sType == WGPUSType_SurfaceDescriptorFromMetalLayer);
    ASSERT(!descriptor.nextInChain->next);
    const auto& metalDescriptor = *reinterpret_cast<const WGPUSurfaceDescriptorFromMetalLayer*>(descriptor.nextInChain);
    CAMetalLayer *layer = bridge_id_cast(metalDescriptor.layer);
    return layer;
}

PresentationContextCoreAnimation::PresentationContextCoreAnimation(const WGPUSurfaceDescriptor& descriptor)
    : m_layer(layerFromSurfaceDescriptor(descriptor))
{
}

PresentationContextCoreAnimation::~PresentationContextCoreAnimation() = default;

void PresentationContextCoreAnimation::configure(Device& device, const WGPUSwapChainDescriptor& descriptor)
{
    m_configuration = std::nullopt;

    if (descriptor.nextInChain)
        return;

    switch (descriptor.format) {
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGBA16Float:
        break;
    case WGPUTextureFormat_RGB10A2Unorm:
        if (device.baseCapabilities().canPresentRGB10A2PixelFormats)
            break;
        return;
    default:
        return;
    }

    m_configuration = Configuration(descriptor.width, descriptor.height, descriptor.usage, fromAPI(descriptor.label), descriptor.format, device);

    m_layer.pixelFormat = Texture::pixelFormat(descriptor.format);
    if (descriptor.usage == WGPUTextureUsage_RenderAttachment)
        m_layer.framebufferOnly = YES;
    m_layer.drawableSize = CGSizeMake(descriptor.width, descriptor.height);
    m_layer.device = device.device();
}

auto PresentationContextCoreAnimation::Configuration::generateCurrentFrameState(CAMetalLayer *layer) -> Configuration::FrameState
{
    auto label = this->label.utf8();

    id<CAMetalDrawable> currentDrawable = [layer nextDrawable];
    id<MTLTexture> backingTexture = currentDrawable.texture;

    WGPUTextureDescriptor textureDescriptor {
        nullptr,
        label.data(),
        usage,
        WGPUTextureDimension_2D, {
            width,
            height,
            1,
        },
        format,
        1,
        1,
        0,
        nullptr,
    };
    auto texture = Texture::create(backingTexture, textureDescriptor, { format }, device);

    WGPUTextureViewDescriptor textureViewDescriptor {
        nullptr,
        label.data(),
        format,
        WGPUTextureViewDimension_2D,
        0,
        1,
        0,
        1,
        WGPUTextureAspect_All,
    };

    auto textureView = TextureView::create(backingTexture, textureViewDescriptor, { { width, height, 1 } }, device);
    return { currentDrawable, texture.ptr(), textureView.ptr() };
}

void PresentationContextCoreAnimation::present()
{
    if (!m_configuration)
        return;

    if (!m_configuration->currentFrameState)
        m_configuration->currentFrameState = m_configuration->generateCurrentFrameState(m_layer);

    [m_configuration->currentFrameState->currentDrawable present];

    m_configuration->currentFrameState = std::nullopt;
}

Texture* PresentationContextCoreAnimation::getCurrentTexture()
{
    if (!m_configuration)
        return nullptr;

    if (!m_configuration->currentFrameState)
        m_configuration->currentFrameState = m_configuration->generateCurrentFrameState(m_layer);

    return m_configuration->currentFrameState->currentTexture.get();
}

TextureView* PresentationContextCoreAnimation::getCurrentTextureView()
{
    if (!m_configuration)
        return nullptr;

    if (!m_configuration->currentFrameState)
        m_configuration->currentFrameState = m_configuration->generateCurrentFrameState(m_layer);

    return m_configuration->currentFrameState->currentTextureView.get();
}

} // namespace WebGPU
