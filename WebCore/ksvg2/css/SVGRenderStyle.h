/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005, 2006 Apple Computer, Inc.

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

#ifndef KSVG_SVGRenderStyle_H
#define KSVG_SVGRenderStyle_H
#if SVG_SUPPORT

#include "Shared.h"
#include "DataRef.h"

#include <ksvg2/svg/SVGPaint.h>
#include <ksvg2/css/SVGRenderStyleDefs.h>

#if WIN32
typedef unsigned long long uint64_t;
#endif

// Helper macros for SVG's 'RenderStyle' properties
#define RS_DEFINE_ATTRIBUTE(Data, Type, Name, Initial) \
    void set##Type(Data val) { noninherited_flags.f._##Name = val; } \
    Data Name() const { return (Data) noninherited_flags.f._##Name; } \
    static Data initial##Type() { return Initial; }

#define RS_DEFINE_ATTRIBUTE_INHERITED(Data, Type, Name, Initial) \
    void set##Type(Data val) { inherited_flags.f._##Name = val; } \
    Data Name() const { return (Data) inherited_flags.f._##Name; } \
    static Data initial##Type() { return Initial; }

#define RS_DEFINE_ATTRIBUTE_DATAREF(Data, Group, Variable, Type, Name) \
    Data Name() const { return Group->Variable; } \
    void set##Type(Data obj) { RS_SET_VARIABLE(Group, Variable, obj) }

#define RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Data, Group, Variable, Type, Name, Initial) \
    RS_DEFINE_ATTRIBUTE_DATAREF(Data, Group, Variable, Type, Name) \
    static Data initial##Type() { return Initial; }

#define RS_SET_VARIABLE(Group, Variable, Value) \
    if(!(Group->Variable == Value)) \
        Group.access()->Variable = Value;

namespace WebCore
{
    class SVGRenderStyle : public Shared<SVGRenderStyle>
    {    
    public:
        SVGRenderStyle();
        SVGRenderStyle(bool); // Used to create the default style.
        SVGRenderStyle(const SVGRenderStyle &other);
        ~SVGRenderStyle();

        bool inheritedNotEqual(const SVGRenderStyle *other) const;

        void inheritFrom(const SVGRenderStyle *inheritParent);
        
        bool operator==(const SVGRenderStyle& o) const;
        bool operator!=(const SVGRenderStyle& o) const { return !(*this == o); }

        // SVG CSS Properties
        SVG_RS_DEFINE_ATTRIBUTE(EAlignmentBaseline, AlignmentBaseline, alignmentBaseline, AB_AUTO)
        SVG_RS_DEFINE_ATTRIBUTE(EDominantBaseline, DominantBaseline, dominantBaseline, DB_AUTO)

        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(ECapStyle, CapStyle, capStyle, CS_BUTT)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EWindRule, ClipRule, clipRule, WR_NONZERO)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EColorInterpolation, ColorInterpolation, colorInterpolation, CI_SRGB)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EColorInterpolation, ColorInterpolationFilters, colorInterpolationFilters, CI_LINEARRGB)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EColorRendering, ColorRendering, colorRendering, CR_AUTO)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EWindRule, FillRule, fillRule, WR_NONZERO)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EImageRendering, ImageRendering, imageRendering, IR_AUTO)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EJoinStyle, JoinStyle, joinStyle, JS_MITER)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EPointerEvents, PointerEvents, pointerEvents, PE_VISIBLE_PAINTED)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EShapeRendering, ShapeRendering, shapeRendering, SR_AUTO)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(ETextAnchor, TextAnchor, textAnchor, TA_START)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(ETextRendering, TextRendering, textRendering, TR_AUTO)
        SVG_RS_DEFINE_ATTRIBUTE_INHERITED(EWritingMode, WritingMode, writingMode, WM_LRTB)

        // SVG CSS Properties (using DataRef's)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, fill, opacity, FillOpacity, fillOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(SVGPaint *, fill, paint, FillPaint, fillPaint, 0)

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, stroke, opacity, StrokeOpacity, strokeOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(SVGPaint *, stroke, paint, StrokePaint, strokePaint, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(CSSValueList *, stroke, dashArray, StrokeDashArray, strokeDashArray, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(unsigned int, stroke, miterLimit, StrokeMiterLimit, strokeMiterLimit, 4)
        
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(CSSValue *, stroke, width, StrokeWidth, strokeWidth, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(CSSValue *, stroke, dashOffset, StrokeDashOffset, strokeDashOffset, 0);
    
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, stops, opacity, StopOpacity, stopOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Color, stops, color, StopColor, stopColor, Color(0, 0, 0))    
        
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(DeprecatedString, clip, clipPath, ClipPath, clipPath, DeprecatedString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(DeprecatedString, mask, maskElement, MaskElement, maskElement, DeprecatedString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(DeprecatedString, markers, startMarker, StartMarker, startMarker, DeprecatedString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(DeprecatedString, markers, midMarker, MidMarker, midMarker, DeprecatedString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(DeprecatedString, markers, endMarker, EndMarker, endMarker, DeprecatedString())

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(DeprecatedString, misc, filter, Filter, filter, DeprecatedString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, misc, floodOpacity, FloodOpacity, floodOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Color, misc, floodColor, FloodColor, floodColor, Color(0, 0, 0))    

    protected:
        // inherit
        struct InheritedFlags
        {
            // 64 bit inherited, don't add to the struct, or the operator will break.
            bool operator==(const InheritedFlags &other) const { return _iflags == other._iflags; }
            bool operator!=(const InheritedFlags &other) const { return _iflags != other._iflags; }

            union
            {
                struct
                {
                    unsigned _colorRendering : 2; // EColorRendering
                    unsigned _imageRendering : 2; // EImageRendering 
                    unsigned _shapeRendering : 2; // EShapeRendering 
                    unsigned _textRendering : 2; // ETextRendering
                    unsigned _clipRule : 1; // EWindRule
                    unsigned _fillRule : 1; // EWindRule
                    unsigned _capStyle : 2; // ECapStyle
                    unsigned _joinStyle : 2; // EJoinStyle
                    unsigned _textAnchor : 2; // ETextAnchor
                    unsigned _colorInterpolation : 2; // EColorInterpolation
                    unsigned _colorInterpolationFilters : 2; // EColorInterpolation
                    unsigned _pointerEvents : 4; // EPointerEvents
                    unsigned _writingMode : 3; // EWritingMode
                    // 5 bits unused
                } f;
                uint32_t _iflags;
            };
        } svg_inherited_flags;

        // don't inherit
        struct NonInheritedFlags
        {
            // 64 bit non-inherited, don't add to the struct, or the operator will break.
            bool operator==(const NonInheritedFlags &other) const { return _niflags == other._niflags; }
            bool operator!=(const NonInheritedFlags &other) const { return _niflags != other._niflags; }

            union
            {
                struct
                {
                    unsigned _alignmentBaseline : 4; // EAlignmentBaseline 
                    unsigned _dominantBaseline : 4; // EDominantBaseline
                    // 24 bits unused
                } f;
                uint32_t _niflags;
            };
        } svg_noninherited_flags;

        // inherited attributes
        DataRef<StyleFillData> fill;
        DataRef<StyleStrokeData> stroke;
        DataRef<StyleStopData> stops;
        DataRef<StyleMiscData> misc;
        DataRef<StyleMarkerData> markers;

        // non-inherited attributes
        DataRef<StyleClipData> clip;
        DataRef<StyleMaskData> mask;

        // static default style
        static SVGRenderStyle *s_defaultStyle;

    private:
        SVGRenderStyle(const SVGRenderStyle *) { }

        void setBitDefaults()
        {
            svg_inherited_flags.f._clipRule = initialClipRule();
            svg_inherited_flags.f._colorRendering = initialColorRendering();
            svg_inherited_flags.f._fillRule = initialFillRule();
            svg_inherited_flags.f._imageRendering = initialImageRendering();
            svg_inherited_flags.f._shapeRendering = initialShapeRendering();
            svg_inherited_flags.f._textRendering = initialTextRendering();
            svg_inherited_flags.f._textAnchor = initialTextAnchor();
            svg_inherited_flags.f._capStyle = initialCapStyle();
            svg_inherited_flags.f._joinStyle = initialJoinStyle();
            svg_inherited_flags.f._colorInterpolation = initialColorInterpolation();
            svg_inherited_flags.f._colorInterpolationFilters = initialColorInterpolationFilters();
            svg_inherited_flags.f._pointerEvents = initialPointerEvents();
            svg_inherited_flags.f._writingMode = initialWritingMode();

            svg_noninherited_flags.f._alignmentBaseline = initialAlignmentBaseline();
            svg_noninherited_flags.f._dominantBaseline = initialDominantBaseline();
        }
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
