/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005-2017 Apple Inc. All rights reserved.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.

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

#include "RenderStyleConstants.h"
#include "SVGRenderStyleDefs.h"
#include "WindRule.h"
#include <wtf/DataRef.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SVGRenderStyle);
class SVGRenderStyle : public RefCounted<SVGRenderStyle> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SVGRenderStyle);
public:
    static Ref<SVGRenderStyle> createDefaultStyle();
    static Ref<SVGRenderStyle> create() { return adoptRef(*new SVGRenderStyle); }
    Ref<SVGRenderStyle> copy() const;
    ~SVGRenderStyle();

    bool inheritedEqual(const SVGRenderStyle&) const;
    void inheritFrom(const SVGRenderStyle&);
    void copyNonInheritedFrom(const SVGRenderStyle&);

    StyleDifference diff(const SVGRenderStyle&) const;

    bool operator==(const SVGRenderStyle&) const;
    bool operator!=(const SVGRenderStyle& other) const { return !(*this == other); }

    // Initial values for all the properties
    static AlignmentBaseline initialAlignmentBaseline() { return AlignmentBaseline::Auto; }
    static DominantBaseline initialDominantBaseline() { return DominantBaseline::Auto; }
    static BaselineShift initialBaselineShift() { return BaselineShift::Baseline; }
    static VectorEffect initialVectorEffect() { return VectorEffect::None; }
    static BufferedRendering initialBufferedRendering() { return BufferedRendering::Auto; }
    static WindRule initialClipRule() { return WindRule::NonZero; }
    static ColorInterpolation initialColorInterpolation() { return ColorInterpolation::SRGB; }
    static ColorInterpolation initialColorInterpolationFilters() { return ColorInterpolation::LinearRGB; }
    static ColorRendering initialColorRendering() { return ColorRendering::Auto; }
    static WindRule initialFillRule() { return WindRule::NonZero; }
    static ShapeRendering initialShapeRendering() { return ShapeRendering::Auto; }
    static TextAnchor initialTextAnchor() { return TextAnchor::Start; }
    static GlyphOrientation initialGlyphOrientationHorizontal() { return GlyphOrientation::Degrees0; }
    static GlyphOrientation initialGlyphOrientationVertical() { return GlyphOrientation::Auto; }
    static float initialFillOpacity() { return 1; }
    static SVGPaintType initialFillPaintType() { return SVGPaintType::RGBColor; }
    static Color initialFillPaintColor() { return Color::black; }
    static String initialFillPaintUri() { return String(); }
    static float initialStrokeOpacity() { return 1; }
    static SVGPaintType initialStrokePaintType() { return SVGPaintType::None; }
    static Color initialStrokePaintColor() { return Color(); }
    static String initialStrokePaintUri() { return String(); }
    static Vector<SVGLengthValue> initialStrokeDashArray() { return { }; }
    static float initialStopOpacity() { return 1; }
    static Color initialStopColor() { return makeSimpleColor(0, 0, 0); }
    static float initialFloodOpacity() { return 1; }
    static Color initialFloodColor() { return makeSimpleColor(0, 0, 0); }
    static Color initialLightingColor() { return makeSimpleColor(255, 255, 255); }
    static ShadowData* initialShadow() { return nullptr; }
    static String initialMaskerResource() { return String(); }
    static String initialMarkerStartResource() { return String(); }
    static String initialMarkerMidResource() { return String(); }
    static String initialMarkerEndResource() { return String(); }
    static MaskType initialMaskType() { return MaskType::Luminance; }
    static SVGLengthValue initialBaselineShiftValue() { return SVGLengthValue(0, SVGLengthType::Number); }
    static SVGLengthValue initialKerning() { return SVGLengthValue(0, SVGLengthType::Number); }

    // SVG CSS Property setters
    void setAlignmentBaseline(AlignmentBaseline val) { m_nonInheritedFlags.flagBits.alignmentBaseline = static_cast<unsigned>(val); }
    void setDominantBaseline(DominantBaseline val) { m_nonInheritedFlags.flagBits.dominantBaseline = static_cast<unsigned>(val); }
    void setBaselineShift(BaselineShift val) { m_nonInheritedFlags.flagBits.baselineShift = static_cast<unsigned>(val); }
    void setVectorEffect(VectorEffect val) { m_nonInheritedFlags.flagBits.vectorEffect = static_cast<unsigned>(val); }
    void setBufferedRendering(BufferedRendering val) { m_nonInheritedFlags.flagBits.bufferedRendering = static_cast<unsigned>(val); }
    void setClipRule(WindRule val) { m_inheritedFlags.clipRule = static_cast<unsigned>(val); }
    void setColorInterpolation(ColorInterpolation val) { m_inheritedFlags.colorInterpolation = static_cast<unsigned>(val); }
    void setColorInterpolationFilters(ColorInterpolation val) { m_inheritedFlags.colorInterpolationFilters = static_cast<unsigned>(val); }
    void setColorRendering(ColorRendering val) { m_inheritedFlags.colorRendering = static_cast<unsigned>(val); }
    void setFillRule(WindRule val) { m_inheritedFlags.fillRule = static_cast<unsigned>(val); }
    void setShapeRendering(ShapeRendering val) { m_inheritedFlags.shapeRendering = static_cast<unsigned>(val); }
    void setTextAnchor(TextAnchor val) { m_inheritedFlags.textAnchor = static_cast<unsigned>(val); }
    void setGlyphOrientationHorizontal(GlyphOrientation val) { m_inheritedFlags.glyphOrientationHorizontal = static_cast<unsigned>(val); }
    void setGlyphOrientationVertical(GlyphOrientation val) { m_inheritedFlags.glyphOrientationVertical = static_cast<unsigned>(val); }
    void setMaskType(MaskType val) { m_nonInheritedFlags.flagBits.maskType = static_cast<unsigned>(val); }
    void setCx(const Length&);
    void setCy(const Length&);
    void setR(const Length&);
    void setRx(const Length&);
    void setRy(const Length&);
    void setX(const Length&);
    void setY(const Length&);
    void setFillOpacity(float);
    void setFillPaint(SVGPaintType, const Color&, const String& uri, bool applyToRegularStyle = true, bool applyToVisitedLinkStyle = false);
    void setStrokeOpacity(float);
    void setStrokePaint(SVGPaintType, const Color&, const String& uri, bool applyToRegularStyle = true, bool applyToVisitedLinkStyle = false);

    void setStrokeDashArray(const Vector<SVGLengthValue>&);
    void setStrokeDashOffset(const Length&);
    void setKerning(const SVGLengthValue&);
    void setStopOpacity(float);
    void setStopColor(const Color&);
    void setFloodOpacity(float);
    void setFloodColor(const Color&);
    void setLightingColor(const Color&);
    void setBaselineShiftValue(const SVGLengthValue&);

    void setShadow(std::unique_ptr<ShadowData>&& data) { m_shadowData.access().shadow = WTFMove(data); }

    // Setters for non-inherited resources
    void setMaskerResource(const String&);

    // Setters for inherited resources
    void setMarkerStartResource(const String&);
    void setMarkerMidResource(const String&);
    void setMarkerEndResource(const String&);

    // Read accessors for all the properties
    AlignmentBaseline alignmentBaseline() const { return static_cast<AlignmentBaseline>(m_nonInheritedFlags.flagBits.alignmentBaseline); }
    DominantBaseline dominantBaseline() const { return static_cast<DominantBaseline>(m_nonInheritedFlags.flagBits.dominantBaseline); }
    BaselineShift baselineShift() const { return static_cast<BaselineShift>(m_nonInheritedFlags.flagBits.baselineShift); }
    VectorEffect vectorEffect() const { return static_cast<VectorEffect>(m_nonInheritedFlags.flagBits.vectorEffect); }
    BufferedRendering bufferedRendering() const { return static_cast<BufferedRendering>(m_nonInheritedFlags.flagBits.bufferedRendering); }
    WindRule clipRule() const { return static_cast<WindRule>(m_inheritedFlags.clipRule); }
    ColorInterpolation colorInterpolation() const { return static_cast<ColorInterpolation>(m_inheritedFlags.colorInterpolation); }
    ColorInterpolation colorInterpolationFilters() const { return static_cast<ColorInterpolation>(m_inheritedFlags.colorInterpolationFilters); }
    ColorRendering colorRendering() const { return static_cast<ColorRendering>(m_inheritedFlags.colorRendering); }
    WindRule fillRule() const { return static_cast<WindRule>(m_inheritedFlags.fillRule); }
    ShapeRendering shapeRendering() const { return static_cast<ShapeRendering>(m_inheritedFlags.shapeRendering); }
    TextAnchor textAnchor() const { return static_cast<TextAnchor>(m_inheritedFlags.textAnchor); }
    GlyphOrientation glyphOrientationHorizontal() const { return static_cast<GlyphOrientation>(m_inheritedFlags.glyphOrientationHorizontal); }
    GlyphOrientation glyphOrientationVertical() const { return static_cast<GlyphOrientation>(m_inheritedFlags.glyphOrientationVertical); }
    float fillOpacity() const { return m_fillData->opacity; }
    SVGPaintType fillPaintType() const { return static_cast<SVGPaintType>(m_fillData->paintType); }
    const Color& fillPaintColor() const { return m_fillData->paintColor; }
    const String& fillPaintUri() const { return m_fillData->paintUri; }    
    float strokeOpacity() const { return m_strokeData->opacity; }
    SVGPaintType strokePaintType() const { return static_cast<SVGPaintType>(m_strokeData->paintType); }
    const Color& strokePaintColor() const { return m_strokeData->paintColor; }
    const String& strokePaintUri() const { return m_strokeData->paintUri; }
    Vector<SVGLengthValue> strokeDashArray() const { return m_strokeData->dashArray; }
    const Length& strokeDashOffset() const { return m_strokeData->dashOffset; }
    SVGLengthValue kerning() const { return m_textData->kerning; }
    float stopOpacity() const { return m_stopData->opacity; }
    const Color& stopColor() const { return m_stopData->color; }
    float floodOpacity() const { return m_miscData->floodOpacity; }
    const Color& floodColor() const { return m_miscData->floodColor; }
    const Color& lightingColor() const { return m_miscData->lightingColor; }
    SVGLengthValue baselineShiftValue() const { return m_miscData->baselineShiftValue; }
    ShadowData* shadow() const { return m_shadowData->shadow.get(); }
    const Length& cx() const { return m_layoutData->cx; }
    const Length& cy() const { return m_layoutData->cy; }
    const Length& r() const { return m_layoutData->r; }
    const Length& rx() const { return m_layoutData->rx; }
    const Length& ry() const { return m_layoutData->ry; }
    const Length& x() const { return m_layoutData->x; }
    const Length& y() const { return m_layoutData->y; }
    const String& maskerResource() const { return m_nonInheritedResourceData->masker; }
    const String& markerStartResource() const { return m_inheritedResourceData->markerStart; }
    const String& markerMidResource() const { return m_inheritedResourceData->markerMid; }
    const String& markerEndResource() const { return m_inheritedResourceData->markerEnd; }
    MaskType maskType() const { return static_cast<MaskType>(m_nonInheritedFlags.flagBits.maskType); }

    SVGPaintType visitedLinkFillPaintType() const { return static_cast<SVGPaintType>(m_fillData->visitedLinkPaintType); }
    const Color& visitedLinkFillPaintColor() const { return m_fillData->visitedLinkPaintColor; }
    const String& visitedLinkFillPaintUri() const { return m_fillData->visitedLinkPaintUri; }
    SVGPaintType visitedLinkStrokePaintType() const { return static_cast<SVGPaintType>(m_strokeData->visitedLinkPaintType); }
    const Color& visitedLinkStrokePaintColor() const { return m_strokeData->visitedLinkPaintColor; }
    const String& visitedLinkStrokePaintUri() const { return m_strokeData->visitedLinkPaintUri; }

    // convenience
    bool hasMasker() const { return !maskerResource().isEmpty(); }
    bool hasMarkers() const { return !markerStartResource().isEmpty() || !markerMidResource().isEmpty() || !markerEndResource().isEmpty(); }
    bool hasStroke() const { return strokePaintType() != SVGPaintType::None; }
    bool hasFill() const { return fillPaintType() != SVGPaintType::None; }
    bool isolatesBlending() const { return hasMasker() || shadow(); }

