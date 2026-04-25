/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005-2017 Apple Inc. All rights reserved.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>

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

#pragma once

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleSVGGlyphOrientationHorizontal.h>
#include <WebCore/StyleSVGGlyphOrientationVertical.h>
#include <WebCore/WindRule.h>
#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

class SVGFillData;
class SVGLayoutData;
class SVGMarkerResourceData;
class SVGNonInheritedMiscData;
class SVGStopData;
class SVGStrokeData;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGData);
class SVGData : public RefCounted<SVGData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SVGData, SVGData);
public:
    static Ref<SVGData> createDefaultStyle();
    static Ref<SVGData> create() { return adoptRef(*new SVGData); }
    Ref<SVGData> copy() const;
    ~SVGData();

    bool inheritedEqual(const SVGData&) const;
    bool nonInheritedEqual(const SVGData&) const;

    void inheritFrom(const SVGData&);
    void copyNonInheritedFrom(const SVGData&);

    bool operator==(const SVGData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const SVGData&) const;
#endif

    struct InheritedFlags {
        bool operator==(const InheritedFlags&) const = default;

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const InheritedFlags&) const;
#endif

        PREFERRED_TYPE(ShapeRendering) unsigned shapeRendering : 2;
        PREFERRED_TYPE(WindRule) unsigned clipRule : 1;
        PREFERRED_TYPE(WindRule) unsigned fillRule : 1;
        PREFERRED_TYPE(TextAnchor) unsigned textAnchor : 2;
        PREFERRED_TYPE(ColorInterpolation) unsigned colorInterpolation : 2;
        PREFERRED_TYPE(ColorInterpolation) unsigned colorInterpolationFilters : 2;
        PREFERRED_TYPE(SVGGlyphOrientationHorizontal) unsigned glyphOrientationHorizontal : 2;
        PREFERRED_TYPE(SVGGlyphOrientationVertical) unsigned glyphOrientationVertical : 3;
    };

    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags&) const = default;

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const NonInheritedFlags&) const;
#endif

        PREFERRED_TYPE(AlignmentBaseline) unsigned alignmentBaseline : 4;
        PREFERRED_TYPE(DominantBaseline) unsigned dominantBaseline : 4;
        PREFERRED_TYPE(VectorEffect) unsigned vectorEffect : 1;
        PREFERRED_TYPE(BufferedRendering) unsigned bufferedRendering : 2;
        PREFERRED_TYPE(MaskType) unsigned maskType : 1;
    };

    InheritedFlags inheritedFlags;
    NonInheritedFlags nonInheritedFlags;

    // Inherited data
    DataRef<SVGFillData> fillData;
    DataRef<SVGStrokeData> strokeData;
    DataRef<SVGMarkerResourceData> markerResourceData;

    // Non-inherited data
    DataRef<SVGStopData> stopData;
    DataRef<SVGNonInheritedMiscData> miscData;
    DataRef<SVGLayoutData> layoutData;

private:
    SVGData();
    SVGData(const SVGData&);

    enum CreateDefaultType { CreateDefault };
    SVGData(CreateDefaultType); // Used to create the default style.

    void NODELETE setBitDefaults();
};

} // namespace Style
} // namespace WebCore
