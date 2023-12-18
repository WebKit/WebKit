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
#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include <wtf/text/StringBuilder.h>

#include <utility>

namespace WebCore {

static CSSCounterStyleDescriptors::Ranges rangeFromStyleProperties(const StyleProperties& properties)
{
    auto ranges = properties.getPropertyCSSValue(CSSPropertyRange);
    if (!ranges)
        return { };
    return rangeFromCSSValue(ranges.releaseNonNull());
}

CSSCounterStyleDescriptors::Ranges rangeFromCSSValue(Ref<CSSValue> value)
{
    auto* list = dynamicDowncast<CSSValueList>(value.get());
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


static CSSCounterStyleDescriptors::Symbol symbolFromCSSValue(const CSSValue* value)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return { };

    return { primitiveValue->isCustomIdent(), primitiveValue->stringValue() };
}

CSSCounterStyleDescriptors::Symbol symbolFromCSSValue(RefPtr<CSSValue> value)
{
    return symbolFromCSSValue(value.get());
}

static CSSCounterStyleDescriptors::Name nameFromCSSValue(Ref<CSSValue> value)
{
    RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(WTFMove(value));
    if (!primitiveValue)
        return { };

    return makeAtomString(primitiveValue->stringValue());
}

static CSSCounterStyleDescriptors::AdditiveSymbols additiveSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto value = properties.getPropertyCSSValue(CSSPropertyAdditiveSymbols);
    if (!value)
        return { };
    return additiveSymbolsFromCSSValue(value.releaseNonNull());
}

CSSCounterStyleDescriptors::AdditiveSymbols additiveSymbolsFromCSSValue(Ref<CSSValue> value)
{
    CSSCounterStyleDescriptors::AdditiveSymbols result;
    for (auto& additiveSymbol : downcast<CSSValueList>(value.get())) {
        auto& pair = downcast<CSSValuePair>(additiveSymbol);
        auto weight = downcast<CSSPrimitiveValue>(pair.first()).value<unsigned>();
        auto symbol = symbolFromCSSValue(&pair.second());
        result.constructAndAppend(symbol, weight);
    }
    return result;
}

static CSSCounterStyleDescriptors::Pad padFromStyleProperties(const StyleProperties& properties)
{
    auto value = properties.getPropertyCSSValue(CSSPropertyPad);
    if (!value)
        return { };
    return padFromCSSValue(value.releaseNonNull());
}

CSSCounterStyleDescriptors::Pad padFromCSSValue(Ref<CSSValue> value)
{
    auto list = downcast<CSSValueList>(WTFMove(value));
    ASSERT(list->size() == 2);
    auto length = downcast<CSSPrimitiveValue>(list.get()[0]).intValue();
    ASSERT(length >= 0);
    return { static_cast<unsigned>(std::max(0, length)), symbolFromCSSValue(&list.get()[1]) };
}

static CSSCounterStyleDescriptors::NegativeSymbols negativeSymbolsFromStyleProperties(const StyleProperties& properties)
{
    auto negative = properties.getPropertyCSSValue(CSSPropertyNegative);
    if (!negative)
        return { };
    return negativeSymbolsFromCSSValue(negative.releaseNonNull());
}

CSSCounterStyleDescriptors::NegativeSymbols negativeSymbolsFromCSSValue(Ref<CSSValue> value)
{
    CSSCounterStyleDescriptors::NegativeSymbols result;
    if (auto list = dynamicDowncast<CSSValueList>(value.get())) {
        ASSERT(list->size() == 2);
        result.m_prefix = symbolFromCSSValue(list->item(0));
        result.m_suffix = symbolFromCSSValue(list->item(1));
    } else
        result.m_prefix = symbolFromCSSValue(value.ptr());
    return result;
}

static Vector<CSSCounterStyleDescriptors::Symbol> symbolsFromStyleProperties(const StyleProperties& properties)
{
    auto symbolsValues = properties.getPropertyCSSValue(CSSPropertySymbols);
    if (!symbolsValues)
        return { };
    return symbolsFromCSSValue(symbolsValues.releaseNonNull());
}

