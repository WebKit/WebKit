/*
    Copyright (C) 2008 Eric Seidel <eric@webkit.org>
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007, 2010 Rob Buis <buis@kde.org>
    Copyright (C) 2005, 2006 Apple Inc.

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

#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "RenderTheme.h"
#include "SVGPaint.h"

#include <wtf/SetForScope.h>

namespace WebCore {

static bool isValidSystemControlColorValue(CSSValueID id)
{
    return id >= CSSValueActiveborder && CSSParser::isValidSystemColorValue(id);
}

bool CSSParser::parseSVGValue(CSSPropertyID propId, bool important)
{
    if (!m_valueList->current())
        return false;
    ValueWithCalculation valueWithCalculation(*m_valueList->current());

    CSSValueID id = valueWithCalculation.value().id;

    bool valid_primitive = false;
    RefPtr<CSSValue> parsedValue;

    switch (propId) {
    /* The comment to the right defines all valid value of these
     * properties as defined in SVG 1.1, Appendix N. Property index */
    case CSSPropertyBaselineShift:
    // baseline | super | sub | <percentage> | <length> | inherit
        if (id == CSSValueBaseline || id == CSSValueSub ||
           id >= CSSValueSuper)
            valid_primitive = true;
        else
            valid_primitive = validateUnit(valueWithCalculation, FLength | FPercent, SVGAttributeMode);
        break;

    case CSSPropertyEnableBackground:
    // accumulate | new [x] [y] [width] [height] | inherit
        if (id == CSSValueAccumulate) // TODO : new
            valid_primitive = true;
        break;

    case CSSPropertyMarkerStart:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMask:
        if (id == CSSValueNone)
            valid_primitive = true;
        else if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSPrimitiveValue::create(valueWithCalculation.value().string, CSSPrimitiveValue::CSS_URI);
            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyStrokeMiterlimit:   // <miterlimit> | inherit
        valid_primitive = validateUnit(valueWithCalculation, FNumber | FNonNeg, SVGAttributeMode);
        break;

    case CSSPropertyStrokeOpacity:   // <opacity-value> | inherit
    case CSSPropertyFillOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyFloodOpacity:
        valid_primitive = (!id && validateUnit(valueWithCalculation, FNumber | FPercent, SVGAttributeMode));
        break;

    case CSSPropertyColorProfile: // auto | sRGB | <name> | <uri> inherit
        if (id == CSSValueAuto || id == CSSValueSrgb)
            valid_primitive = true;
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in applyRule(..)
     */

    case CSSPropertyGlyphOrientationVertical: // auto | <angle> | inherit
        if (id == CSSValueAuto) {
            valid_primitive = true;
            break;
        }
        FALLTHROUGH;

    case CSSPropertyGlyphOrientationHorizontal: // <angle> (restricted to _deg_ per SVG 1.1 spec) | inherit
        if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_DEG || valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_NUMBER) {
            parsedValue = CSSPrimitiveValue::create(valueWithCalculation.value().fValue, CSSPrimitiveValue::CSS_DEG);

            if (parsedValue)
                m_valueList->next();
        }
        break;
    case CSSPropertyPaintOrder:
        if (id == CSSValueNormal)
            valid_primitive = true;
        else
            parsedValue = parsePaintOrder();
        break;
    case CSSPropertyFill:                 // <paint> | inherit
    case CSSPropertyStroke:               // <paint> | inherit
        {
            if (id == CSSValueNone)
                parsedValue = SVGPaint::createNone();
            else if (id == CSSValueCurrentcolor)
                parsedValue = SVGPaint::createCurrentColor();
            else if (isValidSystemControlColorValue(id) || id == CSSValueMenu)
                parsedValue = SVGPaint::createColor(RenderTheme::defaultTheme()->systemColor(id));
            else if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_URI) {
                if (m_valueList->next()) {
                    Color color = parseColorFromValue(*m_valueList->current());
                    if (color.isValid())
                        parsedValue = SVGPaint::createURIAndColor(valueWithCalculation.value().string, color);
                    else if (m_valueList->current()->id == CSSValueNone)
                        parsedValue = SVGPaint::createURIAndNone(valueWithCalculation.value().string);
                }
                if (!parsedValue)
                    parsedValue = SVGPaint::createURI(valueWithCalculation.value().string);
            } else
                parsedValue = parseSVGPaint();

            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyStopColor: // TODO : icccolor
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
        if (CSSParser::isValidSystemColorValue(id)
            || (id >= CSSValueAliceblue && id <= CSSValueYellowgreen))
            parsedValue = SVGColor::createFromString(valueWithCalculation.value().string);
        else if (id == CSSValueCurrentcolor)
            parsedValue = SVGColor::createCurrentColor();
        else // TODO : svgcolor (iccColor)
            parsedValue = parseSVGColor();

        if (parsedValue)
            m_valueList->next();

        break;

    case CSSPropertyStrokeWidth:         // <length> | inherit
    case CSSPropertyStrokeDashoffset:
        valid_primitive = validateUnit(valueWithCalculation, FLength | FPercent, SVGAttributeMode);
        break;
    case CSSPropertyStrokeDasharray:     // none | <dasharray> | inherit
        if (id == CSSValueNone)
            valid_primitive = true;
        else
            parsedValue = parseSVGStrokeDasharray();

        break;

    case CSSPropertyKerning:              // auto | normal | <length> | inherit
        if (id == CSSValueAuto || id == CSSValueNormal)
            valid_primitive = true;
        else
            valid_primitive = validateUnit(valueWithCalculation, FLength, SVGAttributeMode);
        break;

    case CSSPropertyClipPath:    // <uri> | none | inherit
    case CSSPropertyFilter:
        if (id == CSSValueNone)
            valid_primitive = true;
        else if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSPrimitiveValue::create(valueWithCalculation.value().string, (CSSPrimitiveValue::UnitTypes) valueWithCalculation.value().unit);
            if (parsedValue)
                m_valueList->next();
        }
        break;
    case CSSPropertyWebkitSvgShadow:
        if (id == CSSValueNone)
            valid_primitive = true;
        else {
            auto shadowValueList = parseShadow(*m_valueList, propId);
            if (shadowValueList) {
                addProperty(propId, WTFMove(shadowValueList), important);
                m_valueList->next();
                return true;
            }
            return false;
        }
        break;

    /* shorthand properties */
    case CSSPropertyMarker:
    {
        ShorthandScope scope(this, propId);
        SetForScope<bool> change(m_implicitShorthand, true);
        if (!parseValue(CSSPropertyMarkerStart, important))
            return false;
        if (m_valueList->current()) {
            rollbackLastProperties(1);
            return false;
        }
        CSSValue* value = m_parsedProperties.last().value();
        addProperty(CSSPropertyMarkerMid, value, important);
        addProperty(CSSPropertyMarkerEnd, value, important);
        return true;
    }
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyX:
    case CSSPropertyY:
        valid_primitive = (!id && validateUnit(valueWithCalculation, FLength | FPercent));
        break;
    default:
        // If you crash here, it's because you added a css property and are not handling it
        // in either this switch statement or the one in CSSParser::parseValue
        ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", propId);
        return false;
    }

    if (valid_primitive) {
        if (id != 0)
            parsedValue = CSSPrimitiveValue::createIdentifier(id);
        else if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = CSSPrimitiveValue::create(valueWithCalculation.value().string, (CSSPrimitiveValue::UnitTypes) valueWithCalculation.value().unit);
        else if (valueWithCalculation.value().unit >= CSSPrimitiveValue::CSS_NUMBER && valueWithCalculation.value().unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = CSSPrimitiveValue::create(valueWithCalculation.value().fValue, (CSSPrimitiveValue::UnitTypes) valueWithCalculation.value().unit);
        else if (valueWithCalculation.value().unit >= CSSParserValue::Q_EMS)
            parsedValue = CSSPrimitiveValue::createAllowingMarginQuirk(valueWithCalculation.value().fValue, CSSPrimitiveValue::CSS_EMS);
        if (isCalculation(valueWithCalculation))
            parsedValue = CSSPrimitiveValue::create(valueWithCalculation.calculation());
        m_valueList->next();
    }
    if (!parsedValue || (m_valueList->current() && !inShorthand()))
        return false;

    addProperty(propId, WTFMove(parsedValue), important);
    return true;
}

