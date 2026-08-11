/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "SkiaCompositingLayerFilters.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "ColorMatrix.h"
#include "FilterOperations.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorFilter.h>
#include <skia/effects/SkImageFilters.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

namespace SkiaCompositingLayerFilters {

sk_sp<SkImageFilter> create(const FilterOperation& filterOperation, sk_sp<SkImageFilter> input, SkTileMode blurTileMode)
{
    switch (filterOperation.type()) {
    case FilterOperation::Type::Grayscale: {
        ColorMatrix<5, 4> matrix(grayscaleColorMatrix(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount()));
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Sepia: {
        ColorMatrix<5, 4> matrix(sepiaColorMatrix(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount()));
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Saturate: {
        ColorMatrix<5, 4> matrix(saturationColorMatrix(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount()));
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::HueRotate: {
        ColorMatrix<5, 4> matrix(hueRotateColorMatrix(downcast<BasicColorMatrixFilterOperation>(filterOperation).amount()));
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Invert: {
        const auto matrix = invertColorMatrix(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount());
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Opacity: {
        const auto matrix = opacityColorMatrix(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount());
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Brightness: {
        ColorMatrix<5, 4> matrix(brightnessColorMatrix(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount()));
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Contrast: {
        const auto matrix = contrastColorMatrix(downcast<BasicComponentTransferFilterOperation>(filterOperation).amount());
        return SkImageFilters::ColorFilter(SkColorFilters::Matrix(matrix.data().data()), input);
    }
    case FilterOperation::Type::Blur: {
        auto sigma = downcast<BlurFilterOperation>(filterOperation).stdDeviation();
        // FIXME: do we need to add crop rect?
        return SkImageFilters::Blur(sigma, sigma, blurTileMode, input);
    }
    case FilterOperation::Type::DropShadow: {
        auto& dropShadow = downcast<DropShadowFilterOperation>(filterOperation);
        return SkImageFilters::DropShadow(dropShadow.x(), dropShadow.y(), dropShadow.stdDeviation(), dropShadow.stdDeviation(), dropShadow.color(), input);
    }
    case FilterOperation::Type::Passthrough:
    case FilterOperation::Type::Default:
    case FilterOperation::Type::None:
        break;
    }

    return nullptr;
}

sk_sp<SkImageFilter> create(const FilterOperations& filterOperations, SkTileMode blurTileMode)
{
    sk_sp<SkImageFilter> filter;
    for (const auto& filterOperation : filterOperations)
        filter = create(filterOperation, filter, blurTileMode);
    return filter;
}

} // namespace SkiaCompositingLayerFilters
} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
