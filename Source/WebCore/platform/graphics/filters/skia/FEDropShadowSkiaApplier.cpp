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
#include "FEDropShadowSkiaApplier.h"

#if USE(SKIA)

#include "FEDropShadow.h"
#include "Filter.h"
#include "FilterImage.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include <skia/core/SkCanvas.h>
#include <skia/effects/SkImageFilters.h>

namespace WebCore {

bool FEDropShadowSkiaApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
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

    auto offset = filter.scaledByFilterScale(filter.resolvedSize({ m_effect.dx(), m_effect.dy() }));
    auto sigma = filter.scaledByFilterScale(filter.resolvedSize({ m_effect.stdDeviationX(), m_effect.stdDeviationY() }));

    SkPaint paint;

    auto shadowColorWithAlpha = m_effect.shadowColor().colorWithAlphaMultipliedBy(m_effect.shadowOpacity());
    paint.setImageFilter(SkImageFilters::DropShadow(offset.width(), offset.height(), sigma.width(), sigma.height(), shadowColorWithAlpha, nullptr));

    auto inputOffsetWithinResult = input.absoluteImageRectRelativeTo(result).location();
    resultImage->context().platformContext()->drawImage(nativeImage->platformImage(), inputOffsetWithinResult.x(), inputOffsetWithinResult.y(), { }, &paint);
    return true;
}

} // namespace WebCore

#endif // USE(SKIA)
