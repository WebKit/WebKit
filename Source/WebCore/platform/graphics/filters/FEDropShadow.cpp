/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "FEDropShadow.h"

#include "ColorSerialization.h"
#include "FEGaussianBlur.h"
#include "Filter.h"
#include "FilterEffectApplier.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include "ShadowBlur.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEDropShadow> FEDropShadow::create(float stdX, float stdY, float dx, float dy, const Color& shadowColor, float shadowOpacity)
{
    return adoptRef(*new FEDropShadow(stdX, stdY, dx, dy, shadowColor, shadowOpacity));
}

FEDropShadow::FEDropShadow(float stdX, float stdY, float dx, float dy, const Color& shadowColor, float shadowOpacity)
    : FilterEffect(FilterEffect::Type::FEDropShadow)
    , m_stdX(stdX)
    , m_stdY(stdY)
    , m_dx(dx)
    , m_dy(dy)
    , m_shadowColor(shadowColor)
    , m_shadowOpacity(shadowOpacity)
{
}

void FEDropShadow::determineAbsolutePaintRect(const Filter& filter)
{
    FloatRect absolutePaintRect = inputEffect(0)->absolutePaintRect();
    FloatRect absoluteOffsetPaintRect(absolutePaintRect);
    absoluteOffsetPaintRect.move(filter.scaledByFilterScale({ m_dx, m_dy }));
    absolutePaintRect.unite(absoluteOffsetPaintRect);

    IntSize kernelSize = FEGaussianBlur::calculateKernelSize(filter, { m_stdX, m_stdY });

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    absolutePaintRect.inflateX(3 * kernelSize.width() * 0.5f);
    absolutePaintRect.inflateY(3 * kernelSize.height() * 0.5f);

    if (clipsToBounds())
        absolutePaintRect.intersect(maxEffectRect());
    else
        absolutePaintRect.unite(maxEffectRect());

    setAbsolutePaintRect(enclosingIntRect(absolutePaintRect));
}

// FIXME: Move the class FEDropShadowSoftwareApplier to separate source and header files.
class FEDropShadowSoftwareApplier : public FilterEffectConcreteApplier<FEDropShadow> {
    using Base = FilterEffectConcreteApplier<FEDropShadow>;

public:
    using Base::Base;

    bool apply(const Filter&, const FilterEffectVector& inputEffects) override;
};

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

bool FEDropShadow::platformApplySoftware(const Filter& filter)
{
    return FEDropShadowSoftwareApplier(*this).apply(filter, inputEffects());
}
    
IntOutsets FEDropShadow::outsets() const
{
    IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ m_stdX, m_stdY });
    return {
        std::max<int>(0, outsetSize.height() - m_dy),
        std::max<int>(0, outsetSize.width() + m_dx),
        std::max<int>(0, outsetSize.height() + m_dy),
        std::max<int>(0, outsetSize.width() - m_dx)
    };
}

TextStream& FEDropShadow::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent <<"[feDropShadow";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\" dx=\"" << m_dx << "\" dy=\"" << m_dy << "\" flood-color=\"" << serializationForRenderTreeAsText(m_shadowColor) <<"\" flood-opacity=\"" << m_shadowOpacity << "]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}
    
} // namespace WebCore
