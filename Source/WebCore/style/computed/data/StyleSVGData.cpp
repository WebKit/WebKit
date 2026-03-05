/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2010 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>

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
#include "StyleSVGData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

static const SVGData& defaultSVGData()
{
    static NeverDestroyed<DataRef<SVGData>> style(SVGData::createDefaultStyle());
    return *style.get();
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGData);

Ref<SVGData> SVGData::createDefaultStyle()
{
    return adoptRef(*new SVGData(CreateDefault));
}

SVGData::SVGData()
    : fillData(defaultSVGData().fillData)
    , strokeData(defaultSVGData().strokeData)
    , markerResourceData(defaultSVGData().markerResourceData)
    , stopData(defaultSVGData().stopData)
    , miscData(defaultSVGData().miscData)
    , layoutData(defaultSVGData().layoutData)
{
    setBitDefaults();
}

SVGData::SVGData(CreateDefaultType)
    : fillData(SVGFillData::create())
    , strokeData(SVGStrokeData::create())
    , markerResourceData(SVGMarkerResourceData::create())
    , stopData(SVGStopData::create())
    , miscData(SVGNonInheritedMiscData::create())
    , layoutData(SVGLayoutData::create())
{
    setBitDefaults();
}

inline SVGData::SVGData(const SVGData& other)
    : RefCounted<SVGData>()
    , inheritedFlags(other.inheritedFlags)
    , nonInheritedFlags(other.nonInheritedFlags)
    , fillData(other.fillData)
    , strokeData(other.strokeData)
    , markerResourceData(other.markerResourceData)
    , stopData(other.stopData)
    , miscData(other.miscData)
    , layoutData(other.layoutData)
{
    ASSERT(other == *this, "SVGData should be properly copied.");
}

Ref<SVGData> SVGData::copy() const
{
    return adoptRef(*new SVGData(*this));
}

SVGData::~SVGData() = default;

bool SVGData::operator==(const SVGData& other) const
{
    return inheritedEqual(other) && nonInheritedEqual(other);
}

void SVGData::setBitDefaults()
{
    inheritedFlags.clipRule = static_cast<unsigned>(ComputedStyle::initialClipRule());
    inheritedFlags.fillRule = static_cast<unsigned>(ComputedStyle::initialFillRule());
    inheritedFlags.shapeRendering = static_cast<unsigned>(ComputedStyle::initialShapeRendering());
    inheritedFlags.textAnchor = static_cast<unsigned>(ComputedStyle::initialTextAnchor());
    inheritedFlags.colorInterpolation = static_cast<unsigned>(ComputedStyle::initialColorInterpolation());
    inheritedFlags.colorInterpolationFilters = static_cast<unsigned>(ComputedStyle::initialColorInterpolationFilters());
    inheritedFlags.glyphOrientationHorizontal = static_cast<unsigned>(ComputedStyle::initialGlyphOrientationHorizontal());
    inheritedFlags.glyphOrientationVertical = static_cast<unsigned>(ComputedStyle::initialGlyphOrientationVertical());

    nonInheritedFlags.alignmentBaseline = static_cast<unsigned>(ComputedStyle::initialAlignmentBaseline());
    nonInheritedFlags.dominantBaseline = static_cast<unsigned>(ComputedStyle::initialDominantBaseline());
    nonInheritedFlags.vectorEffect = static_cast<unsigned>(ComputedStyle::initialVectorEffect());
    nonInheritedFlags.bufferedRendering = static_cast<unsigned>(ComputedStyle::initialBufferedRendering());
    nonInheritedFlags.maskType = static_cast<unsigned>(ComputedStyle::initialMaskType());
}

bool SVGData::inheritedEqual(const SVGData& other) const
{
    return fillData == other.fillData
        && strokeData == other.strokeData
        && markerResourceData == other.markerResourceData
        && inheritedFlags == other.inheritedFlags;
}

bool SVGData::nonInheritedEqual(const SVGData& other) const
{
    return stopData == other.stopData
        && miscData == other.miscData
        && layoutData == other.layoutData
        && nonInheritedFlags == other.nonInheritedFlags;
}

void SVGData::inheritFrom(const SVGData& other)
{
    fillData = other.fillData;
    strokeData = other.strokeData;
    markerResourceData = other.markerResourceData;

    inheritedFlags = other.inheritedFlags;
}

void SVGData::copyNonInheritedFrom(const SVGData& other)
{
    nonInheritedFlags = other.nonInheritedFlags;
    stopData = other.stopData;
    miscData = other.miscData;
    layoutData = other.layoutData;
}

#if !LOG_DISABLED

void SVGData::InheritedFlags::dumpDifferences(TextStream& ts, const SVGData::InheritedFlags& other) const
{
    LOG_IF_DIFFERENT_WITH_CAST(ShapeRendering, shapeRendering);
    LOG_IF_DIFFERENT_WITH_CAST(WindRule, clipRule);
    LOG_IF_DIFFERENT_WITH_CAST(WindRule, fillRule);
    LOG_IF_DIFFERENT_WITH_CAST(TextAnchor, textAnchor);
    LOG_IF_DIFFERENT_WITH_CAST(ColorInterpolation, colorInterpolation);
    LOG_IF_DIFFERENT_WITH_CAST(ColorInterpolation, colorInterpolationFilters);
    LOG_IF_DIFFERENT_WITH_CAST(SVGGlyphOrientationHorizontal, glyphOrientationHorizontal);
    LOG_IF_DIFFERENT_WITH_CAST(SVGGlyphOrientationVertical, glyphOrientationVertical);
}

void SVGData::NonInheritedFlags::dumpDifferences(TextStream& ts, const SVGData::NonInheritedFlags& other) const
{
    LOG_IF_DIFFERENT_WITH_CAST(AlignmentBaseline, alignmentBaseline);
    LOG_IF_DIFFERENT_WITH_CAST(DominantBaseline, dominantBaseline);
    LOG_IF_DIFFERENT_WITH_CAST(VectorEffect, vectorEffect);
    LOG_IF_DIFFERENT_WITH_CAST(BufferedRendering, bufferedRendering);
    LOG_IF_DIFFERENT_WITH_CAST(MaskType, maskType);
}

void SVGData::dumpDifferences(TextStream& ts, const SVGData& other) const
{
    inheritedFlags.dumpDifferences(ts, other.inheritedFlags);
    nonInheritedFlags.dumpDifferences(ts, other.nonInheritedFlags);

    fillData->dumpDifferences(ts, other.fillData);
    strokeData->dumpDifferences(ts, other.strokeData);
    markerResourceData->dumpDifferences(ts, other.markerResourceData);

    stopData->dumpDifferences(ts, other.stopData);
    miscData->dumpDifferences(ts, other.miscData);
    layoutData->dumpDifferences(ts, other.layoutData);
}
#endif

} // namespace Style
} // namespace WebCore
