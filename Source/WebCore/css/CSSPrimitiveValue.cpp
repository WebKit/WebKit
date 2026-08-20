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
    if (RefPtr calcValue = const_cast<CSSCalc::Value*>(cssCalcValue()))
        return CSS::UnevaluatedCalcBase { calcValue.releaseNonNull() }.primitiveType();
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
    setPrimitiveUnitType(CSSUnitType::Calc);
    m_value.calc = &value.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    auto type = primitiveUnitType();
    switch (type) {
    case CSSUnitType::Calc:
        m_value.calc->deref();
        break;
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        break;
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Px:
    case CSSUnitType::Cm:
    case CSSUnitType::Mm:
    case CSSUnitType::In:
    case CSSUnitType::Pt:
    case CSSUnitType::Pc:
    case CSSUnitType::Deg:
    case CSSUnitType::Rad:
    case CSSUnitType::Grad:
    case CSSUnitType::Ms:
    case CSSUnitType::S:
    case CSSUnitType::Hz:
    case CSSUnitType::Khz:
    case CSSUnitType::Turn:
    case CSSUnitType::Vw:
    case CSSUnitType::Vh:
    case CSSUnitType::Vmin:
    case CSSUnitType::Vmax:
    case CSSUnitType::Vb:
    case CSSUnitType::Vi:
    case CSSUnitType::Svw:
    case CSSUnitType::Svh:
    case CSSUnitType::Svmin:
    case CSSUnitType::Svmax:
    case CSSUnitType::Svb:
    case CSSUnitType::Svi:
    case CSSUnitType::Lvw:
    case CSSUnitType::Lvh:
    case CSSUnitType::Lvmin:
    case CSSUnitType::Lvmax:
    case CSSUnitType::Lvb:
    case CSSUnitType::Lvi:
    case CSSUnitType::Dvw:
    case CSSUnitType::Dvh:
    case CSSUnitType::Dvmin:
    case CSSUnitType::Dvmax:
    case CSSUnitType::Dvb:
    case CSSUnitType::Dvi:
    case CSSUnitType::Dppx:
    case CSSUnitType::X:
    case CSSUnitType::Dpi:
    case CSSUnitType::Dpcm:
    case CSSUnitType::Fr:
    case CSSUnitType::Q:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::Unknown:
    case CSSUnitType::Cqw:
    case CSSUnitType::Cqh:
    case CSSUnitType::Cqi:
    case CSSUnitType::Cqb:
    case CSSUnitType::Cqmin:
    case CSSUnitType::Cqmax:
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
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::Number));
}

