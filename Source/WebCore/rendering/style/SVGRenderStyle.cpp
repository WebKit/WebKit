/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2010 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

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
#include "SVGRenderStyle.h"

#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "IntRect.h"
#include "NodeRenderStyle.h"
#include "RenderStyleDifference.h"
#include "SVGElement.h"
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
    : m_fillData(defaultSVGStyle().m_fillData)
    , m_strokeData(defaultSVGStyle().m_strokeData)
    , m_inheritedResourceData(defaultSVGStyle().m_inheritedResourceData)
    , m_stopData(defaultSVGStyle().m_stopData)
    , m_miscData(defaultSVGStyle().m_miscData)
    , m_layoutData(defaultSVGStyle().m_layoutData)
{
    setBitDefaults();
}

SVGRenderStyle::SVGRenderStyle(CreateDefaultType)
    : m_fillData(StyleFillData::create())
    , m_strokeData(StyleStrokeData::create())
    , m_inheritedResourceData(StyleInheritedResourceData::create())
    , m_stopData(StyleStopData::create())
    , m_miscData(StyleMiscData::create())
    , m_layoutData(StyleLayoutData::create())
{
    setBitDefaults();
}

inline SVGRenderStyle::SVGRenderStyle(const SVGRenderStyle& other)
    : RefCounted<SVGRenderStyle>()
    , m_inheritedFlags(other.m_inheritedFlags)
    , m_nonInheritedFlags(other.m_nonInheritedFlags)
    , m_fillData(other.m_fillData)
    , m_strokeData(other.m_strokeData)
    , m_inheritedResourceData(other.m_inheritedResourceData)
    , m_stopData(other.m_stopData)
    , m_miscData(other.m_miscData)
    , m_layoutData(other.m_layoutData)
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
    return m_fillData == other.m_fillData
        && m_strokeData == other.m_strokeData
        && m_stopData == other.m_stopData
        && m_miscData == other.m_miscData
        && m_layoutData == other.m_layoutData
        && m_inheritedResourceData == other.m_inheritedResourceData
        && m_inheritedFlags == other.m_inheritedFlags
        && m_nonInheritedFlags == other.m_nonInheritedFlags;
}

bool SVGRenderStyle::inheritedEqual(const SVGRenderStyle& other) const
{
    return m_fillData == other.m_fillData
        && m_strokeData == other.m_strokeData
        && m_inheritedResourceData == other.m_inheritedResourceData
        && m_inheritedFlags == other.m_inheritedFlags;
}

void SVGRenderStyle::inheritFrom(const SVGRenderStyle& other)
{
    m_fillData = other.m_fillData;
    m_strokeData = other.m_strokeData;
    m_inheritedResourceData = other.m_inheritedResourceData;

    m_inheritedFlags = other.m_inheritedFlags;
}

void SVGRenderStyle::copyNonInheritedFrom(const SVGRenderStyle& other)
{
    m_nonInheritedFlags = other.m_nonInheritedFlags;
    m_stopData = other.m_stopData;
    m_miscData = other.m_miscData;
    m_layoutData = other.m_layoutData;
}

static bool colorChangeRequiresRepaint(const StyleColor& a, const StyleColor& b, bool currentColorDiffers)
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
    if (m_inheritedResourceData != other.m_inheritedResourceData)
        return true;

    // All text related properties influence layout.
    if (m_inheritedFlags.textAnchor != other.m_inheritedFlags.textAnchor
        || m_inheritedFlags.glyphOrientationHorizontal != other.m_inheritedFlags.glyphOrientationHorizontal
        || m_inheritedFlags.glyphOrientationVertical != other.m_inheritedFlags.glyphOrientationVertical
        || m_nonInheritedFlags.flagBits.alignmentBaseline != other.m_nonInheritedFlags.flagBits.alignmentBaseline
        || m_nonInheritedFlags.flagBits.dominantBaseline != other.m_nonInheritedFlags.flagBits.dominantBaseline
        || m_nonInheritedFlags.flagBits.baselineShift != other.m_nonInheritedFlags.flagBits.baselineShift)
        return true;

    // Text related properties influence layout.
    if (m_miscData->baselineShiftValue != other.m_miscData->baselineShiftValue)
        return true;

    // The x or y properties require relayout.
    if (m_layoutData != other.m_layoutData)
        return true; 

    // Some stroke properties, requires relayouts, as the cached stroke boundaries need to be recalculated.
    if (m_strokeData->paintType != other.m_strokeData->paintType
        || m_strokeData->paintUri != other.m_strokeData->paintUri
        || m_strokeData->dashArray != other.m_strokeData->dashArray
        || m_strokeData->dashOffset != other.m_strokeData->dashOffset
        || m_strokeData->visitedLinkPaintUri != other.m_strokeData->visitedLinkPaintUri
        || m_strokeData->visitedLinkPaintType != other.m_strokeData->visitedLinkPaintType)
        return true;

    // vector-effect changes require a re-layout.
    if (m_nonInheritedFlags.flagBits.vectorEffect != other.m_nonInheritedFlags.flagBits.vectorEffect)
        return true;

    return false;
}

