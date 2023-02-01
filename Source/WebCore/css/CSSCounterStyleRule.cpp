/*
 * Copyright (C) 2021 Tyler Wilcock <twilco.o@protonmail.com>.
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
#include "CSSCounterStyleRule.h"

#include "CSSCounterStyleDescriptors.h"
#include "CSSPropertyParser.h"
#include "CSSStyleSheet.h"
#include "CSSTokenizer.h"
#include "CSSValuePair.h"
#include "MutableStyleProperties.h"
#include "Pair.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

StyleRuleCounterStyle::StyleRuleCounterStyle(const AtomString& name, Ref<StyleProperties>&& properties, CSSCounterStyleDescriptors&& descriptors)
    : StyleRuleBase(StyleRuleType::CounterStyle)
    , m_name(name)
    , m_properties(WTFMove(properties))
    , m_descriptors(WTFMove(descriptors))
{
}

Ref<StyleRuleCounterStyle> StyleRuleCounterStyle::create(const AtomString& name, Ref<StyleProperties>&& properties)
{
    auto descriptors = CSSCounterStyleDescriptors::create(name, properties);
    return adoptRef(*new StyleRuleCounterStyle(name, WTFMove(properties), WTFMove(descriptors)));
}

CSSCounterStyleDescriptors::System toCounterStyleSystemEnum(RefPtr<CSSValue> system)
{
    if (!system || !system->isPrimitiveValue())
        return CSSCounterStyleDescriptors::System::Symbolic;

    auto& primitiveSystemValue = downcast<CSSPrimitiveValue>(*system);
    ASSERT(primitiveSystemValue.isValueID() || primitiveSystemValue.isPair());
    CSSValueID systemKeyword = CSSValueInvalid;
    if (primitiveSystemValue.isValueID())
        systemKeyword = primitiveSystemValue.valueID();
    else if (auto* pair = primitiveSystemValue.pairValue()) {
        // This value must be `fixed` or `extends`, both of which can or must have an additional component.
        auto firstValue = pair->first();
        ASSERT(firstValue && firstValue->isValueID());
        if (firstValue)
            systemKeyword = firstValue->valueID();
    }
    switch (systemKeyword) {
    case CSSValueCyclic:
        return CSSCounterStyleDescriptors::System::Cyclic;
    case CSSValueFixed:
        return CSSCounterStyleDescriptors::System::Fixed;
    case CSSValueSymbolic:
        return CSSCounterStyleDescriptors::System::Symbolic;
    case CSSValueAlphabetic:
        return CSSCounterStyleDescriptors::System::Alphabetic;
    case CSSValueNumeric:
        return CSSCounterStyleDescriptors::System::Numeric;
    case CSSValueAdditive:
        return CSSCounterStyleDescriptors::System::Additive;
    case CSSValueExtends:
        return CSSCounterStyleDescriptors::System::Extends;
    default:
        ASSERT_NOT_REACHED();
        return CSSCounterStyleDescriptors::System::Symbolic;
    }
}

static bool symbolsValidForSystem(CSSCounterStyleDescriptors::System system, RefPtr<CSSValue> symbols, RefPtr<CSSValue> additiveSymbols)
{
    switch (system) {
    case CSSCounterStyleDescriptors::System::Cyclic:
    case CSSCounterStyleDescriptors::System::Fixed:
    case CSSCounterStyleDescriptors::System::Symbolic:
        return symbols && symbols->isValueList() && downcast<CSSValueList>(*symbols).length();
    case CSSCounterStyleDescriptors::System::Alphabetic:
    case CSSCounterStyleDescriptors::System::Numeric:
        return symbols && symbols->isValueList() && downcast<CSSValueList>(*symbols).length() >= 2u;
    case CSSCounterStyleDescriptors::System::Additive:
        return additiveSymbols && additiveSymbols->isValueList() && downcast<CSSValueList>(*additiveSymbols).length();
    case CSSCounterStyleDescriptors::System::Extends:
        return !symbols && !additiveSymbols;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool StyleRuleCounterStyle::newValueInvalidOrEqual(CSSPropertyID propertyID, const RefPtr<CSSValue> newValue) const
{
    auto currentValue = m_properties->getPropertyCSSValue(propertyID);
    if (compareCSSValuePtr(currentValue, newValue))
        return true;

    RefPtr<CSSValue> symbols;
    RefPtr<CSSValue> additiveSymbols;
    switch (propertyID) {
    case CSSPropertySystem:
        // If the attribute being set is `system`, and the new value would change the algorithm used, do nothing
        // and abort these steps.
        // (It's okay to change an aspect of the algorithm, like the first symbol value of a `fixed` system.)
        return toCounterStyleSystemEnum(currentValue) != toCounterStyleSystemEnum(newValue);
    case CSSPropertySymbols:
        symbols = newValue;
        additiveSymbols = m_properties->getPropertyCSSValue(CSSPropertyAdditiveSymbols);
        break;
    case CSSPropertyAdditiveSymbols:
        symbols = m_properties->getPropertyCSSValue(CSSPropertySymbols);
        additiveSymbols = newValue;
        break;
    default:
        return false;
    }
    auto system = m_properties->getPropertyCSSValue(CSSPropertySystem);
    return symbolsValidForSystem(toCounterStyleSystemEnum(system), symbols, additiveSymbols);
}

StyleRuleCounterStyle::~StyleRuleCounterStyle() = default;

MutableStyleProperties& StyleRuleCounterStyle::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = m_properties->mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

Ref<CSSCounterStyleRule> CSSCounterStyleRule::create(StyleRuleCounterStyle& rule, CSSStyleSheet* sheet)
{
    return adoptRef(*new CSSCounterStyleRule(rule, sheet));
}

CSSCounterStyleRule::CSSCounterStyleRule(StyleRuleCounterStyle& counterStyleRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_counterStyleRule(counterStyleRule)
{
}

CSSCounterStyleRule::~CSSCounterStyleRule() = default;

String CSSCounterStyleRule::cssText() const
{
    String systemText = system();
    const char* systemPrefix = systemText.isEmpty() ? "" : " system: ";
    const char* systemSuffix = systemText.isEmpty() ? "" : ";";

    String symbolsText = symbols();
    const char* symbolsPrefix = symbolsText.isEmpty() ? "" : " symbols: ";
    const char* symbolsSuffix = symbolsText.isEmpty() ? "" : ";";

    String additiveSymbolsText = additiveSymbols();
    const char* additiveSymbolsPrefix = additiveSymbolsText.isEmpty() ? "" : " additive-symbols: ";
    const char* additiveSymbolsSuffix = additiveSymbolsText.isEmpty() ? "" : ";";

    String negativeText = negative();
    const char* negativePrefix = negativeText.isEmpty() ? "" : " negative: ";
    const char* negativeSuffix = negativeText.isEmpty() ? "" : ";";

    String prefixText = prefix();
    const char* prefixTextPrefix = prefixText.isEmpty() ? "" : " prefix: ";
    const char* prefixTextSuffix = prefixText.isEmpty() ? "" : ";";

    String suffixText = suffix();
    const char* suffixTextPrefix = suffixText.isEmpty() ? "" : " suffix: ";
    const char* suffixTextSuffix = suffixText.isEmpty() ? "" : ";";

    String padText = pad();
    const char* padPrefix = padText.isEmpty() ? "" : " pad: ";
    const char* padSuffix = padText.isEmpty() ? "" : ";";

    String rangeText = range();
    const char* rangePrefix = rangeText.isEmpty() ? "" : " range: ";
    const char* rangeSuffix = rangeText.isEmpty() ? "" : ";";

    String fallbackText = fallback();
    const char* fallbackPrefix = fallbackText.isEmpty() ? "" : " fallback: ";
    const char* fallbackSuffix = fallbackText.isEmpty() ? "" : ";";

    String speakAsText = speakAs();
    const char* speakAsPrefix = speakAsText.isEmpty() ? "" : " speak-as: ";
    const char* speakAsSuffix = speakAsText.isEmpty() ? "" : ";";

    return makeString("@counter-style ", name(), " {",
        systemPrefix, systemText, systemSuffix,
        symbolsPrefix, symbolsText, symbolsSuffix,
        additiveSymbolsPrefix, additiveSymbolsText, additiveSymbolsSuffix,
        negativePrefix, negativeText, negativeSuffix,
        prefixTextPrefix, prefixText, prefixTextSuffix,
        suffixTextPrefix, suffixText, suffixTextSuffix,
        padPrefix, padText, padSuffix,
        rangePrefix, rangeText, rangeSuffix,
        fallbackPrefix, fallbackText, fallbackSuffix,
        speakAsPrefix, speakAsText, speakAsSuffix,
    " }");
}

void CSSCounterStyleRule::reattach(StyleRuleBase& rule)
{
    m_counterStyleRule = downcast<StyleRuleCounterStyle>(rule);
}

// https://drafts.csswg.org/css-counter-styles-3/#dom-csscounterstylerule-name
void CSSCounterStyleRule::setName(const String& text)
{
    auto tokenizer = CSSTokenizer(text);
    auto tokenRange = tokenizer.tokenRange();
    auto name = CSSPropertyParserHelpers::consumeCounterStyleNameInPrelude(tokenRange);
    if (name.isNull() || name == m_counterStyleRule->name())
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);
    m_counterStyleRule->setName(name);
}

void CSSCounterStyleRule::setterInternal(CSSPropertyID propertyID, const String& valueText)
{
    auto tokenizer = CSSTokenizer(valueText);
    auto tokenRange = tokenizer.tokenRange();
    auto newValue = CSSPropertyParser::parseCounterStyleDescriptor(propertyID, tokenRange, parserContext());
    if (m_counterStyleRule->newValueInvalidOrEqual(propertyID, newValue))
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);
    m_counterStyleRule->mutableProperties().setProperty(propertyID, WTFMove(newValue));
}

void CSSCounterStyleRule::setSystem(const String& text)
{
    setterInternal(CSSPropertySystem, text);
}

void CSSCounterStyleRule::setNegative(const String& text)
{
    setterInternal(CSSPropertyNegative, text);
}

void CSSCounterStyleRule::setPrefix(const String& text)
{
    setterInternal(CSSPropertyPrefix, text);
}

void CSSCounterStyleRule::setSuffix(const String& text)
{
    setterInternal(CSSPropertySuffix, text);
}

void CSSCounterStyleRule::setRange(const String& text)
{
    setterInternal(CSSPropertyRange, text);
}

void CSSCounterStyleRule::setPad(const String& text)
{
    setterInternal(CSSPropertyPad, text);
}

void CSSCounterStyleRule::setFallback(const String& text)
{
    setterInternal(CSSPropertyFallback, text);
}

void CSSCounterStyleRule::setSymbols(const String& text)
{
    setterInternal(CSSPropertySymbols, text);
}

void CSSCounterStyleRule::setAdditiveSymbols(const String& text)
{
    setterInternal(CSSPropertyAdditiveSymbols, text);
}

void CSSCounterStyleRule::setSpeakAs(const String& text)
{
    setterInternal(CSSPropertySpeakAs, text);
}

} // namespace WebCore
