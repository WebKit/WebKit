/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)

#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSParser.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSQuirkPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "SVGPaint.h"
#include "ksvgcssproperties.c"
#include "ksvgcssvalues.c"

using namespace std;

namespace WebCore {

bool CSSParser::parseSVGValue(int propId, bool important)
{
    Value* value = valueList->current();
    if (!value)
        return false;

    int id = value->id;

    bool valid_primitive = false;
    CSSValue* parsedValue = 0;

    switch (propId) {
    /* The comment to the right defines all valid value of these
     * properties as defined in SVG 1.1, Appendix N. Property index */
    case SVGCSS_PROP_ALIGNMENT_BASELINE:
    // auto | baseline | before-edge | text-before-edge | middle |
    // central | after-edge | text-after-edge | ideographic | alphabetic |
    // hanging | mathematical | inherit
        if (id == CSS_VAL_AUTO || id == CSS_VAL_BASELINE || id == CSS_VAL_MIDDLE ||
          (id >= SVGCSS_VAL_BEFORE_EDGE && id <= SVGCSS_VAL_MATHEMATICAL))
            valid_primitive = true;
        break;

    case SVGCSS_PROP_BASELINE_SHIFT:
    // baseline | super | sub | <percentage> | <length> | inherit
        if (id == CSS_VAL_BASELINE || id == CSS_VAL_SUB ||
           id >= CSS_VAL_SUPER)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent, false);
        break;

    case SVGCSS_PROP_DOMINANT_BASELINE:
    // auto | use-script | no-change | reset-size | ideographic |
    // alphabetic | hanging | mathematical | central | middle |
    // text-after-edge | text-before-edge | inherit
        if (id == CSS_VAL_AUTO || id == CSS_VAL_MIDDLE ||
          (id >= SVGCSS_VAL_USE_SCRIPT && id <= SVGCSS_VAL_RESET_SIZE) ||
          (id >= SVGCSS_VAL_CENTRAL && id <= SVGCSS_VAL_MATHEMATICAL))
            valid_primitive = true;
        break;

    case SVGCSS_PROP_ENABLE_BACKGROUND:
    // accumulate | new [x] [y] [width] [height] | inherit
        if (id == SVGCSS_VAL_ACCUMULATE) // TODO : new
            valid_primitive = true;
        break;

    case SVGCSS_PROP_MARKER_START:
    case SVGCSS_PROP_MARKER_MID:
    case SVGCSS_PROP_MARKER_END:
    case SVGCSS_PROP_MASK:
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = new CSSPrimitiveValue(domString(value->string), CSSPrimitiveValue::CSS_URI);
            if (parsedValue)
                valueList->next();
        }
        break;

    case SVGCSS_PROP_CLIP_RULE:            // nonzero | evenodd | inherit
    case SVGCSS_PROP_FILL_RULE:
        if (id == SVGCSS_VAL_NONZERO || id == SVGCSS_VAL_EVENODD)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_MITERLIMIT:   // <miterlimit> | inherit
        valid_primitive = validUnit(value, FNumber|FNonNeg, false);
        break;

