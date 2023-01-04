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

#include "StyleProperties.h"

#include <utility>

namespace WebCore {

static CSSCounterStyleDescriptors::Ranges translateRangeFromStyleProperties(const StyleProperties& properties)
{
    auto ranges = properties.getPropertyCSSValue(CSSPropertySystem).get();
    // auto range will return an empty Ranges
    if (!ranges || !is<CSSValueList>(ranges))
        return { };
    auto& list = downcast<CSSValueList>(*ranges);
    CSSCounterStyleDescriptors::Ranges result;
    for (auto& rangeValue : list) {
        ASSERT(rangeValue->isPrimitiveValue());
        if (!rangeValue->isPrimitiveValue())
            return { };
        auto& bounds = downcast<CSSPrimitiveValue>(rangeValue.get());
        ASSERT(bounds.isPair());
        if (!bounds.isPair())
            return { };
        auto boundsPair = bounds.pairValue();
        if (!boundsPair)
            return { };
        auto low = boundsPair->first();
        auto high = boundsPair->second();
        if (!low || !high)
            return { };
        int convertedLow { std::numeric_limits<int>::min() };
        int convertedHigh { std::numeric_limits<int>::max() };
        if (low->isInteger())
            convertedLow = low->intValue();
        if (high->isInteger())
            convertedHigh = high->intValue();
        std::pair<int, int> newRange { convertedLow, convertedHigh };
        result.append(newRange);
    }
    return result;
}

static String symbolToString(const CSSValue* value)
{
    if (!value || !value->isPrimitiveValue())
        return String();

    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    return primitiveValue.stringValue();
}

static CSSCounterStyleDescriptors::Pad translatePadFromStyleProperties(const StyleProperties& properties)
{
    auto pad = properties.getPropertyCSSValue(CSSPropertyPad).get();
    if (!pad || !is<CSSValueList>(pad))
        return CSSCounterStyleDescriptors::Pad();

    auto& list = downcast<CSSValueList>(*pad);
    ASSERT(list.size() == 2);
    auto length = downcast<CSSPrimitiveValue>(list.item(0));
    if (!length)
        return CSSCounterStyleDescriptors::Pad();
    auto lengthValue = length->intValue();
    ASSERT(lengthValue >= 0);
    return { static_cast<unsigned>(std::max(0, lengthValue)), symbolToString(list.item(1)) };
}

static CSSCounterStyleDescriptors::NegativeSymbols translateNegativeSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto negative = properties.getPropertyCSSValue(CSSPropertyNegative).get();
    if (!negative || !is<CSSValueList>(negative))
        return CSSCounterStyleDescriptors::NegativeSymbols();

    auto& list = downcast<CSSValueList>(*negative);
    ASSERT(list.size() == 1 || list.size() == 2);
    CSSCounterStyleDescriptors::NegativeSymbols result;
    if (list.size() >= 1)
        result.m_prefix = symbolToString(list.item(0));
    else if (list.size() == 2)
        result.m_suffix = symbolToString(list.item(1));
    else {
        ASSERT_NOT_REACHED();
        return CSSCounterStyleDescriptors::NegativeSymbols();
    }
    return result;
}

static Vector<CSSCounterStyleDescriptors::Symbol> translateSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto symbolsValues = properties.getPropertyCSSValue(CSSPropertySymbols).get();
    if (!symbolsValues || !is<CSSValueList>(symbolsValues))
        return { };

    Vector<CSSCounterStyleDescriptors::Symbol> result;
    auto& list = downcast<CSSValueList>(*symbolsValues);
    for (auto& symbolValue : list) {
        auto symbolString = symbolToString(&symbolValue.get());
        if (!symbolString.isNull())
            result.append((symbolString));
    }
    return result;
}

static CSSCounterStyleDescriptors::Name translateFallbackNameFromStyleProperties(const StyleProperties& properties)
{
    auto fallback = properties.getPropertyCSSValue(CSSPropertyFallback).get();
    if (!fallback)
        return "decimal"_s;
    return makeAtomString(symbolToString(fallback));
}

static CSSCounterStyleDescriptors::Symbol translatePrefixFromStyleProperties(const StyleProperties& properties)
{
    auto prefix = properties.getPropertyCSSValue(CSSPropertyPrefix).get();
    if (!prefix)
        return ""_s;
    return symbolToString(prefix);
}

static CSSCounterStyleDescriptors::Symbol translateSuffixFromStyleProperties(const StyleProperties& properties)
{
    auto suffix = properties.getPropertyCSSValue(CSSPropertySuffix).get();
    // https://www.w3.org/TR/css-counter-styles-3/#counter-style-suffix
    // ("." full stop followed by a space)
    if (!suffix)
        return "\2E\20"_s;
    return symbolToString(suffix);
}

static std::pair<CSSCounterStyleDescriptors::Name, int> extractDataFromSystemDescriptor(const StyleProperties& properties, CSSCounterStyleDescriptors::System system)
{
    auto systemValue = properties.getPropertyCSSValue(CSSPropertySystem).get();
    // If no value is provided after `fixed`, the first synbol value is implicitly 1 (https://www.w3.org/TR/css-counter-styles-3/#first-symbol-value).
    if (!systemValue || !systemValue->isPrimitiveValue())
        return { "decimal"_s, 1 };

    std::pair<CSSCounterStyleDescriptors::Name, int> result;

    auto& primitiveSystemValue = downcast<CSSPrimitiveValue>(*systemValue);
    ASSERT(primitiveSystemValue.isValueID() || primitiveSystemValue.isPair());
    if (auto* pair = primitiveSystemValue.pairValue()) {
        // This value must be `fixed` or `extends`, both of which can or must have an additional component.
        auto secondValue = pair->second();
        if (system == CSSCounterStyleDescriptors::System::Extends) {
            ASSERT(secondValue && secondValue->isCustomIdent());
            result.first = secondValue && secondValue->isCustomIdent() ? makeAtomString(secondValue->stringValue()) : makeAtomString("decimal"_s);
        } else if (system == CSSCounterStyleDescriptors::System::Fixed) {
            ASSERT(secondValue && secondValue->isInteger());
            result.second = secondValue && secondValue->isInteger() ? secondValue->intValue() : 1;
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
    auto system = toCounterStyleSystemEnum(properties.getPropertyCSSValue(CSSPropertySystem).get());
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
        .m_additiveSymbols = { },
        .m_speakAs = SpeakAs::Auto,
        .m_extendsName = systemData.first,
        .m_fixedSystemFirstSymbolValue = systemData.second,
        .m_explicitlySetDescriptors = { }
    };
    descriptors.setExplicitlySetDescriptors(properties);
    return descriptors;
}

} // namespace WebCore
