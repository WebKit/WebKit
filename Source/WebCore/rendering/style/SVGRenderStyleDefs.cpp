/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Inc.

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

#include "RenderStyle.h"
#include "SVGRenderStyle.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

StyleFillData::StyleFillData()
    : opacity(SVGRenderStyle::initialFillOpacity())
    , paintColor(SVGRenderStyle::initialFillPaintColor())
    , visitedLinkPaintColor(SVGRenderStyle::initialFillPaintColor())
    , paintUri(SVGRenderStyle::initialFillPaintUri())
    , visitedLinkPaintUri(SVGRenderStyle::initialFillPaintUri())
    , paintType(SVGRenderStyle::initialFillPaintType())
    , visitedLinkPaintType(SVGRenderStyle::initialStrokePaintType())
{
}

inline StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>()
    , opacity(other.opacity)
    , paintColor(other.paintColor)
    , visitedLinkPaintColor(other.visitedLinkPaintColor)
    , paintUri(other.paintUri)
    , visitedLinkPaintUri(other.visitedLinkPaintUri)
    , paintType(other.paintType)
    , visitedLinkPaintType(other.visitedLinkPaintType)
{
}

Ref<StyleFillData> StyleFillData::copy() const
{
    return adoptRef(*new StyleFillData(*this));
}

bool StyleFillData::operator==(const StyleFillData& other) const
{
    return opacity == other.opacity
        && paintColor == other.paintColor
        && visitedLinkPaintColor == other.visitedLinkPaintColor
        && paintUri == other.paintUri
        && visitedLinkPaintUri == other.visitedLinkPaintUri
        && paintType == other.paintType
        && visitedLinkPaintType == other.visitedLinkPaintType;
    
}

StyleStrokeData::StyleStrokeData()
    : opacity(SVGRenderStyle::initialStrokeOpacity())
    , paintColor(SVGRenderStyle::initialStrokePaintColor())
    , visitedLinkPaintColor(SVGRenderStyle::initialStrokePaintColor())
    , paintUri(SVGRenderStyle::initialStrokePaintUri())
    , visitedLinkPaintUri(SVGRenderStyle::initialStrokePaintUri())
    , dashOffset(RenderStyle::initialZeroLength())
    , dashArray(SVGRenderStyle::initialStrokeDashArray())
    , paintType(SVGRenderStyle::initialStrokePaintType())
    , visitedLinkPaintType(SVGRenderStyle::initialStrokePaintType())
{
}

inline StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>()
    , opacity(other.opacity)
    , paintColor(other.paintColor)
    , visitedLinkPaintColor(other.visitedLinkPaintColor)
    , paintUri(other.paintUri)
    , visitedLinkPaintUri(other.visitedLinkPaintUri)
    , dashOffset(other.dashOffset)
    , dashArray(other.dashArray)
    , paintType(other.paintType)
    , visitedLinkPaintType(other.visitedLinkPaintType)
{
}

Ref<StyleStrokeData> StyleStrokeData::copy() const
{
    return adoptRef(*new StyleStrokeData(*this));
}

bool StyleStrokeData::operator==(const StyleStrokeData& other) const
{
    return opacity == other.opacity
        && paintColor == other.paintColor
        && visitedLinkPaintColor == other.visitedLinkPaintColor
        && paintUri == other.paintUri
        && visitedLinkPaintUri == other.visitedLinkPaintUri
        && dashOffset == other.dashOffset
        && dashArray == other.dashArray
        && paintType == other.paintType
        && visitedLinkPaintType == other.visitedLinkPaintType;
}

StyleStopData::StyleStopData()
    : opacity(SVGRenderStyle::initialStopOpacity())
    , color(SVGRenderStyle::initialStopColor())
{
}

inline StyleStopData::StyleStopData(const StyleStopData& other)
    : RefCounted<StyleStopData>()
    , opacity(other.opacity)
    , color(other.color)
{
}

Ref<StyleStopData> StyleStopData::copy() const
{
    return adoptRef(*new StyleStopData(*this));
}

bool StyleStopData::operator==(const StyleStopData& other) const
{
    return opacity == other.opacity
        && color == other.color;
}

StyleTextData::StyleTextData()
    : kerning(SVGRenderStyle::initialKerning())
{
}

inline StyleTextData::StyleTextData(const StyleTextData& other)
    : RefCounted<StyleTextData>()
    , kerning(other.kerning)
{
}

Ref<StyleTextData> StyleTextData::copy() const
{
    return adoptRef(*new StyleTextData(*this));
}

bool StyleTextData::operator==(const StyleTextData& other) const
{
    return kerning == other.kerning;
}

StyleMiscData::StyleMiscData()
    : floodOpacity(SVGRenderStyle::initialFloodOpacity())
    , floodColor(SVGRenderStyle::initialFloodColor())
    , lightingColor(SVGRenderStyle::initialLightingColor())
    , baselineShiftValue(SVGRenderStyle::initialBaselineShiftValue())
{
}

inline StyleMiscData::StyleMiscData(const StyleMiscData& other)
    : RefCounted<StyleMiscData>()
    , floodOpacity(other.floodOpacity)
    , floodColor(other.floodColor)
    , lightingColor(other.lightingColor)
    , baselineShiftValue(other.baselineShiftValue)
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
        && baselineShiftValue == other.baselineShiftValue;
}

StyleShadowSVGData::StyleShadowSVGData()
{
}

inline StyleShadowSVGData::StyleShadowSVGData(const StyleShadowSVGData& other)
    : RefCounted<StyleShadowSVGData>()
    , shadow(other.shadow ? makeUnique<ShadowData>(*other.shadow) : nullptr)
{
}

Ref<StyleShadowSVGData> StyleShadowSVGData::copy() const
{
    return adoptRef(*new StyleShadowSVGData(*this));
}

bool StyleShadowSVGData::operator==(const StyleShadowSVGData& other) const
{
    return arePointingToEqualData(shadow, other.shadow);
}

StyleResourceData::StyleResourceData()
    : clipper(SVGRenderStyle::initialClipperResource())
    , masker(SVGRenderStyle::initialMaskerResource())
{
}

inline StyleResourceData::StyleResourceData(const StyleResourceData& other)
    : RefCounted<StyleResourceData>()
    , clipper(other.clipper)
    , masker(other.masker)
{
}

Ref<StyleResourceData> StyleResourceData::copy() const
{
    return adoptRef(*new StyleResourceData(*this));
}

bool StyleResourceData::operator==(const StyleResourceData& other) const
{
    return clipper == other.clipper
        && masker == other.masker;
}

StyleInheritedResourceData::StyleInheritedResourceData()
    : markerStart(SVGRenderStyle::initialMarkerStartResource())
    , markerMid(SVGRenderStyle::initialMarkerMidResource())
    , markerEnd(SVGRenderStyle::initialMarkerEndResource())
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

StyleLayoutData::StyleLayoutData()
    : cx(RenderStyle::initialZeroLength())
    , cy(RenderStyle::initialZeroLength())
    , r(RenderStyle::initialZeroLength())
    , rx(RenderStyle::initialZeroLength())
    , ry(RenderStyle::initialZeroLength())
    , x(RenderStyle::initialZeroLength())
    , y(RenderStyle::initialZeroLength())
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
        && y == other.y;
}

}
