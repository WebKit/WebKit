/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2010 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2025 Samuel Weinig <sam@webkit.org>

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
#include "SVGRenderStyle.h"

#include "IntRect.h"
#include "NodeRenderStyle.h"
#include "RenderStyleDifference.h"
#include "RenderStyleInlines.h"
#include "SVGElement.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "WebAnimationTypes.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static const SVGRenderStyle& defaultSVGStyle()
{
    static NeverDestroyed<DataRef<SVGRenderStyle>> style(SVGRenderStyle::createDefaultStyle());
    return *style.get();
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGRenderStyle);

Ref<SVGRenderStyle> SVGRenderStyle::createDefaultStyle()
{
    return adoptRef(*new SVGRenderStyle(CreateDefault));
}

SVGRenderStyle::SVGRenderStyle()
    : fillData(defaultSVGStyle().fillData)
    , strokeData(defaultSVGStyle().strokeData)
    , inheritedResourceData(defaultSVGStyle().inheritedResourceData)
    , stopData(defaultSVGStyle().stopData)
    , miscData(defaultSVGStyle().miscData)
    , layoutData(defaultSVGStyle().layoutData)
{
    setBitDefaults();
}

SVGRenderStyle::SVGRenderStyle(CreateDefaultType)
    : fillData(StyleFillData::create())
    , strokeData(StyleStrokeData::create())
    , inheritedResourceData(StyleInheritedResourceData::create())
    , stopData(StyleStopData::create())
    , miscData(StyleMiscData::create())
    , layoutData(StyleLayoutData::create())
{
    setBitDefaults();
}

inline SVGRenderStyle::SVGRenderStyle(const SVGRenderStyle& other)
    : RefCounted<SVGRenderStyle>()
    , inheritedFlags(other.inheritedFlags)
    , nonInheritedFlags(other.nonInheritedFlags)
    , fillData(other.fillData)
    , strokeData(other.strokeData)
    , inheritedResourceData(other.inheritedResourceData)
    , stopData(other.stopData)
    , miscData(other.miscData)
    , layoutData(other.layoutData)
{
    ASSERT(other == *this, "SVGRenderStyle should be properly copied.");
}

Ref<SVGRenderStyle> SVGRenderStyle::copy() const
{
    return adoptRef(*new SVGRenderStyle(*this));
}

SVGRenderStyle::~SVGRenderStyle() = default;

bool SVGRenderStyle::operator==(const SVGRenderStyle& other) const
{
    return inheritedEqual(other) && nonInheritedEqual(other);
}

void SVGRenderStyle::setBitDefaults()
{
    inheritedFlags.clipRule = static_cast<unsigned>(RenderStyle::initialClipRule());
    inheritedFlags.fillRule = static_cast<unsigned>(RenderStyle::initialFillRule());
    inheritedFlags.shapeRendering = static_cast<unsigned>(RenderStyle::initialShapeRendering());
    inheritedFlags.textAnchor = static_cast<unsigned>(RenderStyle::initialTextAnchor());
    inheritedFlags.colorInterpolation = static_cast<unsigned>(RenderStyle::initialColorInterpolation());
    inheritedFlags.colorInterpolationFilters = static_cast<unsigned>(RenderStyle::initialColorInterpolationFilters());
    inheritedFlags.glyphOrientationHorizontal = static_cast<unsigned>(RenderStyle::initialGlyphOrientationHorizontal());
    inheritedFlags.glyphOrientationVertical = static_cast<unsigned>(RenderStyle::initialGlyphOrientationVertical());

    nonInheritedFlags.alignmentBaseline = static_cast<unsigned>(RenderStyle::initialAlignmentBaseline());
    nonInheritedFlags.dominantBaseline = static_cast<unsigned>(RenderStyle::initialDominantBaseline());
    nonInheritedFlags.vectorEffect = static_cast<unsigned>(RenderStyle::initialVectorEffect());
    nonInheritedFlags.bufferedRendering = static_cast<unsigned>(RenderStyle::initialBufferedRendering());
    nonInheritedFlags.maskType = static_cast<unsigned>(RenderStyle::initialMaskType());
}

bool SVGRenderStyle::inheritedEqual(const SVGRenderStyle& other) const
{
    return fillData == other.fillData
        && strokeData == other.strokeData
        && inheritedResourceData == other.inheritedResourceData
        && inheritedFlags == other.inheritedFlags;
}

bool SVGRenderStyle::nonInheritedEqual(const SVGRenderStyle& other) const
{
    return stopData == other.stopData
        && miscData == other.miscData
        && layoutData == other.layoutData
        && nonInheritedFlags == other.nonInheritedFlags;
}

