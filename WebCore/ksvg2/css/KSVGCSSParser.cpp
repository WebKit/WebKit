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

#include "config.h"
#if SVG_SUPPORT

#include "ksvg.h"

#include "SVGPaintImpl.h"
#include "cssparser.h"
#include "cssproperties.h"
#include "cssvalues.h"
#include "ksvgcssproperties.c"
#include "ksvgcssvalues.c"

namespace WebCore {

typedef Value KDOMCSSValue;
typedef ValueList KDOMCSSValueList;

bool CSSParser::parseSVGValue(int propId, bool important)
{
    if(!valueList)
        return false;

    KDOMCSSValue *value = valueList->current();
    if(!value)
        return false;

    int id = value->id;
    if (id == CSS_VAL_INHERIT) {
        addProperty(propId, new CSSInheritedValueImpl(), important);
        return true;
    } else if (id == CSS_VAL_INITIAL) {
        addProperty(propId, new CSSInitialValueImpl(), important);
        return true;
    }
    
    bool valid_primitive = false;
    CSSValueImpl *parsedValue = 0;

    switch(propId)
    {
    /* The comment to the right defines all valid value of these
     * properties as defined in SVG 1.1, Appendix N. Property index */
    case SVGCSS_PROP_ALIGNMENT_BASELINE:
    // auto | baseline | before-edge | text-before-edge | middle |
    // central | after-edge | text-after-edge | ideographic | alphabetic |
    // hanging | mathematical | inherit
        if(id == CSS_VAL_AUTO || id == CSS_VAL_BASELINE || id == CSS_VAL_MIDDLE ||
          (id >= SVGCSS_VAL_BEFORE_EDGE && id <= SVGCSS_VAL_MATHEMATICAL))
            valid_primitive = true;
        break;

    case SVGCSS_PROP_BASELINE_SHIFT:
    // baseline | super | sub | <percentage> | <length> | inherit
        if(id == CSS_VAL_BASELINE || id == CSS_VAL_SUB ||
           id >= CSS_VAL_SUPER)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent, false);
        break;

    case SVGCSS_PROP_DOMINANT_BASELINE:
    // auto | use-script | no-change | reset-size | ideographic |
    // alphabetic | hanging | mathematical | central | middle |
    // text-after-edge | text-before-edge | inherit
        if(id == CSS_VAL_AUTO || id == CSS_VAL_MIDDLE ||
          (id >= SVGCSS_VAL_USE_SCRIPT && id <= SVGCSS_VAL_RESET_SIZE) ||
          (id >= SVGCSS_VAL_CENTRAL && id <= SVGCSS_VAL_MATHEMATICAL))
            valid_primitive = true;
        break;

    case SVGCSS_PROP_ENABLE_BACKGROUND:
    // accumulate | new [x] [y] [width] [height] | inherit
        if(id == SVGCSS_VAL_ACCUMULATE) // TODO : new
            valid_primitive = true;
        break;

    case SVGCSS_PROP_MARKER_START:
    case SVGCSS_PROP_MARKER_MID:
    case SVGCSS_PROP_MARKER_END:
    case SVGCSS_PROP_MASK:
        if(id == CSS_VAL_NONE)
            valid_primitive = true;
        else if(value->unit == CSSPrimitiveValue::CSS_URI)
        {
            parsedValue = new CSSPrimitiveValueImpl(domString(value->string), CSSPrimitiveValue::CSS_URI);
            if(parsedValue)
                valueList->next();
        }
        break;

    case SVGCSS_PROP_CLIP_RULE:            // nonzero | evenodd | inherit
    case SVGCSS_PROP_FILL_RULE:
        if(id == SVGCSS_VAL_NONZERO || id == SVGCSS_VAL_EVENODD)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_MITERLIMIT:   // <miterlimit> | inherit
        valid_primitive = validUnit(value, FInteger|FNonNeg, false);
        break;

    case SVGCSS_PROP_STROKE_LINEJOIN:   // miter | round | bevel | inherit
        if(id == SVGCSS_VAL_MITER || id == CSS_VAL_ROUND || id == SVGCSS_VAL_BEVEL)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_LINECAP:    // butt | round | square | inherit
        if(id == SVGCSS_VAL_BUTT || id == CSS_VAL_ROUND || id == CSS_VAL_SQUARE)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_OPACITY:   // <opacity-value> | inherit
    case SVGCSS_PROP_FILL_OPACITY:
    case SVGCSS_PROP_STOP_OPACITY:
    case SVGCSS_PROP_FLOOD_OPACITY:
        valid_primitive = (!id && validUnit(value, FNumber|FPercent, false));
        break;

    case SVGCSS_PROP_SHAPE_RENDERING:
    // auto | optimizeSpeed | crispEdges | geometricPrecision | inherit
        if(id == CSS_VAL_AUTO || id == SVGCSS_VAL_OPTIMIZESPEED ||
            id == SVGCSS_VAL_CRISPEDGES || id == SVGCSS_VAL_GEOMETRICPRECISION)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_TEXT_RENDERING:   // auto | optimizeSpeed | optimizeLegibility | geometricPrecision | inherit
        if(id == CSS_VAL_AUTO || id == SVGCSS_VAL_OPTIMIZESPEED || id == SVGCSS_VAL_OPTIMIZELEGIBILITY ||
       id == SVGCSS_VAL_GEOMETRICPRECISION)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_IMAGE_RENDERING:  // auto | optimizeSpeed |
    case SVGCSS_PROP_COLOR_RENDERING:  // optimizeQuality | inherit
        if(id == CSS_VAL_AUTO || id == SVGCSS_VAL_OPTIMIZESPEED ||
            id == SVGCSS_VAL_OPTIMIZEQUALITY)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_COLOR_PROFILE: // auto | sRGB | <name> | <uri> inherit
        if(id == CSS_VAL_AUTO || id == SVGCSS_VAL_SRGB)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_COLOR_INTERPOLATION:   // auto | sRGB | linearRGB | inherit
    case SVGCSS_PROP_COLOR_INTERPOLATION_FILTERS:  
        if(id == CSS_VAL_AUTO || id == SVGCSS_VAL_SRGB || id == SVGCSS_VAL_LINEARRGB)
            valid_primitive = true;
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in applyRule(..)
     */

    case SVGCSS_PROP_POINTER_EVENTS:
    // visiblePainted | visibleFill | visibleStroke | visible |
    // painted | fill | stroke || all | inherit
        if(id == CSS_VAL_VISIBLE || 
          (id >= SVGCSS_VAL_VISIBLEPAINTED && id <= SVGCSS_VAL_ALL))
            valid_primitive = true;
        break;

    case SVGCSS_PROP_TEXT_ANCHOR:    // start | middle | end | inherit
        if(id == CSS_VAL_START || id == CSS_VAL_MIDDLE || id == CSS_VAL_END)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_GLYPH_ORIENTATION_VERTICAL: // auto | <angle> | inherit
        if(id == CSS_VAL_AUTO)
        {
            valid_primitive = true;
            break;
        }
    case SVGCSS_PROP_GLYPH_ORIENTATION_HORIZONTAL: // <angle> | inherit
        if(value->unit == CSSPrimitiveValue::CSS_DEG)
            parsedValue = new CSSPrimitiveValueImpl(value->fValue, CSSPrimitiveValue::CSS_DEG);
        else if(value->unit == CSSPrimitiveValue::CSS_GRAD)
            parsedValue = new CSSPrimitiveValueImpl(value->fValue, CSSPrimitiveValue::CSS_GRAD);
        else if(value->unit == CSSPrimitiveValue::CSS_RAD)
            parsedValue = new CSSPrimitiveValueImpl(value->fValue, CSSPrimitiveValue::CSS_RAD);
        break;

    case SVGCSS_PROP_FILL:                 // <paint> | inherit
    case SVGCSS_PROP_STROKE:               // <paint> | inherit
        {
            if(id == CSS_VAL_NONE)
                parsedValue = new SVGPaintImpl(SVG_PAINTTYPE_NONE);
            else if(id == SVGCSS_VAL_CURRENTCOLOR)
                parsedValue = new SVGPaintImpl(SVG_PAINTTYPE_CURRENTCOLOR);
            else if(value->unit == CSSPrimitiveValue::CSS_URI)
                parsedValue = new SVGPaintImpl(SVG_PAINTTYPE_URI, domString(value->string).impl());
            else
                parsedValue = parseSVGPaint();

            if(parsedValue)
                valueList->next();
        }
        break;

    case CSS_PROP_COLOR:                // <color> | inherit
        if((id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) ||
           (id >= SVGCSS_VAL_ALICEBLUE && id <= SVGCSS_VAL_YELLOWGREEN))
            parsedValue = new SVGColorImpl(domString(value->string).impl());
        else
            parsedValue = parseSVGColor();

        if(parsedValue)
            valueList->next();
        break;

    case SVGCSS_PROP_STOP_COLOR: // TODO : icccolor
    case SVGCSS_PROP_FLOOD_COLOR:
    case SVGCSS_PROP_LIGHTING_COLOR:
        if((id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) ||
           (id >= SVGCSS_VAL_ALICEBLUE && id <= SVGCSS_VAL_YELLOWGREEN))
            parsedValue = new SVGColorImpl(domString(value->string).impl());
        else if(id == SVGCSS_VAL_CURRENTCOLOR)
            parsedValue = new SVGColorImpl(SVG_COLORTYPE_CURRENTCOLOR);
        else // TODO : svgcolor (iccColor)
            parsedValue = parseSVGColor();

        if(parsedValue)
            valueList->next();

        break;

    case SVGCSS_PROP_WRITING_MODE:
    // lr-tb | rl_tb | tb-rl | lr | rl | tb | inherit
        if(id >= SVGCSS_VAL_LR_TB && id <= SVGCSS_VAL_TB)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_WIDTH:         // <length> | inherit
    case SVGCSS_PROP_STROKE_DASHOFFSET:
        valid_primitive = validUnit(value, FLength | FPercent, false);
        break;
    case SVGCSS_PROP_STROKE_DASHARRAY:     // none | <dasharray> | inherit
        if(id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            parsedValue = parseSVGStrokeDasharray();

        break;

    case SVGCSS_PROP_KERNING:              // auto | normal | <length> | inherit
        if(id == CSS_VAL_AUTO)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, false);
        break;
    /* shorthand properties */
    case SVGCSS_PROP_MARKER:
    {
            const int properties[3] = { SVGCSS_PROP_MARKER_START,
                                        SVGCSS_PROP_MARKER_MID,
                                        SVGCSS_PROP_MARKER_END };
            return parseShorthand(propId, properties, 3, important);
    }

    case SVGCSS_PROP_CLIP_PATH:    // <uri> | none | inherit
    case SVGCSS_PROP_FILTER:
        if(id == CSS_VAL_NONE)
            valid_primitive = true;
        else if(value->unit == CSSPrimitiveValue::CSS_URI)
        {
            parsedValue = new CSSPrimitiveValueImpl(domString(value->string), (CSSPrimitiveValue::UnitTypes) value->unit);
            if(parsedValue)
                valueList->next();
        }
        break;

    default:
        return false;
    }

