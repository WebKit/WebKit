/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
             (C) 2003 Apple Computer, Inc.
             (C) 2004 Allan Sandfeld Jensen(kde@carewolf.com)
             (C) 2004 Germain Garand(germain@ebooksfrance.org)

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

#include "config.h"

#include <q3cstring.h>
#include <qpaintdevice.h>

#include <kdom/css/CSSStyleRuleImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "cssvalues.h"
#include <ksvg2/css/cssvalues.h>
#include "SVGColorImpl.h"
#include "SVGPaintImpl.h"
#include <ksvg2/css/cssproperties.h>
#include "SVGRenderStyle.h"
#include "SVGRenderStyleDefs.h"
#include "SVGStyledElementImpl.h"
#include "khtml/css/cssstyleselector.h"

#include <stdlib.h>

using namespace KSVG;

#define HANDLE_INHERIT(prop, Prop) \
if(isInherit) \
{\
    svgstyle->set##Prop(parentStyle->svgStyle()->prop());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
else if(isInitial) \
    svgstyle->set##Prop(SVGRenderStyle::initial##Prop());

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
else if(isInitial) \
    svgstyle->set##Prop(SVGRenderStyle::initial##Value());

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if(id == propID) \
{\
    svgstyle->set##Prop(parentStyle->prop());\
    return;\
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if(id == propID) \
{\
    svgstyle->set##Prop(SVGRenderStyle::initial##Prop());\
    return;\
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if(id == propID) \
{\
    svgstyle->set##Prop(SVGRenderStyle::initial##Value());\
    return;\
}

void KDOM::CSSStyleSelector::applySVGProperty(int id, KDOM::CSSValueImpl *value)
{
    KDOM::CSSPrimitiveValueImpl *primitiveValue = 0;
    if(value->isPrimitiveValue())
        primitiveValue = static_cast<KDOM::CSSPrimitiveValueImpl *>(value);

    KDOM::Length l;
    SVGRenderStyle *svgstyle = style->accessSVGStyle();
    
    unsigned short valueType = value->cssValueType();
    
    bool isInherit = parentNode && valueType == KDOM::CSSPrimitiveValue::CSS_INHERIT;
    bool isInitial = valueType == KDOM::CSSPrimitiveValue::CSS_INITIAL || (!parentNode && valueType == KDOM::CSSPrimitiveValue::CSS_INHERIT);

    // What follows is a list that maps the CSS properties into their
    // corresponding front-end RenderStyle values. Shorthands(e.g. border,
    // background) occur in this list as well and are only hit when mapping
    // "inherit" or "initial" into front-end values.
    switch(id)
    {
        // ident only properties
        case SVGCSS_PROP_ALIGNMENT_BASELINE:
        {
            HANDLE_INHERIT_AND_INITIAL(alignmentBaseline, AlignmentBaseline)
            if(!primitiveValue)
                break;

            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_AUTO:
                    svgstyle->setAlignmentBaseline(AB_AUTO);
                    break;
                case CSS_VAL_BASELINE:
                    svgstyle->setAlignmentBaseline(AB_BASELINE);
                    break;
                case SVGCSS_VAL_BEFORE_EDGE:
                    svgstyle->setAlignmentBaseline(AB_BEFORE_EDGE);
                    break;
                case SVGCSS_VAL_TEXT_BEFORE_EDGE:
                    svgstyle->setAlignmentBaseline(AB_TEXT_BEFORE_EDGE);
                    break;
                case CSS_VAL_MIDDLE:
                    svgstyle->setAlignmentBaseline(AB_MIDDLE);
                    break;
                case SVGCSS_VAL_CENTRAL:
                    svgstyle->setAlignmentBaseline(AB_CENTRAL);
                    break;
                case SVGCSS_VAL_AFTER_EDGE:
                    svgstyle->setAlignmentBaseline(AB_AFTER_EDGE);
                    break;
                case SVGCSS_VAL_TEXT_AFTER_EDGE:
                    svgstyle->setAlignmentBaseline(AB_TEXT_AFTER_EDGE);
                    break;
                case SVGCSS_VAL_IDEOGRAPHIC:
                    svgstyle->setAlignmentBaseline(AB_IDEOGRAPHIC);
                    break;
                case SVGCSS_VAL_ALPHABETIC:
                    svgstyle->setAlignmentBaseline(AB_ALPHABETIC);
                    break;
                case SVGCSS_VAL_HANGING:
                    svgstyle->setAlignmentBaseline(AB_HANGING);
                    break;
                case SVGCSS_VAL_MATHEMATICAL:
                    svgstyle->setAlignmentBaseline(AB_MATHEMATICAL);
                    break;
                default:
                    return;
            }

            break;
        }
        case SVGCSS_PROP_POINTER_EVENTS:
        {
            HANDLE_INHERIT_AND_INITIAL(pointerEvents, PointerEvents)
            if(!primitiveValue)
                break;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_ALL:
                    svgstyle->setPointerEvents(PE_ALL);
                    break;
                case CSS_VAL_NONE:
                    svgstyle->setPointerEvents(PE_NONE);
                    break;
                case SVGCSS_VAL_VISIBLEPAINTED:
                    svgstyle->setPointerEvents(PE_VISIBLE_PAINTED);
                    break;
                case SVGCSS_VAL_VISIBLEFILL:
                    svgstyle->setPointerEvents(PE_VISIBLE_FILL);
                    break;
                case SVGCSS_VAL_VISIBLESTROKE:
                    svgstyle->setPointerEvents(PE_VISIBLE_STROKE);
                    break;
                case CSS_VAL_VISIBLE:
                    svgstyle->setPointerEvents(PE_VISIBLE);
                    break;
                case SVGCSS_VAL_PAINTED:
                    svgstyle->setPointerEvents(PE_PAINTED);
                    break;
                case SVGCSS_VAL_FILL:
                    svgstyle->setPointerEvents(PE_FILL);
                    break;
                case SVGCSS_VAL_STROKE:
                    svgstyle->setPointerEvents(PE_STROKE);
                default:
                    return;
            }

            break;
        }
        case SVGCSS_PROP_DOMINANT_BASELINE:
        {
            HANDLE_INHERIT_AND_INITIAL(dominantBaseline, DominantBaseline)
            if(!primitiveValue)
                break;
    
            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_AUTO:
                    svgstyle->setDominantBaseline(DB_AUTO);
                    break;
                case SVGCSS_VAL_USE_SCRIPT:
                    svgstyle->setDominantBaseline(DB_USE_SCRIPT);
                    break;
                case SVGCSS_VAL_NO_CHANGE:
                    svgstyle->setDominantBaseline(DB_NO_CHANGE);
                    break;
                case SVGCSS_VAL_RESET_SIZE:
                    svgstyle->setDominantBaseline(DB_RESET_SIZE);
                    break;
                case SVGCSS_VAL_IDEOGRAPHIC:
                    svgstyle->setDominantBaseline(DB_IDEOGRAPHIC);
                    break;
                case SVGCSS_VAL_ALPHABETIC:
                    svgstyle->setDominantBaseline(DB_ALPHABETIC);
                    break;
                case SVGCSS_VAL_HANGING:
                    svgstyle->setDominantBaseline(DB_HANGING);
                    break;
                case SVGCSS_VAL_MATHEMATICAL:
                    svgstyle->setDominantBaseline(DB_MATHEMATICAL);
                    break;
                case SVGCSS_VAL_CENTRAL:
                    svgstyle->setDominantBaseline(DB_CENTRAL);
                    break;
                case CSS_VAL_MIDDLE:
                    svgstyle->setDominantBaseline(DB_MIDDLE);
                    break;
                case SVGCSS_VAL_TEXT_AFTER_EDGE:
                    svgstyle->setDominantBaseline(DB_TEXT_AFTER_EDGE);
                    break;
                case SVGCSS_VAL_TEXT_BEFORE_EDGE:
                    svgstyle->setDominantBaseline(DB_TEXT_BEFORE_EDGE);
                    break;
                default:
                    return;
            }
    
            break;
        }
        case SVGCSS_PROP_COLOR_INTERPOLATION:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolation, ColorInterpolation)
            if(!primitiveValue)
                return;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_SRGB:
                    svgstyle->setColorInterpolation(CI_SRGB);
                    break;
                case SVGCSS_VAL_LINEARRGB:
                    svgstyle->setColorInterpolation(CI_LINEARRGB);
                    break;
                case CSS_VAL_AUTO:
                    svgstyle->setColorInterpolation(CI_AUTO);
                default:
                    return;
            }
    
            break;
        }
        case SVGCSS_PROP_COLOR_INTERPOLATION_FILTERS:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolationFilters, ColorInterpolationFilters)
            if(!primitiveValue)
                return;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_SRGB:
                    svgstyle->setColorInterpolationFilters(CI_SRGB);
                    break;
                case SVGCSS_VAL_LINEARRGB:
                    svgstyle->setColorInterpolationFilters(CI_LINEARRGB);
                    break;
                case CSS_VAL_AUTO:
                    svgstyle->setColorInterpolationFilters(CI_AUTO);
                default:
                    return;
            }
            
            break;
        }
        case SVGCSS_PROP_CLIP_RULE:
        {
            HANDLE_INHERIT_AND_INITIAL(clipRule, ClipRule)
            if(!primitiveValue)
                break;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_NONZERO:
                    svgstyle->setClipRule(WR_NONZERO);
                    break;
                case SVGCSS_VAL_EVENODD:
                    svgstyle->setClipRule(WR_EVENODD);
                    break;
                default:
                    break;
            }
        
            break;
        }
        case SVGCSS_PROP_FILL_RULE:
        {
            HANDLE_INHERIT_AND_INITIAL(fillRule, FillRule)
            if(!primitiveValue)
                break;
            
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_NONZERO:
                    svgstyle->setFillRule(WR_NONZERO);
                    break;
                case SVGCSS_VAL_EVENODD:
                    svgstyle->setFillRule(WR_EVENODD);
                default:
                    return;
            }
        
            break;
        }
        case SVGCSS_PROP_STROKE_LINEJOIN:
        {
            HANDLE_INHERIT_AND_INITIAL(joinStyle, JoinStyle)

            if(!primitiveValue)
                break;
                
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_MITER:
                    svgstyle->setJoinStyle(JS_MITER);
                    break;
                case CSS_VAL_ROUND:
                    svgstyle->setJoinStyle(JS_ROUND);
                    break;
                case SVGCSS_VAL_BEVEL:
                    svgstyle->setJoinStyle(JS_BEVEL);
                default:
                    return;
            }
            
            break;
        }
        case SVGCSS_PROP_IMAGE_RENDERING:
        {
            HANDLE_INHERIT_AND_INITIAL(imageRendering, ImageRendering)
            if(!primitiveValue)
                return;
            
            switch(primitiveValue->getIdent())
            {
                case SVGCSS_VAL_OPTIMIZESPEED:
                    svgstyle->setImageRendering(IR_OPTIMIZESPEED);
                default:
                    return;
            }
    
            break;
        }
        // end of ident only properties
        case SVGCSS_PROP_FILL:
        {
            HANDLE_INHERIT_AND_INITIAL(fillPaint, FillPaint)
            if(!primitiveValue && value)
            {
                SVGPaintImpl *paint = static_cast<SVGPaintImpl *>(value);
                if(paint)
                    svgstyle->setFillPaint(paint);
            }
            
            break;
        }
        case SVGCSS_PROP_STROKE:
        {
            HANDLE_INHERIT_AND_INITIAL(strokePaint, StrokePaint)
            if(!primitiveValue && value)
            {
                SVGPaintImpl *paint = static_cast<SVGPaintImpl *>(value);
                if(paint)
                    svgstyle->setStrokePaint(paint);
            }
            
            break;
        }
        case SVGCSS_PROP_STROKE_WIDTH:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeWidth, StrokeWidth)
            if(!primitiveValue)
                return;
        
            svgstyle->setStrokeWidth(primitiveValue);
            break;
        }
        case SVGCSS_PROP_STROKE_DASHARRAY:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
            if(!primitiveValue && value)
            {
                KDOM::CSSValueListImpl *dashes = static_cast<KDOM::CSSValueListImpl *>(value);
                if(dashes)
                    svgstyle->setStrokeDashArray(dashes);
            }
        
            break;
        }
        case SVGCSS_PROP_STROKE_DASHOFFSET:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashOffset, StrokeDashOffset)
            if(!primitiveValue)
                return;

            svgstyle->setStrokeDashOffset(primitiveValue);
            break;
        }
        case SVGCSS_PROP_FILL_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(fillOpacity, FillOpacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_NUMBER);
            else
                return;

            svgstyle->setFillOpacity(f);
            break;
        }
        case SVGCSS_PROP_STROKE_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeOpacity, StrokeOpacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_NUMBER);
            else
                return;

            svgstyle->setStrokeOpacity(f);
            break;
        }
        case SVGCSS_PROP_STOP_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(stopOpacity, StopOpacity)
            if(!primitiveValue)
                return;
        
            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_NUMBER);
            else
                return;

            svgstyle->setStopOpacity(f);
            break;
        }
        case SVGCSS_PROP_MARKER_START:
        {
            HANDLE_INHERIT_AND_INITIAL(startMarker, StartMarker)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue().qstring();
            else
                return;

            svgstyle->setStartMarker(s);
            break;
        }
        case SVGCSS_PROP_MARKER_MID:
        {
            HANDLE_INHERIT_AND_INITIAL(midMarker, MidMarker)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue().qstring();
            else
                return;

            svgstyle->setMidMarker(s);
            break;
        }
        case SVGCSS_PROP_MARKER_END:
        {
            HANDLE_INHERIT_AND_INITIAL(endMarker, EndMarker)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue().qstring();
            else
                return;

            svgstyle->setEndMarker(s);
            break;
        }
        case SVGCSS_PROP_STROKE_LINECAP:
        {
            HANDLE_INHERIT_AND_INITIAL(capStyle, CapStyle)
            if (!primitiveValue)
                break;
            
            switch (primitiveValue->getIdent())
            {
                case SVGCSS_VAL_BUTT:
                    svgstyle->setCapStyle(CS_BUTT);
                    break;
                case CSS_VAL_ROUND:
                    svgstyle->setCapStyle(CS_ROUND);
                    break;
                case CSS_VAL_SQUARE:
                    svgstyle->setCapStyle(CS_SQUARE);
                default:
                    return;
            }

            break;
        }
        case SVGCSS_PROP_STROKE_MITERLIMIT:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeMiterLimit, StrokeMiterLimit)
            if(!primitiveValue)
                return;

            float f = 0.0;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_NUMBER);
            else
                return;

            svgstyle->setStrokeMiterLimit(qRound(f));
            break;
        }
        case SVGCSS_PROP_FILTER:
        {
            HANDLE_INHERIT_AND_INITIAL(filter, Filter)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue().qstring();
            else
                return;
            svgstyle->setFilter(s);
            break;
        }
        case SVGCSS_PROP_CLIP_PATH:
        {
            HANDLE_INHERIT_AND_INITIAL(clipPath, ClipPath)
            if(!primitiveValue)
                return;

            QString s;
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue().qstring();
            else
                return;

            svgstyle->setClipPath(s);
            break;
        }
        case SVGCSS_PROP_TEXT_ANCHOR:
        {
            HANDLE_INHERIT_AND_INITIAL(textAnchor, TextAnchor)
            if (!primitiveValue)
                break;
            
            switch(primitiveValue->getIdent())
            {
                case CSS_VAL_START:
                    svgstyle->setTextAnchor(TA_START);
                    break;
                case CSS_VAL_MIDDLE:
                    svgstyle->setTextAnchor(TA_MIDDLE);
                    break;
                case CSS_VAL_END:
                    svgstyle->setTextAnchor(TA_END);
                default:
                    return;
            }
            
            break;
        }
