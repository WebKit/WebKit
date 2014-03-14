/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

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

#ifndef SVGRenderStyleDefs_h
#define SVGRenderStyleDefs_h

#include "SVGLength.h"
#include "SVGPaint.h"
#include "ShadowData.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    enum EBaselineShift {
        BS_BASELINE, BS_SUB, BS_SUPER, BS_LENGTH
    };

    enum ETextAnchor {
        TA_START, TA_MIDDLE, TA_END
    };

    enum EColorInterpolation {
        CI_AUTO, CI_SRGB, CI_LINEARRGB
    };

    enum EColorRendering {
        CR_AUTO, CR_OPTIMIZESPEED, CR_OPTIMIZEQUALITY
    };
    enum EShapeRendering {
        SR_AUTO, SR_OPTIMIZESPEED, SR_CRISPEDGES, SR_GEOMETRICPRECISION
    };

    enum SVGWritingMode {
        WM_LRTB, WM_LR, WM_RLTB, WM_RL, WM_TBRL, WM_TB
    };

    enum EGlyphOrientation {
        GO_0DEG, GO_90DEG, GO_180DEG, GO_270DEG, GO_AUTO
    };

    enum EAlignmentBaseline {
        AB_AUTO, AB_BASELINE, AB_BEFORE_EDGE, AB_TEXT_BEFORE_EDGE,
        AB_MIDDLE, AB_CENTRAL, AB_AFTER_EDGE, AB_TEXT_AFTER_EDGE,
        AB_IDEOGRAPHIC, AB_ALPHABETIC, AB_HANGING, AB_MATHEMATICAL
    };

    enum EDominantBaseline {
        DB_AUTO, DB_USE_SCRIPT, DB_NO_CHANGE, DB_RESET_SIZE,
        DB_IDEOGRAPHIC, DB_ALPHABETIC, DB_HANGING, DB_MATHEMATICAL,
        DB_CENTRAL, DB_MIDDLE, DB_TEXT_AFTER_EDGE, DB_TEXT_BEFORE_EDGE
    };
    
    enum EVectorEffect {
        VE_NONE,
        VE_NON_SCALING_STROKE
    };

    enum EBufferedRendering {
        BR_AUTO,
        BR_DYNAMIC,
        BR_STATIC
    };

    enum EMaskType {
        MT_LUMINANCE,
        MT_ALPHA
    };

    // These are all minimized combinations of paint-order.
    enum PaintOrder {
        PaintOrderNormal = 0,
        PaintOrderFill = 1,
        PaintOrderFillMarkers = 2,
        PaintOrderStroke = 3,
        PaintOrderStrokeMarkers = 4,
        PaintOrderMarkers = 5,
        PaintOrderMarkersStroke = 6
    };

    enum PaintType {
        PaintTypeFill,
        PaintTypeStroke,
        PaintTypeMarkers
    };

    class CSSValue;
    class CSSValueList;
    class SVGPaint;

    // Inherited/Non-Inherited Style Datastructures
    class StyleFillData : public RefCounted<StyleFillData> {
    public:
        static PassRef<StyleFillData> create() { return adoptRef(*new StyleFillData); }
        PassRef<StyleFillData> copy() const;

        bool operator==(const StyleFillData&) const;
        bool operator!=(const StyleFillData& other) const
        {
            return !(*this == other);
        }

        float opacity;
        SVGPaint::SVGPaintType paintType;
        Color paintColor;
        String paintUri;
        SVGPaint::SVGPaintType visitedLinkPaintType;
        Color visitedLinkPaintColor;
        String visitedLinkPaintUri;

    private:
        StyleFillData();
        StyleFillData(const StyleFillData&);
    };

    class StyleStrokeData : public RefCounted<StyleStrokeData> {
    public:
        static PassRef<StyleStrokeData> create() { return adoptRef(*new StyleStrokeData); }
        PassRef<StyleStrokeData> copy() const;

        bool operator==(const StyleStrokeData&) const;
        bool operator!=(const StyleStrokeData& other) const
        {
            return !(*this == other);
        }

        float opacity;
        float miterLimit;

        SVGLength width;
        SVGLength dashOffset;
        Vector<SVGLength> dashArray;

        SVGPaint::SVGPaintType paintType;
        Color paintColor;
        String paintUri;
        SVGPaint::SVGPaintType visitedLinkPaintType;
        Color visitedLinkPaintColor;
        String visitedLinkPaintUri;

    private:        
        StyleStrokeData();
        StyleStrokeData(const StyleStrokeData&);
    };

    class StyleStopData : public RefCounted<StyleStopData> {
    public:
        static PassRef<StyleStopData> create() { return adoptRef(*new StyleStopData); }
        PassRef<StyleStopData> copy() const;

        bool operator==(const StyleStopData&) const;
        bool operator!=(const StyleStopData& other) const
        {
            return !(*this == other);
        }

        float opacity;
        Color color;

    private:        
        StyleStopData();
        StyleStopData(const StyleStopData&);
    };

    class StyleTextData : public RefCounted<StyleTextData> {
    public:
        static PassRef<StyleTextData> create() { return adoptRef(*new StyleTextData); }
        PassRef<StyleTextData> copy() const;
        
        bool operator==(const StyleTextData& other) const;
        bool operator!=(const StyleTextData& other) const
        {
            return !(*this == other);
        }

        SVGLength kerning;

    private:
        StyleTextData();
        StyleTextData(const StyleTextData&);
    };

    // Note: the rule for this class is, *no inheritance* of these props
    class StyleMiscData : public RefCounted<StyleMiscData> {
    public:
        static PassRef<StyleMiscData> create() { return adoptRef(*new StyleMiscData); }
        PassRef<StyleMiscData> copy() const;

        bool operator==(const StyleMiscData&) const;
        bool operator!=(const StyleMiscData& other) const
        {
            return !(*this == other);
        }

        Color floodColor;
        float floodOpacity;
        Color lightingColor;

        // non-inherited text stuff lives here not in StyleTextData.
        SVGLength baselineShiftValue;

    private:
        StyleMiscData();
        StyleMiscData(const StyleMiscData&);
    };

    class StyleShadowSVGData : public RefCounted<StyleShadowSVGData> {
    public:
        static PassRef<StyleShadowSVGData> create() { return adoptRef(*new StyleShadowSVGData); }
        PassRef<StyleShadowSVGData> copy() const;

        bool operator==(const StyleShadowSVGData&) const;
        bool operator!=(const StyleShadowSVGData& other) const
        {
            return !(*this == other);
        }

        std::unique_ptr<ShadowData> shadow;

    private:
        StyleShadowSVGData();
        StyleShadowSVGData(const StyleShadowSVGData&);
    };

    // Non-inherited resources
    class StyleResourceData : public RefCounted<StyleResourceData> {
    public:
        static PassRef<StyleResourceData> create() { return adoptRef(*new StyleResourceData); }
        PassRef<StyleResourceData> copy() const;

        bool operator==(const StyleResourceData&) const;
        bool operator!=(const StyleResourceData& other) const
        {
            return !(*this == other);
        }

        String clipper;
        String filter;
        String masker;

    private:
        StyleResourceData();
        StyleResourceData(const StyleResourceData&);
    };

    // Inherited resources
    class StyleInheritedResourceData : public RefCounted<StyleInheritedResourceData> {
    public:
        static PassRef<StyleInheritedResourceData> create() { return adoptRef(*new StyleInheritedResourceData); }
        PassRef<StyleInheritedResourceData> copy() const;

        bool operator==(const StyleInheritedResourceData&) const;
        bool operator!=(const StyleInheritedResourceData& other) const
        {
            return !(*this == other);
        }

        String markerStart;
        String markerMid;
        String markerEnd;

    private:
        StyleInheritedResourceData();
        StyleInheritedResourceData(const StyleInheritedResourceData&);
    };

} // namespace WebCore

#endif // SVGRenderStyleDefs_h
