/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FEColorMatrixSkiaApplier.h"

#if USE(SKIA)

#include "FEColorMatrix.h"
#include "FilterImage.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorFilter.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEColorMatrixSkiaApplier);

bool FEColorMatrixSkiaApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 1);
    auto& input = inputs[0].get();

    RefPtr resultImage = result.imageBuffer();
    RefPtr sourceImage = input.imageBuffer();
    if (!resultImage || !sourceImage)
        return false;

    auto nativeImage = sourceImage->createNativeImageReference();
    if (!nativeImage || !nativeImage->platformImage())
        return false;

    auto values = FEColorMatrix::normalizedFloats(m_effect.values());
    Vector<float> matrix;
    float components[9];

    switch (m_effect.type()) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
        matrix = values;
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
        FEColorMatrix::calculateSaturateComponents(components, values[0]);
        matrix = Vector<float>({
            components[0], components[1], components[2], 0.0, 0.0,
            components[3], components[4], components[5], 0.0, 0.0,
            components[6], components[7], components[8], 0.0, 0.0,
            0.0, 0.0, 0.0, 1.0, 0.0,
        });
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        FEColorMatrix::calculateHueRotateComponents(components, values[0]);
        matrix = Vector<float>({
            components[0], components[1], components[2], 0.0, 0.0,
            components[3], components[4], components[5], 0.0, 0.0,
            components[6], components[7], components[8], 0.0, 0.0,
            0.0, 0.0, 0.0, 1.0, 0.0,
        });
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        matrix = Vector<float>({
            0.0,    0.0,    0.0,    0.0, 0.0,
            0.0,    0.0,    0.0,    0.0, 0.0,
            0.0,    0.0,    0.0,    0.0, 0.0,
            0.2125, 0.7154, 0.0721, 0.0, 0.0,
        });
        break;

    case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
        return false;
    }

    SkPaint paint;
    paint.setColorFilter(SkColorFilters::Matrix(matrix.data()));

    auto inputOffsetWithinResult = input.absoluteImageRectRelativeTo(result).location();
    resultImage->context().platformContext()->drawImage(nativeImage->platformImage(), inputOffsetWithinResult.x(), inputOffsetWithinResult.y(), { }, &paint);
    return true;
}

} // namespace WebCore

#endif // USE(SKIA)