private:
    SVGRenderStyle();
    SVGRenderStyle(const SVGRenderStyle&);

    enum CreateDefaultType { CreateDefault };
    SVGRenderStyle(CreateDefaultType); // Used to create the default style.

    void setBitDefaults();

    struct InheritedFlags {
        bool operator==(const InheritedFlags&) const;
        bool operator!=(const InheritedFlags& other) const { return !(*this == other); }

        unsigned colorRendering : 2; // ColorRendering
        unsigned shapeRendering : 2; // ShapeRendering
        unsigned clipRule : 1; // WindRule
        unsigned fillRule : 1; // WindRule
        unsigned textAnchor : 2; // TextAnchor
        unsigned colorInterpolation : 2; // ColorInterpolation
        unsigned colorInterpolationFilters : 2; // ColorInterpolation
        unsigned glyphOrientationHorizontal : 3; // GlyphOrientation
        unsigned glyphOrientationVertical : 3; // GlyphOrientation
    };

    struct NonInheritedFlags {
        // 32 bit non-inherited, don't add to the struct, or the operator will break.
        bool operator==(const NonInheritedFlags& other) const { return flags == other.flags; }
        bool operator!=(const NonInheritedFlags& other) const { return flags != other.flags; }

        union {
            struct {
                unsigned alignmentBaseline : 4; // AlignmentBaseline
                unsigned dominantBaseline : 4; // DominantBaseline
                unsigned baselineShift : 2; // BaselineShift
                unsigned vectorEffect: 1; // VectorEffect
                unsigned bufferedRendering: 2; // BufferedRendering
                unsigned maskType: 1; // MaskType
                // 18 bits unused
            } flagBits;
            uint32_t flags;
        };
    };

    InheritedFlags m_inheritedFlags;
    NonInheritedFlags m_nonInheritedFlags;

    // inherited attributes
    DataRef<StyleFillData> m_fillData;
    DataRef<StyleStrokeData> m_strokeData;
    DataRef<StyleTextData> m_textData;
    DataRef<StyleInheritedResourceData> m_inheritedResourceData;

    // non-inherited attributes
    DataRef<StyleStopData> m_stopData;
    DataRef<StyleMiscData> m_miscData;
    DataRef<StyleShadowSVGData> m_shadowData;
    DataRef<StyleLayoutData> m_layoutData;
    DataRef<StyleResourceData> m_nonInheritedResourceData;
};

