/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include <qrect.h>

#include <ksvg2/impl/SVGPaintImpl.h>
#include <kdom/css/impl/RenderStyle.h>
#include <ksvg2/css/impl/SVGRenderStyleDefs.h>

namespace KSVG
{
    class SVGRenderStyle : public KDOM::RenderStyle
    {    
    public:
        SVGRenderStyle();
        SVGRenderStyle(bool); // Used to create the default style.
        SVGRenderStyle(const SVGRenderStyle &other);
        virtual ~SVGRenderStyle();

        virtual bool equals(KDOM::RenderStyle *other) const;

        static void cleanup();

        virtual void inheritFrom(const RenderStyle *inheritParent);

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

        // SVG CSS Properties (using DataRef's)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, fill, opacity, FillOpacity, fillOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(SVGPaintImpl *, fill, paint, FillPaint, fillPaint, 0)

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, stroke, opacity, StrokeOpacity, strokeOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(SVGPaintImpl *, stroke, paint, StrokePaint, strokePaint, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(KDOM::CSSValueListImpl *, stroke, dashArray, StrokeDashArray, strokeDashArray, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(unsigned int, stroke, miterLimit, StrokeMiterLimit, strokeMiterLimit, 4)
        
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(KDOM::CSSValueImpl *, stroke, width, StrokeWidth, strokeWidth, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(KDOM::CSSValueImpl *, stroke, dashOffset, StrokeDashOffset, strokeDashOffset, 0);
    
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, stops, opacity, StopOpacity, stopOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QColor, stops, color, StopColor, stopColor, QColor(Qt::black))    
        
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QString, clip, clipPath, ClipPath, clipPath, QString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QString, markers, startMarker, StartMarker, startMarker, QString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QString, markers, midMarker, MidMarker, midMarker, QString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QString, markers, endMarker, EndMarker, endMarker, QString())

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, misc, opacity, Opacity, opacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QString, misc, filter, Filter, filter, QString())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, misc, floodOpacity, FloodOpacity, floodOpacity, 1.0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QColor, misc, floodColor, FloodColor, floodColor, QColor(Qt::black))    

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
                    EColorRendering _colorRendering : 2;
                    EImageRendering _imageRendering : 2;
                    EShapeRendering _shapeRendering : 2;
                    ETextRendering  _textRendering : 2;
                    EWindRule _clipRule : 1; 
                    EWindRule _fillRule : 1;
                    ECapStyle _capStyle : 2;
                    EJoinStyle _joinStyle : 2;
                    ETextAnchor _textAnchor : 2;
                    EColorInterpolation _colorInterpolation : 2;
                    EColorInterpolation _colorInterpolationFilters : 2;
                    EPointerEvents _pointerEvents : 4;

                    unsigned int unused : 32;
                    unsigned int unused2 : 8;
                } f;
                Q_UINT64 _iflags;
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
                    EAlignmentBaseline _alignmentBaseline : 4;
                    EDominantBaseline _dominantBaseline : 4;

                    unsigned int unused : 32;
                    unsigned int unused2 : 24;
                } f;
                Q_UINT64 _niflags;
            };
        } svg_noninherited_flags;

        // inherited attributes
        KDOM::DataRef<StyleFillData> fill;
        KDOM::DataRef<StyleStrokeData> stroke;
        KDOM::DataRef<StyleStopData> stops;
        KDOM::DataRef<StyleMiscData> misc;
        KDOM::DataRef<StyleMarkerData> markers;

        // non-inherited attributes
        KDOM::DataRef<StyleClipData> clip;

        // static default style
        static SVGRenderStyle *s_defaultStyle;

    private:
        SVGRenderStyle(const SVGRenderStyle *) : KDOM::RenderStyle() { }

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
            svg_inherited_flags.f.unused = 0;
            svg_inherited_flags.f.unused2 = 0;

            svg_noninherited_flags.f._alignmentBaseline = initialAlignmentBaseline();
            svg_noninherited_flags.f._dominantBaseline = initialDominantBaseline();
            svg_noninherited_flags.f.unused = 0;
            svg_noninherited_flags.f.unused2 = 0;
        }
    };
};

#endif

// vim:ts=4:noet
