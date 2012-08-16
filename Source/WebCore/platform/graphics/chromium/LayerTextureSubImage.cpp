/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerTextureSubImage.h"

#include "Extensions3DChromium.h"
#include "LayerRendererChromium.h" // For GLC() macro
#include "TraceEvent.h"
#include <public/WebGraphicsContext3D.h>

using WebKit::WebGraphicsContext3D;

namespace WebCore {

LayerTextureSubImage::LayerTextureSubImage(bool useMapTexSubImage)
    : m_useMapTexSubImage(useMapTexSubImage)
    , m_subImageSize(0)
{
}

LayerTextureSubImage::~LayerTextureSubImage()
{
}

void LayerTextureSubImage::upload(const uint8_t* image, const IntRect& imageRect,
                                  const IntRect& sourceRect, const IntSize& destOffset,
                                  GC3Denum format, WebGraphicsContext3D* context)
{
    if (m_useMapTexSubImage)
        uploadWithMapTexSubImage(image, imageRect, sourceRect, destOffset, format, context);
    else
        uploadWithTexSubImage(image, imageRect, sourceRect, destOffset, format, context);
}

void LayerTextureSubImage::uploadWithTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                                 const IntRect& sourceRect, const IntSize& destOffset,
                                                 GC3Denum format, WebGraphicsContext3D* context)
{
    TRACE_EVENT0("cc", "LayerTextureSubImage::uploadWithTexSubImage");

    // Offset from image-rect to source-rect.
    IntPoint offset(sourceRect.x() - imageRect.x(), sourceRect.y() - imageRect.y());

    const uint8_t* pixelSource;
    if (imageRect.width() == sourceRect.width() && !offset.x())
        pixelSource = &image[4 * offset.y() * imageRect.width()];
    else {
        size_t neededSize = 4 * sourceRect.width() * sourceRect.height();
        if (m_subImageSize < neededSize) {
          m_subImage = adoptArrayPtr(new uint8_t[neededSize]);
          m_subImageSize = neededSize;
        }
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into a temp buffer for uploading.
        for (int row = 0; row < sourceRect.height(); ++row)
            memcpy(&m_subImage[sourceRect.width() * 4 * row],
                   &image[4 * (offset.x() + (offset.y() + row) * imageRect.width())],
                   sourceRect.width() * 4);

        pixelSource = &m_subImage[0];
    }

    GLC(context, context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, destOffset.width(), destOffset.height(), sourceRect.width(), sourceRect.height(), format, GraphicsContext3D::UNSIGNED_BYTE, pixelSource));
}

void LayerTextureSubImage::uploadWithMapTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                                    const IntRect& sourceRect, const IntSize& destOffset,
                                                    GC3Denum format, WebGraphicsContext3D* context)
{
    TRACE_EVENT0("cc", "LayerTextureSubImage::uploadWithMapTexSubImage");
    // Offset from image-rect to source-rect.
    IntPoint offset(sourceRect.x() - imageRect.x(), sourceRect.y() - imageRect.y());

    // Upload tile data via a mapped transfer buffer
    uint8_t* pixelDest = static_cast<uint8_t*>(context->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, destOffset.width(), destOffset.height(), sourceRect.width(), sourceRect.height(), format, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY));

    if (!pixelDest) {
        uploadWithTexSubImage(image, imageRect, sourceRect, destOffset, format, context);
        return;
    }

    unsigned int componentsPerPixel;
    unsigned int bytesPerComponent;
    if (!GraphicsContext3D::computeFormatAndTypeParameters(format, GraphicsContext3D::UNSIGNED_BYTE, &componentsPerPixel, &bytesPerComponent)) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (imageRect.width() == sourceRect.width() && !offset.x())
        memcpy(pixelDest, &image[offset.y() * imageRect.width() * componentsPerPixel * bytesPerComponent], imageRect.width() * sourceRect.height() * componentsPerPixel * bytesPerComponent);
    else {
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into the pixelDest
        for (int row = 0; row < sourceRect.height(); ++row)
            memcpy(&pixelDest[sourceRect.width() * row * componentsPerPixel * bytesPerComponent],
                   &image[4 * (offset.x() + (offset.y() + row) * imageRect.width())],
                   sourceRect.width() * componentsPerPixel * bytesPerComponent);
    }
    GLC(context, context->unmapTexSubImage2DCHROMIUM(pixelDest));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
