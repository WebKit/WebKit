/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Igalia S.L.
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
#include "BitmapTextureImageBuffer.h"

#include "GraphicsLayer.h"

namespace WebCore {

void BitmapTextureImageBuffer::updateContents(const void* data, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine, UpdateContentsFlag)
{
#if PLATFORM(CAIRO)
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(data()),
        CAIRO_FORMAT_ARGB32, targetRect.width(), targetRect.height(), bytesPerLine));
    m_image->context()->platformContext()->drawSurfaceToContext(surface.get(), targetRect,
        IntRect(sourceOffset, targetRect.size()), m_image->context());
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(targetRect);
    UNUSED_PARAM(sourceOffset);
    UNUSED_PARAM(bytesPerLine);
#endif
}

void BitmapTextureImageBuffer::updateContents(TextureMapper*, GraphicsLayer* sourceLayer, const IntRect& targetRect, const IntPoint& sourceOffset, UpdateContentsFlag)
{
    GraphicsContext* context = m_image->context();

    context->clearRect(targetRect);

    IntRect sourceRect(targetRect);
    sourceRect.setLocation(sourceOffset);
    context->save();
    context->clip(targetRect);
    context->translate(targetRect.x() - sourceOffset.x(), targetRect.y() - sourceOffset.y());
    sourceLayer->paintGraphicsLayerContents(*context, sourceRect);
    context->restore();
}

void BitmapTextureImageBuffer::didReset()
{
    m_image = ImageBuffer::create(contentSize());
}

void BitmapTextureImageBuffer::updateContents(Image* image, const IntRect& targetRect, const IntPoint& offset, UpdateContentsFlag)
{
    m_image->context()->drawImage(image, ColorSpaceDeviceRGB, targetRect, IntRect(offset, targetRect.size()), CompositeCopy);
}

} // namespace WebCore