    case SVGCSS_PROP_STROKE_LINEJOIN:   // miter | round | bevel | inherit
        if (id == SVGCSS_VAL_MITER || id == CSS_VAL_ROUND || id == SVGCSS_VAL_BEVEL)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_LINECAP:    // butt | round | square | inherit
        if (id == SVGCSS_VAL_BUTT || id == CSS_VAL_ROUND || id == CSS_VAL_SQUARE)
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
        if (id == CSS_VAL_AUTO || id == SVGCSS_VAL_OPTIMIZESPEED ||
            id == SVGCSS_VAL_CRISPEDGES || id == SVGCSS_VAL_GEOMETRICPRECISION)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_TEXT_RENDERING:   // auto | optimizeSpeed | optimizeLegibility | geometricPrecision | inherit
        if (id == CSS_VAL_AUTO || id == SVGCSS_VAL_OPTIMIZESPEED || id == SVGCSS_VAL_OPTIMIZELEGIBILITY ||
       id == SVGCSS_VAL_GEOMETRICPRECISION)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_IMAGE_RENDERING:  // auto | optimizeSpeed |
    case SVGCSS_PROP_COLOR_RENDERING:  // optimizeQuality | inherit
        if (id == CSS_VAL_AUTO || id == SVGCSS_VAL_OPTIMIZESPEED ||
            id == SVGCSS_VAL_OPTIMIZEQUALITY)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_COLOR_PROFILE: // auto | sRGB | <name> | <uri> inherit
        if (id == CSS_VAL_AUTO || id == SVGCSS_VAL_SRGB)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_COLOR_INTERPOLATION:   // auto | sRGB | linearRGB | inherit
    case SVGCSS_PROP_COLOR_INTERPOLATION_FILTERS:  
        if (id == CSS_VAL_AUTO || id == SVGCSS_VAL_SRGB || id == SVGCSS_VAL_LINEARRGB)
            valid_primitive = true;
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in applyRule(..)
     */

    case SVGCSS_PROP_POINTER_EVENTS:
    // none | visiblePainted | visibleFill | visibleStroke | visible |
    // painted | fill | stroke | none | all | inherit
        if (id == CSS_VAL_VISIBLE || id == CSS_VAL_NONE ||
          (id >= SVGCSS_VAL_VISIBLEPAINTED && id <= SVGCSS_VAL_ALL))
            valid_primitive = true;
        break;

    case SVGCSS_PROP_TEXT_ANCHOR:    // start | middle | end | inherit
        if (id == CSS_VAL_START || id == CSS_VAL_MIDDLE || id == CSS_VAL_END)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_GLYPH_ORIENTATION_VERTICAL: // auto | <angle> | inherit
        if (id == CSS_VAL_AUTO) {
            valid_primitive = true;
            break;
        }
        /* fallthrough intentional */
    case SVGCSS_PROP_GLYPH_ORIENTATION_HORIZONTAL: // <angle> | inherit
        if (value->unit == CSSPrimitiveValue::CSS_DEG)
            parsedValue = new CSSPrimitiveValue(value->fValue, CSSPrimitiveValue::CSS_DEG);
        else if (value->unit == CSSPrimitiveValue::CSS_GRAD)
            parsedValue = new CSSPrimitiveValue(value->fValue, CSSPrimitiveValue::CSS_GRAD);
        else if (value->unit == CSSPrimitiveValue::CSS_RAD)
            parsedValue = new CSSPrimitiveValue(value->fValue, CSSPrimitiveValue::CSS_RAD);
        break;

    case SVGCSS_PROP_FILL:                 // <paint> | inherit
    case SVGCSS_PROP_STROKE:               // <paint> | inherit
        {
            if (id == CSS_VAL_NONE)
                parsedValue = new SVGPaint(SVGPaint::SVG_PAINTTYPE_NONE);
            else if (id == SVGCSS_VAL_CURRENTCOLOR)
                parsedValue = new SVGPaint(SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR);
            else if (value->unit == CSSPrimitiveValue::CSS_URI)
                parsedValue = new SVGPaint(SVGPaint::SVG_PAINTTYPE_URI, domString(value->string));
            else
                parsedValue = parseSVGPaint();

            if (parsedValue)
                valueList->next();
        }
        break;

    case CSS_PROP_COLOR:                // <color> | inherit
        if ((id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) ||
           (id >= SVGCSS_VAL_ALICEBLUE && id <= SVGCSS_VAL_YELLOWGREEN))
            parsedValue = new SVGColor(domString(value->string));
        else
            parsedValue = parseSVGColor();

        if (parsedValue)
            valueList->next();
        break;

