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
#include "cc/CCGraphicsContext.h"

namespace WebCore {

LayerTextureSubImage::LayerTextureSubImage(bool useMapTexSubImage)
    : m_useMapTexSubImage(useMapTexSubImage)
{
}

LayerTextureSubImage::~LayerTextureSubImage()
{
}

void LayerTextureSubImage::setSubImageSize(const IntSize& subImageSize)
{
    if (subImageSize == m_subImageSize)
        return;

    m_subImageSize = subImageSize;
    m_subImage.clear();
}

void LayerTextureSubImage::upload(const uint8_t* image, const IntRect& imageRect,
                                  const IntRect& sourceRect, const IntRect& destRect,
                                  GC3Denum format, CCGraphicsContext* context)
{
    if (m_useMapTexSubImage)
        uploadWithMapTexSubImage(image, imageRect, sourceRect, destRect, format, context);
    else
        uploadWithTexSubImage(image, imageRect, sourceRect, destRect, format, context);
}

void LayerTextureSubImage::uploadWithTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                                 const IntRect& sourceRect, const IntRect& destRect,
                                                 GC3Denum format, CCGraphicsContext* context)
{
    TRACE_EVENT0("cc", "LayerTextureSubImage::uploadWithTexSubImage");
    if (!m_subImage)
        m_subImage = adoptArrayPtr(new uint8_t[m_subImageSize.width() * m_subImageSize.height() * 4]);

    // Offset from image-rect to source-rect.
    IntPoint offset(sourceRect.x() - imageRect.x(), sourceRect.y() - imageRect.y());

    const uint8_t* pixelSource;
    if (imageRect.width() == sourceRect.width() && !offset.x())
        pixelSource = &image[4 * offset.y() * imageRect.width()];
    else {
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into a temp buffer for uploading.
        for (int row = 0; row < destRect.height(); ++row)
            memcpy(&m_subImage[destRect.width() * 4 * row],
                   &image[4 * (offset.x() + (offset.y() + row) * imageRect.width())],
                   destRect.width() * 4);

        pixelSource = &m_subImage[0];
    }

    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    GLC(context3d, context3d->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, destRect.x(), destRect.y(), destRect.width(), destRect.height(), format, GraphicsContext3D::UNSIGNED_BYTE, pixelSource));
}

void LayerTextureSubImage::uploadWithMapTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                                    const IntRect& sourceRect, const IntRect& destRect,
                                                    GC3Denum format, CCGraphicsContext* context)
{
    TRACE_EVENT0("cc", "LayerTextureSubImage::uploadWithMapTexSubImage");
    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }

    // Offset from image-rect to source-rect.
    IntPoint offset(sourceRect.x() - imageRect.x(), sourceRect.y() - imageRect.y());

    // Upload tile data via a mapped transfer buffer
    uint8_t* pixelDest = static_cast<uint8_t*>(context3d->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, destRect.x(), destRect.y(), destRect.width(), destRect.height(), format, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY));

    if (!pixelDest) {
        uploadWithTexSubImage(image, imageRect, sourceRect, destRect, format, context);
        return;
    }

    unsigned int componentsPerPixel;
    unsigned int bytesPerComponent;
    if (!GraphicsContext3D::computeFormatAndTypeParameters(format, GraphicsContext3D::UNSIGNED_BYTE, &componentsPerPixel, &bytesPerComponent)) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (imageRect.width() == sourceRect.width() && !offset.x())
        memcpy(pixelDest, &image[offset.y() * imageRect.width() * componentsPerPixel * bytesPerComponent], imageRect.width() * destRect.height() * componentsPerPixel * bytesPerComponent);
    else {
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into the pixelDest
        for (int row = 0; row < destRect.height(); ++row)
            memcpy(&pixelDest[destRect.width() * row * componentsPerPixel * bytesPerComponent],
                   &image[4 * (offset.x() + (offset.y() + row) * imageRect.width())],
                   destRect.width() * componentsPerPixel * bytesPerComponent);
    }
    GLC(context3d, context3d->unmapTexSubImage2DCHROMIUM(pixelDest));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
