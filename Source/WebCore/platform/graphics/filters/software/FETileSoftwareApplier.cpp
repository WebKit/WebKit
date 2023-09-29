/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FETileSoftwareApplier.h"

#include "AffineTransform.h"
#include "FETile.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"

namespace WebCore {

bool FETileSoftwareApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    RefPtr resultImage = result.imageBuffer();
    RefPtr inputImage = input.imageBuffer();
    if (!resultImage || !inputImage)
        return false;

    auto inputImageRect = input.absoluteImageRect();
    auto resultImageRect = result.absoluteImageRect();

    auto tileRect = input.maxEffectRect(filter);
    tileRect.scale(filter.filterScale());

    auto maxResultRect = result.maxEffectRect(filter);
    maxResultRect.scale(filter.filterScale());

    auto tileImage = ImageBuffer::create(tileRect.size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, bufferOptionsForRendingMode(filter.renderingMode()));
    if (!tileImage)
        return false;

    auto& tileImageContext = tileImage->context();
    tileImageContext.translate(-tileRect.location());
    tileImageContext.drawImageBuffer(*inputImage, inputImageRect.location());

    AffineTransform patternTransform;
    patternTransform.translate(tileRect.location() - maxResultRect.location());

    auto pattern = Pattern::create({ tileImage.releaseNonNull() }, { true, true, patternTransform });

    auto& resultContext = resultImage->context();
    resultContext.setFillPattern(WTFMove(pattern));
    resultContext.fillRect(FloatRect(FloatPoint(), resultImageRect.size()));
    return true;
}

} // namespace WebCore