    if(valid_primitive)
    {
        if(id != 0)
            parsedValue = new CSSPrimitiveValueImpl(id);
        else if(value->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = new CSSPrimitiveValueImpl(domString(value->string), (CSSPrimitiveValue::UnitTypes) value->unit);
        else if(value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = new CSSPrimitiveValueImpl(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if(value->unit >= KDOMCSSValue::Q_EMS)
            parsedValue = new CSSQuirkPrimitiveValueImpl(value->fValue, CSSPrimitiveValue::CSS_EMS);
        valueList->next();
        if (valueList->current()) {
            delete parsedValue;
            parsedValue = 0;
        }
    }
    if(parsedValue)
    {
        addProperty(propId, parsedValue, important);
        return true;
    }
    return false;
}

CSSValueImpl *CSSParser::parseSVGStrokeDasharray()
{
    CSSValueListImpl *ret = new CSSValueListImpl;
    KDOMCSSValue *value = valueList->current();
    bool valid_primitive = true;
    while(valid_primitive && value)
    {
        valid_primitive = validUnit(value, FLength | FPercent |FNonNeg, false);
        if(value->id != 0)
            ret->append(new CSSPrimitiveValueImpl(value->id));
        else if(value->unit == CSSPrimitiveValue::CSS_STRING)
            ret->append(new CSSPrimitiveValueImpl(domString(value->string), (CSSPrimitiveValue::UnitTypes) value->unit));
        else if(value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            ret->append(new CSSPrimitiveValueImpl(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit));
        value = valueList->next();
        if(value && value->unit == KDOMCSSValue::Operator && value->iValue == ',')
            value = valueList->next();
    }

    return ret;
}

CSSValueImpl *CSSParser::parseSVGPaint()
{
    KDOMCSSValue *value = valueList->current();
    if(!strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&
       value->fValue >= 0. && value->fValue < 1000000.)
    {
        QString str;
        str.sprintf("%06d", (int)(value->fValue+.5));
        return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(str));
    }
    else if(value->unit == CSSPrimitiveValue::CSS_RGBCOLOR)
    {
        QString str = QString::fromLatin1("#") + qString(value->string);
        return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(str));
    }
    else if(value->unit == CSSPrimitiveValue::CSS_IDENT ||
           (!strict && value->unit == CSSPrimitiveValue::CSS_DIMENSION))
    {
        QString str = qString(value->string);
        return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(str));
    }
    else if(value->unit == KDOMCSSValue::Function && value->function->args != 0 &&
            qString(value->function->name).lower() == "rgb(")
    {
        KDOMCSSValueList *args = value->function->args;
        KDOMCSSValue *v = args->current();
        if(!validUnit(v, FInteger|FPercent, true))
            return 0;
        int r = (int) (v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.));
        v = args->next();
        if(v->unit != KDOMCSSValue::Operator && v->iValue != ',')
            return 0;
        v = args->next();
        if(!validUnit(v, FInteger|FPercent, true))
            return 0;
        int g = (int) (v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.));
        v = args->next();
        if(v->unit != KDOMCSSValue::Operator && v->iValue != ',')
            return 0;
        v = args->next();
        if(!validUnit(v, FInteger|FPercent, true))
            return 0;
        int b = (int) (v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.));
        r = kMax(0, kMin(255, r));
        g = kMax(0, kMin(255, g));
        b = kMax(0, kMin(255, b));
        QString str;
        str.sprintf("rgb(%d, %d, %d)", r, g, b);
        return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(str));
    }
    else
        return 0;

    return new SVGPaintImpl();
}

