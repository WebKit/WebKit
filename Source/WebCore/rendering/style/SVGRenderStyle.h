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

#include "DataRef.h"
#include "GraphicsTypes.h"
#include "Path.h"
#include "RenderStyleConstants.h"
#include "SVGRenderStyleDefs.h"

namespace WebCore {

class SVGRenderStyle : public RefCounted<SVGRenderStyle> {
public:
    static Ref<SVGRenderStyle> createDefaultStyle();
    static Ref<SVGRenderStyle> create() { return adoptRef(*new SVGRenderStyle); }
    Ref<SVGRenderStyle> copy() const;
    ~SVGRenderStyle();

    bool inheritedNotEqual(const SVGRenderStyle&) const;
    void inheritFrom(const SVGRenderStyle&);
    void copyNonInheritedFrom(const SVGRenderStyle&);

    StyleDifference diff(const SVGRenderStyle&) const;

    bool operator==(const SVGRenderStyle&) const;
    bool operator!=(const SVGRenderStyle& other) const { return !(*this == other); }

    // Initial values for all the properties
    static EAlignmentBaseline initialAlignmentBaseline() { return AB_AUTO; }
    static EDominantBaseline initialDominantBaseline() { return DB_AUTO; }
    static EBaselineShift initialBaselineShift() { return BS_BASELINE; }
    static EVectorEffect initialVectorEffect() { return VE_NONE; }
    static EBufferedRendering initialBufferedRendering() { return BR_AUTO; }
    static WindRule initialClipRule() { return RULE_NONZERO; }
    static EColorInterpolation initialColorInterpolation() { return CI_SRGB; }
    static EColorInterpolation initialColorInterpolationFilters() { return CI_LINEARRGB; }
    static EColorRendering initialColorRendering() { return CR_AUTO; }
    static WindRule initialFillRule() { return RULE_NONZERO; }
    static EShapeRendering initialShapeRendering() { return SR_AUTO; }
    static ETextAnchor initialTextAnchor() { return TA_START; }
    static EGlyphOrientation initialGlyphOrientationHorizontal() { return GO_0DEG; }
    static EGlyphOrientation initialGlyphOrientationVertical() { return GO_AUTO; }
    static float initialFillOpacity() { return 1; }
    static SVGPaintType initialFillPaintType() { return SVG_PAINTTYPE_RGBCOLOR; }
    static Color initialFillPaintColor() { return Color::black; }
    static String initialFillPaintUri() { return String(); }
    static float initialStrokeOpacity() { return 1; }
    static SVGPaintType initialStrokePaintType() { return SVG_PAINTTYPE_NONE; }
    static Color initialStrokePaintColor() { return Color(); }
    static String initialStrokePaintUri() { return String(); }
    static Vector<SVGLengthValue> initialStrokeDashArray() { return { }; }
    static float initialStrokeMiterLimit() { return 4; }
    static float initialStopOpacity() { return 1; }
    static Color initialStopColor() { return Color(0, 0, 0); }
    static float initialFloodOpacity() { return 1; }
    static Color initialFloodColor() { return Color(0, 0, 0); }
    static Color initialLightingColor() { return Color(255, 255, 255); }
    static ShadowData* initialShadow() { return nullptr; }
    static String initialClipperResource() { return String(); }
    static String initialMaskerResource() { return String(); }
    static String initialMarkerStartResource() { return String(); }
    static String initialMarkerMidResource() { return String(); }
    static String initialMarkerEndResource() { return String(); }
    static EMaskType initialMaskType() { return MT_LUMINANCE; }
    static SVGLengthValue initialBaselineShiftValue();
    static SVGLengthValue initialKerning();

