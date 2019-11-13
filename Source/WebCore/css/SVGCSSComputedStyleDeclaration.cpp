/*
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
    Copyright (C) 2019 Apple Inc. All rights reserved.

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
#include "CSSComputedStyleDeclaration.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSValueList.h"
#include "Document.h"
#include "Element.h"
#include "RenderStyle.h"

namespace WebCore {

static RefPtr<CSSPrimitiveValue> glyphOrientationToCSSPrimitiveValue(GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        return CSSPrimitiveValue::create(0.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees90:
        return CSSPrimitiveValue::create(90.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees180:
        return CSSPrimitiveValue::create(180.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Degrees270:
        return CSSPrimitiveValue::create(270.0f, CSSUnitType::CSS_DEG);
    case GlyphOrientation::Auto:
        return nullptr;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static Ref<CSSValue> strokeDashArrayToCSSValueList(const Vector<SVGLengthValue>& dashes)
{
    if (dashes.isEmpty())
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);

    auto list = CSSValueList::createCommaSeparated();
    for (auto& length : dashes)
        list->append(SVGLengthValue::toCSSPrimitiveValue(length));

    return list;
}

Ref<CSSValue> ComputedStyleExtractor::adjustSVGPaintForCurrentColor(SVGPaintType paintType, const String& url, const Color& color, const Color& currentColor) const
{
    if (paintType >= SVGPaintType::URINone) {
        auto values = CSSValueList::createSpaceSeparated();
        values->append(CSSPrimitiveValue::create(url, CSSUnitType::CSS_URI));
        if (paintType == SVGPaintType::URINone)
            values->append(CSSPrimitiveValue::createIdentifier(CSSValueNone));
        else if (paintType == SVGPaintType::URICurrentColor)
            values->append(CSSPrimitiveValue::create(currentColor));
        else if (paintType == SVGPaintType::URIRGBColor)
            values->append(CSSPrimitiveValue::create(color));
        return values;
    }
    if (paintType == SVGPaintType::None)
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    if (paintType == SVGPaintType::CurrentColor)
        return CSSPrimitiveValue::create(currentColor);
    
    return CSSPrimitiveValue::create(color);
}

RefPtr<CSSValue> ComputedStyleExtractor::svgPropertyValue(CSSPropertyID propertyID)
{
    if (!m_element)
        return nullptr;

    auto* style = m_element->computedStyle();
    if (!style)
        return nullptr;

    const SVGRenderStyle& svgStyle = style->svgStyle();

    switch (propertyID) {
    case CSSPropertyClipRule:
        return CSSPrimitiveValue::create(svgStyle.clipRule());
    case CSSPropertyFloodOpacity:
        return CSSPrimitiveValue::create(svgStyle.floodOpacity(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyStopOpacity:
        return CSSPrimitiveValue::create(svgStyle.stopOpacity(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyColorInterpolation:
        return CSSPrimitiveValue::create(svgStyle.colorInterpolation());
    case CSSPropertyColorInterpolationFilters:
        return CSSPrimitiveValue::create(svgStyle.colorInterpolationFilters());
    case CSSPropertyFillOpacity:
        return CSSPrimitiveValue::create(svgStyle.fillOpacity(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyFillRule:
        return CSSPrimitiveValue::create(svgStyle.fillRule());
    case CSSPropertyColorRendering:
        return CSSPrimitiveValue::create(svgStyle.colorRendering());
    case CSSPropertyShapeRendering:
        return CSSPrimitiveValue::create(svgStyle.shapeRendering());
    case CSSPropertyStrokeOpacity:
        return CSSPrimitiveValue::create(svgStyle.strokeOpacity(), CSSUnitType::CSS_NUMBER);
    case CSSPropertyAlignmentBaseline:
        return CSSPrimitiveValue::create(svgStyle.alignmentBaseline());
    case CSSPropertyDominantBaseline:
        return CSSPrimitiveValue::create(svgStyle.dominantBaseline());
    case CSSPropertyTextAnchor:
        return CSSPrimitiveValue::create(svgStyle.textAnchor());
    case CSSPropertyMask:
        if (!svgStyle.maskerResource().isEmpty())
            return CSSPrimitiveValue::create(svgStyle.maskerResource(), CSSUnitType::CSS_URI);
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyFloodColor:
        return currentColorOrValidColor(style, svgStyle.floodColor());
    case CSSPropertyLightingColor:
        return currentColorOrValidColor(style, svgStyle.lightingColor());
    case CSSPropertyStopColor:
        return currentColorOrValidColor(style, svgStyle.stopColor());
    case CSSPropertyFill:
        return adjustSVGPaintForCurrentColor(svgStyle.fillPaintType(), svgStyle.fillPaintUri(), svgStyle.fillPaintColor(), style->color());
    case CSSPropertyKerning:
        return SVGLengthValue::toCSSPrimitiveValue(svgStyle.kerning());
    case CSSPropertyMarkerEnd:
        if (!svgStyle.markerEndResource().isEmpty())
            return CSSPrimitiveValue::create(svgStyle.markerEndResource(), CSSUnitType::CSS_URI);
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyMarkerMid:
        if (!svgStyle.markerMidResource().isEmpty())
            return CSSPrimitiveValue::create(svgStyle.markerMidResource(), CSSUnitType::CSS_URI);
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyMarkerStart:
        if (!svgStyle.markerStartResource().isEmpty())
            return CSSPrimitiveValue::create(svgStyle.markerStartResource(), CSSUnitType::CSS_URI);
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    case CSSPropertyStroke:
        return adjustSVGPaintForCurrentColor(svgStyle.strokePaintType(), svgStyle.strokePaintUri(), svgStyle.strokePaintColor(), style->color());
    case CSSPropertyStrokeDasharray:
        return strokeDashArrayToCSSValueList(svgStyle.strokeDashArray());
    case CSSPropertyBaselineShift: {
        switch (svgStyle.baselineShift()) {
        case BaselineShift::Baseline:
            return CSSPrimitiveValue::createIdentifier(CSSValueBaseline);
        case BaselineShift::Super:
            return CSSPrimitiveValue::createIdentifier(CSSValueSuper);
        case BaselineShift::Sub:
            return CSSPrimitiveValue::createIdentifier(CSSValueSub);
        case BaselineShift::Length:
            return SVGLengthValue::toCSSPrimitiveValue(svgStyle.baselineShiftValue());
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    case CSSPropertyBufferedRendering:
        return CSSPrimitiveValue::create(svgStyle.bufferedRendering());
    case CSSPropertyGlyphOrientationHorizontal:
        return glyphOrientationToCSSPrimitiveValue(svgStyle.glyphOrientationHorizontal());
    case CSSPropertyGlyphOrientationVertical: {
        if (RefPtr<CSSPrimitiveValue> value = glyphOrientationToCSSPrimitiveValue(svgStyle.glyphOrientationVertical()))
            return value;

        if (svgStyle.glyphOrientationVertical() == GlyphOrientation::Auto)
            return CSSPrimitiveValue::createIdentifier(CSSValueAuto);

        return nullptr;
    }
    case CSSPropertyVectorEffect:
        return CSSPrimitiveValue::create(svgStyle.vectorEffect());
    case CSSPropertyMaskType:
        return CSSPrimitiveValue::create(svgStyle.maskType());
    case CSSPropertyMarker:
    case CSSPropertyEnableBackground:
    case CSSPropertyColorProfile:
        // the above properties are not yet implemented in the engine
        break;
    default:
        // If you crash here, it's because you added a css property and are not handling it
        // in either this switch statement or the one in CSSComputedStyleDelcaration::getPropertyCSSValue
        ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", propertyID);
    }
    return nullptr;
}

}
