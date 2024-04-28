/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "ImageBufferSkiaAcceleratedBackend.h"

#if USE(SKIA)
#include "FontRenderOptions.h"
#include "GLContext.h"
#include "IntRect.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"
#include <skia/core/SkBitmap.h>
#include <skia/core/SkPixmap.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/gl/GrGLTypes.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferSkiaAcceleratedBackend);

std::unique_ptr<ImageBufferSkiaAcceleratedBackend> ImageBufferSkiaAcceleratedBackend::create(const Parameters& parameters, const ImageBufferCreationContext&)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto* glContext = PlatformDisplay::sharedDisplayForCompositing().skiaGLContext();
    if (!glContext || !glContext->makeContextCurrent())
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplayForCompositing().skiaGrContext();
    RELEASE_ASSERT(grContext);
    auto imageInfo = SkImageInfo::MakeN32Premul(backendSize.width(), backendSize.height(), parameters.colorSpace.platformColorSpace());
    SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, 0, kTopLeft_GrSurfaceOrigin, &properties);
    if (!surface || !surface->getCanvas())
        return nullptr;

    return std::unique_ptr<ImageBufferSkiaAcceleratedBackend>(new ImageBufferSkiaAcceleratedBackend(parameters, WTFMove(surface)));
}

ImageBufferSkiaAcceleratedBackend::ImageBufferSkiaAcceleratedBackend(const Parameters& parameters, sk_sp<SkSurface>&& surface)
    : ImageBufferSkiaSurfaceBackend(parameters, WTFMove(surface), RenderingMode::Accelerated)
{
}

ImageBufferSkiaAcceleratedBackend::~ImageBufferSkiaAcceleratedBackend() = default;

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::copyNativeImage()
{
    // FIXME: do we have to do a explicit copy here?
    return NativeImage::create(m_surface->makeImageSnapshot());
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::createNativeImageReference()
{
    return NativeImage::create(m_surface->makeImageSnapshot());
}

void ImageBufferSkiaAcceleratedBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    if (!PlatformDisplay::sharedDisplayForCompositing().skiaGLContext()->makeContextCurrent())
        return;

    auto info = m_surface->imageInfo();
    auto data = SkData::MakeUninitialized(info.computeMinByteSize());
    auto* pixels = static_cast<uint8_t*>(data->writable_data());
    size_t rowBytes = static_cast<size_t>(info.minRowBytes64());
    if (m_surface->readPixels(info, pixels, rowBytes, 0, 0))
        ImageBufferBackend::getPixelBuffer(srcRect, pixels, destination);
}

void ImageBufferSkiaAcceleratedBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    if (!PlatformDisplay::sharedDisplayForCompositing().skiaGLContext()->makeContextCurrent())
        return;

    auto info = m_surface->imageInfo();
    auto data = SkData::MakeUninitialized(info.computeMinByteSize());
    auto* pixels = static_cast<uint8_t*>(data->writable_data());
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, pixels);
    SkPixmap pixmap(info, pixels, info.minRowBytes64());
    m_surface->writePixels(pixmap, 0, 0);
}

} // namespace WebCore

#endif // USE(SKIA)
