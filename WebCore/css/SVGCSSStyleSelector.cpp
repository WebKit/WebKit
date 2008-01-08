/*
    Copyright (C) 2005 Apple Computer, Inc.
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "CSSStyleSelector.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSValueList.h"
#include "SVGColor.h"
#include "SVGNames.h"
#include "SVGPaint.h"
#include "SVGRenderStyle.h"
#include "SVGRenderStyleDefs.h"
#include "SVGStyledElement.h"
#include <stdlib.h>
#include <wtf/MathExtras.h>

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) \
{\
    svgstyle->set##Prop(m_parentStyle->svgStyle()->prop());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
else if (isInitial) \
    svgstyle->set##Prop(SVGRenderStyle::initial##Prop());

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if (id == propID) \
{\
    svgstyle->set##Prop(m_parentStyle->svgStyle()->prop());\
    return;\
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if (id == propID) \
{\
    svgstyle->set##Prop(SVGRenderStyle::initial##Prop());\
    return;\
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if (id == propID) { \
    svgstyle->set##Prop(SVGRenderStyle::initial##Value()); \
    return; \
}

namespace WebCore {

static float roundToNearestGlyphOrientationAngle(float angle)
{
    angle = fabsf(fmodf(angle, 360.0f));

    if (angle <= 45.0f || angle > 315.0f)
        return 0.0f;
    else if (angle > 45.0f && angle <= 135.0f)
        return 90.0f;
    else if (angle > 135.0f && angle <= 225.0f)
        return 180.0f;

    return 270.0f;
}

static int angleToGlyphOrientation(float angle)
{
    angle = roundToNearestGlyphOrientationAngle(angle);

    if (angle == 0.0f)
        return GO_0DEG;
    else if (angle == 90.0f)
        return GO_90DEG;
    else if (angle == 180.0f)
        return GO_180DEG;
    else if (angle == 270.0f)
        return GO_270DEG;

    return -1;
}

void CSSStyleSelector::applySVGProperty(int id, CSSValue* value)
{
    CSSPrimitiveValue* primitiveValue = 0;
    if (value->isPrimitiveValue())
        primitiveValue = static_cast<CSSPrimitiveValue*>(value);

    SVGRenderStyle* svgstyle = m_style->accessSVGStyle();
    unsigned short valueType = value->cssValueType();
    
    bool isInherit = m_parentNode && valueType == CSSPrimitiveValue::CSS_INHERIT;
    bool isInitial = valueType == CSSPrimitiveValue::CSS_INITIAL || (!m_parentNode && valueType == CSSPrimitiveValue::CSS_INHERIT);

    // What follows is a list that maps the CSS properties into their
    // corresponding front-end RenderStyle values. Shorthands(e.g. border,
    // background) occur in this list as well and are only hit when mapping
    // "inherit" or "initial" into front-end values.
    switch (id)
    {
        // ident only properties
        case CSS_PROP_ALIGNMENT_BASELINE:
        {
            HANDLE_INHERIT_AND_INITIAL(alignmentBaseline, AlignmentBaseline)
            if (!primitiveValue)
                break;
            
            svgstyle->setAlignmentBaseline(*primitiveValue);
            break;
        }
        case CSS_PROP_BASELINE_SHIFT:
        {
            HANDLE_INHERIT_AND_INITIAL(baselineShift, BaselineShift);
            if (!primitiveValue)
                break;

            if (primitiveValue->getIdent()) {
                switch (primitiveValue->getIdent()) {
                case CSS_VAL_BASELINE:
                    svgstyle->setBaselineShift(BS_BASELINE);
                    break;
                case CSS_VAL_SUB:
                    svgstyle->setBaselineShift(BS_SUB);
                    break;
                case CSS_VAL_SUPER:
                    svgstyle->setBaselineShift(BS_SUPER);
                    break;
                default:
                    break;
                }
            } else {
                svgstyle->setBaselineShift(BS_LENGTH);
                svgstyle->setBaselineShiftValue(primitiveValue);
            }

            break;
        }
        case CSS_PROP_KERNING:
        {
            if (isInherit) {
                HANDLE_INHERIT_COND(CSS_PROP_KERNING, kerning, Kerning)
                return;
            }
            else if (isInitial) {
                HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_KERNING, Kerning, Kerning)
                return;
            }

            svgstyle->setKerning(primitiveValue);
            break;
        }
        case CSS_PROP_POINTER_EVENTS:
        {
            HANDLE_INHERIT_AND_INITIAL(pointerEvents, PointerEvents)
            if (!primitiveValue)
                break;
            
            svgstyle->setPointerEvents(*primitiveValue);
            break;
        }
        case CSS_PROP_DOMINANT_BASELINE:
        {
            HANDLE_INHERIT_AND_INITIAL(dominantBaseline, DominantBaseline)
            if (primitiveValue)
                svgstyle->setDominantBaseline(*primitiveValue);
            break;
        }
        case CSS_PROP_COLOR_INTERPOLATION:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolation, ColorInterpolation)
            if (primitiveValue)
                svgstyle->setColorInterpolation(*primitiveValue);
            break;
        }
        case CSS_PROP_COLOR_INTERPOLATION_FILTERS:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolationFilters, ColorInterpolationFilters)
            if (primitiveValue)
                svgstyle->setColorInterpolationFilters(*primitiveValue);
            break;
        }
        case CSS_PROP_COLOR_RENDERING:
        {
            HANDLE_INHERIT_AND_INITIAL(colorRendering, ColorRendering)
            if (primitiveValue)
                svgstyle->setColorRendering(*primitiveValue);
            break;
        }
        case CSS_PROP_CLIP_RULE:
        {
            HANDLE_INHERIT_AND_INITIAL(clipRule, ClipRule)
            if (primitiveValue)
                svgstyle->setClipRule(*primitiveValue);
            break;
        }
        case CSS_PROP_FILL_RULE:
        {
            HANDLE_INHERIT_AND_INITIAL(fillRule, FillRule)
            if (primitiveValue)
                svgstyle->setFillRule(*primitiveValue);
            break;
        }
        case CSS_PROP_STROKE_LINEJOIN:
        {
            HANDLE_INHERIT_AND_INITIAL(joinStyle, JoinStyle)
            if (primitiveValue)
                svgstyle->setJoinStyle(*primitiveValue);
            break;
        }
        case CSS_PROP_IMAGE_RENDERING:
        {
            HANDLE_INHERIT_AND_INITIAL(imageRendering, ImageRendering)
            if (primitiveValue)
                svgstyle->setImageRendering(*primitiveValue);
            break;
        }
        case CSS_PROP_SHAPE_RENDERING:
        {
            HANDLE_INHERIT_AND_INITIAL(shapeRendering, ShapeRendering)
            if (primitiveValue)
                svgstyle->setShapeRendering(*primitiveValue);
            break;
        }
        case CSS_PROP_TEXT_RENDERING:
        {
            HANDLE_INHERIT_AND_INITIAL(textRendering, TextRendering)
            if (primitiveValue)
                svgstyle->setTextRendering(*primitiveValue);
            break;
        }
        // end of ident only properties
        case CSS_PROP_FILL:
        {
            HANDLE_INHERIT_AND_INITIAL(fillPaint, FillPaint)
            if (!primitiveValue && value) {
                SVGPaint *paint = static_cast<SVGPaint*>(value);
                if (paint)
                    svgstyle->setFillPaint(paint);
            }
            
            break;
        }
        case CSS_PROP_STROKE:
        {
            HANDLE_INHERIT_AND_INITIAL(strokePaint, StrokePaint)
            if (!primitiveValue && value) {
                SVGPaint *paint = static_cast<SVGPaint*>(value);
                if (paint)
                    svgstyle->setStrokePaint(paint);
            }
            
            break;
        }
        case CSS_PROP_STROKE_WIDTH:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeWidth, StrokeWidth)
            if (!primitiveValue)
                return;
        
            svgstyle->setStrokeWidth(primitiveValue);
            break;
        }
        case CSS_PROP_STROKE_DASHARRAY:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
            if (!primitiveValue && value) {
                CSSValueList* dashes = static_cast<CSSValueList*>(value);
                if (dashes)
                    svgstyle->setStrokeDashArray(dashes);
            }
        
            break;
        }
        case CSS_PROP_STROKE_DASHOFFSET:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashOffset, StrokeDashOffset)
            if (!primitiveValue)
                return;

            svgstyle->setStrokeDashOffset(primitiveValue);
            break;
        }
        case CSS_PROP_FILL_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(fillOpacity, FillOpacity)
            if (!primitiveValue)
                return;
        
            float f = 0.0f;    
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgstyle->setFillOpacity(f);
            break;
        }
        case CSS_PROP_STROKE_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeOpacity, StrokeOpacity)
            if (!primitiveValue)
                return;
        
            float f = 0.0f;    
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgstyle->setStrokeOpacity(f);
            break;
        }
        case CSS_PROP_STOP_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(stopOpacity, StopOpacity)
            if (!primitiveValue)
                return;
        
            float f = 0.0f;    
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgstyle->setStopOpacity(f);
            break;
        }
        case CSS_PROP_MARKER_START:
        {
            HANDLE_INHERIT_AND_INITIAL(startMarker, StartMarker)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setStartMarker(s);
            break;
        }
        case CSS_PROP_MARKER_MID:
        {
            HANDLE_INHERIT_AND_INITIAL(midMarker, MidMarker)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setMidMarker(s);
            break;
        }
        case CSS_PROP_MARKER_END:
        {
            HANDLE_INHERIT_AND_INITIAL(endMarker, EndMarker)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setEndMarker(s);
            break;
        }
        case CSS_PROP_STROKE_LINECAP:
        {
            HANDLE_INHERIT_AND_INITIAL(capStyle, CapStyle)
            if (primitiveValue)
                svgstyle->setCapStyle(*primitiveValue);
            break;
        }
        case CSS_PROP_STROKE_MITERLIMIT:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeMiterLimit, StrokeMiterLimit)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgstyle->setStrokeMiterLimit(f);
            break;
        }
        case CSS_PROP_FILTER:
        {
            HANDLE_INHERIT_AND_INITIAL(filter, Filter)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;
            svgstyle->setFilter(s);
            break;
        }
        case CSS_PROP_MASK:
        {
            HANDLE_INHERIT_AND_INITIAL(maskElement, MaskElement)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setMaskElement(s);
            break;
        }
        case CSS_PROP_CLIP_PATH:
        {
            HANDLE_INHERIT_AND_INITIAL(clipPath, ClipPath)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setClipPath(s);
            break;
        }
        case CSS_PROP_TEXT_ANCHOR:
        {
            HANDLE_INHERIT_AND_INITIAL(textAnchor, TextAnchor)
            if (primitiveValue)
                svgstyle->setTextAnchor(*primitiveValue);
            break;
        }
        case CSS_PROP_WRITING_MODE:
        {
            HANDLE_INHERIT_AND_INITIAL(writingMode, WritingMode)
            if (primitiveValue)
                svgstyle->setWritingMode(*primitiveValue);
            break;
        }
        case CSS_PROP_STOP_COLOR:
        {
            HANDLE_INHERIT_AND_INITIAL(stopColor, StopColor);

            SVGColor* c = static_cast<SVGColor*>(value);
            if (!c)
                return CSSStyleSelector::applyProperty(id, value);

            Color col;
            if (c->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
                col = m_style->color();
            else
                col = c->color();

            svgstyle->setStopColor(col);
            break;
        }
       case CSS_PROP_LIGHTING_COLOR:
        {
            HANDLE_INHERIT_AND_INITIAL(lightingColor, LightingColor);

            SVGColor* c = static_cast<SVGColor*>(value);
            if (!c)
                return CSSStyleSelector::applyProperty(id, value);

            Color col;
            if (c->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
                col = m_style->color();
            else
                col = c->color();

            svgstyle->setLightingColor(col);
            break;
        }
        case CSS_PROP_FLOOD_OPACITY:
        {
            HANDLE_INHERIT_AND_INITIAL(floodOpacity, FloodOpacity)
            if (!primitiveValue)
                return;

            float f = 0.0f;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
                f = primitiveValue->getFloatValue() / 100.0f;
            else if (type == CSSPrimitiveValue::CSS_NUMBER)
                f = primitiveValue->getFloatValue();
            else
                return;

            svgstyle->setFloodOpacity(f);
            break;
        }
        case CSS_PROP_FLOOD_COLOR:
        {
            Color col;
            if (isInitial)
                col = SVGRenderStyle::initialFloodColor();
            else {
                SVGColor *c = static_cast<SVGColor*>(value);
                if (!c)
                    return CSSStyleSelector::applyProperty(id, value);

                if (c->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
                    col = m_style->color();
                else
                    col = c->color();
            }

            svgstyle->setFloodColor(col);
            break;
        }
        case CSS_PROP_GLYPH_ORIENTATION_HORIZONTAL:
        {
            HANDLE_INHERIT_AND_INITIAL(glyphOrientationHorizontal, GlyphOrientationHorizontal)
            if (!primitiveValue)
                return;

            if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_DEG) {
                int orientation = angleToGlyphOrientation(primitiveValue->getFloatValue());
                ASSERT(orientation != -1);

                svgstyle->setGlyphOrientationHorizontal((EGlyphOrientation) orientation);
            }

            break;
        }
        case CSS_PROP_GLYPH_ORIENTATION_VERTICAL:
        {
            HANDLE_INHERIT_AND_INITIAL(glyphOrientationVertical, GlyphOrientationVertical)
            if (!primitiveValue)
                return;

            if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_DEG) {
                int orientation = angleToGlyphOrientation(primitiveValue->getFloatValue());
                ASSERT(orientation != -1);

                svgstyle->setGlyphOrientationVertical((EGlyphOrientation) orientation);
            } else if (primitiveValue->getIdent() == CSS_VAL_AUTO)
                svgstyle->setGlyphOrientationVertical(GO_AUTO);

            break;
        }
        case CSS_PROP_ENABLE_BACKGROUND:
            // Silently ignoring this property for now
            // http://bugs.webkit.org/show_bug.cgi?id=6022
            break;
        default:
            // If you crash here, it's because you added a css property and are not handling it
            // in either this switch statement or the one in CSSStyleSelector::applyProperty
            ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", id);
            return;
    }
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)
