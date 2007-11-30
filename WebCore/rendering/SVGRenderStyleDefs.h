/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

    This file is part of the KDE project

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

#if ENABLE(SVG)
#include "Color.h"
#include "Path.h"
#include "PlatformString.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

// Helper macros for 'SVGRenderStyle'
#define SVG_RS_DEFINE_ATTRIBUTE(Data, Type, Name, Initial) \
    void set##Type(Data val) { svg_noninherited_flags.f._##Name = val; } \
    Data Name() const { return (Data) svg_noninherited_flags.f._##Name; } \
    static Data initial##Type() { return Initial; }

#define SVG_RS_DEFINE_ATTRIBUTE_INHERITED(Data, Type, Name, Initial) \
    void set##Type(Data val) { svg_inherited_flags._##Name = val; } \
    Data Name() const { return (Data) svg_inherited_flags._##Name; } \
    static Data initial##Type() { return Initial; }

// "Helper" macros for SVG's RenderStyle properties
// FIXME: These are impossible to work with or debug.
#define SVG_RS_DEFINE_ATTRIBUTE_DATAREF(Data, Group, Variable, Type, Name) \
    Data Name() const { return Group->Variable; } \
    void set##Type(Data obj) { SVG_RS_SET_VARIABLE(Group, Variable, obj) }

#define SVG_RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Data, Group, Variable, Type, Name, Initial) \
    SVG_RS_DEFINE_ATTRIBUTE_DATAREF(Data, Group, Variable, Type, Name) \
    static Data initial##Type() { return Initial; }

#define SVG_RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL_REFCOUNTED(Data, Group, Variable, Type, Name, Initial) \
    Data* Name() const { return Group->Variable.get(); } \
    void set##Type(PassRefPtr<Data> obj) { \
        if(!(Group->Variable == obj)) \
            Group.access()->Variable = obj; \
    } \
    static Data* initial##Type() { return Initial; }

#define SVG_RS_SET_VARIABLE(Group, Variable, Value) \
    if(!(Group->Variable == Value)) \
        Group.access()->Variable = Value;

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
    
    enum EImageRendering {
        IR_AUTO, IR_OPTIMIZESPEED, IR_OPTIMIZEQUALITY
    };

    enum EShapeRendering {
        SR_AUTO, SR_OPTIMIZESPEED, SR_CRISPEDGES, SR_GEOMETRICPRECISION
    };

    enum ETextRendering {
        TR_AUTO, TR_OPTIMIZESPEED, TR_OPTIMIZELEGIBILITY, TR_GEOMETRICPRECISION
    };

    enum EWritingMode {
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

    enum EPointerEvents {
        PE_NONE, PE_STROKE, PE_FILL, PE_PAINTED, PE_VISIBLE,
        PE_VISIBLE_STROKE, PE_VISIBLE_FILL, PE_VISIBLE_PAINTED, PE_ALL
    };
    
    class CSSValue;
    class CSSValueList;
    class SVGPaint;

    // Inherited/Non-Inherited Style Datastructures
    class StyleFillData : public RefCounted<StyleFillData> {
    public:
        StyleFillData();
        StyleFillData(const StyleFillData &other);

        bool operator==(const StyleFillData &other) const;
        bool operator!=(const StyleFillData &other) const
        {
            return !(*this == other);
        }

        float opacity;
        RefPtr<SVGPaint> paint;

    private:
        StyleFillData &operator=(const StyleFillData &);
    };

    class StyleStrokeData : public RefCounted<StyleStrokeData> {
    public:
        StyleStrokeData();
        StyleStrokeData(const StyleStrokeData&);

        bool operator==(const StyleStrokeData&) const;
        bool operator!=(const StyleStrokeData& other) const
        {
            return !(*this == other);
        }

        float opacity;
        float miterLimit;

        RefPtr<CSSValue> width;
        RefPtr<CSSValue> dashOffset;

        RefPtr<SVGPaint> paint;
        RefPtr<CSSValueList> dashArray;

    private:
        StyleStrokeData &operator=(const StyleStrokeData&);
    };

    class StyleStopData : public RefCounted<StyleStopData> {
    public:
        StyleStopData();
        StyleStopData(const StyleStopData &other);

        bool operator==(const StyleStopData &other) const;
        bool operator!=(const StyleStopData &other) const
        {
            return !(*this == other);
        }

        float opacity;
        Color color;

    private:
        StyleStopData &operator=(const StyleStopData &);
    };

    class StyleTextData : public RefCounted<StyleTextData> {
    public:
        StyleTextData();
        StyleTextData(const StyleTextData& other);

        bool operator==(const StyleTextData& other) const;
        bool operator!=(const StyleTextData& other) const
        {
            return !(*this == other);
        }

        RefPtr<CSSValue> kerning;

    private:
        StyleTextData& operator=(const StyleTextData&);
    };

    class StyleClipData : public RefCounted<StyleClipData> {
    public:
        StyleClipData();
        StyleClipData(const StyleClipData &other);

        bool operator==(const StyleClipData &other) const;
        bool operator!=(const StyleClipData &other) const
        {
            return !(*this == other);
        }

        String clipPath;

    private:
        StyleClipData &operator=(const StyleClipData &);
    };

    class StyleMaskData : public RefCounted<StyleMaskData> {
    public:
        StyleMaskData();
        StyleMaskData(const StyleMaskData &other);

        bool operator==(const StyleMaskData &other) const;
        bool operator!=(const StyleMaskData &other) const { return !(*this == other); }

        String maskElement;

    private:
        StyleMaskData &operator=(const StyleMaskData &);
    };

    class StyleMarkerData : public RefCounted<StyleMarkerData> {
    public:
        StyleMarkerData();
        StyleMarkerData(const StyleMarkerData &other);

        bool operator==(const StyleMarkerData &other) const;
        bool operator!=(const StyleMarkerData &other) const
        {
            return !(*this == other);
        }

        String startMarker;
        String midMarker;
        String endMarker;

    private:
        StyleMarkerData &operator=(const StyleMarkerData &);
    };

    // Note : the rule for this class is, *no inheritance* of these props
    class StyleMiscData : public RefCounted<StyleMiscData> {
    public:
        StyleMiscData();
        StyleMiscData(const StyleMiscData &other);

        bool operator==(const StyleMiscData &other) const;
        bool operator!=(const StyleMiscData &other) const
        {
            return !(*this == other);
        }

        String filter;
        Color floodColor;
        float floodOpacity;

        Color lightingColor;

        // non-inherited text stuff lives here not in StyleTextData.
        RefPtr<CSSValue> baselineShiftValue;

    private:
        StyleMiscData &operator=(const StyleMiscData &);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGRenderStyleDefs_h

// vim:ts=4:noet