CSSValueImpl *CSSParser::parseSVGColor()
{
    KDOMCSSValue *value = valueList->current();
    if(!strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&
       value->fValue >= 0. && value->fValue < 1000000.)
    {
        QString str;
        str.sprintf("%06d", (int)(value->fValue+.5));
        return new SVGColorImpl(new DOMStringImpl(str));
    }
    else if(value->unit == CSSPrimitiveValue::CSS_RGBCOLOR)
    {
        QString str = QString::fromLatin1("#") + qString(value->string);
        return new SVGColorImpl(new DOMStringImpl(str));
    }
    else if(value->unit == CSSPrimitiveValue::CSS_IDENT ||
           (!strict && value->unit == CSSPrimitiveValue::CSS_DIMENSION))
        return new SVGColorImpl(domString(value->string).impl());
    else if(value->unit == KDOMCSSValue::Function && value->function->args != 0 &&
            qString(value->function->name).lower() == "rgb(")
    {
        KDOMCSSValueList *args = value->function->args;
        KDOMCSSValue *v = args->current();
        if(!validUnit(v, FInteger|FPercent, true))
            return 0;
        int r = (int) (v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.));
        v = args->next();
        if(v->unit != Value::Operator && v->iValue != ',')
            return 0;
        v = args->next();
        if(!validUnit(v, FInteger|FPercent, true))
            return 0;
        int g = (int) (v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.));
        v = args->next();
        if(v->unit != Value::Operator && v->iValue != ',')
            return 0;
        v = args->next();
        if(!validUnit(v, FInteger|FPercent, true))
            return 0;
        int b = (int) (v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.));
        r = kMax(0, kMin(255, r));
        g = kMax(0, kMin(255, g));
        b = kMax(0, kMin(255, b));
        QString str;
        str.sprintf("rgb(%d, %d, %d)", r, g, b);
        return new SVGColorImpl(new DOMStringImpl(str));
    }
    else
        return 0;

    return new SVGPaintImpl();
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
