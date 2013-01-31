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

#include "SkiaImageFilterBuilder.h"

#include "DropShadowImageFilter.h"
#include "FilterEffect.h"
#include "FilterOperations.h"
#include "SkBlurImageFilter.h"
#include "SkColorFilterImageFilter.h"
#include "SkColorMatrixFilter.h"
#include "SkMatrix.h"

namespace {

void getBrightnessMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = amount;
    matrix[18] = 1;
}

void getContrastMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = amount;
    matrix[4] = matrix[9] = matrix[14] = (-0.5f * amount + 0.5f) * 255;
    matrix[18] = 1;
}

void getSaturateMatrix(float amount, SkScalar matrix[20])
{
    // Note, these values are computed to ensure matrixNeedsClamping is false
    // for amount in [0..1]
    matrix[0] = 0.213f + 0.787f * amount;
    matrix[1] = 0.715f - 0.715f * amount;
    matrix[2] = 1.f - (matrix[0] + matrix[1]);
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - 0.213f * amount;
    matrix[6] = 0.715f + 0.285f * amount;
    matrix[7] = 1.f - (matrix[5] + matrix[6]);
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - 0.213f * amount;
    matrix[11] = 0.715f - 0.715f * amount;
    matrix[12] = 1.f - (matrix[10] + matrix[11]);
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

void getHueRotateMatrix(float hue, SkScalar matrix[20])
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

void getInvertMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = 1 - 2 * amount;
    matrix[4] = matrix[9] = matrix[14] = amount * 255;
    matrix[18] = 1;
}

void getOpacityMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = 1;
    matrix[18] = amount;
}

void getGrayscaleMatrix(float amount, SkScalar matrix[20])
{
    // Note, these values are computed to ensure matrixNeedsClamping is false
    // for amount in [0..1]
    matrix[0] = 0.2126f + 0.7874f * amount;
    matrix[1] = 0.7152f - 0.7152f * amount;
    matrix[2] = 1.f - (matrix[0] + matrix[1]);
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.2126f - 0.2126f * amount;
    matrix[6] = 0.7152f + 0.2848f * amount;
    matrix[7] = 1.f - (matrix[5] + matrix[6]);
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.2126f - 0.2126f * amount;
    matrix[11] = 0.7152f - 0.7152f * amount;
    matrix[12] = 1.f - (matrix[10] + matrix[11]);
    matrix[13] = matrix[14] = 0;

    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

void getSepiaMatrix(float amount, SkScalar matrix[20])
{
    matrix[0] = 0.393f + 0.607f * amount;
    matrix[1] = 0.769f - 0.769f * amount;
    matrix[2] = 0.189f - 0.189f * amount;
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.349f - 0.349f * amount;
    matrix[6] = 0.686f + 0.314f * amount;
    matrix[7] = 0.168f - 0.168f * amount;
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.272f - 0.272f * amount;
    matrix[11] = 0.534f - 0.534f * amount;
    matrix[12] = 0.131f + 0.869f * amount;
    matrix[13] = matrix[14] = 0;

    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

SkImageFilter* createMatrixImageFilter(SkScalar matrix[20], SkImageFilter* input)
{
    SkAutoTUnref<SkColorFilter> colorFilter(new SkColorMatrixFilter(matrix));
    return SkColorFilterImageFilter::Create(colorFilter, input);
}

};

namespace WebCore {

SkiaImageFilterBuilder::SkiaImageFilterBuilder()
{
}

SkImageFilter* SkiaImageFilterBuilder::build(FilterEffect* effect)
{
    if (!effect)
        return 0;

    FilterBuilderHashMap::iterator it = m_map.find(effect);
    if (it != m_map.end())
        return it->value;

    SkImageFilter* filter = effect->createImageFilter(this);
    m_map.set(effect, filter);
    return filter;
}

SkImageFilter* SkiaImageFilterBuilder::build(const FilterOperations& operations)
{
    SkImageFilter* filter = 0;
    SkScalar matrix[20];
    for (size_t i = 0; i < operations.size(); ++i) {
        const FilterOperation& op = *operations.at(i);
        switch (op.getOperationType()) {
        case FilterOperation::REFERENCE: {
            FilterEffect* filterEffect = static_cast<const ReferenceFilterOperation*>(&op)->filterEffect();
            // FIXME: hook up parent filter to image source
            filter = SkiaImageFilterBuilder::build(filterEffect);
            break;
        }
        case FilterOperation::GRAYSCALE: {
            float amount = static_cast<const BasicColorMatrixFilterOperation*>(&op)->amount();
            getGrayscaleMatrix(1 - amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::SEPIA: {
            float amount = static_cast<const BasicColorMatrixFilterOperation*>(&op)->amount();
            getSepiaMatrix(1 - amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::SATURATE: {
            float amount = static_cast<const BasicColorMatrixFilterOperation*>(&op)->amount();
            getSaturateMatrix(amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            float amount = static_cast<const BasicColorMatrixFilterOperation*>(&op)->amount();
            getHueRotateMatrix(amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::INVERT: {
            float amount = static_cast<const BasicComponentTransferFilterOperation*>(&op)->amount();
            getInvertMatrix(amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::OPACITY: {
            float amount = static_cast<const BasicComponentTransferFilterOperation*>(&op)->amount();
            getOpacityMatrix(amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            float amount = static_cast<const BasicComponentTransferFilterOperation*>(&op)->amount();
            getBrightnessMatrix(amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::CONTRAST: {
            float amount = static_cast<const BasicComponentTransferFilterOperation*>(&op)->amount();
            getContrastMatrix(amount, matrix);
            filter = createMatrixImageFilter(matrix, filter);
            break;
        }
        case FilterOperation::BLUR: {
            float pixelRadius = static_cast<const BlurFilterOperation*>(&op)->stdDeviation().getFloatValue();
            filter = new SkBlurImageFilter(pixelRadius, pixelRadius, filter);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation* drop = static_cast<const DropShadowFilterOperation*>(&op);
            filter = new DropShadowImageFilter(SkIntToScalar(drop->x()), SkIntToScalar(drop->y()), SkIntToScalar(drop->stdDeviation()), drop->color().rgb(), filter);
            break;
        }
#if ENABLE(CSS_SHADERS)
        case FilterOperation::VALIDATED_CUSTOM:
        case FilterOperation::CUSTOM:
            // Not supported.
#endif
        case FilterOperation::PASSTHROUGH:
        case FilterOperation::NONE:
            break;
        }
    }
    return filter;
}

};
