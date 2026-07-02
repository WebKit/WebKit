/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSPrimitiveValue.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSMarkup.h"
#include "CSSParserIdioms.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSSerializationContext.h"
#include "CSSToLengthConversionData.h"
#include "CSSUnevaluatedCalc.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "ComputedStyleDependencies.h"
#include "ContainerQueryEvaluator.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "FontCascade.h"
#include "NodeRenderStyle.h"
#include "RenderBoxInlines.h"
#include "RenderView.h"
#include "StyleComputedStyle.h"
#include "StyleLengthResolution.h"
#include "StylePrimitiveNumericTypes+Rounding.h"
#include <wtf/Hasher.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static HashMap<const CSSPrimitiveValue*, String>& NODELETE serializedPrimitiveValues()
{
    static NeverDestroyed<HashMap<const CSSPrimitiveValue*, String>> map;
    return map;
}

CSSUnitType CSSPrimitiveValue::primitiveType() const
{
    if (RefPtr calcValue = cssCalcValue())
        return calcValue->primitiveType();
    return primitiveUnitType();
}

CSSPrimitiveValue::CSSPrimitiveValue(double number, CSSUnitType type)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(type);
    m_value.number = number;
}

CSSPrimitiveValue::CSSPrimitiveValue(StaticCSSValueTag, double number, CSSUnitType type)
    : CSSPrimitiveValue(number, type)
{
    makeStatic();
}

CSSPrimitiveValue::CSSPrimitiveValue(CSS::UnevaluatedCalcBase&& value)
    : CSSValue(ClassType::Primitive)
{
    setPrimitiveUnitType(CSSUnitType::CSS_CALC);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_CALC:
        m_value.calc->deref();
        break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        break;
    }
    if (m_hasCachedCSSText) {
        ASSERT(serializedPrimitiveValues().contains(this));
        serializedPrimitiveValues().remove(this);
    }
}

static CSSPrimitiveValue* valueFromPool(std::span<AlignedStorage<CSSPrimitiveValue>> pool, double value)
{
    // Casting to a signed integer first since casting a negative floating point value to an unsigned
    // integer is undefined behavior.
    unsigned poolIndex = static_cast<unsigned>(static_cast<int>(value));
    double roundTripValue = poolIndex;
    if (equalSpans(asByteSpan(value), asByteSpan(roundTripValue)) && poolIndex < pool.size())
        return pool[poolIndex].get();
    return nullptr;
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value)
{
    if (RefPtr result = valueFromPool(staticCSSValuePool->m_numberValues, value))
        return result.releaseNonNull();
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::CSS_NUMBER));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_numberValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::CSS_PERCENTAGE:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_percentageValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::CSS_PX:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_pixelValues, value))
            return result.releaseNonNull();
        break;
    default:
        break;
    }
    return adoptRef(*new CSSPrimitiveValue(value, type));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(CSS::UnevaluatedCalcBase value)
{
    return adoptRef(*new CSSPrimitiveValue(WTF::move(value)));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::createInteger(double value)
{
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::CSS_INTEGER));
}

std::optional<bool> CSSPrimitiveValue::isZero() const
{
    if (isCalculated())
        return std::nullopt;
    return !m_value.number;
}

std::optional<bool> CSSPrimitiveValue::isOne() const
{
    if (isCalculated())
        return std::nullopt;
    return m_value.number == 1;
}

std::optional<bool> CSSPrimitiveValue::isPositive() const
{
    if (isCalculated())
        return std::nullopt;
    return m_value.number > 0;
}

std::optional<bool> CSSPrimitiveValue::isNegative() const
{
    if (isCalculated())
        return std::nullopt;
    return m_value.number < 0;
}

NEVER_INLINE String CSSPrimitiveValue::formatNumberValue(ASCIILiteral suffix) const
{
    return CSS::formatCSSNumberValue(CSS::SerializableNumber { m_value.number, suffix });
}

NEVER_INLINE String CSSPrimitiveValue::formatIntegerValue(ASCIILiteral suffix) const
{
    if (!std::isfinite(m_value.number))
        return CSS::formatNonfiniteCSSNumberValue(CSS::SerializableNumber { m_value.number, suffix });
    return makeString(m_value.number, suffix);
}

ALWAYS_INLINE String CSSPrimitiveValue::serializeInternal(const CSS::SerializationContext& context) const
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQMAX:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_X:
        return formatNumberValue(unitTypeString(type));
    case CSSUnitType::CSS_CALC:
        return protect(cssCalcValue())->cssText(context);
    case CSSUnitType::CSS_INTEGER:
        return formatIntegerValue(""_s);
    case CSSUnitType::CSS_QUIRKY_EM:
        return formatNumberValue("em"_s);
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_UNKNOWN:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSPrimitiveValue::customCSSText(const CSS::SerializationContext& context) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        return String();
    default:
        auto& map = serializedPrimitiveValues();
        ASSERT(map.contains(this) == m_hasCachedCSSText);
        if (m_hasCachedCSSText)
            return map.get(this);
        String serializedValue = serializeInternal(context);
        m_hasCachedCSSText = true;
        map.add(this, serializedValue);
        return serializedValue;
    }
}

bool CSSPrimitiveValue::equals(const CSSPrimitiveValue& other) const
{
    if (primitiveUnitType() != other.primitiveUnitType())
        return false;

    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        return false;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return m_value.number == other.m_value.number;
    case CSSUnitType::CSS_CALC:
        return protect(cssCalcValue())->equals(*protect(other.cssCalcValue()));
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        // FIXME: seems like these should be handled.
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

bool CSSPrimitiveValue::addDerivedHash(Hasher& hasher) const
{
    add(hasher, primitiveUnitType());

    switch (primitiveUnitType()) {
    case CSSUnitType::CSS_UNKNOWN:
        break;
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        add(hasher, m_value.number);
        break;
    case CSSUnitType::CSS_CALC:
        add(hasher, m_value.calc);
        break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

// https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
void CSSPrimitiveValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    if (RefPtr calcValue = cssCalcValue()) {
        calcValue->collectComputedStyleDependencies(dependencies);
        return;
    }

    if (auto lengthUnit = CSS::toLengthUnit(primitiveUnitType()))
        CSS::collectComputedStyleDependencies(dependencies, *lengthUnit);
}

Ref<DeprecatedCSSOMValue> CSSPrimitiveValue::customCreateDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return switchOn([&](const auto& value) { return DeprecatedCSSOMPrimitiveValue::create(value, owner); });
}

} // namespace WebCore
