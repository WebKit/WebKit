/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGRenderStyleDefs_H
#define KSVG_SVGRenderStyleDefs_H

// Helper macros for 'SVGRenderStyle'
#define SVG_RS_DEFINE_ATTRIBUTE(Data, Type, Name, Initial) \
    void set##Type(Data val) { svg_noninherited_flags.f._##Name = val; } \
    Data Name() const { return (Data) svg_noninherited_flags.f._##Name; } \
    static Data initial##Type() { return Initial; }

#define SVG_RS_DEFINE_ATTRIBUTE_INHERITED(Data, Type, Name, Initial) \
    void set##Type(Data val) { svg_inherited_flags.f._##Name = val; } \
    Data Name() const { return (Data) svg_inherited_flags.f._##Name; } \
    static Data initial##Type() { return Initial; }

namespace KSVG
{
    enum EWindRule
    {
        WR_NONZERO = 0, WR_EVENODD = 1
    };

    enum ECapStyle
    {
        CS_BUTT = 1, CS_ROUND = 2, CS_SQUARE = 3
    };

    enum EJoinStyle
    {
        JS_MITER = 1, JS_ROUND = 2, JS_BEVEL = 3
    };

    enum ETextAnchor
    {
        TA_START, TA_MIDDLE, TA_END
    };

    enum EColorInterpolation
    {
        CI_AUTO, CI_SRGB, CI_LINEARRGB
    };

    enum EColorRendering
    {
        CR_AUTO, CR_OPTIMIZESPEED, CR_OPTIMIZEQUALITY
    };
    
    enum EImageRendering
    {
        IR_AUTO, IR_OPTIMIZESPEED, IR_OPTIMIZEQUALITY
    };

    enum EShapeRendering
    {
        SR_AUTO, SR_OPTIMIZESPEED, SR_CRISPEDGES, SR_GEOMETRICPRECISION
    };

    enum ETextRendering
    {
        TR_AUTO, TR_OPTIMIZESPEED, TR_OPTIMIZELEGIBILITY, TR_GEOMETRICPRECISION
    };

    enum EWritingMode
    {
        WM_LRTB, WM_LR, WM_RLTB, WM_RL, WM_TBRL, WM_TB
    };
    
    enum EAlignmentBaseline
    {
        AB_AUTO, AB_BASELINE, AB_BEFORE_EDGE, AB_TEXT_BEFORE_EDGE,
        AB_MIDDLE, AB_CENTRAL, AB_AFTER_EDGE, AB_TEXT_AFTER_EDGE,
        AB_IDEOGRAPHIC, AB_ALPHABETIC, AB_HANGING, AB_MATHEMATICAL
    };

    enum EDominantBaseline
    {
        DB_AUTO, DB_USE_SCRIPT, DB_NO_CHANGE, DB_RESET_SIZE,
        DB_IDEOGRAPHIC, DB_ALPHABETIC, DB_HANGING, DB_MATHEMATICAL,
        DB_CENTRAL, DB_MIDDLE, DB_TEXT_AFTER_EDGE, DB_TEXT_BEFORE_EDGE
    };

    enum EPointerEvents
    {
        PE_NONE, PE_STROKE, PE_FILL, PE_PAINTED, PE_VISIBLE,
        PE_VISIBLE_STROKE, PE_VISIBLE_FILL, PE_VISIBLE_PAINTED, PE_ALL
    };

    // Inherited/Non-Inherited Style Datastructures
    class StyleFillData : public KDOM::Shared<StyleFillData>
    {
    public:
        StyleFillData();
        StyleFillData(const StyleFillData &other);

        bool operator==(const StyleFillData &other) const;
        bool operator!=(const StyleFillData &other) const
        {
            return !(*this == other);
        }

        float opacity;
        SVGPaintImpl *paint;

    private:
        StyleFillData &operator=(const StyleFillData &);
    };

    class StyleStrokeData : public KDOM::Shared<StyleStrokeData>
    {
    public:
        StyleStrokeData();
        StyleStrokeData(const StyleStrokeData &other);

        bool operator==(const StyleStrokeData &other) const;
        bool operator!=(const StyleStrokeData &other) const
        {
            return !(*this == other);
        }

        float opacity;
        unsigned int miterLimit;

        KDOM::CSSValueImpl *width;
        KDOM::CSSValueImpl *dashOffset;

        SVGPaintImpl *paint;
        KDOM::CSSValueListImpl *dashArray;

    private:
        StyleStrokeData &operator=(const StyleStrokeData &);
    };

    class StyleStopData : public KDOM::Shared<StyleStopData>
    {
    public:
        StyleStopData();
        StyleStopData(const StyleStopData &other);

        bool operator==(const StyleStopData &other) const;
        bool operator!=(const StyleStopData &other) const
        {
            return !(*this == other);
        }

        float opacity;
        QColor color;

    private:
        StyleStopData &operator=(const StyleStopData &);
    };

    class StyleClipData : public KDOM::Shared<StyleClipData>
    {
    public:
        StyleClipData();
        StyleClipData(const StyleClipData &other);

        bool operator==(const StyleClipData &other) const;
        bool operator!=(const StyleClipData &other) const
        {
            return !(*this == other);
        }

        QString clipPath;

    private:
        StyleClipData &operator=(const StyleClipData &);
    };

    class StyleMaskData : public KDOM::Shared<StyleMaskData>
    {
    public:
        StyleMaskData();
        StyleMaskData(const StyleMaskData &other);

        bool operator==(const StyleMaskData &other) const;
        bool operator!=(const StyleMaskData &other) const { return !(*this == other); }

        QString maskElement;

    private:
        StyleMaskData &operator=(const StyleMaskData &);
    };

    class StyleMarkerData : public KDOM::Shared<StyleMarkerData>
    {
    public:
        StyleMarkerData();
        StyleMarkerData(const StyleMarkerData &other);

        bool operator==(const StyleMarkerData &other) const;
        bool operator!=(const StyleMarkerData &other) const
        {
            return !(*this == other);
        }

        QString startMarker;
        QString midMarker;
        QString endMarker;

    private:
        StyleMarkerData &operator=(const StyleMarkerData &);
    };

    // Note : the rule for this class is, *no inheritance* of these props
    class StyleMiscData : public KDOM::Shared<StyleMiscData>
    {
    public:
        StyleMiscData();
        StyleMiscData(const StyleMiscData &other);

        bool operator==(const StyleMiscData &other) const;
        bool operator!=(const StyleMiscData &other) const
        {
            return !(*this == other);
        }

        QString filter;
        QColor floodColor;
        float floodOpacity;

    private:
        StyleMiscData &operator=(const StyleMiscData &);
    };
};

#endif

// vim:ts=4:noet