#if 0
        case CSS_PROP_COLOR: // colors || inherit
        {
            QColor col;
            if(isInherit)
            {
                HANDLE_INHERIT_COND(CSS_PROP_COLOR, color, Color)
                return;
            }
            else if(isInitial)
                col = KDOM::RenderStyle::initialColor();
            else
            {
                SVGColorImpl *c = static_cast<SVGColorImpl *>(value);
                if(!c)
                    return KDOM::CSSStyleSelector::applyProperty(id, value);
                
                col = c->color();
            }
        
            svgstyle->setColor(col);
            break;
        }
#endif
        case SVGCSS_PROP_STOP_COLOR:
        {
            QColor col;
            if(isInherit)
            {
                style->setColor(parentStyle->color());
                return;
            }
            else if(isInitial)
                col = SVGRenderStyle::initialStopColor();
            else
            {
                SVGColorImpl *c = static_cast<SVGColorImpl *>(value);
                if(!c)
                    return KDOM::CSSStyleSelector::applyProperty(id, value);

                if(c->colorType() == SVG_COLORTYPE_CURRENTCOLOR)
                    col = style->color();
                else
                    col = c->color();
            }

            svgstyle->setStopColor(col);
            break;
        }
        case SVGCSS_PROP_FLOOD_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(floodOpacity, FloodOpacity)
            if(!primitiveValue)
                return;

            float f = 0.0;    
            int type = primitiveValue->primitiveType();
            if(type == KDOM::CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_PERCENTAGE) / 100.;
            else if(type == KDOM::CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue(KDOM::CSSPrimitiveValue::CSS_NUMBER);
            else
                return;

            svgstyle->setFloodOpacity(f);
            break;
        }
        case SVGCSS_PROP_FLOOD_COLOR:
        {
            QColor col;
            if(isInitial)
                col = SVGRenderStyle::initialStopColor();
            else
            {
                SVGColorImpl *c = static_cast<SVGColorImpl *>(value);
                if(!c)
                    return KDOM::CSSStyleSelector::applyProperty(id, value);

                if(c->colorType() == SVG_COLORTYPE_CURRENTCOLOR)
                    col = style->color();
                else
                    col = c->color();
            }

            svgstyle->setFloodColor(col);
            break;
        }
        default:
        {
            kdDebug() << "RULE UNIMPLEMENTED! FIX ME :) id=" << id << endl;
            return;
        }
    }
}

// vim:ts=4:noet
