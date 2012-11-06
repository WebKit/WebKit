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
#include "FEComponentTransfer.h"

#include "NativeImageSkia.h"
#include "SkColorFilterImageFilter.h"
#include "SkTableColorFilter.h"
#include "SkiaImageFilterBuilder.h"

namespace WebCore {

bool FEComponentTransfer::platformApplySkia()
{
    FilterEffect* in = inputEffect(0);
    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    RefPtr<Image> image = in->asImageBuffer()->copyImage(DontCopyBackingStore);
    SkBitmap bitmap = image->nativeImageForCurrentFrame()->bitmap();

    unsigned char rValues[256], gValues[256], bValues[256], aValues[256];
    getValues(rValues, gValues, bValues, aValues);

    SkCanvas* canvas = resultImage->context()->platformContext()->canvas();
    SkPaint paint;
    paint.setColorFilter(SkTableColorFilter::CreateARGB(aValues, rValues, gValues, bValues))->unref();
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    canvas->drawBitmap(bitmap, 0, 0, &paint);

    return true;
}

SkImageFilter* FEComponentTransfer::createImageFilter(SkiaImageFilterBuilder* builder)
{
    SkImageFilter* input = builder->build(inputEffect(0));

    unsigned char rValues[256], gValues[256], bValues[256], aValues[256];
    getValues(rValues, gValues, bValues, aValues);

    SkAutoTUnref<SkColorFilter> colorFilter(SkTableColorFilter::CreateARGB(aValues, rValues, gValues, bValues));

    return SkColorFilterImageFilter::Create(colorFilter, input);
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
