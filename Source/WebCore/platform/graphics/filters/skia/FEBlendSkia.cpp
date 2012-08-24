/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#if ENABLE(FILTERS)
#include "FEBlend.h"

#include "NativeImageSkia.h"
#include "SkBitmapSource.h"
#include "SkBlendImageFilter.h"

namespace WebCore {

static SkBlendImageFilter::Mode toSkiaMode(BlendModeType mode)
{
    switch (mode) {
    case FEBLEND_MODE_NORMAL:
        return SkBlendImageFilter::kNormal_Mode;
    case FEBLEND_MODE_MULTIPLY:
        return SkBlendImageFilter::kMultiply_Mode;
    case FEBLEND_MODE_SCREEN:
        return SkBlendImageFilter::kScreen_Mode;
    case FEBLEND_MODE_DARKEN:
        return SkBlendImageFilter::kDarken_Mode;
    case FEBLEND_MODE_LIGHTEN:
        return SkBlendImageFilter::kLighten_Mode;
    default:
        return SkBlendImageFilter::kNormal_Mode;
    }
}

bool FEBlend::platformApplySkia()
{
    // For now, only use the skia implementation for accelerated rendering.
    if (filter()->renderingMode() != Accelerated)
        return false;

    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);

    if (!in || !in2) 
        return false;

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    RefPtr<Image> foreground = in->asImageBuffer()->copyImage(DontCopyBackingStore);
    RefPtr<Image> background = in2->asImageBuffer()->copyImage(DontCopyBackingStore);

    SkBitmap foregroundBitmap = foreground->nativeImageForCurrentFrame()->bitmap();
    SkBitmap backgroundBitmap = background->nativeImageForCurrentFrame()->bitmap();

    SkAutoTUnref<SkImageFilter> backgroundSource(new SkBitmapSource(backgroundBitmap));
    SkBlendImageFilter::Mode mode = toSkiaMode(m_mode);
    SkAutoTUnref<SkImageFilter> blend(new SkBlendImageFilter(mode, backgroundSource));
    SkPaint paint;
    paint.setImageFilter(blend);
    SkCanvas* canvas = resultImage->context()->platformContext()->canvas();
    canvas->drawBitmap(foregroundBitmap, 0, 0, &paint);
    return true;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