void SVGRenderStyle::inheritFrom(const SVGRenderStyle& other)
{
    fillData = other.fillData;
    strokeData = other.strokeData;
    inheritedResourceData = other.inheritedResourceData;

    inheritedFlags = other.inheritedFlags;
}

void SVGRenderStyle::copyNonInheritedFrom(const SVGRenderStyle& other)
{
    nonInheritedFlags = other.nonInheritedFlags;
    stopData = other.stopData;
    miscData = other.miscData;
    layoutData = other.layoutData;
}

static bool colorChangeRequiresRepaint(const Style::Color& a, const Style::Color& b, bool currentColorDiffers)
{
    if (a != b)
        return true;

    if (a.containsCurrentColor()) {
        ASSERT(b.containsCurrentColor());
        return currentColorDiffers;
    }

    return false;
}

bool SVGRenderStyle::changeRequiresLayout(const SVGRenderStyle& other) const
{
    // If markers change, we need a relayout, as marker boundaries are cached in RenderSVGPath.
    if (inheritedResourceData != other.inheritedResourceData)
        return true;

    // All text related properties influence layout.
    if (inheritedFlags.textAnchor != other.inheritedFlags.textAnchor
        || inheritedFlags.glyphOrientationHorizontal != other.inheritedFlags.glyphOrientationHorizontal
        || inheritedFlags.glyphOrientationVertical != other.inheritedFlags.glyphOrientationVertical
        || nonInheritedFlags.alignmentBaseline != other.nonInheritedFlags.alignmentBaseline
        || nonInheritedFlags.dominantBaseline != other.nonInheritedFlags.dominantBaseline)
        return true;

    // Text related properties influence layout.
    if (miscData->baselineShift != other.miscData->baselineShift)
        return true;

    // The x or y properties require relayout.
    if (layoutData != other.layoutData)
        return true; 

    // Some stroke properties, requires relayouts, as the cached stroke boundaries need to be recalculated.
    if (!strokeData->stroke.hasSameType(other.strokeData->stroke)
        || strokeData->stroke.urlDisregardingType() != other.strokeData->stroke.urlDisregardingType()
        || strokeData->strokeDashArray != other.strokeData->strokeDashArray
        || strokeData->strokeDashOffset != other.strokeData->strokeDashOffset
        || !strokeData->visitedLinkStroke.hasSameType(other.strokeData->visitedLinkStroke)
        || strokeData->visitedLinkStroke.urlDisregardingType() != other.strokeData->visitedLinkStroke.urlDisregardingType())
        return true;

    // vector-effect changes require a re-layout.
    if (nonInheritedFlags.vectorEffect != other.nonInheritedFlags.vectorEffect)
        return true;

    return false;
}

bool SVGRenderStyle::changeRequiresRepaint(const SVGRenderStyle& other, bool currentColorDiffers) const
{
    if (this == &other) {
        ASSERT(currentColorDiffers);
        return containsCurrentColor(strokeData->stroke)
            || containsCurrentColor(strokeData->visitedLinkStroke)
            || containsCurrentColor(miscData->floodColor)
            || containsCurrentColor(miscData->lightingColor)
            || containsCurrentColor(fillData->fill); // FIXME: Should this be checking fillData->visitedLinkFill as well?
    }

    if (strokeData->strokeOpacity != other.strokeData->strokeOpacity
        || colorChangeRequiresRepaint(strokeData->stroke.colorDisregardingType(), other.strokeData->stroke.colorDisregardingType(), currentColorDiffers)
        || colorChangeRequiresRepaint(strokeData->visitedLinkStroke.colorDisregardingType(), other.strokeData->visitedLinkStroke.colorDisregardingType(), currentColorDiffers))
        return true;

    // Painting related properties only need repaints. 
    if (colorChangeRequiresRepaint(miscData->floodColor, other.miscData->floodColor, currentColorDiffers)
        || miscData->floodOpacity != other.miscData->floodOpacity
        || colorChangeRequiresRepaint(miscData->lightingColor, other.miscData->lightingColor, currentColorDiffers))
        return true;

    // If fill data changes, we just need to repaint. Fill boundaries are not influenced by this, only by the Path, that RenderSVGPath contains.
    if (!fillData->fill.hasSameType(other.fillData->fill)
        || colorChangeRequiresRepaint(fillData->fill.colorDisregardingType(), other.fillData->fill.colorDisregardingType(), currentColorDiffers)
        || fillData->fill.urlDisregardingType() != other.fillData->fill.urlDisregardingType()
        || fillData->fillOpacity != other.fillData->fillOpacity)
        return true;

    // If gradient stops change, we just need to repaint. Style updates are already handled through RenderSVGGradientStop.
    if (stopData != other.stopData)
        return true;

    // Changes of these flags only cause repaints.
    if (inheritedFlags.shapeRendering != other.inheritedFlags.shapeRendering
        || inheritedFlags.clipRule != other.inheritedFlags.clipRule
        || inheritedFlags.fillRule != other.inheritedFlags.fillRule
        || inheritedFlags.colorInterpolation != other.inheritedFlags.colorInterpolation
        || inheritedFlags.colorInterpolationFilters != other.inheritedFlags.colorInterpolationFilters)
        return true;

    if (nonInheritedFlags.bufferedRendering != other.nonInheritedFlags.bufferedRendering)
        return true;

    if (nonInheritedFlags.maskType != other.nonInheritedFlags.maskType)
        return true;

    return false;
}

