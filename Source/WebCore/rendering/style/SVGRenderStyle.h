/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005-2017 Apple Inc. All rights reserved.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2025 Samuel Weinig <sam@webkit.org>

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

#include <WebCore/RenderStyle.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/SVGRenderStyleDefs.h>
#include <WebCore/StyleSVGGlyphOrientationHorizontal.h>
#include <WebCore/StyleSVGGlyphOrientationVertical.h>
#include <WebCore/WindRule.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGRenderStyle);
class SVGRenderStyle : public RefCounted<SVGRenderStyle> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SVGRenderStyle, SVGRenderStyle);
public:
    static Ref<SVGRenderStyle> createDefaultStyle();
    static Ref<SVGRenderStyle> create() { return adoptRef(*new SVGRenderStyle); }
    Ref<SVGRenderStyle> copy() const;
    ~SVGRenderStyle();

    bool inheritedEqual(const SVGRenderStyle&) const;
    bool nonInheritedEqual(const SVGRenderStyle&) const;

    void inheritFrom(const SVGRenderStyle&);
    void copyNonInheritedFrom(const SVGRenderStyle&);

    bool changeRequiresRepaint(const SVGRenderStyle& other, bool currentColorDiffers) const;
    bool changeRequiresLayout(const SVGRenderStyle& other) const;

    bool operator==(const SVGRenderStyle&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const SVGRenderStyle&) const;
#endif

    void conservativelyCollectChangedAnimatableProperties(const SVGRenderStyle&, CSSPropertiesBitSet&) const;

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
        PREFERRED_TYPE(Style::SVGGlyphOrientationHorizontal) unsigned glyphOrientationHorizontal : 2;
        PREFERRED_TYPE(Style::SVGGlyphOrientationVertical) unsigned glyphOrientationVertical : 3;
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

    // inherited attributes
    DataRef<StyleFillData> fillData;
    DataRef<StyleStrokeData> strokeData;
    DataRef<StyleInheritedResourceData> inheritedResourceData;

    // non-inherited attributes
    DataRef<StyleStopData> stopData;
    DataRef<StyleMiscData> miscData;
    DataRef<StyleLayoutData> layoutData;

private:
    SVGRenderStyle();
    SVGRenderStyle(const SVGRenderStyle&);

    enum CreateDefaultType { CreateDefault };
    SVGRenderStyle(CreateDefaultType); // Used to create the default style.

    void setBitDefaults();
};

} // namespace WebCore
