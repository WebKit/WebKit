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

bool FEDropShadowSoftwareApplier::apply(const Filter& filter, const FilterEffectVector& inputEffects)
{
    FilterEffect* in = inputEffects[0].get();

    auto resultImage = m_effect.imageBufferResult();
    if (!resultImage)
        return false;

    FloatSize blurRadius = 2 * filter.scaledByFilterScale({ m_effect.stdDeviationX(), m_effect.stdDeviationY() });
    FloatSize offset = filter.scaledByFilterScale({ m_effect.dx(), m_effect.dy() });

    FloatRect drawingRegion = m_effect.drawingRegionOfInputImage(in->absolutePaintRect());
    FloatRect drawingRegionWithOffset(drawingRegion);
    drawingRegionWithOffset.move(offset);

    auto sourceImage = in->imageBufferResult();
    if (!sourceImage)
        return false;

    GraphicsContext& resultContext = resultImage->context();
    resultContext.setAlpha(m_effect.shadowOpacity());
    resultContext.drawImageBuffer(*sourceImage, drawingRegionWithOffset);
    resultContext.setAlpha(1);

    ShadowBlur contextShadow(blurRadius, offset, m_effect.shadowColor());

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, m_effect.resultColorSpace() };
    IntRect shadowArea(IntPoint(), resultImage->truncatedLogicalSize());
    auto pixelBuffer = resultImage->getPixelBuffer(format, shadowArea);
    if (!pixelBuffer)
        return false;

    auto& sourcePixelArray = pixelBuffer->data();
    contextShadow.blurLayerImage(sourcePixelArray.data(), pixelBuffer->size(), 4 * pixelBuffer->size().width());
    
    resultImage->putPixelBuffer(*pixelBuffer, shadowArea);

    resultContext.setCompositeOperation(CompositeOperator::SourceIn);
    resultContext.fillRect(FloatRect(FloatPoint(), m_effect.absolutePaintRect().size()), m_effect.shadowColor());
    resultContext.setCompositeOperation(CompositeOperator::DestinationOver);

    resultImage->context().drawImageBuffer(*sourceImage, drawingRegion);
    return true;
}

} // namespace WebCore
