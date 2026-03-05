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
#include "StyleSVGLayoutData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGLayoutData);

SVGLayoutData::SVGLayoutData()
    : cx(ComputedStyle::initialCx())
    , cy(ComputedStyle::initialCy())
    , r(ComputedStyle::initialR())
    , rx(ComputedStyle::initialRx())
    , ry(ComputedStyle::initialRy())
    , x(ComputedStyle::initialX())
    , y(ComputedStyle::initialY())
    , d(ComputedStyle::initialD())
{
}

inline SVGLayoutData::SVGLayoutData(const SVGLayoutData& other)
    : RefCounted<SVGLayoutData>()
    , cx(other.cx)
    , cy(other.cy)
    , r(other.r)
    , rx(other.rx)
    , ry(other.ry)
    , x(other.x)
    , y(other.y)
    , d(other.d)
{
}

Ref<SVGLayoutData> SVGLayoutData::copy() const
{
    return adoptRef(*new SVGLayoutData(*this));
}

bool SVGLayoutData::operator==(const SVGLayoutData& other) const
{
    return cx == other.cx
        && cy == other.cy
        && r == other.r
        && rx == other.rx
        && ry == other.ry
        && x == other.x
        && y == other.y
        && d == other.d;
}

#if !LOG_DISABLED
void SVGLayoutData::dumpDifferences(TextStream& ts, const SVGLayoutData& other) const
{
    LOG_IF_DIFFERENT(cx);
    LOG_IF_DIFFERENT(cy);
    LOG_IF_DIFFERENT(r);
    LOG_IF_DIFFERENT(rx);
    LOG_IF_DIFFERENT(ry);
    LOG_IF_DIFFERENT(x);
    LOG_IF_DIFFERENT(y);
    LOG_IF_DIFFERENT(d);
}
#endif

TextStream& operator<<(TextStream& ts, const SVGLayoutData& data)
{
    ts.dumpProperty("cx"_s, data.cx);
    ts.dumpProperty("cy"_s, data.cy);
    ts.dumpProperty("r"_s, data.r);
    ts.dumpProperty("rx"_s, data.rx);
    ts.dumpProperty("ry"_s, data.ry);
    ts.dumpProperty("x"_s, data.x);
    ts.dumpProperty("y"_s, data.y);
    ts.dumpProperty("d"_s, data.d);
    return ts;
}

} // namespace Style
} // namespace WebCore