inline void SVGRenderStyle::setCx(const Length& length)
{
    if (!(m_layoutData->cx == length))
        m_layoutData.access().cx = length;
}

inline void SVGRenderStyle::setCy(const Length& length)
{
    if (!(m_layoutData->cy == length))
        m_layoutData.access().cy = length;
}

inline void SVGRenderStyle::setR(const Length& length)
{
    if (!(m_layoutData->r == length))
        m_layoutData.access().r = length;
}

inline void SVGRenderStyle::setRx(const Length& length)
{
    if (!(m_layoutData->rx == length))
        m_layoutData.access().rx = length;
}

inline void SVGRenderStyle::setRy(const Length& length)
{
    if (!(m_layoutData->ry == length))
        m_layoutData.access().ry = length;
}

inline void SVGRenderStyle::setX(const Length& length)
{
    if (!(m_layoutData->x == length))
        m_layoutData.access().x = length;
}

inline void SVGRenderStyle::setY(const Length& length)
{
    if (!(m_layoutData->y == length))
        m_layoutData.access().y = length;
}

inline void SVGRenderStyle::setFillOpacity(float opacity)
{
    if (!(m_fillData->opacity == opacity))
        m_fillData.access().opacity = opacity;
}

inline void SVGRenderStyle::setFillPaint(SVGPaintType type, const Color& color, const String& uri, bool applyToRegularStyle, bool applyToVisitedLinkStyle)
{
    if (applyToRegularStyle) {
        if (!(m_fillData->paintType == type))
            m_fillData.access().paintType = type;
        if (!(m_fillData->paintColor == color))
            m_fillData.access().paintColor = color;
        if (!(m_fillData->paintUri == uri))
            m_fillData.access().paintUri = uri;
    }
    if (applyToVisitedLinkStyle) {
        if (!(m_fillData->visitedLinkPaintType == type))
            m_fillData.access().visitedLinkPaintType = type;
        if (!(m_fillData->visitedLinkPaintColor == color))
            m_fillData.access().visitedLinkPaintColor = color;
        if (!(m_fillData->visitedLinkPaintUri == uri))
            m_fillData.access().visitedLinkPaintUri = uri;
    }
}

