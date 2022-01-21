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

Ref<FEOffset> FEOffset::create(float dx, float dy)
{
    return adoptRef(*new FEOffset(dx, dy));
}

FEOffset::FEOffset(float dx, float dy)
    : FilterEffect(FilterEffect::Type::FEOffset)
    , m_dx(dx)
    , m_dy(dy)
{
}

void FEOffset::setDx(float dx)
{
    m_dx = dx;
}

void FEOffset::setDy(float dy)
{
    m_dy = dy;
}

FloatRect FEOffset::calculateImageRect(const Filter& filter, const FilterImageVector& inputs, const FloatRect& primitiveSubregion) const
{
    auto imageRect = inputs[0]->imageRect();
    imageRect.move(filter.resolvedSize({ m_dx, m_dy }));
    return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
}

IntOutsets FEOffset::outsets(const Filter& filter) const
{
    auto offset = expandedIntSize(filter.resolvedSize({ m_dx, m_dy }));

    IntOutsets outsets;
    if (offset.height() < 0)
        outsets.setTop(-offset.height());
    else
        outsets.setBottom(offset.height());
    if (offset.width() < 0)
        outsets.setLeft(-offset.width());
    else
        outsets.setRight(offset.width());

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
