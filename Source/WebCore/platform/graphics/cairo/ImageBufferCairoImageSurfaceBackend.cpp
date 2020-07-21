/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferCairoImageSurfaceBackend.h"

#if USE(CAIRO)

#include "Color.h"
#include "ImageBackingStore.h"
#include <cairo.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferCairoImageSurfaceBackend);

std::unique_ptr<ImageBufferCairoImageSurfaceBackend> ImageBufferCairoImageSurfaceBackend::create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow*)
{
    static cairo_user_data_key_t s_surfaceDataKey;

    IntSize backendSize = calculateBackendSize(size, resolutionScale);
    if (backendSize.isEmpty() || backendSize.width() > cairoMaxImageSize || backendSize.height() > cairoMaxImageSize)
        return nullptr;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, backendSize.width());
    void* surfaceData;
    if (!tryFastZeroedMalloc(backendSize.height() * stride).getValue(surfaceData))
        return nullptr;

    auto surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(surfaceData), CAIRO_FORMAT_ARGB32, backendSize.width(), backendSize.height(), stride));
    cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) {
        fastFree(data);
    });

    return std::unique_ptr<ImageBufferCairoImageSurfaceBackend>(new ImageBufferCairoImageSurfaceBackend(size, backendSize, resolutionScale, colorSpace, WTFMove(surface)));
}

std::unique_ptr<ImageBufferCairoImageSurfaceBackend> ImageBufferCairoImageSurfaceBackend::create(const FloatSize& size, const GraphicsContext&)
{
    return ImageBufferCairoImageSurfaceBackend::create(size, 1, ColorSpace::SRGB, nullptr);
}

ImageBufferCairoImageSurfaceBackend::ImageBufferCairoImageSurfaceBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, RefPtr<cairo_surface_t>&& surface)
    : ImageBufferCairoSurfaceBackend(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(surface))
{
    ASSERT(cairo_surface_get_type(m_surface.get()) == CAIRO_SURFACE_TYPE_IMAGE);
}

void ImageBufferCairoImageSurfaceBackend::platformTransformColorSpace(const std::array<uint8_t, 256>& lookUpTable)
{
    unsigned char* dataSrc = cairo_image_surface_get_data(m_surface.get());
    int stride = cairo_image_surface_get_stride(m_surface.get());
    for (int y = 0; y < m_logicalSize.height(); ++y) {
        unsigned* row = reinterpret_cast_ptr<unsigned*>(dataSrc + stride * y);
        for (int x = 0; x < m_logicalSize.width(); x++) {
            unsigned* pixel = row + x;
            auto pixelComponents = unpremultiplied(asSRGBA(Packed::ARGB { *pixel }));
            pixelComponents = { lookUpTable[pixelComponents.red], lookUpTable[pixelComponents.green], lookUpTable[pixelComponents.blue], pixelComponents.alpha };
            *pixel = Packed::ARGB { premultipliedCeiling(pixelComponents) }.value;
        }
    }
    cairo_surface_mark_dirty_rectangle(m_surface.get(), 0, 0, m_logicalSize.width(), m_logicalSize.height());
}

} // namespace WebCore

#endif // USE(CAIRO)