Vector<CSSCounterStyleDescriptors::Symbol> symbolsFromCSSValue(Ref<CSSValue> value)
{
    Vector<CSSCounterStyleDescriptors::Symbol> result;
    for (auto& symbolValue : downcast<CSSValueList>(value.get())) {
        auto symbol = symbolFromCSSValue(&symbolValue);
        if (!symbol.text.isNull())
            result.append(symbol);
    }
    return result;
}

static CSSCounterStyleDescriptors::Name fallbackNameFromStyleProperties(const StyleProperties& properties)
{
    auto fallback = properties.getPropertyCSSValue(CSSPropertyFallback);
    if (!fallback)
        return "decimal"_s;
    return fallbackNameFromCSSValue(fallback.releaseNonNull());
}

CSSCounterStyleDescriptors::Name fallbackNameFromCSSValue(Ref<CSSValue> value)
{
    return makeAtomString(nameFromCSSValue(WTFMove(value)));
}

static CSSCounterStyleDescriptors::Symbol prefixFromStyleProperties(const StyleProperties& properties)
{
    auto prefix = properties.getPropertyCSSValue(CSSPropertyPrefix);
    if (!prefix)
        return { };
    return symbolFromCSSValue(WTFMove(prefix));
}

static CSSCounterStyleDescriptors::Symbol suffixFromStyleProperties(const StyleProperties& properties)
{
    auto suffix = properties.getPropertyCSSValue(CSSPropertySuffix);
    // https://www.w3.org/TR/css-counter-styles-3/#counter-style-suffix
    // ("." full stop followed by a space)
    if (!suffix)
        return { false, ". "_s };
    return symbolFromCSSValue(WTFMove(suffix));
}

static CSSCounterStyleDescriptors::SystemData extractSystemDataFromStyleProperties(const StyleProperties& properties, CSSCounterStyleDescriptors::System system)
{
    auto systemValue = properties.getPropertyCSSValue(CSSPropertySystem);
    // If no value is provided after `fixed`, the first synbol value is implicitly 1 (https://www.w3.org/TR/css-counter-styles-3/#first-symbol-value).
    if (!systemValue)
        return { "decimal"_s, 1 };

    return extractSystemDataFromCSSValue(WTFMove(systemValue), system);
}

CSSCounterStyleDescriptors::SystemData extractSystemDataFromCSSValue(RefPtr<CSSValue> systemValue, CSSCounterStyleDescriptors::System system)
{
    std::pair<CSSCounterStyleDescriptors::Name, int> result { "decimal"_s, 1 };
    if (!systemValue)
        return result;
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
    if (getPropertyCSSValue(CSSPropertySymbols))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::Symbols);
    if (getPropertyCSSValue(CSSPropertySpeakAs))
        m_explicitlySetDescriptors.add(ExplicitlySetDescriptors::SpeakAs);
}

CSSCounterStyleDescriptors CSSCounterStyleDescriptors::create(AtomString name, const StyleProperties& properties)
{
    auto systemValue = properties.getPropertyCSSValue(CSSPropertySystem);
    auto system = toCounterStyleSystemEnum(systemValue.get());
    auto systemData = extractSystemDataFromStyleProperties(properties, system);
    CSSCounterStyleDescriptors descriptors {
        .m_name = name,
        .m_system = system,
        .m_negativeSymbols = negativeSymbolsFromStyleProperties(properties),
        .m_prefix = prefixFromStyleProperties(properties),
        .m_suffix = suffixFromStyleProperties(properties),
        .m_ranges = rangeFromStyleProperties(properties),
        .m_pad = padFromStyleProperties(properties),
        .m_fallbackName = fallbackNameFromStyleProperties(properties),
        .m_symbols = symbolsFromStyleProperties(properties),
        .m_additiveSymbols = additiveSymbolsFromStyleProperties(properties),
        .m_speakAs = SpeakAs::Auto,
        .m_extendsName = systemData.first,
        .m_fixedSystemFirstSymbolValue = systemData.second,
        .m_explicitlySetDescriptors = { }
    };
    descriptors.setExplicitlySetDescriptors(properties);
    return descriptors;
}