Ref<CSSPrimitiveValue> CSSPrimitiveValue::create(double value, CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::Number:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_numberValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::Percentage:
        if (RefPtr result = valueFromPool(staticCSSValuePool->m_percentageValues, value))
            return result.releaseNonNull();
        break;
    case CSSUnitType::Px:
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
    return adoptRef(*new CSSPrimitiveValue(value, CSSUnitType::Integer));
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
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Cm:
    case CSSUnitType::Cqb:
    case CSSUnitType::Cqh:
    case CSSUnitType::Cqi:
    case CSSUnitType::Cqmax:
    case CSSUnitType::Cqmin:
    case CSSUnitType::Cqw:
    case CSSUnitType::Deg:
    case CSSUnitType::Dpcm:
    case CSSUnitType::Dpi:
    case CSSUnitType::Dppx:
    case CSSUnitType::Dvb:
    case CSSUnitType::Dvh:
    case CSSUnitType::Dvi:
    case CSSUnitType::Dvmax:
    case CSSUnitType::Dvmin:
    case CSSUnitType::Dvw:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Fr:
    case CSSUnitType::Grad:
    case CSSUnitType::Hz:
    case CSSUnitType::Ic:
    case CSSUnitType::In:
    case CSSUnitType::Khz:
    case CSSUnitType::Lh:
    case CSSUnitType::Lvb:
    case CSSUnitType::Lvh:
    case CSSUnitType::Lvi:
    case CSSUnitType::Lvmax:
    case CSSUnitType::Lvmin:
    case CSSUnitType::Lvw:
    case CSSUnitType::Mm:
    case CSSUnitType::Ms:
    case CSSUnitType::Number:
    case CSSUnitType::Pc:
    case CSSUnitType::Percentage:
    case CSSUnitType::Pt:
    case CSSUnitType::Px:
    case CSSUnitType::Q:
    case CSSUnitType::Rad:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Rlh:
    case CSSUnitType::S:
    case CSSUnitType::Svb:
    case CSSUnitType::Svh:
    case CSSUnitType::Svi:
    case CSSUnitType::Svmax:
    case CSSUnitType::Svmin:
    case CSSUnitType::Svw:
    case CSSUnitType::Turn:
    case CSSUnitType::Vb:
    case CSSUnitType::Vh:
    case CSSUnitType::Vi:
    case CSSUnitType::Vmax:
    case CSSUnitType::Vmin:
    case CSSUnitType::Vw:
    case CSSUnitType::X:
        return formatNumberValue(unitTypeString(type));
    case CSSUnitType::Calc:
        return CSS::UnevaluatedCalcBase { protect(const_cast<CSSCalc::Value&>(*cssCalcValue())) }.serializationForCSS(context);
    case CSSUnitType::Integer:
        return formatIntegerValue(""_s);
    case CSSUnitType::QuirkyEm:
        return formatNumberValue("em"_s);
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Unknown:
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSPrimitiveValue::customCSSText(const CSS::SerializationContext& context) const
{
    switch (primitiveUnitType()) {
    case CSSUnitType::Unknown:
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
    case CSSUnitType::Unknown:
        return false;
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Px:
    case CSSUnitType::Cm:
    case CSSUnitType::Dppx:
    case CSSUnitType::X:
    case CSSUnitType::Dpi:
    case CSSUnitType::Dpcm:
    case CSSUnitType::Mm:
    case CSSUnitType::In:
    case CSSUnitType::Pt:
    case CSSUnitType::Pc:
    case CSSUnitType::Deg:
    case CSSUnitType::Rad:
    case CSSUnitType::Grad:
    case CSSUnitType::Ms:
    case CSSUnitType::S:
    case CSSUnitType::Hz:
    case CSSUnitType::Khz:
    case CSSUnitType::Turn:
    case CSSUnitType::Vw:
    case CSSUnitType::Vh:
    case CSSUnitType::Vmin:
    case CSSUnitType::Vmax:
    case CSSUnitType::Vb:
    case CSSUnitType::Vi:
    case CSSUnitType::Svw:
    case CSSUnitType::Svh:
    case CSSUnitType::Svmin:
    case CSSUnitType::Svmax:
    case CSSUnitType::Svb:
    case CSSUnitType::Svi:
    case CSSUnitType::Lvw:
    case CSSUnitType::Lvh:
    case CSSUnitType::Lvmin:
    case CSSUnitType::Lvmax:
    case CSSUnitType::Lvb:
    case CSSUnitType::Lvi:
    case CSSUnitType::Dvw:
    case CSSUnitType::Dvh:
    case CSSUnitType::Dvmin:
    case CSSUnitType::Dvmax:
    case CSSUnitType::Dvb:
    case CSSUnitType::Dvi:
    case CSSUnitType::Fr:
    case CSSUnitType::Q:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::Cqw:
    case CSSUnitType::Cqh:
    case CSSUnitType::Cqi:
    case CSSUnitType::Cqb:
    case CSSUnitType::Cqmin:
    case CSSUnitType::Cqmax:
        return m_value.number == other.m_value.number;
    case CSSUnitType::Calc:
        return CSS::UnevaluatedCalcBase { protect(const_cast<CSSCalc::Value&>(*cssCalcValue())) } == CSS::UnevaluatedCalcBase { protect(const_cast<CSSCalc::Value&>(*other.cssCalcValue())) };
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
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
    case CSSUnitType::Unknown:
        break;
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Px:
    case CSSUnitType::Cm:
    case CSSUnitType::Dppx:
    case CSSUnitType::X:
    case CSSUnitType::Dpi:
    case CSSUnitType::Dpcm:
    case CSSUnitType::Mm:
    case CSSUnitType::In:
    case CSSUnitType::Pt:
    case CSSUnitType::Pc:
    case CSSUnitType::Deg:
    case CSSUnitType::Rad:
    case CSSUnitType::Grad:
    case CSSUnitType::Ms:
    case CSSUnitType::S:
    case CSSUnitType::Hz:
    case CSSUnitType::Khz:
    case CSSUnitType::Turn:
    case CSSUnitType::Vw:
    case CSSUnitType::Vh:
    case CSSUnitType::Vmin:
    case CSSUnitType::Vmax:
    case CSSUnitType::Vb:
    case CSSUnitType::Vi:
    case CSSUnitType::Svw:
    case CSSUnitType::Svh:
    case CSSUnitType::Svmin:
    case CSSUnitType::Svmax:
    case CSSUnitType::Svb:
    case CSSUnitType::Svi:
    case CSSUnitType::Lvw:
    case CSSUnitType::Lvh:
    case CSSUnitType::Lvmin:
    case CSSUnitType::Lvmax:
    case CSSUnitType::Lvb:
    case CSSUnitType::Lvi:
    case CSSUnitType::Dvw:
    case CSSUnitType::Dvh:
    case CSSUnitType::Dvmin:
    case CSSUnitType::Dvmax:
    case CSSUnitType::Dvb:
    case CSSUnitType::Dvi:
    case CSSUnitType::Fr:
    case CSSUnitType::Q:
    case CSSUnitType::Lh:
    case CSSUnitType::Rlh:
    case CSSUnitType::Cqw:
    case CSSUnitType::Cqh:
    case CSSUnitType::Cqi:
    case CSSUnitType::Cqb:
    case CSSUnitType::Cqmin:
    case CSSUnitType::Cqmax:
        add(hasher, m_value.number);
        break;
    case CSSUnitType::Calc:
        add(hasher, m_value.calc);
        break;
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

// https://drafts.css-houdini.org/css-properties-values-api/#dependency-cycles
void CSSPrimitiveValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    if (RefPtr calcValue = const_cast<CSSCalc::Value*>(cssCalcValue())) {
        CSS::UnevaluatedCalcBase { calcValue.releaseNonNull() }.collectComputedStyleDependencies(dependencies);
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
