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
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleFillData);

StyleFillData::StyleFillData()
    : opacity(SVGRenderStyle::initialFillOpacity())
    , paint(SVGRenderStyle::initialFill())
    , visitedLinkPaint(SVGRenderStyle::initialFill())
{
}

inline StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>()
    , opacity(other.opacity)
    , paint(other.paint)
    , visitedLinkPaint(other.visitedLinkPaint)
{
}

Ref<StyleFillData> StyleFillData::copy() const
{
    return adoptRef(*new StyleFillData(*this));
}

#if !LOG_DISABLED
void StyleFillData::dumpDifferences(TextStream& ts, const StyleFillData& other) const
{
    LOG_IF_DIFFERENT(opacity);
    LOG_IF_DIFFERENT(paint);
    LOG_IF_DIFFERENT(visitedLinkPaint);
}
#endif

bool StyleFillData::operator==(const StyleFillData& other) const
{
    return opacity == other.opacity
        && paint == other.paint
        && visitedLinkPaint == other.visitedLinkPaint;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleStrokeData);

StyleStrokeData::StyleStrokeData()
    : opacity(SVGRenderStyle::initialStrokeOpacity())
    , paint(SVGRenderStyle::initialStroke())
    , visitedLinkPaint(SVGRenderStyle::initialStroke())
    , dashOffset(RenderStyle::zeroLength())
    , dashArray(SVGRenderStyle::initialStrokeDashArray())
{
}

inline StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>()
    , opacity(other.opacity)
    , paint(other.paint)
    , visitedLinkPaint(other.visitedLinkPaint)
    , dashOffset(other.dashOffset)
    , dashArray(other.dashArray)
{
}

Ref<StyleStrokeData> StyleStrokeData::copy() const
{
    return adoptRef(*new StyleStrokeData(*this));
}

bool StyleStrokeData::operator==(const StyleStrokeData& other) const
{
    return opacity == other.opacity
        && paint == other.paint
        && visitedLinkPaint == other.visitedLinkPaint
        && dashOffset == other.dashOffset
        && dashArray == other.dashArray;
}

#if !LOG_DISABLED
void StyleStrokeData::dumpDifferences(TextStream& ts, const StyleStrokeData& other) const
{
    LOG_IF_DIFFERENT(opacity);
    LOG_IF_DIFFERENT(paint);
    LOG_IF_DIFFERENT(visitedLinkPaint);
    LOG_IF_DIFFERENT(dashOffset);
    LOG_IF_DIFFERENT(dashArray);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleStopData);

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

#if !LOG_DISABLED
void StyleStopData::dumpDifferences(TextStream& ts, const StyleStopData& other) const
{
    LOG_IF_DIFFERENT(opacity);
    LOG_IF_DIFFERENT(color);
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMiscData);

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