void SVGRenderStyle::conservativelyCollectChangedAnimatableProperties(const SVGRenderStyle& other, CSSPropertiesBitSet& changingProperties) const
{
    // FIXME: Consider auto-generating this function from CSSProperties.json.

    auto conservativelyCollectChangedAnimatablePropertiesViaFillData = [&](auto& first, auto& second) {
        if (first.fillOpacity != second.fillOpacity)
            changingProperties.m_properties.set(CSSPropertyFillOpacity);
        if (first.fill != second.fill || first.visitedLinkFill != second.visitedLinkFill)
            changingProperties.m_properties.set(CSSPropertyFill);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaStrokeData = [&](auto& first, auto& second) {
        if (first.strokeOpacity != second.strokeOpacity)
            changingProperties.m_properties.set(CSSPropertyStrokeOpacity);
        if (first.strokeDashOffset != second.strokeDashOffset)
            changingProperties.m_properties.set(CSSPropertyStrokeDashoffset);
        if (first.strokeDashArray != second.strokeDashArray)
            changingProperties.m_properties.set(CSSPropertyStrokeDasharray);
        if (first.stroke != second.stroke || first.visitedLinkStroke != second.visitedLinkStroke)
            changingProperties.m_properties.set(CSSPropertyStroke);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaStopData = [&](auto& first, auto& second) {
        if (first.stopOpacity != second.stopOpacity)
            changingProperties.m_properties.set(CSSPropertyStopOpacity);
        if (first.stopColor != second.stopColor)
            changingProperties.m_properties.set(CSSPropertyStopColor);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaMiscData = [&](auto& first, auto& second) {
        if (first.floodOpacity != second.floodOpacity)
            changingProperties.m_properties.set(CSSPropertyFloodOpacity);
        if (first.floodColor != second.floodColor)
            changingProperties.m_properties.set(CSSPropertyFloodColor);
        if (first.lightingColor != second.lightingColor)
            changingProperties.m_properties.set(CSSPropertyLightingColor);
        if (first.baselineShift != second.baselineShift)
            changingProperties.m_properties.set(CSSPropertyBaselineShift);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaLayoutData = [&](auto& first, auto& second) {
        if (first.cx != second.cx)
            changingProperties.m_properties.set(CSSPropertyCx);
        if (first.cy != second.cy)
            changingProperties.m_properties.set(CSSPropertyCy);
        if (first.r != second.r)
            changingProperties.m_properties.set(CSSPropertyR);
        if (first.rx != second.rx)
            changingProperties.m_properties.set(CSSPropertyRx);
        if (first.ry != second.ry)
            changingProperties.m_properties.set(CSSPropertyRy);
        if (first.x != second.x)
            changingProperties.m_properties.set(CSSPropertyX);
        if (first.y != second.y)
            changingProperties.m_properties.set(CSSPropertyY);
        if (first.d != second.d)
            changingProperties.m_properties.set(CSSPropertyD);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaInheritedResourceData = [&](auto& first, auto& second) {
        if (first.markerStart != second.markerStart)
            changingProperties.m_properties.set(CSSPropertyMarkerStart);
        if (first.markerMid != second.markerMid)
            changingProperties.m_properties.set(CSSPropertyMarkerMid);
        if (first.markerEnd != second.markerEnd)
            changingProperties.m_properties.set(CSSPropertyMarkerEnd);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaInheritedFlags = [&](auto& first, auto& second) {
        if (first.shapeRendering != second.shapeRendering)
            changingProperties.m_properties.set(CSSPropertyShapeRendering);
        if (first.clipRule != second.clipRule)
            changingProperties.m_properties.set(CSSPropertyClipRule);
        if (first.fillRule != second.fillRule)
            changingProperties.m_properties.set(CSSPropertyFillRule);
        if (first.textAnchor != second.textAnchor)
            changingProperties.m_properties.set(CSSPropertyTextAnchor);
        if (first.colorInterpolation != second.colorInterpolation)
            changingProperties.m_properties.set(CSSPropertyColorInterpolation);
        if (first.colorInterpolationFilters != second.colorInterpolationFilters)
            changingProperties.m_properties.set(CSSPropertyColorInterpolationFilters);

        // Non animated styles are followings.
        // glyphOrientationHorizontal
        // glyphOrientationVertical
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaNonInheritedFlags = [&](auto& first, auto& second) {
        if (first.alignmentBaseline != second.alignmentBaseline)
            changingProperties.m_properties.set(CSSPropertyAlignmentBaseline);
        if (first.bufferedRendering != second.bufferedRendering)
            changingProperties.m_properties.set(CSSPropertyBufferedRendering);
        if (first.dominantBaseline != second.dominantBaseline)
            changingProperties.m_properties.set(CSSPropertyDominantBaseline);
        if (first.maskType != second.maskType)
            changingProperties.m_properties.set(CSSPropertyMaskType);
        if (first.vectorEffect != second.vectorEffect)
            changingProperties.m_properties.set(CSSPropertyVectorEffect);
    };

    if (fillData.ptr() != other.fillData.ptr())
        conservativelyCollectChangedAnimatablePropertiesViaFillData(*fillData, *other.fillData);
    if (strokeData != other.strokeData)
        conservativelyCollectChangedAnimatablePropertiesViaStrokeData(*strokeData, *other.strokeData);
    if (stopData != other.stopData)
        conservativelyCollectChangedAnimatablePropertiesViaStopData(*stopData, *other.stopData);
    if (miscData != other.miscData)
        conservativelyCollectChangedAnimatablePropertiesViaMiscData(*miscData, *other.miscData);
    if (layoutData != other.layoutData)
        conservativelyCollectChangedAnimatablePropertiesViaLayoutData(*layoutData, *other.layoutData);
    if (inheritedResourceData != other.inheritedResourceData)
        conservativelyCollectChangedAnimatablePropertiesViaInheritedResourceData(*inheritedResourceData, *other.inheritedResourceData);
    if (inheritedFlags != other.inheritedFlags)
        conservativelyCollectChangedAnimatablePropertiesViaInheritedFlags(inheritedFlags, other.inheritedFlags);
    if (nonInheritedFlags != other.nonInheritedFlags)
        conservativelyCollectChangedAnimatablePropertiesViaNonInheritedFlags(nonInheritedFlags, other.nonInheritedFlags);
}

#if !LOG_DISABLED

void SVGRenderStyle::InheritedFlags::dumpDifferences(TextStream& ts, const SVGRenderStyle::InheritedFlags& other) const
{
    LOG_IF_DIFFERENT_WITH_CAST(ShapeRendering, shapeRendering);
    LOG_IF_DIFFERENT_WITH_CAST(WindRule, clipRule);
    LOG_IF_DIFFERENT_WITH_CAST(WindRule, fillRule);
    LOG_IF_DIFFERENT_WITH_CAST(TextAnchor, textAnchor);
    LOG_IF_DIFFERENT_WITH_CAST(ColorInterpolation, colorInterpolation);
    LOG_IF_DIFFERENT_WITH_CAST(ColorInterpolation, colorInterpolationFilters);
    LOG_IF_DIFFERENT_WITH_CAST(Style::SVGGlyphOrientationHorizontal, glyphOrientationHorizontal);
    LOG_IF_DIFFERENT_WITH_CAST(Style::SVGGlyphOrientationVertical, glyphOrientationVertical);
}

void SVGRenderStyle::NonInheritedFlags::dumpDifferences(TextStream& ts, const SVGRenderStyle::NonInheritedFlags& other) const
{
    LOG_IF_DIFFERENT_WITH_CAST(AlignmentBaseline, alignmentBaseline);
    LOG_IF_DIFFERENT_WITH_CAST(DominantBaseline, dominantBaseline);
    LOG_IF_DIFFERENT_WITH_CAST(VectorEffect, vectorEffect);
    LOG_IF_DIFFERENT_WITH_CAST(BufferedRendering, bufferedRendering);
    LOG_IF_DIFFERENT_WITH_CAST(MaskType, maskType);
}

void SVGRenderStyle::dumpDifferences(TextStream& ts, const SVGRenderStyle& other) const
{
    inheritedFlags.dumpDifferences(ts, other.inheritedFlags);
    nonInheritedFlags.dumpDifferences(ts, other.nonInheritedFlags);

    fillData->dumpDifferences(ts, other.fillData);
    strokeData->dumpDifferences(ts, other.strokeData);
    inheritedResourceData->dumpDifferences(ts, other.inheritedResourceData);

    stopData->dumpDifferences(ts, other.stopData);
    miscData->dumpDifferences(ts, other.miscData);
    layoutData->dumpDifferences(ts, other.layoutData);
}
#endif

} // namespace WebCore
