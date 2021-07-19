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
#include "ImageBufferCairoSurfaceBackend.h"

#include "BitmapImage.h"
#include "CairoOperations.h"
#include "Color.h"
#include "GraphicsContext.h"
#include "ImageBufferUtilitiesCairo.h"
#include "PixelBuffer.h"
#include <cairo.h>

#if USE(CAIRO)

namespace WebCore {

ImageBufferCairoSurfaceBackend::ImageBufferCairoSurfaceBackend(const Parameters& parameters, RefPtr<cairo_surface_t>&& surface)
    : ImageBufferCairoBackend(parameters)
    , m_surface(WTFMove(surface))
    , m_context(m_surface.get())
{
    ASSERT(cairo_surface_status(m_surface.get()) == CAIRO_STATUS_SUCCESS);
}

GraphicsContext& ImageBufferCairoSurfaceBackend::context() const
{
    return m_context;
}

IntSize ImageBufferCairoSurfaceBackend::backendSize() const
{
    return { cairo_image_surface_get_width(m_surface.get()), cairo_image_surface_get_height(m_surface.get()) };
}

unsigned ImageBufferCairoSurfaceBackend::bytesPerRow() const
{
    return cairo_image_surface_get_stride(m_surface.get());
}

RefPtr<NativeImage> ImageBufferCairoSurfaceBackend::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    switch (copyBehavior) {
    case CopyBackingStore: {
        auto copy = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        cairo_image_surface_get_width(m_surface.get()),
        cairo_image_surface_get_height(m_surface.get())));

        auto cr = adoptRef(cairo_create(copy.get()));
        cairo_set_operator(cr.get(), CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface(cr.get(), m_surface.get(), 0, 0);
        cairo_paint(cr.get());

        return NativeImage::create(WTFMove(copy));
    }

    case DontCopyBackingStore:
        return NativeImage::create(makeRefPtr(m_surface.get()));
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<NativeImage> ImageBufferCairoSurfaceBackend::cairoSurfaceCoerceToImage() const
{
    BackingStoreCopy copyBehavior;
    if (cairo_surface_get_type(m_surface.get()) == CAIRO_SURFACE_TYPE_IMAGE && cairo_surface_get_content(m_surface.get()) == CAIRO_CONTENT_COLOR_ALPHA)
        copyBehavior = DontCopyBackingStore;
    else
        copyBehavior = CopyBackingStore;
    return copyNativeImage(copyBehavior);
}

std::optional<PixelBuffer> ImageBufferCairoSurfaceBackend::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect) const
{
    return ImageBufferBackend::getPixelBuffer(outputFormat, srcRect, cairo_image_surface_get_data(m_surface.get()));
}

void ImageBufferCairoSurfaceBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, cairo_image_surface_get_data(m_surface.get()));

    IntRect srcRectScaled = toBackendCoordinates(srcRect);
    IntPoint destPointScaled = toBackendCoordinates(destPoint);
    cairo_surface_mark_dirty_rectangle(m_surface.get(), destPointScaled.x(), destPointScaled.y(), srcRectScaled.width(), srcRectScaled.height());
}

} // namespace WebCore

#endif // USE(CAIRO)
