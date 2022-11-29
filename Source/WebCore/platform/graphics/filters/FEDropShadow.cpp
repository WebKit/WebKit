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

bool FEDropShadow::setStdDeviationX(float stdX)
{
    if (m_stdX == stdX)
        return false;
    m_stdX = stdX;
    return true;
}

bool FEDropShadow::setStdDeviationY(float stdY)
{
    if (m_stdY == stdY)
        return false;
    m_stdY = stdY;
    return true;
}

bool FEDropShadow::setDx(float dx)
{
    if (m_dx == dx)
        return false;
    m_dx = dx;
    return true;
}

bool FEDropShadow::setDy(float dy)
{
    if (m_dy == dy)
        return false;
    m_dy = dy;
    return true;
}

bool FEDropShadow::setShadowColor(const Color& shadowColor)
{
    if (m_shadowColor == shadowColor)
        return false;
    m_shadowColor = shadowColor;
    return true;
}

bool FEDropShadow::setShadowOpacity(float shadowOpacity)
{
    if (m_shadowOpacity == shadowOpacity)
        return false;
    m_shadowOpacity = shadowOpacity;
    return true;
}

FloatRect FEDropShadow::calculateImageRect(const Filter& filter, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const
{
    auto imageRect = inputImageRects[0];
    auto imageRectWithOffset(imageRect);
    imageRectWithOffset.move(filter.resolvedSize({ m_dx, m_dy }));
    imageRect.unite(imageRectWithOffset);

    auto kernelSize = FEGaussianBlur::calculateUnscaledKernelSize(filter.resolvedSize({ m_stdX, m_stdY }));

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    imageRect.inflateX(3 * kernelSize.width() * 0.5f);
    imageRect.inflateY(3 * kernelSize.height() * 0.5f);

    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

IntOutsets FEDropShadow::calculateOutsets(const FloatSize& offset, const FloatSize& stdDeviation)
{
    IntSize outsetSize = FEGaussianBlur::calculateOutsetSize(stdDeviation);

    int top = std::max<int>(0, outsetSize.height() - offset.height());
    int right = std::max<int>(0, outsetSize.width() + offset.width());
    int bottom = std::max<int>(0, outsetSize.height() + offset.height());
    int left = std::max<int>(0, outsetSize.width() - offset.width());

    return { top, right, bottom, left };
}

OptionSet<FilterRenderingMode> FEDropShadow::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if HAVE(CGSTYLE_CREATE_SHADOW2)
    if (m_stdX == m_stdY)
        modes.add(FilterRenderingMode::GraphicsContext);
#endif
    return modes;
}

std::optional<GraphicsStyle> FEDropShadow::createGraphicsStyle(const Filter& filter) const
{
    auto offset = filter.resolvedSize({ m_dx, m_dy });
    auto radius = FEGaussianBlur::calculateUnscaledKernelSize(filter.resolvedSize({ m_stdX, m_stdY }));
    auto color = m_shadowColor.colorWithAlpha(m_shadowOpacity);

    return GraphicsDropShadow { offset, radius, color };
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
