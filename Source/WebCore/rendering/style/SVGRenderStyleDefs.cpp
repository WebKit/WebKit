/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.

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
#include "SVGRenderStyleDefs.h"

#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "SVGRenderStyle.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleFillData);

StyleFillData::StyleFillData()
    : fillOpacity(RenderStyle::initialFillOpacity())
    , fill(RenderStyle::initialFill())
    , visitedLinkFill(RenderStyle::initialFill())
{
}

inline StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>()
    , fillOpacity(other.fillOpacity)
    , fill(other.fill)
    , visitedLinkFill(other.visitedLinkFill)
{
}

Ref<StyleFillData> StyleFillData::copy() const
{
    return adoptRef(*new StyleFillData(*this));
}

#if !LOG_DISABLED
void StyleFillData::dumpDifferences(TextStream& ts, const StyleFillData& other) const
{
    LOG_IF_DIFFERENT(fillOpacity);
    LOG_IF_DIFFERENT(fill);
    LOG_IF_DIFFERENT(visitedLinkFill);
}
#endif

bool StyleFillData::operator==(const StyleFillData& other) const
{
    return fillOpacity == other.fillOpacity
        && fill == other.fill
        && visitedLinkFill == other.visitedLinkFill;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleStrokeData);

StyleStrokeData::StyleStrokeData()
    : strokeOpacity(RenderStyle::initialStrokeOpacity())
    , stroke(RenderStyle::initialStroke())
    , visitedLinkStroke(RenderStyle::initialStroke())
    , strokeDashOffset(RenderStyle::initialStrokeDashOffset())
    , strokeDashArray(RenderStyle::initialStrokeDashArray())
{
}

inline StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>()
    , strokeOpacity(other.strokeOpacity)
    , stroke(other.stroke)
    , visitedLinkStroke(other.visitedLinkStroke)
    , strokeDashOffset(other.strokeDashOffset)
    , strokeDashArray(other.strokeDashArray)
{
}

Ref<StyleStrokeData> StyleStrokeData::copy() const
{
    return adoptRef(*new StyleStrokeData(*this));
}

bool StyleStrokeData::operator==(const StyleStrokeData& other) const
{
    return strokeOpacity == other.strokeOpacity
        && stroke == other.stroke
        && visitedLinkStroke == other.visitedLinkStroke
        && strokeDashOffset == other.strokeDashOffset
        && strokeDashArray == other.strokeDashArray;
}

#if !LOG_DISABLED
void StyleStrokeData::dumpDifferences(TextStream& ts, const StyleStrokeData& other) const
{
    LOG_IF_DIFFERENT(strokeOpacity);
    LOG_IF_DIFFERENT(stroke);
    LOG_IF_DIFFERENT(visitedLinkStroke);
    LOG_IF_DIFFERENT(strokeDashOffset);
    LOG_IF_DIFFERENT(strokeDashArray);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleStopData);

StyleStopData::StyleStopData()
    : stopOpacity(RenderStyle::initialStopOpacity())
    , stopColor(RenderStyle::initialStopColor())
{
}

inline StyleStopData::StyleStopData(const StyleStopData& other)
    : RefCounted<StyleStopData>()
    , stopOpacity(other.stopOpacity)
    , stopColor(other.stopColor)
{
}

Ref<StyleStopData> StyleStopData::copy() const
{
    return adoptRef(*new StyleStopData(*this));
}

bool StyleStopData::operator==(const StyleStopData& other) const
{
    return stopOpacity == other.stopOpacity
        && stopColor == other.stopColor;
}

#if !LOG_DISABLED
void StyleStopData::dumpDifferences(TextStream& ts, const StyleStopData& other) const
{
    LOG_IF_DIFFERENT(stopOpacity);
    LOG_IF_DIFFERENT(stopColor);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMiscData);

StyleMiscData::StyleMiscData()
    : floodOpacity(RenderStyle::initialFloodOpacity())
    , floodColor(RenderStyle::initialFloodColor())
    , lightingColor(RenderStyle::initialLightingColor())
    , baselineShift(RenderStyle::initialBaselineShift())
{
}

inline StyleMiscData::StyleMiscData(const StyleMiscData& other)
    : RefCounted<StyleMiscData>()
    , floodOpacity(other.floodOpacity)
    , floodColor(other.floodColor)
    , lightingColor(other.lightingColor)
    , baselineShift(other.baselineShift)
{
}

Ref<StyleMiscData> StyleMiscData::copy() const
{
    return adoptRef(*new StyleMiscData(*this));
}

bool StyleMiscData::operator==(const StyleMiscData& other) const
{
    return floodOpacity == other.floodOpacity
        && floodColor == other.floodColor
        && lightingColor == other.lightingColor
        && baselineShift == other.baselineShift;
}

