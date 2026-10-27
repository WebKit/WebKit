/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Device.h"
#import "Texture.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU::Metal {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PresentationContextCoreAnimation);

PresentationContextCoreAnimation::PresentationContextCoreAnimation(CAMetalLayer *layer)
    : m_layer(layer)
{
}

PresentationContextCoreAnimation::~PresentationContextCoreAnimation() = default;

void PresentationContextCoreAnimation::configure(const WebGPU::CanvasConfiguration& configuration)
{
    m_configuration = std::nullopt;
    m_currentFrame = std::nullopt;

    Ref device = metal(configuration.device.get());
    // The formats that a CAMetalLayer can present.
    switch (configuration.format) {
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgba16float:
        break;
    case WebGPU::TextureFormat::Rgb10a2unorm:
        if (device->baseCapabilities().canPresentRGB10A2PixelFormats)
            break;
        [[fallthrough]];
    default:
        if (configuration.reportValidationErrors)
            device->generateAValidationError("Requested texture format cannot be presented"_s);
        return;
    }

    m_layer.pixelFormat = Texture::pixelFormat(configuration.format);
    m_layer.framebufferOnly = configuration.usage == WebGPU::TextureUsage::RenderAttachment;
    m_layer.drawableSize = CGSizeMake(configuration.width, configuration.height);
    m_layer.opaque = configuration.compositingAlphaMode == WebGPU::CanvasAlphaMode::Opaque;
    m_layer.device = device->device();

    m_configuration = Configuration {
        .device = WTF::move(device),
        .format = configuration.format,
        .usage = configuration.usage,
        .viewFormats = Vector<WebGPU::TextureFormat> { configuration.viewFormats },
        .width = configuration.width,
        .height = configuration.height,
    };
}

void PresentationContextCoreAnimation::unconfigure()
{
    m_configuration = std::nullopt;
    m_currentFrame = std::nullopt;
}

auto PresentationContextCoreAnimation::currentFrame() -> Frame*
{
    if (!m_configuration)
        return nullptr;
    if (m_currentFrame)
        return &*m_currentFrame;

    id<CAMetalDrawable> drawable = [m_layer nextDrawable];
    if (!drawable)
        return nullptr;

    WebGPU::TextureDescriptor descriptor {
        .label = "CAMetalLayer drawable"_s,
        .usage = m_configuration->usage,
        .dimension = WebGPU::TextureDimension::_2d,
        .size = { .width = m_configuration->width, .height = m_configuration->height, .depthOrArrayLayers = 1 },
        .format = m_configuration->format,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormats = m_configuration->viewFormats.span(),
    };
    Ref texture = Texture::create(drawable.texture, descriptor, Vector<WebGPU::TextureFormat> { m_configuration->viewFormats }, m_configuration->device);
    m_currentFrame = Frame { .drawable = drawable, .texture = WTF::move(texture) };
    return &*m_currentFrame;
}

Texture* PresentationContextCoreAnimation::currentTexture(uint32_t)
{
    auto* frame = currentFrame();
    return frame ? frame->texture.ptr() : nullptr;
}

// The drawable is presented when the work that renders into it, which has been submitted, completes.
void PresentationContextCoreAnimation::present(uint32_t)
{
    auto* frame = currentFrame();
    if (!frame)
        return;
    [frame->drawable present];
    m_currentFrame = std::nullopt;
}

} // namespace WebGPU::Metal
