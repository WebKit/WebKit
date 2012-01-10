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
#if USE(SKIA)
#include "FEGaussianBlur.h"

#include "BitmapImageSingleFrameSkia.h"
#include "SkBlurImageFilter.h"

namespace WebCore {

bool FEGaussianBlur::platformApplySkia()
{
    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    FilterEffect* in = inputEffect(0);

    IntRect drawingRegion = drawingRegionOfInputImage(in->absolutePaintRect());

    setIsAlphaImage(in->isAlphaImage());

    float stdX = filter()->applyHorizontalScale(m_stdX);
    float stdY = filter()->applyVerticalScale(m_stdY);
    
    RefPtr<Image> image = in->asImageBuffer()->copyImage(DontCopyBackingStore);

    SkPaint paint;
    GraphicsContext* dstContext = resultImage->context();
    SkCanvas* canvas = dstContext->platformContext()->canvas();
    paint.setImageFilter(new SkBlurImageFilter(stdX, stdY))->unref();
    canvas->saveLayer(0, &paint);
    paint.setColor(0xFFFFFFFF);
    dstContext->drawImage(image.get(), ColorSpaceDeviceRGB, drawingRegion.location(), CompositeCopy);
    canvas->restore();
    return true;
}

};
#endif