#if !LOG_DISABLED
void StyleMiscData::dumpDifferences(TextStream& ts, const StyleMiscData& other) const
{
    LOG_IF_DIFFERENT(floodOpacity);
    LOG_IF_DIFFERENT(floodColor);
    LOG_IF_DIFFERENT(lightingColor);
    LOG_IF_DIFFERENT(baselineShift);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleShadowSVGData);

StyleShadowSVGData::StyleShadowSVGData()
    : shadow { RenderStyle::initialBoxShadow() }
{
}

inline StyleShadowSVGData::StyleShadowSVGData(const StyleShadowSVGData& other)
    : RefCounted<StyleShadowSVGData>()
    , shadow(other.shadow)
{
}

Ref<StyleShadowSVGData> StyleShadowSVGData::copy() const
{
    return adoptRef(*new StyleShadowSVGData(*this));
}

bool StyleShadowSVGData::operator==(const StyleShadowSVGData& other) const
{
    return shadow == other.shadow;
}

#if !LOG_DISABLED
void StyleShadowSVGData::dumpDifferences(TextStream& ts, const StyleShadowSVGData& other) const
{
    LOG_IF_DIFFERENT(shadow);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleInheritedResourceData);

StyleInheritedResourceData::StyleInheritedResourceData()
    : markerStart(RenderStyle::initialMarkerStart())
    , markerMid(RenderStyle::initialMarkerMid())
    , markerEnd(RenderStyle::initialMarkerEnd())
{
}

inline StyleInheritedResourceData::StyleInheritedResourceData(const StyleInheritedResourceData& other)
    : RefCounted<StyleInheritedResourceData>()
    , markerStart(other.markerStart)
    , markerMid(other.markerMid)
    , markerEnd(other.markerEnd)
{
}

Ref<StyleInheritedResourceData> StyleInheritedResourceData::copy() const
{
    return adoptRef(*new StyleInheritedResourceData(*this));
}

bool StyleInheritedResourceData::operator==(const StyleInheritedResourceData& other) const
{
    return markerStart == other.markerStart
        && markerMid == other.markerMid
        && markerEnd == other.markerEnd;
}

#if !LOG_DISABLED
void StyleInheritedResourceData::dumpDifferences(TextStream& ts, const StyleInheritedResourceData& other) const
{
    LOG_IF_DIFFERENT(markerStart);
    LOG_IF_DIFFERENT(markerMid);
    LOG_IF_DIFFERENT(markerEnd);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleLayoutData);

StyleLayoutData::StyleLayoutData()
    : cx(RenderStyle::initialCx())
    , cy(RenderStyle::initialCy())
    , r(RenderStyle::initialR())
    , rx(RenderStyle::initialRx())
    , ry(RenderStyle::initialRy())
    , x(RenderStyle::initialX())
    , y(RenderStyle::initialY())
    , d(RenderStyle::initialD())
{
}

inline StyleLayoutData::StyleLayoutData(const StyleLayoutData& other)
    : RefCounted<StyleLayoutData>()
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

Ref<StyleLayoutData> StyleLayoutData::copy() const
{
    return adoptRef(*new StyleLayoutData(*this));
}

bool StyleLayoutData::operator==(const StyleLayoutData& other) const
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
void StyleLayoutData::dumpDifferences(TextStream& ts, const StyleLayoutData& other) const
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

TextStream& operator<<(TextStream& ts, const StyleFillData& data)
{
    ts.dumpProperty("opacity"_s, data.fillOpacity);
    ts.dumpProperty("paint"_s, data.fill);
    ts.dumpProperty("visited link paint"_s, data.visitedLinkFill);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleStrokeData& data)
{
    ts.dumpProperty("opacity"_s, data.strokeOpacity);
    ts.dumpProperty("paint"_s, data.stroke);
    ts.dumpProperty("visited link paint"_s, data.visitedLinkStroke);
    ts.dumpProperty("dashOffset"_s, data.strokeDashOffset);
    ts.dumpProperty("dash array"_s, data.strokeDashArray);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleStopData& data)
{
    ts.dumpProperty("opacity"_s, data.stopOpacity);
    ts.dumpProperty("color"_s, data.stopColor);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleMiscData& data)
{
    ts.dumpProperty("flood-opacity"_s, data.floodOpacity);
    ts.dumpProperty("flood-color"_s, data.floodColor);
    ts.dumpProperty("lighting-color"_s, data.lightingColor);
    ts.dumpProperty("baseline-shift"_s, data.baselineShift);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleShadowSVGData& data)
{
    ts.dumpProperty("shadow"_s, data.shadow);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleInheritedResourceData& data)
{
    ts.dumpProperty("marker-start"_s, data.markerStart);
    ts.dumpProperty("marker-mid"_s, data.markerMid);
    ts.dumpProperty("marker-end"_s, data.markerEnd);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleLayoutData& data)
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

} // namespace WebCore