inline void SVGRenderStyle::setStrokeOpacity(float opacity)
{
    if (!(m_strokeData->opacity == opacity))
        m_strokeData.access().opacity = opacity;
}

inline void SVGRenderStyle::setStrokePaint(SVGPaintType type, const Color& color, const String& uri, bool applyToRegularStyle, bool applyToVisitedLinkStyle)
{
    if (applyToRegularStyle) {
        if (!(m_strokeData->paintType == type))
            m_strokeData.access().paintType = type;
        if (!(m_strokeData->paintColor == color))
            m_strokeData.access().paintColor = color;
        if (!(m_strokeData->paintUri == uri))
            m_strokeData.access().paintUri = uri;
    }
    if (applyToVisitedLinkStyle) {
        if (!(m_strokeData->visitedLinkPaintType == type))
            m_strokeData.access().visitedLinkPaintType = type;
        if (!(m_strokeData->visitedLinkPaintColor == color))
            m_strokeData.access().visitedLinkPaintColor = color;
        if (!(m_strokeData->visitedLinkPaintUri == uri))
            m_strokeData.access().visitedLinkPaintUri = uri;
    }
}

inline void SVGRenderStyle::setStrokeDashArray(const Vector<SVGLengthValue>& array)
{
    if (!(m_strokeData->dashArray == array))
        m_strokeData.access().dashArray = array;
}

