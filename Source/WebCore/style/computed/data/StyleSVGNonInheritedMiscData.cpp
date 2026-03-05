/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2026 Samuel Weinig <sam@webkit.org>

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "StyleSVGNonInheritedMiscData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGNonInheritedMiscData);

SVGNonInheritedMiscData::SVGNonInheritedMiscData()
    : floodOpacity(ComputedStyle::initialFloodOpacity())
    , floodColor(ComputedStyle::initialFloodColor())
    , lightingColor(ComputedStyle::initialLightingColor())
    , baselineShift(ComputedStyle::initialBaselineShift())
{
}

inline SVGNonInheritedMiscData::SVGNonInheritedMiscData(const SVGNonInheritedMiscData& other)
    : RefCounted<SVGNonInheritedMiscData>()
    , floodOpacity(other.floodOpacity)
    , floodColor(other.floodColor)
    , lightingColor(other.lightingColor)
    , baselineShift(other.baselineShift)
{
}

Ref<SVGNonInheritedMiscData> SVGNonInheritedMiscData::copy() const
{
    return adoptRef(*new SVGNonInheritedMiscData(*this));
}

bool SVGNonInheritedMiscData::operator==(const SVGNonInheritedMiscData& other) const
{
    return floodOpacity == other.floodOpacity
        && floodColor == other.floodColor
        && lightingColor == other.lightingColor
        && baselineShift == other.baselineShift;
}

#if !LOG_DISABLED
void SVGNonInheritedMiscData::dumpDifferences(TextStream& ts, const SVGNonInheritedMiscData& other) const
{
    LOG_IF_DIFFERENT(floodOpacity);
    LOG_IF_DIFFERENT(floodColor);
    LOG_IF_DIFFERENT(lightingColor);
    LOG_IF_DIFFERENT(baselineShift);
}
#endif

TextStream& operator<<(TextStream& ts, const SVGNonInheritedMiscData& data)
{
    ts.dumpProperty("flood-opacity"_s, data.floodOpacity);
    ts.dumpProperty("flood-color"_s, data.floodColor);
    ts.dumpProperty("lighting-color"_s, data.lightingColor);
    ts.dumpProperty("baseline-shift"_s, data.baselineShift);
    return ts;
}

} // namespace Style
} // namespace WebCore