    case SVGCSS_PROP_STOP_COLOR: // TODO : icccolor
    case SVGCSS_PROP_FLOOD_COLOR:
    case SVGCSS_PROP_LIGHTING_COLOR:
        if ((id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) ||
           (id >= SVGCSS_VAL_ALICEBLUE && id <= SVGCSS_VAL_YELLOWGREEN))
            parsedValue = new SVGColor(domString(value->string));
        else if (id == SVGCSS_VAL_CURRENTCOLOR)
            parsedValue = new SVGColor(SVGColor::SVG_COLORTYPE_CURRENTCOLOR);
        else // TODO : svgcolor (iccColor)
            parsedValue = parseSVGColor();

        if (parsedValue)
            valueList->next();

        break;

    case SVGCSS_PROP_WRITING_MODE:
    // lr-tb | rl_tb | tb-rl | lr | rl | tb | inherit
        if (id >= SVGCSS_VAL_LR_TB && id <= SVGCSS_VAL_TB)
            valid_primitive = true;
        break;

    case SVGCSS_PROP_STROKE_WIDTH:         // <length> | inherit
    case SVGCSS_PROP_STROKE_DASHOFFSET:
        valid_primitive = validUnit(value, FLength | FPercent, false);
        break;
    case SVGCSS_PROP_STROKE_DASHARRAY:     // none | <dasharray> | inherit
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            parsedValue = parseSVGStrokeDasharray();

        break;

    case SVGCSS_PROP_KERNING:              // auto | normal | <length> | inherit
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NORMAL)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, false);
        break;

    case SVGCSS_PROP_CLIP_PATH:    // <uri> | none | inherit
    case SVGCSS_PROP_FILTER:
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = new CSSPrimitiveValue(domString(value->string), (CSSPrimitiveValue::UnitTypes) value->unit);
            if (parsedValue)
                valueList->next();
        }
        break;

    /* shorthand properties */
    case SVGCSS_PROP_MARKER:
    {
        if (!parseValue(SVGCSS_PROP_MARKER_START, important))
            return false;
        CSSValue *value = parsedProperties[numParsedProperties - 1]->value();
        m_implicitShorthand = true;
        addProperty(SVGCSS_PROP_MARKER_MID, value, important);
        addProperty(SVGCSS_PROP_MARKER_END, value, important);
        m_implicitShorthand = false;
        return true;
    }
    default:
        return false;
    }

    if (valid_primitive) {
        if (id != 0)
            parsedValue = new CSSPrimitiveValue(id);
        else if (value->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = new CSSPrimitiveValue(domString(value->string), (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= Value::Q_EMS)
            parsedValue = new CSSQuirkPrimitiveValue(value->fValue, CSSPrimitiveValue::CSS_EMS);
        valueList->next();
    }
    if (parsedValue) {
        if (!valueList->current() || inShorthand()) {
            addProperty(propId, parsedValue, important);
            return true;
        }
        delete parsedValue;
    }
    return false;
}

CSSValue* CSSParser::parseSVGStrokeDasharray()
{
    CSSValueList* ret = new CSSValueList;
    Value* value = valueList->current();
    bool valid_primitive = true;
    while (value) {
        valid_primitive = validUnit(value, FLength | FPercent |FNonNeg, false);
        if (!valid_primitive)
            break;
        if (value->id != 0)
            ret->append(new CSSPrimitiveValue(value->id));
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            ret->append(new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit));
        value = valueList->next();
        if (value && value->unit == Value::Operator && value->iValue == ',')
            value = valueList->next();
    }
    if (!valid_primitive) {
        delete ret;
        ret = 0;
    }

    return ret;
}

CSSValue* CSSParser::parseSVGPaint()
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(valueList->current(), c, true))
        return new SVGPaint();
    return new SVGPaint(Color(c));
}

CSSValue* CSSParser::parseSVGColor()
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(valueList->current(), c, true))
        return 0;
    return new SVGColor(Color(c));
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
