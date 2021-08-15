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
#include "SVGElement.h"
#include <wtf/NeverDestroyed.h>

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
    , m_textData(defaultSVGStyle().m_textData)
    , m_inheritedResourceData(defaultSVGStyle().m_inheritedResourceData)
    , m_stopData(defaultSVGStyle().m_stopData)
    , m_miscData(defaultSVGStyle().m_miscData)
    , m_layoutData(defaultSVGStyle().m_layoutData)
    , m_nonInheritedResourceData(defaultSVGStyle().m_nonInheritedResourceData)
{
    setBitDefaults();
}

SVGRenderStyle::SVGRenderStyle(CreateDefaultType)
    : m_fillData(StyleFillData::create())
    , m_strokeData(StyleStrokeData::create())
    , m_textData(StyleTextData::create())
    , m_inheritedResourceData(StyleInheritedResourceData::create())
    , m_stopData(StyleStopData::create())
    , m_miscData(StyleMiscData::create())
    , m_layoutData(StyleLayoutData::create())
    , m_nonInheritedResourceData(StyleResourceData::create())
{
    setBitDefaults();
}

inline SVGRenderStyle::SVGRenderStyle(const SVGRenderStyle& other)
    : RefCounted<SVGRenderStyle>()
    , m_inheritedFlags(other.m_inheritedFlags)
    , m_nonInheritedFlags(other.m_nonInheritedFlags)
    , m_fillData(other.m_fillData)
    , m_strokeData(other.m_strokeData)
    , m_textData(other.m_textData)
    , m_inheritedResourceData(other.m_inheritedResourceData)
    , m_stopData(other.m_stopData)
    , m_miscData(other.m_miscData)
    , m_layoutData(other.m_layoutData)
    , m_nonInheritedResourceData(other.m_nonInheritedResourceData)
{
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
        && m_textData == other.m_textData
        && m_stopData == other.m_stopData
        && m_miscData == other.m_miscData
        && m_layoutData == other.m_layoutData
        && m_inheritedResourceData == other.m_inheritedResourceData
        && m_nonInheritedResourceData == other.m_nonInheritedResourceData
        && m_inheritedFlags == other.m_inheritedFlags
        && m_nonInheritedFlags == other.m_nonInheritedFlags;
}

bool SVGRenderStyle::inheritedEqual(const SVGRenderStyle& other) const
{
    return m_fillData == other.m_fillData
        && m_strokeData == other.m_strokeData
        && m_textData == other.m_textData
        && m_inheritedResourceData == other.m_inheritedResourceData
        && m_inheritedFlags == other.m_inheritedFlags;
}

void SVGRenderStyle::inheritFrom(const SVGRenderStyle& other)
{
    m_fillData = other.m_fillData;
    m_strokeData = other.m_strokeData;
    m_textData = other.m_textData;
    m_inheritedResourceData = other.m_inheritedResourceData;

    m_inheritedFlags = other.m_inheritedFlags;
}

void SVGRenderStyle::copyNonInheritedFrom(const SVGRenderStyle& other)
{
    m_nonInheritedFlags = other.m_nonInheritedFlags;
    m_stopData = other.m_stopData;
    m_miscData = other.m_miscData;
    m_layoutData = other.m_layoutData;
    m_nonInheritedResourceData = other.m_nonInheritedResourceData;
}