bool SVGRenderStyle::changeRequiresRepaint(const SVGRenderStyle& other, bool currentColorDiffers) const
{
    if (m_strokeData->opacity != other.m_strokeData->opacity
        || colorChangeRequiresRepaint(m_strokeData->paintColor, other.m_strokeData->paintColor, currentColorDiffers)
        || colorChangeRequiresRepaint(m_strokeData->visitedLinkPaintColor, other.m_strokeData->visitedLinkPaintColor, currentColorDiffers))
        return true;

    // Painting related properties only need repaints. 
    if (colorChangeRequiresRepaint(m_miscData->floodColor, other.m_miscData->floodColor, currentColorDiffers)
        || m_miscData->floodOpacity != other.m_miscData->floodOpacity
        || colorChangeRequiresRepaint(m_miscData->lightingColor, other.m_miscData->lightingColor, currentColorDiffers))
        return true;

    // If fill data changes, we just need to repaint. Fill boundaries are not influenced by this, only by the Path, that RenderSVGPath contains.
    if (m_fillData->paintType != other.m_fillData->paintType 
        || colorChangeRequiresRepaint(m_fillData->paintColor, other.m_fillData->paintColor, currentColorDiffers)
        || m_fillData->paintUri != other.m_fillData->paintUri 
        || m_fillData->opacity != other.m_fillData->opacity)
        return true;

    // If gradient stops change, we just need to repaint. Style updates are already handled through RenderSVGGradientSTop.
    if (m_stopData != other.m_stopData)
        return true;

    // Changes of these flags only cause repaints.
    if (m_inheritedFlags.shapeRendering != other.m_inheritedFlags.shapeRendering
        || m_inheritedFlags.clipRule != other.m_inheritedFlags.clipRule
        || m_inheritedFlags.fillRule != other.m_inheritedFlags.fillRule
        || m_inheritedFlags.colorInterpolation != other.m_inheritedFlags.colorInterpolation
        || m_inheritedFlags.colorInterpolationFilters != other.m_inheritedFlags.colorInterpolationFilters)
        return true;

    if (m_nonInheritedFlags.flagBits.bufferedRendering != other.m_nonInheritedFlags.flagBits.bufferedRendering)
        return true;

    if (m_nonInheritedFlags.flagBits.maskType != other.m_nonInheritedFlags.flagBits.maskType)
        return true;

    return false;
}

