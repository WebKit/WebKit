/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "FEDropShadowSoftwareApplier.h"

#include "FEDropShadow.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include "ShadowBlur.h"

namespace WebCore {

bool FEDropShadowSoftwareApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    RefPtr resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto stdDeviation = filter.resolvedSize({ m_effect.stdDeviationX(), m_effect.stdDeviationY() });
    auto blurRadius = 2 * filter.scaledByFilterScale(stdDeviation);
    
    auto offset = filter.resolvedSize({ m_effect.dx(), m_effect.dy() });
    auto absoluteOffset = filter.scaledByFilterScale(offset);

    FloatRect inputImageRect = input.absoluteImageRectRelativeTo(result);
    FloatRect inputImageRectWithOffset(inputImageRect);
    inputImageRectWithOffset.move(absoluteOffset);

    RefPtr inputImage = input.imageBuffer();
    if (!inputImage)
        return false;

    auto& resultContext = resultImage->context();
    resultContext.setAlpha(m_effect.shadowOpacity());
    resultContext.drawImageBuffer(*inputImage, inputImageRectWithOffset);
    resultContext.setAlpha(1);

    ShadowBlur contextShadow(blurRadius, absoluteOffset, m_effect.shadowColor());

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, result.colorSpace() };
    IntRect shadowArea(IntPoint(), resultImage->truncatedLogicalSize());
    auto pixelBuffer = resultImage->getPixelBuffer(format, shadowArea);
    if (!pixelBuffer)
        return false;

    contextShadow.blurLayerImage(pixelBuffer->bytes(), pixelBuffer->size(), 4 * pixelBuffer->size().width());

    resultImage->putPixelBuffer(*pixelBuffer, shadowArea);

    resultContext.setCompositeOperation(CompositeOperator::SourceIn);
    resultContext.fillRect(FloatRect(FloatPoint(), result.absoluteImageRect().size()), m_effect.shadowColor());
    resultContext.setCompositeOperation(CompositeOperator::DestinationOver);

    resultImage->context().drawImageBuffer(*inputImage, inputImageRect);
    return true;
}

} // namespace WebCore