StyleDifference SVGRenderStyle::diff(const SVGRenderStyle& other) const
{
    // NOTE: All comparisions that may return StyleDifference::Layout have to go before those who return StyleDifference::Repaint

    // If kerning changes, we need a relayout, to force SVGCharacterData to be recalculated in the SVGRootInlineBox.
    if (m_textData != other.m_textData)
        return StyleDifference::Layout;

    // If resources change, we need a relayout, as the presence of resources influences the repaint rect.
    if (m_nonInheritedResourceData != other.m_nonInheritedResourceData)
        return StyleDifference::Layout;

    // If markers change, we need a relayout, as marker boundaries are cached in RenderSVGPath.
    if (m_inheritedResourceData != other.m_inheritedResourceData)
        return StyleDifference::Layout;

    // All text related properties influence layout.
    if (m_inheritedFlags.textAnchor != other.m_inheritedFlags.textAnchor
        || m_inheritedFlags.glyphOrientationHorizontal != other.m_inheritedFlags.glyphOrientationHorizontal
        || m_inheritedFlags.glyphOrientationVertical != other.m_inheritedFlags.glyphOrientationVertical
        || m_nonInheritedFlags.flagBits.alignmentBaseline != other.m_nonInheritedFlags.flagBits.alignmentBaseline
        || m_nonInheritedFlags.flagBits.dominantBaseline != other.m_nonInheritedFlags.flagBits.dominantBaseline
        || m_nonInheritedFlags.flagBits.baselineShift != other.m_nonInheritedFlags.flagBits.baselineShift)
        return StyleDifference::Layout;

    // Text related properties influence layout.
    bool miscNotEqual = m_miscData != other.m_miscData;
    if (miscNotEqual && m_miscData->baselineShiftValue != other.m_miscData->baselineShiftValue)
        return StyleDifference::Layout;

    // The x or y properties require relayout.
    if (m_layoutData != other.m_layoutData)
        return StyleDifference::Layout; 

    // Some stroke properties, requires relayouts, as the cached stroke boundaries need to be recalculated.
    if (m_strokeData != other.m_strokeData) {
        if (m_strokeData->paintType != other.m_strokeData->paintType
            || m_strokeData->paintColor != other.m_strokeData->paintColor
            || m_strokeData->paintUri != other.m_strokeData->paintUri
            || m_strokeData->dashArray != other.m_strokeData->dashArray
            || m_strokeData->dashOffset != other.m_strokeData->dashOffset
            || m_strokeData->visitedLinkPaintColor != other.m_strokeData->visitedLinkPaintColor
            || m_strokeData->visitedLinkPaintUri != other.m_strokeData->visitedLinkPaintUri
            || m_strokeData->visitedLinkPaintType != other.m_strokeData->visitedLinkPaintType)
            return StyleDifference::Layout;

        // Only the stroke-opacity case remains, where we only need a repaint.
        ASSERT(m_strokeData->opacity != other.m_strokeData->opacity);
        return StyleDifference::Repaint;
    }

    // vector-effect changes require a re-layout.
    if (m_nonInheritedFlags.flagBits.vectorEffect != other.m_nonInheritedFlags.flagBits.vectorEffect)
        return StyleDifference::Layout;

    // NOTE: All comparisions below may only return StyleDifference::Repaint

    // Painting related properties only need repaints. 
    if (miscNotEqual) {
        if (m_miscData->floodColor != other.m_miscData->floodColor
            || m_miscData->floodOpacity != other.m_miscData->floodOpacity
            || m_miscData->lightingColor != other.m_miscData->lightingColor)
            return StyleDifference::Repaint;
    }

    // If fill data changes, we just need to repaint. Fill boundaries are not influenced by this, only by the Path, that RenderSVGPath contains.
    if (m_fillData->paintType != other.m_fillData->paintType || m_fillData->paintColor != other.m_fillData->paintColor
        || m_fillData->paintUri != other.m_fillData->paintUri || m_fillData->opacity != other.m_fillData->opacity)
        return StyleDifference::Repaint;

    // If gradient stops change, we just need to repaint. Style updates are already handled through RenderSVGGradientSTop.
    if (m_stopData != other.m_stopData)
        return StyleDifference::Repaint;

    // Changes of these flags only cause repaints.
    if (m_inheritedFlags.colorRendering != other.m_inheritedFlags.colorRendering
        || m_inheritedFlags.shapeRendering != other.m_inheritedFlags.shapeRendering
        || m_inheritedFlags.clipRule != other.m_inheritedFlags.clipRule
        || m_inheritedFlags.fillRule != other.m_inheritedFlags.fillRule
        || m_inheritedFlags.colorInterpolation != other.m_inheritedFlags.colorInterpolation
        || m_inheritedFlags.colorInterpolationFilters != other.m_inheritedFlags.colorInterpolationFilters)
        return StyleDifference::Repaint;

    if (m_nonInheritedFlags.flagBits.bufferedRendering != other.m_nonInheritedFlags.flagBits.bufferedRendering)
        return StyleDifference::Repaint;

    if (m_nonInheritedFlags.flagBits.maskType != other.m_nonInheritedFlags.flagBits.maskType)
        return StyleDifference::Repaint;

    return StyleDifference::Equal;
}

}
