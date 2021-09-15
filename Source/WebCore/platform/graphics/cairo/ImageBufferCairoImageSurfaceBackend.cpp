/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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

IntSize ImageBufferCairoImageSurfaceBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    if (backendSize.isEmpty())
        return { };

    if (backendSize.width() > cairoMaxImageSize || backendSize.height() > cairoMaxImageSize)
        return { };

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, backendSize.width());
    if (stride == -1)
        return { };

    CheckedSize bytes = CheckedUint32(backendSize.height()) * stride;
    if (bytes.hasOverflowed())
        return { };

    return backendSize;
}

unsigned ImageBufferCairoImageSurfaceBackend::calculateBytesPerRow(const IntSize& backendSize)
{
    ASSERT(!backendSize.isEmpty());
    return cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, backendSize.width());
}

size_t ImageBufferCairoImageSurfaceBackend::calculateMemoryCost(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    return ImageBufferBackend::calculateMemoryCost(backendSize, calculateBytesPerRow(backendSize));
}

std::unique_ptr<ImageBufferCairoImageSurfaceBackend> ImageBufferCairoImageSurfaceBackend::create(const Parameters& parameters, const HostWindow*)
{
    ASSERT(parameters.pixelFormat == PixelFormat::BGRA8);

    static cairo_user_data_key_t s_surfaceDataKey;

    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, backendSize.width());
    void* surfaceData;
    if (!tryFastCalloc(backendSize.height(), stride).getValue(surfaceData))
        return nullptr;

    auto surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(surfaceData), CAIRO_FORMAT_ARGB32, backendSize.width(), backendSize.height(), stride));
    cairo_surface_set_user_data(surface.get(), &s_surfaceDataKey, surfaceData, [](void* data) {
        fastFree(data);
    });
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
        return nullptr;

    return std::unique_ptr<ImageBufferCairoImageSurfaceBackend>(new ImageBufferCairoImageSurfaceBackend(parameters, WTFMove(surface)));
}

std::unique_ptr<ImageBufferCairoImageSurfaceBackend> ImageBufferCairoImageSurfaceBackend::create(const Parameters& parameters, const GraphicsContext&)
{
    return ImageBufferCairoImageSurfaceBackend::create(parameters, nullptr);
}

ImageBufferCairoImageSurfaceBackend::ImageBufferCairoImageSurfaceBackend(const Parameters& parameters, RefPtr<cairo_surface_t>&& surface)
    : ImageBufferCairoSurfaceBackend(parameters, WTFMove(surface))
{
    ASSERT(cairo_surface_get_type(m_surface.get()) == CAIRO_SURFACE_TYPE_IMAGE);
}

unsigned ImageBufferCairoImageSurfaceBackend::bytesPerRow() const
{
    IntSize backendSize = calculateBackendSize(m_parameters);
    return calculateBytesPerRow(backendSize);
}

void ImageBufferCairoImageSurfaceBackend::platformTransformColorSpace(const std::array<uint8_t, 256>& lookUpTable)
{
    unsigned char* dataSrc = cairo_image_surface_get_data(m_surface.get());
    int stride = cairo_image_surface_get_stride(m_surface.get());
    for (int y = 0; y < logicalSize().height(); ++y) {
        unsigned* row = reinterpret_cast_ptr<unsigned*>(dataSrc + stride * y);
        for (int x = 0; x < logicalSize().width(); x++) {
            unsigned* pixel = row + x;
            auto pixelComponents = unpremultiplied(asSRGBA(PackedColor::ARGB { *pixel }));
            pixelComponents = { lookUpTable[pixelComponents.red], lookUpTable[pixelComponents.green], lookUpTable[pixelComponents.blue], pixelComponents.alpha };
            *pixel = PackedColor::ARGB { premultipliedCeiling(pixelComponents) }.value;
        }
    }
    cairo_surface_mark_dirty_rectangle(m_surface.get(), 0, 0, logicalSize().width(), logicalSize().height());
}

} // namespace WebCore

#endif // USE(CAIRO)
