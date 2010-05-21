/*
    Copyright (C) 2005 Apple Computer, Inc.
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2008 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
             (C) 2003 Apple Computer, Inc.
             (C) 2004 Allan Sandfeld Jensen(kde@carewolf.com)
             (C) 2004 Germain Garand(germain@ebooksfrance.org)

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
#include "Document.h"
#include "ShadowValue.h"
#include "SVGColor.h"
#include "SVGNames.h"
#include "SVGPaint.h"
#include "SVGRenderStyle.h"
#include "SVGRenderStyleDefs.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"
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

static Color colorFromSVGColorCSSValue(CSSValue* value, const Color& fgColor)
{
    ASSERT(value->isSVGColor());
    SVGColor* c = static_cast<SVGColor*>(value);
    Color color;
    if (c->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
        color = fgColor;
    else
        color = c->color();
    return color;
}

void CSSStyleSelector::applySVGProperty(int id, CSSValue* value)
{
    ASSERT(value);
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
        case CSSPropertyAlignmentBaseline:
        {
            HANDLE_INHERIT_AND_INITIAL(alignmentBaseline, AlignmentBaseline)
            if (!primitiveValue)
                break;
            
            svgstyle->setAlignmentBaseline(*primitiveValue);
            break;
        }
        case CSSPropertyBaselineShift:
        {
            HANDLE_INHERIT_AND_INITIAL(baselineShift, BaselineShift);
            if (!primitiveValue)
                break;

            if (primitiveValue->getIdent()) {
                switch (primitiveValue->getIdent()) {
                case CSSValueBaseline:
                    svgstyle->setBaselineShift(BS_BASELINE);
                    break;
                case CSSValueSub:
                    svgstyle->setBaselineShift(BS_SUB);
                    break;
                case CSSValueSuper:
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
        case CSSPropertyKerning:
        {
            HANDLE_INHERIT_AND_INITIAL(kerning, Kerning);
            svgstyle->setKerning(primitiveValue);
            break;
        }
        case CSSPropertyDominantBaseline:
        {
            HANDLE_INHERIT_AND_INITIAL(dominantBaseline, DominantBaseline)
            if (primitiveValue)
                svgstyle->setDominantBaseline(*primitiveValue);
            break;
        }
        case CSSPropertyColorInterpolation:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolation, ColorInterpolation)
            if (primitiveValue)
                svgstyle->setColorInterpolation(*primitiveValue);
            break;
        }
        case CSSPropertyColorInterpolationFilters:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolationFilters, ColorInterpolationFilters)
            if (primitiveValue)
                svgstyle->setColorInterpolationFilters(*primitiveValue);
            break;
        }
        case CSSPropertyColorRendering:
        {
            HANDLE_INHERIT_AND_INITIAL(colorRendering, ColorRendering)
            if (primitiveValue)
                svgstyle->setColorRendering(*primitiveValue);
            break;
        }
        case CSSPropertyClipRule:
        {
            HANDLE_INHERIT_AND_INITIAL(clipRule, ClipRule)
            if (primitiveValue)
                svgstyle->setClipRule(*primitiveValue);
            break;
        }
        case CSSPropertyFillRule:
        {
            HANDLE_INHERIT_AND_INITIAL(fillRule, FillRule)
            if (primitiveValue)
                svgstyle->setFillRule(*primitiveValue);
            break;
        }
        case CSSPropertyStrokeLinejoin:
        {
            HANDLE_INHERIT_AND_INITIAL(joinStyle, JoinStyle)
            if (primitiveValue)
                svgstyle->setJoinStyle(*primitiveValue);
            break;
        }
        case CSSPropertyImageRendering:
        {
            HANDLE_INHERIT_AND_INITIAL(imageRendering, ImageRendering)
            if (primitiveValue)
                svgstyle->setImageRendering(*primitiveValue);
            break;
        }
        case CSSPropertyShapeRendering:
        {
            HANDLE_INHERIT_AND_INITIAL(shapeRendering, ShapeRendering)
            if (primitiveValue)
                svgstyle->setShapeRendering(*primitiveValue);
            break;
        }
        // end of ident only properties
        case CSSPropertyFill:
        {
            HANDLE_INHERIT_AND_INITIAL(fillPaint, FillPaint)
            if (value->isSVGPaint())
                svgstyle->setFillPaint(static_cast<SVGPaint*>(value));
            break;
        }
        case CSSPropertyStroke:
        {
            HANDLE_INHERIT_AND_INITIAL(strokePaint, StrokePaint)
            if (value->isSVGPaint())
                svgstyle->setStrokePaint(static_cast<SVGPaint*>(value));
            
            break;
        }
        case CSSPropertyStrokeWidth:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeWidth, StrokeWidth)
            if (primitiveValue)
                svgstyle->setStrokeWidth(primitiveValue);
            break;
        }
        case CSSPropertyStrokeDasharray:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
            if (value->isValueList())
                svgstyle->setStrokeDashArray(static_cast<CSSValueList*>(value));
            break;
        }
        case CSSPropertyStrokeDashoffset:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashOffset, StrokeDashOffset)
            if (primitiveValue)
                svgstyle->setStrokeDashOffset(primitiveValue);
            break;
        }
        case CSSPropertyFillOpacity:
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
        case CSSPropertyStrokeOpacity:
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
        case CSSPropertyStopOpacity:
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
        case CSSPropertyMarkerStart:
        {
            HANDLE_INHERIT_AND_INITIAL(markerStartResource, MarkerStartResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setMarkerStartResource(SVGURIReference::getTarget(s));
            break;
        }
        case CSSPropertyMarkerMid:
        {
            HANDLE_INHERIT_AND_INITIAL(markerMidResource, MarkerMidResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setMarkerMidResource(SVGURIReference::getTarget(s));
            break;
        }
        case CSSPropertyMarkerEnd:
        {
            HANDLE_INHERIT_AND_INITIAL(markerEndResource, MarkerEndResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setMarkerEndResource(SVGURIReference::getTarget(s));
            break;
        }
        case CSSPropertyStrokeLinecap:
        {
            HANDLE_INHERIT_AND_INITIAL(capStyle, CapStyle)
            if (primitiveValue)
                svgstyle->setCapStyle(*primitiveValue);
            break;
        }
        case CSSPropertyStrokeMiterlimit:
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
        case CSSPropertyFilter:
        {
            HANDLE_INHERIT_AND_INITIAL(filterResource, FilterResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setFilterResource(SVGURIReference::getTarget(s));
            break;
        }
        case CSSPropertyMask:
        {
            HANDLE_INHERIT_AND_INITIAL(maskerResource, MaskerResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;
            
            svgstyle->setMaskerResource(SVGURIReference::getTarget(s));
            break;
        }
        case CSSPropertyClipPath:
        {
            HANDLE_INHERIT_AND_INITIAL(clipperResource, ClipperResource)
            if (!primitiveValue)
                return;

            String s;
            int type = primitiveValue->primitiveType();
            if (type == CSSPrimitiveValue::CSS_URI)
                s = primitiveValue->getStringValue();
            else
                return;

            svgstyle->setClipperResource(SVGURIReference::getTarget(s));
            break;
        }
        case CSSPropertyTextAnchor:
        {
            HANDLE_INHERIT_AND_INITIAL(textAnchor, TextAnchor)
            if (primitiveValue)
                svgstyle->setTextAnchor(*primitiveValue);
            break;
        }
        case CSSPropertyWritingMode:
        {
            HANDLE_INHERIT_AND_INITIAL(writingMode, WritingMode)
            if (primitiveValue)
                svgstyle->setWritingMode(*primitiveValue);
            break;
        }
        case CSSPropertyStopColor:
        {
            HANDLE_INHERIT_AND_INITIAL(stopColor, StopColor);
            svgstyle->setStopColor(colorFromSVGColorCSSValue(value, m_style->color()));
            break;
        }
       case CSSPropertyLightingColor:
        {
            HANDLE_INHERIT_AND_INITIAL(lightingColor, LightingColor);
            svgstyle->setLightingColor(colorFromSVGColorCSSValue(value, m_style->color()));
            break;
        }
        case CSSPropertyFloodOpacity:
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
        case CSSPropertyFloodColor:
        {
            if (isInitial) {
                svgstyle->setFloodColor(SVGRenderStyle::initialFloodColor());
                return;
            }
            svgstyle->setFloodColor(colorFromSVGColorCSSValue(value, m_style->color()));
            break;
        }
        case CSSPropertyGlyphOrientationHorizontal:
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
        case CSSPropertyGlyphOrientationVertical:
        {
            HANDLE_INHERIT_AND_INITIAL(glyphOrientationVertical, GlyphOrientationVertical)
            if (!primitiveValue)
                return;

            if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_DEG) {
                int orientation = angleToGlyphOrientation(primitiveValue->getFloatValue());
                ASSERT(orientation != -1);

                svgstyle->setGlyphOrientationVertical((EGlyphOrientation) orientation);
            } else if (primitiveValue->getIdent() == CSSValueAuto)
                svgstyle->setGlyphOrientationVertical(GO_AUTO);

            break;
        }
        case CSSPropertyEnableBackground:
            // Silently ignoring this property for now
            // http://bugs.webkit.org/show_bug.cgi?id=6022
            break;
        case CSSPropertyWebkitSvgShadow: {
            if (isInherit)
                return svgstyle->setShadow(m_parentStyle->svgStyle()->shadow() ? new ShadowData(*m_parentStyle->svgStyle()->shadow()) : 0);
            if (isInitial || primitiveValue) // initial | none
                return svgstyle->setShadow(0);

            if (!value->isValueList())
                return;

            CSSValueList *list = static_cast<CSSValueList*>(value);
            ASSERT(list->length() == 1);
            ShadowValue* item = static_cast<ShadowValue*>(list->itemWithoutBoundsCheck(0));
            int x = item->x->computeLengthInt(style(), m_rootElementStyle);
            int y = item->y->computeLengthInt(style(), m_rootElementStyle);
            int blur = item->blur ? item->blur->computeLengthInt(style(), m_rootElementStyle) : 0;
            Color color;
            if (item->color)
                color = getColorFromPrimitiveValue(item->color.get());

            // -webkit-svg-shadow does should not have a spread or style
            ASSERT(!item->spread);
            ASSERT(!item->style);
                
            ShadowData* shadowData = new ShadowData(x, y, blur, 0, Normal, color.isValid() ? color : Color::transparent);
            svgstyle->setShadow(shadowData);
            return;
        }
        default:
            // If you crash here, it's because you added a css property and are not handling it
            // in either this switch statement or the one in CSSStyleSelector::applyProperty
            ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", id);
            return;
    }
}

}

#endif
