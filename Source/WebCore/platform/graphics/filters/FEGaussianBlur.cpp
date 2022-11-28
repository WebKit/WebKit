/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015-2022 Apple, Inc. All rights reserved.
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
#include "FEGaussianBlur.h"

#include "FEGaussianBlurSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEGaussianBlur> FEGaussianBlur::create(float x, float y, EdgeModeType edgeMode)
{
    return adoptRef(*new FEGaussianBlur(x, y, edgeMode));
}

FEGaussianBlur::FEGaussianBlur(float x, float y, EdgeModeType edgeMode)
    : FilterEffect(FilterEffect::Type::FEGaussianBlur)
    , m_stdX(x)
    , m_stdY(y)
    , m_edgeMode(edgeMode)
{
}

bool FEGaussianBlur::setStdDeviationX(float stdX)
{
    if (m_stdX == stdX)
        return false;
    m_stdX = stdX;
    return true;
}

bool FEGaussianBlur::setStdDeviationY(float stdY)
{
    if (m_stdY == stdY)
        return false;
    m_stdY = stdY;
    return true;
}

bool FEGaussianBlur::setEdgeMode(EdgeModeType edgeMode)
{
    if (m_edgeMode == edgeMode)
        return false;
    m_edgeMode = edgeMode;
    return true;
}

static inline float gaussianKernelFactor()
{
    return 3 / 4.f * sqrtf(2 * piFloat);
}

static int clampedToKernelSize(float value)
{
    static constexpr unsigned maxKernelSize = 500;

    // Limit the kernel size to 500. A bigger radius won't make a big difference for the result image but
    // inflates the absolute paint rect too much. This is compatible with Firefox' behavior.
    unsigned size = std::max<unsigned>(2, static_cast<unsigned>(floorf(value * gaussianKernelFactor() + 0.5f)));
    return clampTo<int>(std::min(size, maxKernelSize));
}
    
IntSize FEGaussianBlur::calculateUnscaledKernelSize(FloatSize stdDeviation)
{
    ASSERT(stdDeviation.width() >= 0 && stdDeviation.height() >= 0);
    IntSize kernelSize;

    if (stdDeviation.width())
        kernelSize.setWidth(clampedToKernelSize(stdDeviation.width()));

    if (stdDeviation.height())
        kernelSize.setHeight(clampedToKernelSize(stdDeviation.height()));

    return kernelSize;
}

IntSize FEGaussianBlur::calculateKernelSize(const Filter& filter, FloatSize stdDeviation)
{
    stdDeviation = filter.resolvedSize(stdDeviation);
    return calculateUnscaledKernelSize(filter.scaledByFilterScale(stdDeviation));
}

IntSize FEGaussianBlur::calculateOutsetSize(FloatSize stdDeviation)
{
    auto kernelSize = calculateUnscaledKernelSize(stdDeviation);

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    return { 3 * kernelSize.width() / 2, 3 * kernelSize.height() / 2 };
}

FloatRect FEGaussianBlur::calculateImageRect(const Filter& filter, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const
{
    auto imageRect = inputImageRects[0];

    // Edge modes other than 'none' do not inflate the affected paint rect.
    if (m_edgeMode != EdgeModeType::None)
        return enclosingIntRect(imageRect);

    auto kernelSize = calculateUnscaledKernelSize(filter.resolvedSize({ m_stdX, m_stdY }));

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    imageRect.inflateX(3 * kernelSize.width() * 0.5f);
    imageRect.inflateY(3 * kernelSize.height() * 0.5f);

    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

IntOutsets FEGaussianBlur::calculateOutsets(const FloatSize& stdDeviation)
{
    IntSize outsetSize = calculateOutsetSize(stdDeviation);
    return { outsetSize.height(), outsetSize.width(), outsetSize.height(), outsetSize.width() };
}

bool FEGaussianBlur::resultIsAlphaImage(const FilterImageVector& inputs) const
{
    return inputs[0]->isAlphaImage();
}

std::unique_ptr<FilterEffectApplier> FEGaussianBlur::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEGaussianBlurSoftwareApplier>(*this);
}

TextStream& FEGaussianBlur::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feGaussianBlur";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
