/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCounterStyleDescriptors.h"

#include "CSSCounterStyleRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

#include <utility>

namespace WebCore {

static CSSCounterStyleDescriptors::Ranges translateRangeFromStyleProperties(const StyleProperties& properties)
{
    auto ranges = properties.getPropertyCSSValue(CSSPropertyRange);
    if (!ranges)
        return { };
    auto* list = dynamicDowncast<CSSValueList>(ranges.get());
    if (!list)
        return { };
    CSSCounterStyleDescriptors::Ranges result;
    for (auto& rangeValue : *list) {
        if (!rangeValue.isPair())
            return { };
        auto& low = downcast<CSSPrimitiveValue>(rangeValue.first());
        auto& high = downcast<CSSPrimitiveValue>(rangeValue.second());
        int convertedLow { std::numeric_limits<int>::min() };
        int convertedHigh { std::numeric_limits<int>::max() };
        if (low.isInteger())
            convertedLow = low.intValue();
        if (high.isInteger())
            convertedHigh = high.intValue();
        result.append({ convertedLow, convertedHigh });
    }
    return result;
}

static String symbolToString(const CSSValue* value)
{
    if (!value || !value->isPrimitiveValue())
        return { };

    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    return primitiveValue.stringValue();
}

static CSSCounterStyleDescriptors::AdditiveSymbols translateAdditiveSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto value = properties.getPropertyCSSValue(CSSPropertyAdditiveSymbols);
    if (!value)
        return { };

    CSSCounterStyleDescriptors::AdditiveSymbols result;
    for (auto& additiveSymbol : downcast<CSSValueList>(*value)) {
        auto& pair = downcast<CSSValuePair>(additiveSymbol);
        auto weight = downcast<CSSPrimitiveValue>(pair.first()).value<unsigned>();
        auto symbol = symbolToString(&pair.second());
        result.constructAndAppend(symbol, weight);
    }
    return result;
}

static CSSCounterStyleDescriptors::Pad translatePadFromStyleProperties(const StyleProperties& properties)
{
    auto value = properties.getPropertyCSSValue(CSSPropertyPad);
    if (!value)
        return { };

    auto& list = downcast<CSSValueList>(*value);
    ASSERT(list.size() == 2);
    auto length = downcast<CSSPrimitiveValue>(list[0]).intValue();
    ASSERT(length >= 0);
    return { static_cast<unsigned>(std::max(0, length)), symbolToString(&list[1]) };
}

static CSSCounterStyleDescriptors::NegativeSymbols translateNegativeSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto negative = properties.getPropertyCSSValue(CSSPropertyNegative);
    if (!negative)
        return { };

    CSSCounterStyleDescriptors::NegativeSymbols result;
    if (auto list = dynamicDowncast<CSSValueList>(*negative)) {
        ASSERT(list->size() == 2);
        result.m_prefix = symbolToString(list->item(0));
        result.m_suffix = symbolToString(list->item(1));
    } else
        result.m_prefix = symbolToString(negative.get());
    return result;
}

static Vector<CSSCounterStyleDescriptors::Symbol> translateSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto symbolsValues = properties.getPropertyCSSValue(CSSPropertySymbols);
    if (!symbolsValues)
        return { };

    Vector<CSSCounterStyleDescriptors::Symbol> result;
    for (auto& symbol : downcast<CSSValueList>(*symbolsValues)) {
        auto string = symbolToString(&symbol);
        if (!string.isNull())
            result.append(string);
    }
    return result;
}

static CSSCounterStyleDescriptors::Name translateFallbackNameFromStyleProperties(const StyleProperties& properties)
{
    auto fallback = properties.getPropertyCSSValue(CSSPropertyFallback);
    if (!fallback)
        return "decimal"_s;
    return makeAtomString(symbolToString(fallback.get()));
}

static CSSCounterStyleDescriptors::Symbol translatePrefixFromStyleProperties(const StyleProperties& properties)
{
    auto prefix = properties.getPropertyCSSValue(CSSPropertyPrefix);
    if (!prefix)
        return { };
    return symbolToString(prefix.get());
}

static CSSCounterStyleDescriptors::Symbol translateSuffixFromStyleProperties(const StyleProperties& properties)
{
    auto suffix = properties.getPropertyCSSValue(CSSPropertySuffix);
    // https://www.w3.org/TR/css-counter-styles-3/#counter-style-suffix
    // ("." full stop followed by a space)
    if (!suffix)
        return ". "_s;
    return symbolToString(suffix.get());
}

