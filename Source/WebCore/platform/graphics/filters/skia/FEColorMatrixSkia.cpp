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
#if ENABLE(FILTERS) && USE(SKIA)
#include "FEColorMatrix.h"

#include "NativeImageSkia.h"
#include "SkColorMatrixFilter.h"

namespace WebCore {

static void saturateMatrix(float s, SkScalar matrix[20])
{
    matrix[0] = 0.213f + 0.787f * s;
    matrix[1] = 0.715f - 0.715f * s;
    matrix[2] = 0.072f - 0.072f * s;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - 0.213f * s;
    matrix[6] = 0.715f + 0.285f * s;
    matrix[7] = 0.072f - 0.072f * s;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - 0.213f * s;
    matrix[11] = 0.715f - 0.715f * s;
    matrix[12] = 0.072f + 0.928f * s;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = 0;
    matrix[18] = 1;
    matrix[19] = 0;
}

static void hueRotateMatrix(float hue, SkScalar matrix[20])
{
    float cosHue = cosf(hue * piFloat / 180); 
    float sinHue = sinf(hue * piFloat / 180); 
    matrix[0] = 0.213f + cosHue * 0.787f - sinHue * 0.213f;
    matrix[1] = 0.715f - cosHue * 0.715f - sinHue * 0.715f;
    matrix[2] = 0.072f - cosHue * 0.072f + sinHue * 0.928f;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - cosHue * 0.213f + sinHue * 0.143f;
    matrix[6] = 0.715f + cosHue * 0.285f + sinHue * 0.140f;
    matrix[7] = 0.072f - cosHue * 0.072f - sinHue * 0.283f;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - cosHue * 0.213f - sinHue * 0.787f;
    matrix[11] = 0.715f - cosHue * 0.715f + sinHue * 0.715f;
    matrix[12] = 0.072f + cosHue * 0.928f + sinHue * 0.072f;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = 0;
    matrix[18] = 1;
    matrix[19] = 0;
}

static void luminanceToAlphaMatrix(SkScalar matrix[20])
{
    memset(matrix, 0, sizeof(matrix));
    matrix[15] = 0.2125f;
    matrix[16] = 0.7154f;
    matrix[17] = 0.0721f;
}

bool FEColorMatrix::platformApplySkia()
{
    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    FilterEffect* in = inputEffect(0);

    IntRect imageRect(IntPoint(), absolutePaintRect().size());

    SkScalar matrix[20];

    switch (m_type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        for (int i = 0; i < 20; ++i)
            matrix[i] = m_values[i];

        matrix[4] *= SkScalar(255);
        matrix[9] *= SkScalar(255);
        matrix[14] *= SkScalar(255);
        matrix[19] *= SkScalar(255);
        break;
    case FECOLORMATRIX_TYPE_SATURATE: 
        saturateMatrix(m_values[0], matrix);
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        hueRotateMatrix(m_values[0], matrix);
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        luminanceToAlphaMatrix(matrix);
        break;
    }

    RefPtr<Image> image = in->asImageBuffer()->copyImage(DontCopyBackingStore);
    NativeImageSkia* nativeImage = image->nativeImageForCurrentFrame();
    if (!nativeImage)
        return false;

    SkCanvas* canvas = resultImage->context()->platformContext()->canvas();
    SkPaint paint;
    paint.setColorFilter(new SkColorMatrixFilter(matrix))->unref();
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    canvas->drawBitmap(nativeImage->bitmap(), 0, 0, &paint);
    return true;
}

} // namespace WebCore

#endif // ENABLE(FILTERS) && USE(SKIA)
