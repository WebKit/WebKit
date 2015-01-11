/*
    Copyright (C) 2005 Apple Inc.
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2008 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
             (C) 2003 Apple Inc.
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
#include "StyleBuilderConverter.h"
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

// FIXME: This method should go away once all SVG CSS properties have been
// ported to the new generated StyleBuilder.
void StyleResolver::applySVGProperty(CSSPropertyID id, CSSValue* value)
{
    ASSERT(value);
    CSSPrimitiveValue* primitiveValue = nullptr;
    if (is<CSSPrimitiveValue>(*value))
        primitiveValue = downcast<CSSPrimitiveValue>(value);

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
                svgStyle.setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(*primitiveValue));
            }

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
            if (is<SVGPaint>(*value)) {
                SVGPaint& svgPaint = downcast<SVGPaint>(*value);
                svgStyle.setFillPaint(svgPaint.paintType(), StyleBuilderConverter::convertSVGColor(*this, svgPaint), svgPaint.uri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
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
            if (is<SVGPaint>(*value)) {
                SVGPaint& svgPaint = downcast<SVGPaint>(*value);
                svgStyle.setStrokePaint(svgPaint.paintType(), StyleBuilderConverter::convertSVGColor(*this, svgPaint), svgPaint.uri(), applyPropertyToRegularStyle(), applyPropertyToVisitedLinkStyle());
            }
            break;
        }
        case CSSPropertyWebkitSvgShadow: {
            if (isInherit)
                return svgStyle.setShadow(state.parentStyle()->svgStyle().shadow() ? std::make_unique<ShadowData>(*state.parentStyle()->svgStyle().shadow()) : nullptr);
            if (isInitial || primitiveValue) // initial | none
                return svgStyle.setShadow(nullptr);

            if (!is<CSSValueList>(*value))
                return;

            CSSValueList& list = downcast<CSSValueList>(*value);
            if (!list.length())
                return;

            CSSValue* firstValue = list.itemWithoutBoundsCheck(0);
            if (!is<CSSShadowValue>(*firstValue))
                return;
            CSSShadowValue& item = downcast<CSSShadowValue>(*firstValue);
            IntPoint location(item.x->computeLength<int>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)),
                item.y->computeLength<int>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)));
            int blur = item.blur ? item.blur->computeLength<int>(state.cssToLengthConversionData().copyWithAdjustedZoom(1.0f)) : 0;
            Color color;
            if (item.color)
                color = colorFromPrimitiveValue(*item.color);

            // -webkit-svg-shadow does should not have a spread or style
            ASSERT(!item.spread);
            ASSERT(!item.style);

            auto shadowData = std::make_unique<ShadowData>(location, blur, 0, Normal, false, color.isValid() ? color : Color::transparent);
            svgStyle.setShadow(WTF::move(shadowData));
            return;
        }
        default:
            // If you crash here, it's because you added a css property and are not handling it
            // in either this switch statement or the one in StyleResolver::applyProperty
            ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", id);
            return;
    }
}

}
