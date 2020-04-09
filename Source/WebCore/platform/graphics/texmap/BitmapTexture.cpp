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
#include "BitmapTexture.h"

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "ImageBuffer.h"
#include "TextureMapper.h"

namespace WebCore {

void BitmapTexture::updateContents(TextureMapper&, GraphicsLayer* sourceLayer, const IntRect& targetRect, const IntPoint& offset, float scale)
{
    // Making an unconditionally unaccelerated buffer here is OK because this code
    // isn't used by any platforms that respect the accelerated bit.
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(targetRect.size(), RenderingMode::Unaccelerated);

    if (!imageBuffer)
        return;

    GraphicsContext& context = imageBuffer->context();
    context.setImageInterpolationQuality(InterpolationQuality::Default);
    context.setTextDrawingMode(TextDrawingMode::Fill);

    IntRect sourceRect(targetRect);
    sourceRect.setLocation(offset);
    sourceRect.scale(1 / scale);
    context.applyDeviceScaleFactor(scale);
    context.translate(-sourceRect.x(), -sourceRect.y());

    sourceLayer->paintGraphicsLayerContents(context, sourceRect);

#if USE(DIRECT2D)
    // We can't access the bits in the image buffer with an active beginDraw.
    context.endDraw();
#endif

    RefPtr<Image> image = imageBuffer->copyImage(DontCopyBackingStore);
    if (!image)
        return;

    updateContents(image.get(), targetRect, IntPoint());
}

} // namespace
