/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2011 Igalia S.L.
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
#include "ShareableBitmap.h"

#include "BitmapImage.h"
#include "CairoOperations.h"
#include "CairoUtilities.h"
#include "GraphicsContextCairo.h"
#include "NotImplemented.h"

namespace WebCore {

static const cairo_format_t cairoFormat = CAIRO_FORMAT_ARGB32;

std::optional<DestinationColorSpace> ShareableBitmapConfiguration::validateColorSpace(std::optional<DestinationColorSpace> colorSpace)
{
    return colorSpace;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerPixel(const DestinationColorSpace&)
{
    return 4;
}

CheckedUint32 ShareableBitmapConfiguration::calculateBytesPerRow(const IntSize& size, const DestinationColorSpace&)
{
    return cairo_format_stride_for_width(cairoFormat, size.width());
}

static inline RefPtr<cairo_surface_t> createSurfaceFromData(void* data, const IntSize& size)
{
    const int stride = cairo_format_stride_for_width(cairoFormat, size.width());
    return adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(data), cairoFormat, size.width(), size.height(), stride));
}

std::unique_ptr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    RefPtr<cairo_surface_t> image = createCairoSurface();
    return makeUnique<GraphicsContextCairo>(image.get());
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    paint(context, 1, dstPoint, srcRect);
}

void ShareableBitmap::paint(GraphicsContext& context, float scaleFactor, const IntPoint& dstPoint, const IntRect& srcRect)
{
    RefPtr<cairo_surface_t> surface = createSurfaceFromData(data(), size());
    cairo_surface_set_device_scale(surface.get(), scaleFactor, scaleFactor);
    FloatRect destRect(dstPoint, srcRect.size());

    ASSERT(context.hasPlatformContext());
    auto& state = context.state();
    Cairo::drawSurface(*context.platformContext(), surface.get(), destRect, srcRect, state.imageInterpolationQuality(), state.alpha(), Cairo::ShadowState(state));
}

RefPtr<cairo_surface_t> ShareableBitmap::createPersistentCairoSurface()
{
    return createSurfaceFromData(data(), size());
}

RefPtr<cairo_surface_t> ShareableBitmap::createCairoSurface()
{
    RefPtr<cairo_surface_t> image = createSurfaceFromData(data(), size());

    ref(); // Balanced by deref in releaseSurfaceData.
    static cairo_user_data_key_t dataKey;
    cairo_surface_set_user_data(image.get(), &dataKey, this, releaseSurfaceData);
    return image;
}

void ShareableBitmap::releaseSurfaceData(void* typelessBitmap)
{
    static_cast<ShareableBitmap*>(typelessBitmap)->deref(); // Balanced by ref in createCairoSurface.
}

RefPtr<Image> ShareableBitmap::createImage()
{
    RefPtr<cairo_surface_t> surface = createCairoSurface();
    if (!surface)
        return nullptr;

    return BitmapImage::create(WTFMove(surface));
}

void ShareableBitmap::setOwnershipOfMemory(const ProcessIdentity&)
{
}

} // namespace WebCore