static std::pair<CSSCounterStyleDescriptors::Name, int> extractDataFromSystemDescriptor(const StyleProperties& properties, CSSCounterStyleDescriptors::System system)
{
    auto systemValue = properties.getPropertyCSSValue(CSSPropertySystem);
    // If no value is provided after `fixed`, the first synbol value is implicitly 1 (https://www.w3.org/TR/css-counter-styles-3/#first-symbol-value).
    if (!systemValue)
        return { "decimal"_s, 1 };

    std::pair<CSSCounterStyleDescriptors::Name, int> result { "decimal"_s, 1 };
    ASSERT(systemValue->isValueID() || systemValue->isPair());
    if (systemValue->isPair()) {
        // This value must be `fixed` or `extends`, both of which can or must have an additional component.
        auto& secondValue = systemValue->second();
        if (system == CSSCounterStyleDescriptors::System::Extends) {
            ASSERT(secondValue.isCustomIdent());
            result.first = AtomString { secondValue.isCustomIdent() ? secondValue.customIdent() : "decimal"_s };
        } else if (system == CSSCounterStyleDescriptors::System::Fixed) {
            ASSERT(secondValue.isInteger());
            result.second = secondValue.isInteger() ? secondValue.integer() : 1;
        }
    }
    return result;
}

void CSSCounterStyleDescriptors::setExplicitlySetDescriptors(const StyleProperties& properties)
{
    auto getPropertyCSSValue = [&](CSSPropertyID id) -> RefPtr<CSSValue> {
        return properties.getPropertyCSSValue(id);
    };

    if (getPropertyCSSValue(CSSPropertySystem))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::System);
    if (getPropertyCSSValue(CSSPropertyNegative))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Negative);
    if (getPropertyCSSValue(CSSPropertyPrefix))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Prefix);
    if (getPropertyCSSValue(CSSPropertySuffix))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Suffix);
    if (getPropertyCSSValue(CSSPropertyRange))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Range);
    if (getPropertyCSSValue(CSSPropertyPad))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Pad);
    if (getPropertyCSSValue(CSSPropertyFallback))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Fallback);
    if (getPropertyCSSValue(CSSPropertyAdditiveSymbols))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::AdditiveSymbols);
    if (getPropertyCSSValue(CSSPropertySpeakAs))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::SpeakAs);
}

CSSCounterStyleDescriptors CSSCounterStyleDescriptors::create(AtomString name, const StyleProperties& properties)
{
    auto systemValue = properties.getPropertyCSSValue(CSSPropertySystem);
    auto system = toCounterStyleSystemEnum(systemValue.get());
    auto systemData = extractDataFromSystemDescriptor(properties, system);
    CSSCounterStyleDescriptors descriptors {
        .m_name = name,
        .m_system = system,
        .m_negativeSymbols = translateNegativeSymbolsFromStyleProperties(properties),
        .m_prefix = translatePrefixFromStyleProperties(properties),
        .m_suffix = translateSuffixFromStyleProperties(properties),
        .m_ranges = translateRangeFromStyleProperties(properties),
        .m_pad = translatePadFromStyleProperties(properties),
        .m_fallbackName = translateFallbackNameFromStyleProperties(properties),
        .m_symbols = translateSymbolsFromStyleProperties(properties),
        .m_additiveSymbols = translateAdditiveSymbolsFromStyleProperties(properties),
        .m_speakAs = SpeakAs::Auto,
        .m_extendsName = systemData.first,
        .m_fixedSystemFirstSymbolValue = systemData.second,
        .m_explicitlySetDescriptors = { }
    };
    descriptors.setExplicitlySetDescriptors(properties);
    return descriptors;
}

bool CSSCounterStyleDescriptors::areSymbolsValidForSystem() const
{
    switch (m_system) {
    case System::Cyclic:
    case System::Fixed:
    case System::Symbolic:
        return m_symbols.size();
    case System::Alphabetic:
    case System::Numeric:
        return m_symbols.size() >= 2u;
    case System::Additive:
        return m_additiveSymbols.size();
    case System::Extends:
        return !m_symbols.size() && !m_additiveSymbols.size();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool CSSCounterStyleDescriptors::isValid() const
{
    return areSymbolsValidForSystem();
}

} // namespace WebCore