    // SVG CSS Property setters
    void setAlignmentBaseline(EAlignmentBaseline val) { m_nonInheritedFlags.flagBits.alignmentBaseline = val; }
    void setDominantBaseline(EDominantBaseline val) { m_nonInheritedFlags.flagBits.dominantBaseline = val; }
    void setBaselineShift(EBaselineShift val) { m_nonInheritedFlags.flagBits.baselineShift = val; }
    void setVectorEffect(EVectorEffect val) { m_nonInheritedFlags.flagBits.vectorEffect = val; }
    void setBufferedRendering(EBufferedRendering val) { m_nonInheritedFlags.flagBits.bufferedRendering = val; }
    void setClipRule(WindRule val) { m_inheritedFlags.clipRule = val; }
    void setColorInterpolation(EColorInterpolation val) { m_inheritedFlags.colorInterpolation = val; }
    void setColorInterpolationFilters(EColorInterpolation val) { m_inheritedFlags.colorInterpolationFilters = val; }
    void setColorRendering(EColorRendering val) { m_inheritedFlags.colorRendering = val; }
    void setFillRule(WindRule val) { m_inheritedFlags.fillRule = val; }
    void setShapeRendering(EShapeRendering val) { m_inheritedFlags.shapeRendering = val; }
    void setTextAnchor(ETextAnchor val) { m_inheritedFlags.textAnchor = val; }
    void setGlyphOrientationHorizontal(EGlyphOrientation val) { m_inheritedFlags.glyphOrientationHorizontal = val; }
    void setGlyphOrientationVertical(EGlyphOrientation val) { m_inheritedFlags.glyphOrientationVertical = val; }
    void setMaskType(EMaskType val) { m_nonInheritedFlags.flagBits.maskType = val; }
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
    void setStrokeMiterLimit(float);
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
    void setClipperResource(const String&);
    void setMaskerResource(const String&);

    // Setters for inherited resources
    void setMarkerStartResource(const String&);
    void setMarkerMidResource(const String&);
    void setMarkerEndResource(const String&);

    // Read accessors for all the properties
    EAlignmentBaseline alignmentBaseline() const { return (EAlignmentBaseline) m_nonInheritedFlags.flagBits.alignmentBaseline; }
    EDominantBaseline dominantBaseline() const { return (EDominantBaseline) m_nonInheritedFlags.flagBits.dominantBaseline; }
    EBaselineShift baselineShift() const { return (EBaselineShift) m_nonInheritedFlags.flagBits.baselineShift; }
    EVectorEffect vectorEffect() const { return (EVectorEffect) m_nonInheritedFlags.flagBits.vectorEffect; }
    EBufferedRendering bufferedRendering() const { return (EBufferedRendering) m_nonInheritedFlags.flagBits.bufferedRendering; }
    WindRule clipRule() const { return (WindRule) m_inheritedFlags.clipRule; }
    EColorInterpolation colorInterpolation() const { return (EColorInterpolation) m_inheritedFlags.colorInterpolation; }
    EColorInterpolation colorInterpolationFilters() const { return (EColorInterpolation) m_inheritedFlags.colorInterpolationFilters; }
    EColorRendering colorRendering() const { return (EColorRendering) m_inheritedFlags.colorRendering; }
    WindRule fillRule() const { return (WindRule) m_inheritedFlags.fillRule; }
    EShapeRendering shapeRendering() const { return (EShapeRendering) m_inheritedFlags.shapeRendering; }
    ETextAnchor textAnchor() const { return (ETextAnchor) m_inheritedFlags.textAnchor; }
    EGlyphOrientation glyphOrientationHorizontal() const { return (EGlyphOrientation) m_inheritedFlags.glyphOrientationHorizontal; }
    EGlyphOrientation glyphOrientationVertical() const { return (EGlyphOrientation) m_inheritedFlags.glyphOrientationVertical; }
    float fillOpacity() const { return m_fillData->opacity; }
    const SVGPaintType& fillPaintType() const { return m_fillData->paintType; }
    const Color& fillPaintColor() const { return m_fillData->paintColor; }
    const String& fillPaintUri() const { return m_fillData->paintUri; }    
    float strokeOpacity() const { return m_strokeData->opacity; }
    const SVGPaintType& strokePaintType() const { return m_strokeData->paintType; }
    const Color& strokePaintColor() const { return m_strokeData->paintColor; }
    const String& strokePaintUri() const { return m_strokeData->paintUri; }
    Vector<SVGLengthValue> strokeDashArray() const { return m_strokeData->dashArray; }
    float strokeMiterLimit() const { return m_strokeData->miterLimit; }
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
    const String& clipperResource() const { return m_nonInheritedResourceData->clipper; }
    const String& maskerResource() const { return m_nonInheritedResourceData->masker; }
    const String& markerStartResource() const { return m_inheritedResourceData->markerStart; }
    const String& markerMidResource() const { return m_inheritedResourceData->markerMid; }
    const String& markerEndResource() const { return m_inheritedResourceData->markerEnd; }
    EMaskType maskType() const { return (EMaskType) m_nonInheritedFlags.flagBits.maskType; }

