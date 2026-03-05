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
#include "StyleSVGMarkerResourceData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGMarkerResourceData);

SVGMarkerResourceData::SVGMarkerResourceData()
    : markerStart(ComputedStyle::initialMarkerStart())
    , markerMid(ComputedStyle::initialMarkerMid())
    , markerEnd(ComputedStyle::initialMarkerEnd())
{
}

inline SVGMarkerResourceData::SVGMarkerResourceData(const SVGMarkerResourceData& other)
    : RefCounted<SVGMarkerResourceData>()
    , markerStart(other.markerStart)
    , markerMid(other.markerMid)
    , markerEnd(other.markerEnd)
{
}

Ref<SVGMarkerResourceData> SVGMarkerResourceData::copy() const
{
    return adoptRef(*new SVGMarkerResourceData(*this));
}

bool SVGMarkerResourceData::operator==(const SVGMarkerResourceData& other) const
{
    return markerStart == other.markerStart
        && markerMid == other.markerMid
        && markerEnd == other.markerEnd;
}

#if !LOG_DISABLED
void SVGMarkerResourceData::dumpDifferences(TextStream& ts, const SVGMarkerResourceData& other) const
{
    LOG_IF_DIFFERENT(markerStart);
    LOG_IF_DIFFERENT(markerMid);
    LOG_IF_DIFFERENT(markerEnd);
}
#endif

TextStream& operator<<(TextStream& ts, const SVGMarkerResourceData& data)
{
    ts.dumpProperty("marker-start"_s, data.markerStart);
    ts.dumpProperty("marker-mid"_s, data.markerMid);
    ts.dumpProperty("marker-end"_s, data.markerEnd);
    return ts;
}

} // namespace Style
} // namespace WebCore
