/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebGPUSurfaceCocoa.h"

#include "PlatformCALayer.h"
#include "pal/graphics/WebGPU/WebGPUCanvasConfiguration.h"
#include "pal/graphics/WebGPU/WebGPUDevice.h"
#include "pal/graphics/WebGPU/WebGPUTextureDescriptor.h"

namespace WebCore {

static bool validateSurfaceCreation(IntSize, const PAL::WebGPU::CanvasConfiguration& configuration)
{
    // FIXME: Support RGBA8 or RGBA16Float if possible.
    if (configuration.format != PAL::WebGPU::TextureFormat::Bgra8unorm) {
        // FIXME: this generates a device validation error.
        return false;
    }

    // FIXME: Support view formats
    if (!configuration.viewFormats.isEmpty())
        return false;

    return true;
}

RefPtr<WebGPUSurfaceCocoa> WebGPUSurfaceCocoa::create(IntSize size, PAL::WebGPU::CanvasConfiguration configuration)
{
    if (!validateSurfaceCreation(size, configuration))
        return nullptr;

    return adoptRef(*new WebGPUSurfaceCocoa(WTFMove(size), WTFMove(configuration)));
}

WebGPUSurfaceCocoa::WebGPUSurfaceCocoa(IntSize size, PAL::WebGPU::CanvasConfiguration configuration)
    : m_size(WTFMove(size))
    , m_configuration(WTFMove(configuration))
{
    ASSERT(validateSurfaceCreation(size, configuration));
}

void WebGPUSurfaceCocoa::prepareToDelegateDisplay(PlatformCALayer&)
{
}

void WebGPUSurfaceCocoa::display(PlatformCALayer& layer)
{
    if (m_frontBuffer)
        layer.setContents(m_frontBuffer->surface());
}

GraphicsLayer::CompositingCoordinatesOrientation WebGPUSurfaceCocoa::orientation() const
{
    return GraphicsLayer::CompositingCoordinatesOrientation::TopDown;
}

void WebGPUSurfaceCocoa::present()
{
    std::swap(m_frontBuffer, m_backBuffer);
    m_backBufferTexture = nullptr;
    std::swap(m_backBuffer, m_secondaryBackBuffer);
}

bool WebGPUSurfaceCocoa::isValidBuffer(const IOSurface* buffer)
{
    if (!buffer)
        return false;

    if (buffer->size() != m_size)
        return false;

    if (buffer->isInUse())
        return false;

    return true;
}

void WebGPUSurfaceCocoa::ensureValidBackBuffer()
{
    if (isValidBuffer(m_backBuffer.get()))
        return;

    // The back buffer is not valid, so we should not reuse it and the back buffer texture.
    // Destroy the back buffer texture here.
    if (m_backBufferTexture) {
        m_backBufferTexture->destroy();
        m_backBufferTexture = nullptr;
    }

    // Then try to recycle the secondary back buffer as the new back buffer.
    m_backBuffer = WTFMove(m_secondaryBackBuffer);

    if (isValidBuffer(m_backBuffer.get()))
        return;

    // Can't use the secondary back buffer, so destroy it and create a new one.
    m_backBuffer = IOSurface::create(nullptr, m_size, colorSpace());
    ASSERT(isValidBuffer(m_backBuffer.get()));
}

Ref<PAL::WebGPU::Texture> WebGPUSurfaceCocoa::currentTexture()
{
    ensureValidBackBuffer();

    if (!m_backBufferTexture) {
        PAL::WebGPU::TextureDescriptor textureDescriptor {
            { "WebGPU Display texture"_s },
            PAL::WebGPU::Extent3DDict { static_cast<unsigned>(m_size.width()), static_cast<unsigned>(m_size.height()), 1 },
            1 /* mipMapCount */,
            1 /* sampleCount */,
            PAL::WebGPU::TextureDimension::_2d,
            m_configuration.format,
            m_configuration.usage,
            m_configuration.viewFormats
        };

        m_backBufferTexture = m_configuration.device.createIOSurfaceBackedTexture(textureDescriptor, m_backBuffer->surface());
    }

    return Ref { *m_backBufferTexture };
}

PixelFormat WebGPUSurfaceCocoa::pixelFormat() const
{
    switch (m_configuration.format) {
    case PAL::WebGPU::TextureFormat::Rgba8snorm:
        return PixelFormat::RGBA8;
    case PAL::WebGPU::TextureFormat::Bgra8unorm:
        return PixelFormat::BGRA8;
    default:
        ASSERT_NOT_REACHED();
        return PixelFormat::RGBA8;
    }
}

DestinationColorSpace WebGPUSurfaceCocoa::colorSpace() const
{
    switch (m_configuration.colorSpace) {
    case PAL::WebGPU::PredefinedColorSpace::SRGB:
        return DestinationColorSpace::SRGB();
    }
}

} // namespace WebCore.