    const SVGPaintType& visitedLinkFillPaintType() const { return m_fillData->visitedLinkPaintType; }
    const Color& visitedLinkFillPaintColor() const { return m_fillData->visitedLinkPaintColor; }
    const String& visitedLinkFillPaintUri() const { return m_fillData->visitedLinkPaintUri; }
    const SVGPaintType& visitedLinkStrokePaintType() const { return m_strokeData->visitedLinkPaintType; }
    const Color& visitedLinkStrokePaintColor() const { return m_strokeData->visitedLinkPaintColor; }
    const String& visitedLinkStrokePaintUri() const { return m_strokeData->visitedLinkPaintUri; }

    // convenience
    bool hasClipper() const { return !clipperResource().isEmpty(); }
    bool hasMasker() const { return !maskerResource().isEmpty(); }
    bool hasMarkers() const { return !markerStartResource().isEmpty() || !markerMidResource().isEmpty() || !markerEndResource().isEmpty(); }
    bool hasStroke() const { return strokePaintType() != SVG_PAINTTYPE_NONE; }
    bool hasFill() const { return fillPaintType() != SVG_PAINTTYPE_NONE; }
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

        unsigned colorRendering : 2; // EColorRendering
        unsigned shapeRendering : 2; // EShapeRendering
        unsigned clipRule : 1; // WindRule
        unsigned fillRule : 1; // WindRule
        unsigned textAnchor : 2; // ETextAnchor
        unsigned colorInterpolation : 2; // EColorInterpolation
        unsigned colorInterpolationFilters : 2; // EColorInterpolation
        unsigned glyphOrientationHorizontal : 3; // EGlyphOrientation
        unsigned glyphOrientationVertical : 3; // EGlyphOrientation
    };

    struct NonInheritedFlags {
        // 32 bit non-inherited, don't add to the struct, or the operator will break.
        bool operator==(const NonInheritedFlags& other) const { return flags == other.flags; }
        bool operator!=(const NonInheritedFlags& other) const { return flags != other.flags; }

        union {
            struct {
                unsigned alignmentBaseline : 4; // EAlignmentBaseline
                unsigned dominantBaseline : 4; // EDominantBaseline
                unsigned baselineShift : 2; // EBaselineShift
                unsigned vectorEffect: 1; // EVectorEffect
                unsigned bufferedRendering: 2; // EBufferedRendering
                unsigned maskType: 1; // EMaskType
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

inline SVGLengthValue SVGRenderStyle::initialBaselineShiftValue()
{
    SVGLengthValue length;
    length.newValueSpecifiedUnits(LengthTypeNumber, 0);
    return length;
}

inline SVGLengthValue SVGRenderStyle::initialKerning()
{
    SVGLengthValue length;
    length.newValueSpecifiedUnits(LengthTypeNumber, 0);
    return length;
}

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

inline void SVGRenderStyle::setStrokeMiterLimit(float limit)
{
    if (!(m_strokeData->miterLimit == limit))
        m_strokeData.access().miterLimit = limit;
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

inline void SVGRenderStyle::setClipperResource(const String& resource)
{
    if (!(m_nonInheritedResourceData->clipper == resource))
        m_nonInheritedResourceData.access().clipper = resource;
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
    m_inheritedFlags.clipRule = initialClipRule();
    m_inheritedFlags.colorRendering = initialColorRendering();
    m_inheritedFlags.fillRule = initialFillRule();
    m_inheritedFlags.shapeRendering = initialShapeRendering();
    m_inheritedFlags.textAnchor = initialTextAnchor();
    m_inheritedFlags.colorInterpolation = initialColorInterpolation();
    m_inheritedFlags.colorInterpolationFilters = initialColorInterpolationFilters();
    m_inheritedFlags.glyphOrientationHorizontal = initialGlyphOrientationHorizontal();
    m_inheritedFlags.glyphOrientationVertical = initialGlyphOrientationVertical();

    m_nonInheritedFlags.flags = 0;
    m_nonInheritedFlags.flagBits.alignmentBaseline = initialAlignmentBaseline();
    m_nonInheritedFlags.flagBits.dominantBaseline = initialDominantBaseline();
    m_nonInheritedFlags.flagBits.baselineShift = initialBaselineShift();
    m_nonInheritedFlags.flagBits.vectorEffect = initialVectorEffect();
    m_nonInheritedFlags.flagBits.bufferedRendering = initialBufferedRendering();
    m_nonInheritedFlags.flagBits.maskType = initialMaskType();
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