inline void SVGRenderStyle::setStrokeDashOffset(const Length& offset)
{
    if (!(m_strokeData->dashOffset == offset))
        m_strokeData.access().dashOffset = offset;
}

inline void SVGRenderStyle::setKerning(const SVGLengthValue& kerning)
{
    if (!(m_textData->kerning == kerning))
        m_textData.access().kerning = kerning;
}

inline void SVGRenderStyle::setStopOpacity(float opacity)
{
    if (!(m_stopData->opacity == opacity))
        m_stopData.access().opacity = opacity;
}

inline void SVGRenderStyle::setStopColor(const Color& color)
{
    if (!(m_stopData->color == color))
        m_stopData.access().color = color;
}

inline void SVGRenderStyle::setFloodOpacity(float opacity)
{
    if (!(m_miscData->floodOpacity == opacity))
        m_miscData.access().floodOpacity = opacity;
}

inline void SVGRenderStyle::setFloodColor(const Color& color)
{
    if (!(m_miscData->floodColor == color))
        m_miscData.access().floodColor = color;
}

inline void SVGRenderStyle::setLightingColor(const Color& color)
{
    if (!(m_miscData->lightingColor == color))
        m_miscData.access().lightingColor = color;
}

inline void SVGRenderStyle::setBaselineShiftValue(const SVGLengthValue& shiftValue)
{
    if (!(m_miscData->baselineShiftValue == shiftValue))
        m_miscData.access().baselineShiftValue = shiftValue;
}

inline void SVGRenderStyle::setMaskerResource(const String& resource)
{
    if (!(m_nonInheritedResourceData->masker == resource))
        m_nonInheritedResourceData.access().masker = resource;
}

inline void SVGRenderStyle::setMarkerStartResource(const String& resource)
{
    if (!(m_inheritedResourceData->markerStart == resource))
        m_inheritedResourceData.access().markerStart = resource;
}

inline void SVGRenderStyle::setMarkerMidResource(const String& resource)
{
    if (!(m_inheritedResourceData->markerMid == resource))
        m_inheritedResourceData.access().markerMid = resource;
}

inline void SVGRenderStyle::setMarkerEndResource(const String& resource)
{
    if (!(m_inheritedResourceData->markerEnd == resource))
        m_inheritedResourceData.access().markerEnd = resource;
}

inline void SVGRenderStyle::setBitDefaults()
{
    m_inheritedFlags.clipRule = static_cast<unsigned>(initialClipRule());
    m_inheritedFlags.colorRendering = static_cast<unsigned>(initialColorRendering());
    m_inheritedFlags.fillRule = static_cast<unsigned>(initialFillRule());
    m_inheritedFlags.shapeRendering = static_cast<unsigned>(initialShapeRendering());
    m_inheritedFlags.textAnchor = static_cast<unsigned>(initialTextAnchor());
    m_inheritedFlags.colorInterpolation = static_cast<unsigned>(initialColorInterpolation());
    m_inheritedFlags.colorInterpolationFilters = static_cast<unsigned>(initialColorInterpolationFilters());
    m_inheritedFlags.glyphOrientationHorizontal = static_cast<unsigned>(initialGlyphOrientationHorizontal());
    m_inheritedFlags.glyphOrientationVertical = static_cast<unsigned>(initialGlyphOrientationVertical());

    m_nonInheritedFlags.flags = 0;
    m_nonInheritedFlags.flagBits.alignmentBaseline = static_cast<unsigned>(initialAlignmentBaseline());
    m_nonInheritedFlags.flagBits.dominantBaseline = static_cast<unsigned>(initialDominantBaseline());
    m_nonInheritedFlags.flagBits.baselineShift = static_cast<unsigned>(initialBaselineShift());
    m_nonInheritedFlags.flagBits.vectorEffect = static_cast<unsigned>(initialVectorEffect());
    m_nonInheritedFlags.flagBits.bufferedRendering = static_cast<unsigned>(initialBufferedRendering());
    m_nonInheritedFlags.flagBits.maskType = static_cast<unsigned>(initialMaskType());
}

inline bool SVGRenderStyle::InheritedFlags::operator==(const InheritedFlags& other) const
{
    return colorRendering == other.colorRendering
        && shapeRendering == other.shapeRendering
        && clipRule == other.clipRule
        && fillRule == other.fillRule
        && textAnchor == other.textAnchor
        && colorInterpolation == other.colorInterpolation
        && colorInterpolationFilters == other.colorInterpolationFilters
        && glyphOrientationHorizontal == other.glyphOrientationHorizontal
        && glyphOrientationVertical == other.glyphOrientationVertical;
}

} // namespace WebCore