RefPtr<CSSValueList> CSSParser::parseSVGStrokeDasharray()
{
    RefPtr<CSSValueList> ret = CSSValueList::createCommaSeparated();
    CSSParserValue* value = m_valueList->current();
    bool valid_primitive = true;
    while (value) {
        ValueWithCalculation valueWithCalculation(*value);
        valid_primitive = validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg, SVGAttributeMode);
        if (!valid_primitive)
            break;
        // FIXME: This code doesn't handle calculated values.
        if (value->id != 0)
            ret->append(CSSPrimitiveValue::createIdentifier(value->id));
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            ret->append(CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit));
        value = m_valueList->next();
        if (value && value->unit == CSSParserValue::Operator && value->iValue == ',')
            value = m_valueList->next();
    }
    if (!valid_primitive)
        return nullptr;
    return ret;
}

RefPtr<SVGPaint> CSSParser::parseSVGPaint()
{
    Color color = parseColorFromValue(*m_valueList->current());
    if (!color.isValid())
        return nullptr;
    return SVGPaint::createColor(color);
}

RefPtr<SVGColor> CSSParser::parseSVGColor()
{
    Color color = parseColorFromValue(*m_valueList->current());
    if (!color.isValid())
        return nullptr;
    return SVGColor::createFromColor(color);
}

RefPtr<CSSValueList> CSSParser::parsePaintOrder()
{
    CSSParserValue* value = m_valueList->current();

    Vector<CSSValueID> paintTypeList;
    RefPtr<CSSPrimitiveValue> fill;
    RefPtr<CSSPrimitiveValue> stroke;
    RefPtr<CSSPrimitiveValue> markers;
    while (value) {
        if (value->id == CSSValueFill && !fill)
            fill = CSSPrimitiveValue::createIdentifier(value->id);
        else if (value->id == CSSValueStroke && !stroke)
            stroke = CSSPrimitiveValue::createIdentifier(value->id);
        else if (value->id == CSSValueMarkers && !markers)
            markers = CSSPrimitiveValue::createIdentifier(value->id);
        else
            return nullptr;
        paintTypeList.append(value->id);
        value = m_valueList->next();
    }

    // After parsing we serialize the paint-order list. Since it is not possible to
    // pop a last list items from CSSValueList without bigger cost, we create the
    // list after parsing. 
    CSSValueID firstPaintOrderType = paintTypeList.at(0);
    auto paintOrderList = CSSValueList::createSpaceSeparated();
    switch (firstPaintOrderType) {
    case CSSValueFill:
        FALLTHROUGH;
    case CSSValueStroke:
        paintOrderList->append(firstPaintOrderType == CSSValueFill ? fill.releaseNonNull() : stroke.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueMarkers)
                paintOrderList->append(markers.releaseNonNull());
        }
        break;
    case CSSValueMarkers:
        paintOrderList->append(markers.releaseNonNull());
        if (paintTypeList.size() > 1) {
            if (paintTypeList.at(1) == CSSValueStroke)
                paintOrderList->append(stroke.releaseNonNull());
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return WTFMove(paintOrderList);
}

}
