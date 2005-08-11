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


#include <kdebug.h>
#include <kglobal.h>

#include <stdlib.h>
#include <assert.h>

#include <kdom/DOMString.h>
#include <kdom/impl/DocumentImpl.h>
#include <kdom/css/impl/cssvalues.h>
#include <kdom/css/impl/CSSValueImpl.h>
#include <kdom/css/impl/cssproperties.h>
#include <kdom/css/impl/CSSValueListImpl.h>
#include <kdom/css/impl/CSSStyleRuleImpl.h>
#include <kdom/impl/DOMImplementationImpl.h>
#include <kdom/css/impl/CSSPrimitiveValueImpl.h>

#include "ksvg.h"
#include "KSVGCSSParser.h"
#include <ksvg2/css/impl/cssvalues.h>
#include "SVGPaintImpl.h"
#include <ksvg2/css/impl/cssproperties.h>
#include "SVGCSSStyleDeclarationImpl.h"

#include <kdom/impl/DOMStringImpl.h>

using namespace KDOM;
using namespace KSVG;

#include "ksvgcssvalues.c"
#include "ksvgcssproperties.c"

SVGCSSParser::SVGCSSParser(bool strictParsing) : CSSParser(strictParsing)
{
}

SVGCSSParser::~SVGCSSParser()
{
}

bool SVGCSSParser::parseValue(int propId, bool important, int expected)
{
	if(!valueList)
		return false;

	KDOMCSSValue *value = valueList->current();
	if(!value)
		return false;

	int id = value->id;
	if(id == CSS_VAL_INHERIT && expected == 1)
	{
		addProperty(propId, new CSSInheritedValueImpl(), important);
		return true;
	}
	else if(id == CSS_VAL_INITIAL && expected == 1)
	{
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
		if(id == CSS_VAL_AUTO || id == CSS_VAL_MIDDLE ||
		  (id >= SVGCSS_VAL_BASELINE && id <= SVGCSS_VAL_MATHEMATICAL))
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
		else if(value->unit == CSS_URI)
		{
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, domString(value->string), CSS_URI);
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
		if(id == SVGCSS_VAL_MITER || id == SVGCSS_VAL_ROUND || id == SVGCSS_VAL_BEVEL)
			valid_primitive = true;
		break;

	case SVGCSS_PROP_STROKE_LINECAP:    // butt | round | square | inherit
		if(id == SVGCSS_VAL_BUTT || id == SVGCSS_VAL_ROUND || id == SVGCSS_VAL_SQUARE)
			valid_primitive = true;
		break;

	case SVGCSS_PROP_OPACITY:          // <opacity-value> | inherit
	case SVGCSS_PROP_STROKE_OPACITY:
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
     * correctly and allows optimization in khtml::applyRule(..)
     */

	case SVGCSS_PROP_POINTER_EVENTS:
	// visiblePainted | visibleFill | visibleStroke | visible |
	// painted | fill | stroke || all | inherit
		if(id == CSS_VAL_VISIBLE || 
		  (id >= SVGCSS_VAL_VISIBLEPAINTED && id <= SVGCSS_VAL_ALL))
			valid_primitive = true;
		break;

	case SVGCSS_PROP_TEXT_ANCHOR:    // start | middle | end | inherit
		if(id >= SVGCSS_VAL_START && id <= SVGCSS_VAL_END)
			valid_primitive = true;
		break;

	case SVGCSS_PROP_GLYPH_ORIENTATION_VERTICAL: // auto | <angle> | inherit
		if(id == CSS_VAL_AUTO)
		{
			valid_primitive = true;
			break;
		}
	case SVGCSS_PROP_GLYPH_ORIENTATION_HORIZONTAL: // <angle> | inherit
		if(value->unit == CSS_DEG)
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, value->fValue, CSS_DEG);
		else if(value->unit == CSS_GRAD)
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, value->fValue, CSS_GRAD);
		else if(value->unit == CSS_RAD)
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, value->fValue, CSS_RAD);
		break;

	case SVGCSS_PROP_FILL:                 // <paint> | inherit
	case SVGCSS_PROP_STROKE:               // <paint> | inherit
		{
			if(id == CSS_VAL_NONE)
				parsedValue = new SVGPaintImpl(SVG_PAINTTYPE_NONE);
			else if(id == SVGCSS_VAL_CURRENTCOLOR)
				parsedValue = new SVGPaintImpl(SVG_PAINTTYPE_CURRENTCOLOR);
			else if(value->unit == CSS_URI)
				parsedValue = new SVGPaintImpl(SVG_PAINTTYPE_URI, new DOMStringImpl(domString(value->string).string()));
			else
				parsedValue = parsePaint();

			if(parsedValue)
				valueList->next();
		}
		break;

	case CSS_PROP_COLOR:                // <color> | inherit
		if((id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) ||
		   (id >= SVGCSS_VAL_ALICEBLUE && id <= SVGCSS_VAL_YELLOWGREEN))
			parsedValue = new SVGColorImpl(new DOMStringImpl(domString(value->string).string()));
		else
			parsedValue = parseColor();

		if(parsedValue)
			valueList->next();
		break;

	case SVGCSS_PROP_STOP_COLOR: // TODO : icccolor
	case SVGCSS_PROP_FLOOD_COLOR:
	case SVGCSS_PROP_LIGHTING_COLOR:
		if((id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) ||
		   (id >= SVGCSS_VAL_ALICEBLUE && id <= SVGCSS_VAL_YELLOWGREEN))
			parsedValue = new SVGColorImpl(new DOMStringImpl(domString(value->string).string()));
		else if(id == SVGCSS_VAL_CURRENTCOLOR)
			parsedValue = new SVGColorImpl(SVG_COLORTYPE_CURRENTCOLOR);
		else // TODO : svgcolor (iccColor)
			parsedValue = parseColor();

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
			parsedValue = parseStrokeDasharray();

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
			return parseShortHand(properties, 3, important);
	}

	case SVGCSS_PROP_CLIP_PATH:	// <uri> | none | inherit
	case SVGCSS_PROP_FILTER:
		if(id == CSS_VAL_NONE)
			valid_primitive = true;
		else if(value->unit == CSS_URI)
		{
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, domString(value->string), (UnitTypes) value->unit);
			if(parsedValue)
				valueList->next();
		}
		break;

	default:
// #ifdef CSS_DEBUG
//         kdDebug(6080) << "illegal or CSS2 Aural property: " << val << endl;
// #endif
		return CSSParser::parseValue(propId, important, expected);
	}

	if(valid_primitive)
	{
		if(id != 0)
		{
			// qDebug(" new value: id=%d", id);
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, id);
		}
		else if(value->unit == CSS_STRING)
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, domString(value->string), (UnitTypes) value->unit);
		else if(value->unit >= CSS_NUMBER && value->unit <= CSS_KHZ)
		{
			// qDebug(" new value: value=%.2f, unit=%d", value->fValue, value->unit);
			parsedValue = new CSSPrimitiveValueImpl(m_cdfInterface, value->fValue, (UnitTypes) value->unit);
		}
		else if(value->unit >= KDOMCSSValue::Q_EMS)
		{
			// qDebug(" new quirks value: value=%.2f, unit=%d", value->fValue, value->unit);
			parsedValue = new CSSQuirkPrimitiveValueImpl(m_cdfInterface, value->fValue, CSS_EMS);
		}
		--expected;
		valueList->next();
		if(valueList->current() && expected == 0)
		{
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

CSSValueImpl *SVGCSSParser::parseStrokeDasharray()
{
	CSSValueListImpl *ret = new CSSValueListImpl;
	KDOMCSSValue *value = valueList->current();
	bool valid_primitive = true;
	while(valid_primitive && value)
	{
		valid_primitive = validUnit(value, FLength | FPercent |FNonNeg, false);
		if(value->id != 0)
		{
			// qDebug(" new value: id=%d", id);
			ret->append(new CSSPrimitiveValueImpl(m_cdfInterface, value->id));
		}
		else if(value->unit == CSS_STRING)
			ret->append(new CSSPrimitiveValueImpl(m_cdfInterface, domString(value->string), (UnitTypes) value->unit));
		else if(value->unit >= CSS_NUMBER && value->unit <= CSS_KHZ)
		{
			// qDebug(" new value: value=%.2f, unit=%d", value->fValue, value->unit);
			ret->append(new CSSPrimitiveValueImpl(m_cdfInterface, value->fValue, (UnitTypes) value->unit));
		}
		value = valueList->next();
		if(value && value->unit == KDOMCSSValue::Operator && value->iValue == ',')
			value = valueList->next();
	}

	return ret;
}

CSSValueImpl *SVGCSSParser::parsePaint()
{
	KDOMCSSValue *value = valueList->current();
	if(!strict && value->unit == CSS_NUMBER &&
	   value->fValue >= 0. && value->fValue < 1000000.)
	{
		QString str;
		str.sprintf("%06d", (int)(value->fValue+.5));
		return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(str));
	}
	else if(value->unit == CSS_RGBCOLOR)
	{
		QString str = QString::fromLatin1("#") + domString(value->string).string();
		return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(str));
	}
	else if(value->unit == CSS_IDENT ||
		   (!strict && value->unit == CSS_DIMENSION))
		return new SVGPaintImpl(SVG_PAINTTYPE_RGBCOLOR, 0, new DOMStringImpl(domString(value->string).string()));
	else if(value->unit == KDOMCSSValue::Function && value->function->args != 0 &&
			value->function->args->numValues == 5 /* rgb + two commas */ &&
			qString(value->function->name).lower() == "rgb(")
	{
		KDOMCSSValueList *args = value->function->args;
		KDOMCSSValue *v = args->current();
		if(!validUnit(v, FInteger|FPercent, true))
			return 0;
		int r = (int) (v->fValue * (v->unit == CSS_PERCENTAGE ? 256./100. : 1.));
		v = args->next();
		if(v->unit != KDOMCSSValue::Operator && v->iValue != ',')
			return 0;
		v = args->next();
		if(!validUnit(v, FInteger|FPercent, true))
			return 0;
		int g = (int) (v->fValue * (v->unit == CSS_PERCENTAGE ? 256./100. : 1.));
		v = args->next();
		if(v->unit != KDOMCSSValue::Operator && v->iValue != ',')
			return 0;
		v = args->next();
		if(!validUnit(v, FInteger|FPercent, true))
			return 0;
		int b = (int) (v->fValue * (v->unit == CSS_PERCENTAGE ? 256./100. : 1.));
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

CSSValueImpl *SVGCSSParser::parseColor()
{
	KDOMCSSValue *value = valueList->current();
	if(!strict && value->unit == CSS_NUMBER &&
	   value->fValue >= 0. && value->fValue < 1000000.)
	{
		QString str;
		str.sprintf("%06d", (int)(value->fValue+.5));
		return new SVGColorImpl(new DOMStringImpl(str));
	}
	else if(value->unit == CSS_RGBCOLOR)
	{
		QString str = QString::fromLatin1("#") + domString(value->string).string();
		return new SVGColorImpl(new DOMStringImpl(str));
	}
	else if(value->unit == CSS_IDENT ||
		   (!strict && value->unit == CSS_DIMENSION))
		return new SVGColorImpl(new DOMStringImpl(domString(value->string).string()));
	else if(value->unit == KDOMCSSValue::Function && value->function->args != 0 &&
			value->function->args->numValues == 5 /* rgb + two commas */ &&
			qString(value->function->name).lower() == "rgb(")
	{
		KDOMCSSValueList *args = value->function->args;
		KDOMCSSValue *v = args->current();
		if(!validUnit(v, FInteger|FPercent, true))
			return 0;
		int r = (int) (v->fValue * (v->unit == CSS_PERCENTAGE ? 256./100. : 1.));
		v = args->next();
		if(v->unit != KDOMCSSValue::Operator && v->iValue != ',')
			return 0;
		v = args->next();
		if(!validUnit(v, FInteger|FPercent, true))
			return 0;
		int g = (int) (v->fValue * (v->unit == CSS_PERCENTAGE ? 256./100. : 1.));
		v = args->next();
		if(v->unit != KDOMCSSValue::Operator && v->iValue != ',')
			return 0;
		v = args->next();
		if(!validUnit(v, FInteger|FPercent, true))
			return 0;
		int b = (int) (v->fValue * (v->unit == CSS_PERCENTAGE ? 256./100. : 1.));
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

bool SVGCSSParser::parseShape(int propId, bool important)
{
	// Small hack, allows to run parseShape in non-strict mode.
	// Needed, because svg clip property allows unitless values
	// and css2 clip does not.
	bool temp = strict;
	strict = false;
	bool ret = KDOM::CSSParser::parseShape(propId, important);
	strict = temp;
	return ret;
}

CSSStyleDeclarationImpl *SVGCSSParser::createCSSStyleDeclaration(CSSStyleRuleImpl *rule, QPtrList<CSSProperty> *propList)
{
	return new SVGCSSStyleDeclarationImpl(document()->implementation()->cdfInterface(), rule, propList);
}

// vim:ts=4:noet