bool CSSCounterStyleDescriptors::areSymbolsValidForSystem(CSSCounterStyleDescriptors::System system, const Vector<CSSCounterStyleDescriptors::Symbol>& symbols, const CSSCounterStyleDescriptors::AdditiveSymbols& additiveSymbols)
{
    switch (system) {
    case System::Cyclic:
    case System::Fixed:
    case System::Symbolic:
        return symbols.size();
    case System::Alphabetic:
    case System::Numeric:
        return symbols.size() >= 2u;
    case System::Additive:
        return additiveSymbols.size();
    case System::SimplifiedChineseInformal:
    case System::SimplifiedChineseFormal:
    case System::TraditionalChineseInformal:
    case System::TraditionalChineseFormal:
    case System::EthiopicNumeric:
    case System::Extends:
        return !symbols.size() && !additiveSymbols.size();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool CSSCounterStyleDescriptors::isValid() const
{
    return areSymbolsValidForSystem(m_system, m_symbols, m_additiveSymbols);
}

void CSSCounterStyleDescriptors::setName(CSSCounterStyleDescriptors::Name name)
{
    if (name.isNull() || m_name == name)
        return;
    m_name = WTFMove(name);
}

void CSSCounterStyleDescriptors::setSystemData(CSSCounterStyleDescriptors::SystemData systemData)
{
    if (m_extendsName == systemData.first && m_fixedSystemFirstSymbolValue == systemData.second)
        return;
    m_extendsName = systemData.first;
    m_fixedSystemFirstSymbolValue = systemData.second;
}

void CSSCounterStyleDescriptors::setNegative(CSSCounterStyleDescriptors::NegativeSymbols negative)
{
    if (m_negativeSymbols == negative)
        return;
    m_negativeSymbols = WTFMove(negative);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Negative, true);
}

void CSSCounterStyleDescriptors::setPrefix(CSSCounterStyleDescriptors::Symbol prefix)
{
    if (m_prefix == prefix)
        return;
    m_prefix = WTFMove(prefix);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Prefix, true);
}

void CSSCounterStyleDescriptors::setSuffix(CSSCounterStyleDescriptors::Symbol suffix)
{
    if (m_suffix == suffix)
        return;
    m_suffix = WTFMove(suffix);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Suffix, true);
}

void CSSCounterStyleDescriptors::setRanges(CSSCounterStyleDescriptors::Ranges ranges)
{
    if (m_ranges == ranges)
        return;
    m_ranges = WTFMove(ranges);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Range, true);
}

void CSSCounterStyleDescriptors::setPad(CSSCounterStyleDescriptors::Pad pad)
{
    if (m_pad == pad)
        return;
    m_pad = WTFMove(pad);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Pad, true);
}

void CSSCounterStyleDescriptors::setFallbackName(CSSCounterStyleDescriptors::Name name)
{
    if (m_fallbackName == name)
        return;
    m_fallbackName = WTFMove(name);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Fallback, true);
}

void CSSCounterStyleDescriptors::setSymbols(Vector<CSSCounterStyleDescriptors::Symbol> symbols)
{
    if (m_symbols == symbols || !areSymbolsValidForSystem(m_system, symbols, m_additiveSymbols))
        return;
    m_symbols = WTFMove(symbols);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::Symbols, true);
}

