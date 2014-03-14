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
#include "StyleResolver.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSShadowValue.h"
#include "CSSValueList.h"
#include "Document.h"
#include "SVGColor.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGPaint.h"
#include "SVGRenderStyle.h"
#include "SVGRenderStyleDefs.h"
#include "SVGURIReference.h"
#include <stdlib.h>
#include <wtf/MathExtras.h>

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) \
{ \
    svgStyle.set##Prop(state.parentStyle()->svgStyle().prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    svgStyle.set##Prop(SVGRenderStyle::initial##Prop()); \
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

static Color colorFromSVGColorCSSValue(SVGColor* svgColor, const Color& fgColor)
{
    Color color;
    if (svgColor->colorType() == SVGColor::SVG_COLORTYPE_CURRENTCOLOR)
        color = fgColor;
    else
        color = svgColor->color();
    return color;
}

void StyleResolver::applySVGProperty(CSSPropertyID id, CSSValue* value)
{
    ASSERT(value);
    CSSPrimitiveValue* primitiveValue = 0;
    if (value->isPrimitiveValue())
        primitiveValue = toCSSPrimitiveValue(value);

    const State& state = m_state;
    SVGRenderStyle& svgStyle = state.style()->accessSVGStyle();

    bool isInherit = state.parentStyle() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentStyle() && value->isInheritedValue());

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

            svgStyle.setAlignmentBaseline(*primitiveValue);
            break;
        }
        case CSSPropertyBaselineShift:
        {
            HANDLE_INHERIT_AND_INITIAL(baselineShift, BaselineShift);
            if (!primitiveValue)
                break;

            if (primitiveValue->getValueID()) {
                switch (primitiveValue->getValueID()) {
                case CSSValueBaseline:
                    svgStyle.setBaselineShift(BS_BASELINE);
                    break;
                case CSSValueSub:
                    svgStyle.setBaselineShift(BS_SUB);
                    break;
                case CSSValueSuper:
                    svgStyle.setBaselineShift(BS_SUPER);
                    break;
                default:
                    break;
                }
            } else {
                svgStyle.setBaselineShift(BS_LENGTH);
                svgStyle.setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(primitiveValue));
            }

            break;
        }
        case CSSPropertyKerning:
        {
            HANDLE_INHERIT_AND_INITIAL(kerning, Kerning);
            if (primitiveValue)
                svgStyle.setKerning(SVGLength::fromCSSPrimitiveValue(primitiveValue));
            break;
        }
        case CSSPropertyDominantBaseline:
        {
            HANDLE_INHERIT_AND_INITIAL(dominantBaseline, DominantBaseline)
            if (primitiveValue)
                svgStyle.setDominantBaseline(*primitiveValue);
            break;
        }
        case CSSPropertyColorInterpolation:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolation, ColorInterpolation)
            if (primitiveValue)
                svgStyle.setColorInterpolation(*primitiveValue);
            break;
        }
        case CSSPropertyColorInterpolationFilters:
        {
            HANDLE_INHERIT_AND_INITIAL(colorInterpolationFilters, ColorInterpolationFilters)
            if (primitiveValue)
                svgStyle.setColorInterpolationFilters(*primitiveValue);
            break;
        }
        case CSSPropertyColorProfile:
        {
            // Not implemented.
            break;
        }
        case CSSPropertyColorRendering:
        {
            HANDLE_INHERIT_AND_INITIAL(colorRendering, ColorRendering)
            if (primitiveValue)
                svgStyle.setColorRendering(*primitiveValue);
            break;
        }
        case CSSPropertyClipRule:
        {
            HANDLE_INHERIT_AND_INITIAL(clipRule, ClipRule)
            if (primitiveValue)
                svgStyle.setClipRule(*primitiveValue);
            break;
        }
        case CSSPropertyPaintOrder: {
            HANDLE_INHERIT_AND_INITIAL(paintOrder, PaintOrder)
            // 'normal' is the only primitiveValue
            if (primitiveValue)
                svgStyle.setPaintOrder(PaintOrderNormal);
            if (!value->isValueList())
                break;
            CSSValueList* orderTypeList = toCSSValueList(value);

            // Serialization happened during parsing. No additional checking needed.
            unsigned length = orderTypeList->length();
            primitiveValue = toCSSPrimitiveValue(orderTypeList->itemWithoutBoundsCheck(0));
            PaintOrder paintOrder;
            switch (primitiveValue->getValueID()) {
            case CSSValueFill:
                paintOrder = length > 1 ? PaintOrderFillMarkers : PaintOrderFill;
                break;
            case CSSValueStroke:
                paintOrder = length > 1 ? PaintOrderStrokeMarkers : PaintOrderStroke;
                break;
            case CSSValueMarkers:
                paintOrder = length > 1 ? PaintOrderMarkersStroke : PaintOrderMarkers;
                break;
            default:
                ASSERT_NOT_REACHED();
                paintOrder = PaintOrderNormal;
            }
            svgStyle.setPaintOrder(static_cast<PaintOrder>(paintOrder));
            break;
        }
        case CSSPropertyFillRule:
        {
            HANDLE_INHERIT_AND_INITIAL(fillRule, FillRule)
            if (primitiveValue)
                svgStyle.setFillRule(*primitiveValue);
            break;
        }
        case CSSPropertyStrokeLinejoin:
        {
            HANDLE_INHERIT_AND_INITIAL(joinStyle, JoinStyle)
            if (primitiveValue)
                svgStyle.setJoinStyle(*primitiveValue);
            break;
        }
        case CSSPropertyShapeRendering:
        {
            HANDLE_INHERIT_AND_INITIAL(shapeRendering, ShapeRendering)
            if (primitiveValue)
                svgStyle.setShapeRendering(*primitiveValue);
            break;
        }
        // end of ident only properties
        case CSSPropertyFill:
        {
            if (isInherit) {
                const SVGRenderStyle& svgParentStyle = state.parentStyle()->svgStyle();
                svgStyle.setFillPaint(svgParentStyle.fillPaintType(), svgParentStyle.fillPaintColor(), svgParentStyle.fillPaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (isInitial) {
                svgStyle.setFillPaint(SVGRenderStyle::initialFillPaintType(), SVGRenderStyle::initialFillPaintColor(), SVGRenderStyle::initialFillPaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (value->isSVGPaint()) {
                SVGPaint* svgPaint = toSVGPaint(value);
                svgStyle.setFillPaint(svgPaint->paintType(), colorFromSVGColorCSSValue(svgPaint, state.style()->color()), svgPaint->uri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
            }
            break;
        }
        case CSSPropertyStroke:
        {
            if (isInherit) {
                const SVGRenderStyle& svgParentStyle = state.parentStyle()->svgStyle();
                svgStyle.setStrokePaint(svgParentStyle.strokePaintType(), svgParentStyle.strokePaintColor(), svgParentStyle.strokePaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (isInitial) {
                svgStyle.setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
                return;
            }
            if (value->isSVGPaint()) {
                SVGPaint* svgPaint = toSVGPaint(value);
                svgStyle.setStrokePaint(svgPaint->paintType(), colorFromSVGColorCSSValue(svgPaint, state.style()->color()), svgPaint->uri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
            }
            break;
        }
        case CSSPropertyStrokeWidth:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeWidth, StrokeWidth)
            if (primitiveValue)
                svgStyle.setStrokeWidth(SVGLength::fromCSSPrimitiveValue(primitiveValue));
            break;
        }
        case CSSPropertyStrokeDasharray:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashArray, StrokeDashArray)
            if (!value->isValueList()) {
                svgStyle.setStrokeDashArray(SVGRenderStyle::initialStrokeDashArray());
                break;
            }

            CSSValueList* dashes = toCSSValueList(value);

            Vector<SVGLength> array;
            size_t length = dashes->length();
            for (size_t i = 0; i < length; ++i) {
                CSSValue* currValue = dashes->itemWithoutBoundsCheck(i);
                if (!currValue->isPrimitiveValue())
                    continue;

                CSSPrimitiveValue* dash = toCSSPrimitiveValue(dashes->itemWithoutBoundsCheck(i));
                array.append(SVGLength::fromCSSPrimitiveValue(dash));
            }

            svgStyle.setStrokeDashArray(array);
            break;
        }
        case CSSPropertyStrokeDashoffset:
        {
            HANDLE_INHERIT_AND_INITIAL(strokeDashOffset, StrokeDashOffset)
            if (primitiveValue)
                svgStyle.setStrokeDashOffset(SVGLength::fromCSSPrimitiveValue(primitiveValue));
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

            svgStyle.setFillOpacity(f);
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

            svgStyle.setStrokeOpacity(f);
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

            svgStyle.setStopOpacity(f);
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

            svgStyle.setMarkerStartResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
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

            svgStyle.setMarkerMidResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
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

            svgStyle.setMarkerEndResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyStrokeLinecap:
        {
            HANDLE_INHERIT_AND_INITIAL(capStyle, CapStyle)
            if (primitiveValue)
                svgStyle.setCapStyle(*primitiveValue);
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

            svgStyle.setStrokeMiterLimit(f);
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

            svgStyle.setFilterResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
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

            svgStyle.setMaskerResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
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

            svgStyle.setClipperResource(SVGURIReference::fragmentIdentifierFromIRIString(s, state.document()));
            break;
        }
        case CSSPropertyTextAnchor:
        {
            HANDLE_INHERIT_AND_INITIAL(textAnchor, TextAnchor)
            if (primitiveValue)
                svgStyle.setTextAnchor(*primitiveValue);
            break;
        }
        case CSSPropertyWritingMode:
        {
            HANDLE_INHERIT_AND_INITIAL(writingMode, WritingMode)
            if (primitiveValue)
                svgStyle.setWritingMode(*primitiveValue);
            break;
        }
        case CSSPropertyStopColor:
        {
            HANDLE_INHERIT_AND_INITIAL(stopColor, StopColor);
            if (value->isSVGColor())
                svgStyle.setStopColor(colorFromSVGColorCSSValue(toSVGColor(value), state.style()->color()));
            break;
        }
       case CSSPropertyLightingColor:
        {
            HANDLE_INHERIT_AND_INITIAL(lightingColor, LightingColor);
            if (value->isSVGColor())
                svgStyle.setLightingColor(colorFromSVGColorCSSValue(toSVGColor(value), state.style()->color()));
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

            svgStyle.setFloodOpacity(f);
            break;
        }
        case CSSPropertyFloodColor:
        {
            HANDLE_INHERIT_AND_INITIAL(floodColor, FloodColor);
            if (value->isSVGColor())
                svgStyle.setFloodColor(colorFromSVGColorCSSValue(toSVGColor(value), state.style()->color()));
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

                svgStyle.setGlyphOrientationHorizontal((EGlyphOrientation) orientation);
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

                svgStyle.setGlyphOrientationVertical((EGlyphOrientation) orientation);
            } else if (primitiveValue->getValueID() == CSSValueAuto)
                svgStyle.setGlyphOrientationVertical(GO_AUTO);

            break;
        }
        case CSSPropertyEnableBackground:
            // Silently ignoring this property for now
            // http://bugs.webkit.org/show_bug.cgi?id=6022
            break;
        case CSSPropertyWebkitSvgShadow: {
            if (isInherit)
                return svgStyle.setShadow(state.parentStyle()->svgStyle().shadow() ? std::make_unique<ShadowData>(*state.parentStyle()->svgStyle().shadow()) : nullptr);
            if (isInitial || primitiveValue) // initial | none
                return svgStyle.setShadow(nullptr);

            if (!value->isValueList())
                return;

            CSSValueList* list = toCSSValueList(value);
            if (!list->length())
                return;

            CSSValue* firstValue = list->itemWithoutBoundsCheck(0);
            if (!firstValue->isShadowValue())
                return;
            CSSShadowValue* item = toCSSShadowValue(firstValue);
            IntPoint location(item->x->computeLength<int>(state.style(), state.rootElementStyle()),
                item->y->computeLength<int>(state.style(), state.rootElementStyle()));
            int blur = item->blur ? item->blur->computeLength<int>(state.style(), state.rootElementStyle()) : 0;
            Color color;
            if (item->color)
                color = colorFromPrimitiveValue(item->color.get());

            // -webkit-svg-shadow does should not have a spread or style
            ASSERT(!item->spread);
            ASSERT(!item->style);

            auto shadowData = std::make_unique<ShadowData>(location, blur, 0, Normal, false, color.isValid() ? color : Color::transparent);
            svgStyle.setShadow(std::move(shadowData));
            return;
        }
        case CSSPropertyVectorEffect: {
            HANDLE_INHERIT_AND_INITIAL(vectorEffect, VectorEffect)
            if (!primitiveValue)
                break;

            svgStyle.setVectorEffect(*primitiveValue);
            break;
        }
        case CSSPropertyBufferedRendering: {
            HANDLE_INHERIT_AND_INITIAL(bufferedRendering, BufferedRendering)
            if (!primitiveValue)
                break;

            svgStyle.setBufferedRendering(*primitiveValue);
            break;
        }
        case CSSPropertyMaskType: {
            HANDLE_INHERIT_AND_INITIAL(maskType, MaskType)
            if (!primitiveValue)
                break;

            svgStyle.setMaskType(*primitiveValue);
            break;
        }
        default:
            // If you crash here, it's because you added a css property and are not handling it
            // in either this switch statement or the one in StyleResolver::applyProperty
            ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", id);
            return;
    }
}

}