void SVGRenderStyle::conservativelyCollectChangedAnimatableProperties(const SVGRenderStyle& other, CSSPropertiesBitSet& changingProperties) const
{
    // FIXME: Consider auto-generating this function from CSSProperties.json.

    auto conservativelyCollectChangedAnimatablePropertiesViaFillData = [&](auto& first, auto& second) {
        if (first.opacity != second.opacity)
            changingProperties.m_properties.set(CSSPropertyFillOpacity);
        if (first.paintColor != second.paintColor
            || first.visitedLinkPaintColor != second.visitedLinkPaintColor
            || first.paintUri != second.paintUri
            || first.visitedLinkPaintUri != second.visitedLinkPaintUri
            || first.paintType != second.paintType
            || first.visitedLinkPaintType != second.visitedLinkPaintType)
            changingProperties.m_properties.set(CSSPropertyFill);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaStrokeData = [&](auto& first, auto& second) {
        if (first.opacity != second.opacity)
            changingProperties.m_properties.set(CSSPropertyStrokeOpacity);
        if (first.dashOffset != second.dashOffset)
            changingProperties.m_properties.set(CSSPropertyStrokeDashoffset);
        if (first.dashArray != second.dashArray)
            changingProperties.m_properties.set(CSSPropertyStrokeDasharray);
        if (first.paintColor != second.paintColor
            || first.visitedLinkPaintColor != second.visitedLinkPaintColor
            || first.paintUri != second.paintUri
            || first.visitedLinkPaintUri != second.visitedLinkPaintUri
            || first.paintType != second.paintType
            || first.visitedLinkPaintType != second.visitedLinkPaintType)
            changingProperties.m_properties.set(CSSPropertyStroke);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaStopData = [&](auto& first, auto& second) {
        if (first.opacity != second.opacity)
            changingProperties.m_properties.set(CSSPropertyStopOpacity);
        if (first.color != second.color)
            changingProperties.m_properties.set(CSSPropertyStopColor);
    };

    auto conservativelyCollectChangedAnimatablePropertiesViaMiscData = [&](auto& first, auto& second) {
        if (first.floodOpacity != second.floodOpacity)
            changingProperties.m_properties.set(CSSPropertyFloodOpacity);
        if (first.floodColor != second.floodColor)
            changingProperties.m_properties.set(CSSPropertyFloodColor);
        if (first.lightingColor != second.lightingColor)
            changingProperties.m_properties.set(CSSPropertyLightingColor);
        if (first.baselineShiftValue != second.baselineShiftValue)
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
        if (first.flagBits.alignmentBaseline != second.flagBits.alignmentBaseline)
            changingProperties.m_properties.set(CSSPropertyAlignmentBaseline);
        if (first.flagBits.baselineShift != second.flagBits.baselineShift)
            changingProperties.m_properties.set(CSSPropertyBaselineShift);
        if (first.flagBits.bufferedRendering != second.flagBits.bufferedRendering)
            changingProperties.m_properties.set(CSSPropertyBufferedRendering);
        if (first.flagBits.dominantBaseline != second.flagBits.dominantBaseline)
            changingProperties.m_properties.set(CSSPropertyDominantBaseline);
        if (first.flagBits.maskType != second.flagBits.maskType)
            changingProperties.m_properties.set(CSSPropertyMaskType);
        if (first.flagBits.vectorEffect != second.flagBits.vectorEffect)
            changingProperties.m_properties.set(CSSPropertyVectorEffect);
    };

    if (m_fillData.ptr() != other.m_fillData.ptr())
        conservativelyCollectChangedAnimatablePropertiesViaFillData(*m_fillData, *other.m_fillData);
    if (m_strokeData != other.m_strokeData)
        conservativelyCollectChangedAnimatablePropertiesViaStrokeData(*m_strokeData, *other.m_strokeData);
    if (m_stopData != other.m_stopData)
        conservativelyCollectChangedAnimatablePropertiesViaStopData(*m_stopData, *other.m_stopData);
    if (m_miscData != other.m_miscData)
        conservativelyCollectChangedAnimatablePropertiesViaMiscData(*m_miscData, *other.m_miscData);
    if (m_layoutData != other.m_layoutData)
        conservativelyCollectChangedAnimatablePropertiesViaLayoutData(*m_layoutData, *other.m_layoutData);
    if (m_inheritedResourceData != other.m_inheritedResourceData)
        conservativelyCollectChangedAnimatablePropertiesViaInheritedResourceData(*m_inheritedResourceData, *other.m_inheritedResourceData);
    if (m_inheritedFlags != other.m_inheritedFlags)
        conservativelyCollectChangedAnimatablePropertiesViaInheritedFlags(m_inheritedFlags, other.m_inheritedFlags);
    if (m_nonInheritedFlags != other.m_nonInheritedFlags)
        conservativelyCollectChangedAnimatablePropertiesViaNonInheritedFlags(m_nonInheritedFlags, other.m_nonInheritedFlags);
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
    LOG_IF_DIFFERENT_WITH_CAST(GlyphOrientation, glyphOrientationHorizontal);
    LOG_IF_DIFFERENT_WITH_CAST(GlyphOrientation, glyphOrientationVertical);
}

void SVGRenderStyle::NonInheritedFlags::dumpDifferences(TextStream& ts, const SVGRenderStyle::NonInheritedFlags& other) const
{
    LOG_IF_DIFFERENT_WITH_CAST(AlignmentBaseline, flagBits.alignmentBaseline);
    LOG_IF_DIFFERENT_WITH_CAST(DominantBaseline, flagBits.dominantBaseline);
    LOG_IF_DIFFERENT_WITH_CAST(BaselineShift, flagBits.baselineShift);
    LOG_IF_DIFFERENT_WITH_CAST(VectorEffect, flagBits.vectorEffect);
    LOG_IF_DIFFERENT_WITH_CAST(BufferedRendering, flagBits.bufferedRendering);
    LOG_IF_DIFFERENT_WITH_CAST(MaskType, flagBits.maskType);
}

void SVGRenderStyle::dumpDifferences(TextStream& ts, const SVGRenderStyle& other) const
{
    m_inheritedFlags.dumpDifferences(ts, other.m_inheritedFlags);
    m_nonInheritedFlags.dumpDifferences(ts, other.m_nonInheritedFlags);

    m_fillData->dumpDifferences(ts, other.m_fillData);
    m_strokeData->dumpDifferences(ts, other.m_strokeData);
    m_inheritedResourceData->dumpDifferences(ts, other.m_inheritedResourceData);

    m_stopData->dumpDifferences(ts, other.m_stopData);
    m_miscData->dumpDifferences(ts, other.m_miscData);
    m_layoutData->dumpDifferences(ts, other.m_layoutData);
}
#endif

}
