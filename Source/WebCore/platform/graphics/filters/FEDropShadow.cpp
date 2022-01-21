/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
#include "FEDropShadowSoftwareApplier.h"
#include "FEGaussianBlur.h"
#include "Filter.h"
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

FloatRect FEDropShadow::calculateImageRect(const Filter& filter, const FilterImageVector& inputs, const FloatRect& primitiveSubregion) const
{
    auto imageRect = inputs[0]->imageRect();
    auto imageRectWithOffset(imageRect);
    imageRectWithOffset.move(filter.resolvedSize({ m_dx, m_dy }));
    imageRect.unite(imageRectWithOffset);

    auto kernelSize = FEGaussianBlur::calculateUnscaledKernelSize(filter.resolvedSize({ m_stdX, m_stdY }));

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    imageRect.inflateX(3 * kernelSize.width() * 0.5f);
    imageRect.inflateY(3 * kernelSize.height() * 0.5f);

    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

IntOutsets FEDropShadow::outsets(const Filter&) const
{
    IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ m_stdX, m_stdY });
    return {
        std::max<int>(0, outsetSize.height() - m_dy),
        std::max<int>(0, outsetSize.width() + m_dx),
        std::max<int>(0, outsetSize.height() + m_dy),
        std::max<int>(0, outsetSize.width() - m_dx)
    };
}

std::unique_ptr<FilterEffectApplier> FEDropShadow::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEDropShadowSoftwareApplier>(*this);
}
    
TextStream& FEDropShadow::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent <<"[feDropShadow";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\"";
    ts << " dx=\"" << m_dx << "\" dy=\"" << m_dy << "\"";
    ts << " flood-color=\"" << serializationForRenderTreeAsText(m_shadowColor) << "\"";
    ts << " flood-opacity=\"" << m_shadowOpacity << "\"";

    ts << "]\n";
    return ts;
}
    
} // namespace WebCore
