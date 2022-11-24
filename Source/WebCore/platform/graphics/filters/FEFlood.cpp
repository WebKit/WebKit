/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "FEFlood.h"

#include "ColorSerialization.h"
#include "FEFloodSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEFlood> FEFlood::create(const Color& floodColor, float floodOpacity)
{
    return adoptRef(*new FEFlood(floodColor, floodOpacity));
}

FEFlood::FEFlood(const Color& floodColor, float floodOpacity)
    : FilterEffect(FilterEffect::Type::FEFlood)
    , m_floodColor(floodColor)
    , m_floodOpacity(floodOpacity)
{
}

bool FEFlood::setFloodColor(const Color& color)
{
    if (m_floodColor == color)
        return false;
    m_floodColor = color;
    return true;
}

bool FEFlood::setFloodOpacity(float floodOpacity)
{
    if (m_floodOpacity == floodOpacity)
        return false;
    m_floodOpacity = floodOpacity;
    return true;
}

FloatRect FEFlood::calculateImageRect(const Filter& filter, Span<const FloatRect>, const FloatRect& primitiveSubregion) const
{
    return filter.maxEffectRect(primitiveSubregion);
}

std::unique_ptr<FilterEffectApplier> FEFlood::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEFloodSoftwareApplier>(*this);
}

TextStream& FEFlood::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feFlood";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " flood-color=\"" << serializationForRenderTreeAsText(floodColor()) << "\"";
    ts << " flood-opacity=\"" << floodOpacity() << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