void CSSCounterStyleDescriptors::setAdditiveSymbols(CSSCounterStyleDescriptors::AdditiveSymbols additiveSymbols)
{
    if (m_additiveSymbols == additiveSymbols || !areSymbolsValidForSystem(m_system, m_symbols, additiveSymbols))
        return;
    m_additiveSymbols = WTFMove(additiveSymbols);
    m_explicitlySetDescriptors.set(ExplicitlySetDescriptors::AdditiveSymbols, true);
}

String CSSCounterStyleDescriptors::Symbol::cssText() const
{
    StringBuilder builder;
    if (isCustomIdent)
        serializeIdentifier(text, builder);
    else
        serializeString(text, builder);
    return builder.toString();
}

String CSSCounterStyleDescriptors::nameCSSText() const
{
    return m_name;
}

String CSSCounterStyleDescriptors::systemCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::System))
        return emptyString();
    if (m_isExtendedResolved)
        return makeString("extends "_s, m_extendsName);

    switch (m_system) {
    case System::Cyclic:
        return "cyclic"_s;
    case System::Numeric:
        return "numeric"_s;
    case System::Alphabetic:
        return "alphabetic"_s;
    case System::Symbolic:
        return "symbolic"_s;
    case System::Additive:
        return "additive"_s;
    case System::Fixed:
        return makeString("fixed "_s, m_fixedSystemFirstSymbolValue);
    case System::Extends:
        return makeString("extends "_s, m_extendsName);
    // Internal values should not be exposed.
    case System::SimplifiedChineseInformal:
    case System::SimplifiedChineseFormal:
    case System::TraditionalChineseInformal:
    case System::TraditionalChineseFormal:
    case System::EthiopicNumeric:
        return emptyString();
    }
    return emptyString();
}

String CSSCounterStyleDescriptors::negativeCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Negative))
        return emptyString();
    return makeString(m_negativeSymbols.m_prefix.cssText(), " "_s, m_negativeSymbols.m_suffix.cssText());
}

String CSSCounterStyleDescriptors::prefixCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Prefix))
        return emptyString();
    return m_prefix.cssText();
}

String CSSCounterStyleDescriptors::suffixCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Suffix))
        return emptyString();
    return m_suffix.cssText();
}

String CSSCounterStyleDescriptors::rangesCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Range))
        return emptyString();
    if (m_ranges.isEmpty())
        return "auto"_s;

    StringBuilder builder;
    for (size_t i = 0; i < m_ranges.size(); ++i) {
        if (i)
            builder.append(", ");
        auto& range = m_ranges[i];
        if (range.first == std::numeric_limits<int>::min())
            builder.append("infinite");
        else
            builder.append(range.first);
        builder.append(" ");
        if (range.second== std::numeric_limits<int>::max())
            builder.append("infinite");
        else
            builder.append(range.second);
    }
    return builder.toString();
}

String CSSCounterStyleDescriptors::Pad::cssText() const
{
    return makeString(m_padMinimumLength, " ", m_padSymbol.cssText());
}

String CSSCounterStyleDescriptors::padCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Pad))
        return emptyString();
    return m_pad.cssText();
}

String CSSCounterStyleDescriptors::fallbackCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Fallback))
        return emptyString();
    return makeString(m_fallbackName);
}

String CSSCounterStyleDescriptors::symbolsCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::Symbols))
        return emptyString();
    StringBuilder builder;
    for (size_t i = 0; i < m_symbols.size(); ++i) {
        if (i)
            builder.append(" ");
        builder.append(m_symbols[i].cssText());
    }
    return builder.toString();
}

String CSSCounterStyleDescriptors::additiveSymbolsCSSText() const
{
    if (!m_explicitlySetDescriptors.contains(ExplicitlySetDescriptors::AdditiveSymbols))
        return emptyString();
    StringBuilder builder;
    for (size_t i = 0; i < m_additiveSymbols.size(); ++i) {
        if (i)
            builder.append(", ");
        builder.append(m_additiveSymbols[i].second);
        builder.append(" ");
        builder.append(m_additiveSymbols[i].first.cssText());
    }
    return builder.toString();
}
} // namespace WebCore
