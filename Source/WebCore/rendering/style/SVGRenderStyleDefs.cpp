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
#include <wtf/text/TextStream.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleFillData);

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
    : masker(SVGRenderStyle::initialMaskerResource())
{
}

inline StyleResourceData::StyleResourceData(const StyleResourceData& other)
    : RefCounted<StyleResourceData>()
    , masker(other.masker)
{
}

Ref<StyleResourceData> StyleResourceData::copy() const
{
    return adoptRef(*new StyleResourceData(*this));
}

bool StyleResourceData::operator==(const StyleResourceData& other) const
{
    return masker == other.masker;
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
    , rx(RenderStyle::initialRadius())
    , ry(RenderStyle::initialRadius())
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


TextStream& operator<<(TextStream& ts, AlignmentBaseline value)
{
    switch (value) {
    case AlignmentBaseline::Auto: ts << "auto"; break;
    case AlignmentBaseline::Baseline: ts << "baseline"; break;
    case AlignmentBaseline::BeforeEdge: ts << "before-edge"; break;
    case AlignmentBaseline::TextBeforeEdge: ts << "text-before-edge"; break;
    case AlignmentBaseline::Middle: ts << "middle"; break;
    case AlignmentBaseline::Central: ts << "central"; break;
    case AlignmentBaseline::AfterEdge: ts << "after-edge"; break;
    case AlignmentBaseline::TextAfterEdge: ts << "text-after-edge"; break;
    case AlignmentBaseline::Ideographic: ts << "ideographic"; break;
    case AlignmentBaseline::Alphabetic: ts << "alphabetic"; break;
    case AlignmentBaseline::Hanging: ts << "hanging"; break;
    case AlignmentBaseline::Mathematical: ts << "mathematical"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BaselineShift value)
{
    switch (value) {
    case BaselineShift::Baseline: ts << "baseline"; break;
    case BaselineShift::Sub: ts << "sub"; break;
    case BaselineShift::Super: ts << "super"; break;
    case BaselineShift::Length: ts << "length"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, BufferedRendering value)
{
    switch (value) {
    case BufferedRendering::Auto: ts << "auto"; break;
    case BufferedRendering::Dynamic: ts << "dynamic"; break;
    case BufferedRendering::Static: ts << "static"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColorInterpolation value)
{
    switch (value) {
    case ColorInterpolation::Auto: ts << "auto"; break;
    case ColorInterpolation::SRGB: ts << "sRGB"; break;
    case ColorInterpolation::LinearRGB: ts << "linearRGB"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ColorRendering value)
{
    switch (value) {
    case ColorRendering::Auto: ts << "auto"; break;
    case ColorRendering::OptimizeSpeed: ts << "optimizeSpeed"; break;
    case ColorRendering::OptimizeQuality: ts << "optimizeQuality"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, DominantBaseline value)
{
    switch (value) {
    case DominantBaseline::Auto: ts << "auto"; break;
    case DominantBaseline::UseScript: ts << "use-script"; break;
    case DominantBaseline::NoChange: ts << "no-change"; break;
    case DominantBaseline::ResetSize: ts << "reset-size"; break;
    case DominantBaseline::Ideographic: ts << "ideographic"; break;
    case DominantBaseline::Alphabetic: ts << "alphabetic"; break;
    case DominantBaseline::Hanging: ts << "hanging"; break;
    case DominantBaseline::Mathematical: ts << "mathematical"; break;
    case DominantBaseline::Central: ts << "central"; break;
    case DominantBaseline::Middle: ts << "middle"; break;
    case DominantBaseline::TextAfterEdge: ts << "text-after-edge"; break;
    case DominantBaseline::TextBeforeEdge: ts << "text-before-edge"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GlyphOrientation value)
{
    switch (value) {
    case GlyphOrientation::Degrees0: ts << "0"; break;
    case GlyphOrientation::Degrees90: ts << "90"; break;
    case GlyphOrientation::Degrees180: ts << "180"; break;
    case GlyphOrientation::Degrees270: ts << "270"; break;
    case GlyphOrientation::Auto: ts << "Auto"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, MaskType value)
{
    switch (value) {
    case MaskType::Luminance: ts << "luminance"; break;
    case MaskType::Alpha: ts << "alpha"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, SVGPaintType paintType)
{
    switch (paintType) {
    case SVGPaintType::RGBColor: ts << "rgb-color"; break;
    case SVGPaintType::None: ts << "none"; break;
    case SVGPaintType::CurrentColor: ts << "current-color"; break;
    case SVGPaintType::URINone: ts << "uri-none"; break;
    case SVGPaintType::URICurrentColor: ts << "uri-current-color"; break;
    case SVGPaintType::URIRGBColor: ts << "uri-rgb-color"; break;
    case SVGPaintType::URI: ts << "uri"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, ShapeRendering value)
{
    switch (value) {
    case ShapeRendering::Auto: ts << "auto"; break;
    case ShapeRendering::OptimizeSpeed: ts << "optimizeSpeed"; break;
    case ShapeRendering::CrispEdges: ts << "crispEdges"; break;
    case ShapeRendering::GeometricPrecision: ts << "geometricPrecision"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextAnchor value)
{
    switch (value) {
    case TextAnchor::Start: ts << "start"; break;
    case TextAnchor::Middle: ts << "middle"; break;
    case TextAnchor::End: ts << "end"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, VectorEffect value)
{
    switch (value) {
    case VectorEffect::None: ts << "none"; break;
    case VectorEffect::NonScalingStroke: ts << "non-scaling-stroke"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleFillData& data)
{
    ts.dumpProperty("opacity", data.opacity);
    ts.dumpProperty("paint-color", data.paintColor);
    ts.dumpProperty("visited link paint-color", data.visitedLinkPaintColor);
    ts.dumpProperty("paint uri", data.paintUri);
    ts.dumpProperty("visited link paint uri", data.visitedLinkPaintUri);
    ts.dumpProperty("visited link paint type", data.paintType);
    ts.dumpProperty("visited link paint type", data.visitedLinkPaintType);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleStrokeData& data)
{
    ts.dumpProperty("opacity", data.opacity);
    ts.dumpProperty("paint-color", data.paintColor);
    ts.dumpProperty("visited link paint-color", data.visitedLinkPaintColor);
    ts.dumpProperty("paint uri", data.paintUri);
    ts.dumpProperty("visited link paint uri", data.visitedLinkPaintUri);
    ts.dumpProperty("dashOffset", data.dashOffset);
    ts.dumpProperty("dash array", data.dashArray);
    ts.dumpProperty("visited link paint type", data.paintType);
    ts.dumpProperty("visited link paint type", data.visitedLinkPaintType);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleStopData& data)
{
    ts.dumpProperty("opacity", data.opacity);
    ts.dumpProperty("color", data.color);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleTextData& data)
{
    ts.dumpProperty("kerning", data.kerning);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleMiscData& data)
{
    ts.dumpProperty("flood-opacity", data.floodOpacity);
    ts.dumpProperty("flood-color", data.floodColor);
    ts.dumpProperty("lighting-color", data.lightingColor);
    ts.dumpProperty("baseline-shift", data.baselineShiftValue);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleShadowSVGData& data)
{
    if (data.shadow)
        ts.dumpProperty("shadow", *data.shadow);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleResourceData& data)
{
    ts.dumpProperty("masker", data.masker);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleInheritedResourceData& data)
{
    ts.dumpProperty("marker-start", data.markerStart);
    ts.dumpProperty("marker-mid", data.markerMid);
    ts.dumpProperty("marker-end", data.markerEnd);
    return ts;
}

TextStream& operator<<(TextStream& ts, const StyleLayoutData& data)
{
    ts.dumpProperty("cx", data.cx);
    ts.dumpProperty("cy", data.cy);
    ts.dumpProperty("r", data.r);
    ts.dumpProperty("rx", data.rx);
    ts.dumpProperty("ry", data.ry);
    ts.dumpProperty("x", data.x);
    ts.dumpProperty("y", data.y);
    return ts;
}

} // namespace WebCore