#if !LOG_DISABLED
void StyleMiscData::dumpDifferences(TextStream& ts, const StyleMiscData& other) const
{
    LOG_IF_DIFFERENT(floodOpacity);
    LOG_IF_DIFFERENT(floodColor);
    LOG_IF_DIFFERENT(lightingColor);
    LOG_IF_DIFFERENT(baselineShiftValue);
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
    : cx(RenderStyle::zeroLength())
    , cy(RenderStyle::zeroLength())
    , r(RenderStyle::zeroLength())
    , rx(RenderStyle::initialRadius())
    , ry(RenderStyle::initialRadius())
    , x(RenderStyle::zeroLength())
    , y(RenderStyle::zeroLength())
    , d(nullptr)
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

TextStream& operator<<(TextStream& ts, AlignmentBaseline value)
{
    switch (value) {
    case AlignmentBaseline::Baseline: ts << "baseline"_s; break;
    case AlignmentBaseline::BeforeEdge: ts << "before-edge"_s; break;
    case AlignmentBaseline::TextBeforeEdge: ts << "text-before-edge"_s; break;
    case AlignmentBaseline::Middle: ts << "middle"_s; break;
    case AlignmentBaseline::Central: ts << "central"_s; break;
    case AlignmentBaseline::AfterEdge: ts << "after-edge"_s; break;
    case AlignmentBaseline::TextAfterEdge: ts << "text-after-edge"_s; break;
    case AlignmentBaseline::Ideographic: ts << "ideographic"_s; break;
    case AlignmentBaseline::Alphabetic: ts << "alphabetic"_s; break;
    case AlignmentBaseline::Hanging: ts << "hanging"_s; break;
    case AlignmentBaseline::Mathematical: ts << "mathematical"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BaselineShift value)
{
    switch (value) {
    case BaselineShift::Baseline: ts << "baseline"_s; break;
    case BaselineShift::Sub: ts << "sub"_s; break;
    case BaselineShift::Super: ts << "super"_s; break;
    case BaselineShift::Length: ts << "length"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BufferedRendering value)
{
    switch (value) {
    case BufferedRendering::Auto: ts << "auto"_s; break;
    case BufferedRendering::Dynamic: ts << "dynamic"_s; break;
    case BufferedRendering::Static: ts << "static"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColorInterpolation value)
{
    switch (value) {
    case ColorInterpolation::Auto: ts << "auto"_s; break;
    case ColorInterpolation::SRGB: ts << "sRGB"_s; break;
    case ColorInterpolation::LinearRGB: ts << "linearRGB"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColorRendering value)
{
    switch (value) {
    case ColorRendering::Auto: ts << "auto"_s; break;
    case ColorRendering::OptimizeSpeed: ts << "optimizeSpeed"_s; break;
    case ColorRendering::OptimizeQuality: ts << "optimizeQuality"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, DominantBaseline value)
{
    switch (value) {
    case DominantBaseline::Auto: ts << "auto"_s; break;
    case DominantBaseline::UseScript: ts << "use-script"_s; break;
    case DominantBaseline::NoChange: ts << "no-change"_s; break;
    case DominantBaseline::ResetSize: ts << "reset-size"_s; break;
    case DominantBaseline::Ideographic: ts << "ideographic"_s; break;
    case DominantBaseline::Alphabetic: ts << "alphabetic"_s; break;
    case DominantBaseline::Hanging: ts << "hanging"_s; break;
    case DominantBaseline::Mathematical: ts << "mathematical"_s; break;
    case DominantBaseline::Central: ts << "central"_s; break;
    case DominantBaseline::Middle: ts << "middle"_s; break;
    case DominantBaseline::TextAfterEdge: ts << "text-after-edge"_s; break;
    case DominantBaseline::TextBeforeEdge: ts << "text-before-edge"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GlyphOrientation value)
{
    switch (value) {
    case GlyphOrientation::Degrees0: ts << '0'; break;
    case GlyphOrientation::Degrees90: ts << "90"_s; break;
    case GlyphOrientation::Degrees180: ts << "180"_s; break;
    case GlyphOrientation::Degrees270: ts << "270"_s; break;
    case GlyphOrientation::Auto: ts << "Auto"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MaskType value)
{
    switch (value) {
    case MaskType::Luminance: ts << "luminance"_s; break;
    case MaskType::Alpha: ts << "alpha"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ShapeRendering value)
{
    switch (value) {
    case ShapeRendering::Auto: ts << "auto"_s; break;
    case ShapeRendering::OptimizeSpeed: ts << "optimizeSpeed"_s; break;
    case ShapeRendering::CrispEdges: ts << "crispEdges"_s; break;
    case ShapeRendering::GeometricPrecision: ts << "geometricPrecision"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextAnchor value)
{
    switch (value) {
    case TextAnchor::Start: ts << "start"_s; break;
    case TextAnchor::Middle: ts << "middle"_s; break;
    case TextAnchor::End: ts << "end"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, VectorEffect value)
{
    switch (value) {
    case VectorEffect::None: ts << "none"_s; break;
    case VectorEffect::NonScalingStroke: ts << "non-scaling-stroke"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleFillData& data)
{
    ts.dumpProperty("opacity"_s, data.opacity);
    ts.dumpProperty("paint"_s, data.paint);
    ts.dumpProperty("visited link paint"_s, data.visitedLinkPaint);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleStrokeData& data)
{
    ts.dumpProperty("opacity"_s, data.opacity);
    ts.dumpProperty("paint"_s, data.paint);
    ts.dumpProperty("visited link paint"_s, data.visitedLinkPaint);
    ts.dumpProperty("dashOffset"_s, data.dashOffset);
    ts.dumpProperty("dash array"_s, data.dashArray);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleStopData& data)
{
    ts.dumpProperty("opacity"_s, data.opacity);
    ts.dumpProperty("color"_s, data.color);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleMiscData& data)
{
    ts.dumpProperty("flood-opacity"_s, data.floodOpacity);
    ts.dumpProperty("flood-color"_s, data.floodColor);
    ts.dumpProperty("lighting-color"_s, data.lightingColor);
    ts.dumpProperty("baseline-shift"_s, data.baselineShiftValue);
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
    return ts;
}

} // namespace WebCore
