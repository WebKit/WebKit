/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "FEOffset.h"

#include "Filter.h"
#include "FEOffsetSoftwareApplier.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEOffset> FEOffset::create(float dx, float dy, DestinationColorSpace colorSpace)
{
    return adoptRef(*new FEOffset(dx, dy, colorSpace));
}

FEOffset::FEOffset(float dx, float dy, DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::FEOffset, colorSpace)
    , m_dx(dx)
    , m_dy(dy)
{
}

bool FEOffset::operator==(const FEOffset& other) const
{
    return FilterEffect::operator==(other)
        && m_dx == other.m_dx
        && m_dy == other.m_dy;
}

bool FEOffset::setDx(float dx)
{
    if (m_dx == dx)
        return false;
    m_dx = dx;
    return true;
}

bool FEOffset::setDy(float dy)
{
    if (m_dy == dy)
        return false;
    m_dy = dy;
    return true;
}

FloatRect FEOffset::calculateImageRect(const Filter& filter, std::span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const
{
    auto imageRect = inputImageRects[0];
    imageRect.move(filter.resolvedSize({ m_dx, m_dy }));
    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

IntOutsets FEOffset::calculateOutsets(const FloatSize& offset)
{
    auto adjustedOffset = expandedIntSize(offset);

    IntOutsets outsets;
    if (adjustedOffset.height() < 0)
        outsets.setTop(-adjustedOffset.height());
    else
        outsets.setBottom(adjustedOffset.height());
    if (adjustedOffset.width() < 0)
        outsets.setLeft(-adjustedOffset.width());
    else
        outsets.setRight(adjustedOffset.width());

    return outsets;
}

bool FEOffset::resultIsAlphaImage(const FilterImageVector& inputs) const
{
    return inputs[0]->isAlphaImage();
}

std::unique_ptr<FilterEffectApplier> FEOffset::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEOffsetSoftwareApplier>(*this);
}

TextStream& FEOffset::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feOffset";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " dx=\"" << dx() << "\" dy=\"" << dy() << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
