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
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

StyleRuleCounterStyle::StyleRuleCounterStyle(const AtomString& name, CSSCounterStyleDescriptors&& descriptors)
    : StyleRuleBase(StyleRuleType::CounterStyle)
    , m_name(name)
    , m_descriptors(WTFMove(descriptors))
{
}

Ref<StyleRuleCounterStyle> StyleRuleCounterStyle::create(const AtomString& name, CSSCounterStyleDescriptors&& descriptors)
{
    return adoptRef(*new StyleRuleCounterStyle(name, WTFMove(descriptors)));
}

CSSCounterStyleDescriptors::System toCounterStyleSystemEnum(const CSSValue* system)
{
    if (!system)
        return CSSCounterStyleDescriptors::System::Symbolic;

    ASSERT(system->isValueID() || system->isPair());
    CSSValueID systemKeyword = CSSValueInvalid;
    if (system->isValueID())
        systemKeyword = system->valueID();
    else if (system->isPair()) {
        // This value must be `fixed` or `extends`, both of which can or must have an additional component.
        systemKeyword = system->first().valueID();
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
    case CSSValueInternalDisclosureClosed:
        return CSSCounterStyleDescriptors::System::DisclosureClosed;
    case CSSValueInternalDisclosureOpen:
        return CSSCounterStyleDescriptors::System::DisclosureOpen;
    case CSSValueInternalSimplifiedChineseInformal:
        return CSSCounterStyleDescriptors::System::SimplifiedChineseInformal;
    case CSSValueInternalSimplifiedChineseFormal:
        return CSSCounterStyleDescriptors::System::SimplifiedChineseFormal;
    case CSSValueInternalTraditionalChineseInformal:
        return CSSCounterStyleDescriptors::System::TraditionalChineseInformal;
    case CSSValueInternalTraditionalChineseFormal:
        return CSSCounterStyleDescriptors::System::TraditionalChineseFormal;
    case CSSValueInternalEthiopicNumeric:
        return CSSCounterStyleDescriptors::System::EthiopicNumeric;
    case CSSValueExtends:
        return CSSCounterStyleDescriptors::System::Extends;
    default:
        ASSERT_NOT_REACHED();
        return CSSCounterStyleDescriptors::System::Symbolic;
    }
}

StyleRuleCounterStyle::~StyleRuleCounterStyle() = default;

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

void CSSCounterStyleRule::cssText(StringBuilder& builder) const
{
    auto& descriptors = this->descriptors();

    builder.append("@counter-style "_s, name(), " {"_s);

    if (descriptors.hasSystemCSSText()) {
        builder.append(" system: "_s);
        descriptors.systemCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasSymbolsCSSText()) {
        builder.append(" symbols: "_s);
        descriptors.symbolsCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasAdditiveSymbolsCSSText()) {
        builder.append(" additive-symbols: "_s);
        descriptors.additiveSymbolsCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasNegativeCSSText()) {
        builder.append(" negative: "_s);
        descriptors.negativeCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasPrefixCSSText()) {
        builder.append(" prefix: "_s);
        descriptors.prefixCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasSuffixCSSText()) {
        builder.append(" suffix: "_s);
        descriptors.suffixCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasPadCSSText()) {
        builder.append(" pad: "_s);
        descriptors.padCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasRangesCSSText()) {
        builder.append(" range: "_s);
        descriptors.rangesCSSText(builder);
        builder.append(';');
    }

    if (descriptors.hasFallbackCSSText()) {
        builder.append(" fallback: "_s);
        descriptors.fallbackCSSText(builder);
        builder.append(';');
    }

    // FIXME: @counter-style speak-as not supported (rdar://103019111).

    builder.append(" }"_s);
}

void CSSCounterStyleRule::reattach(StyleRuleBase& rule)
{
    m_counterStyleRule = downcast<StyleRuleCounterStyle>(rule);
}

RefPtr<CSSValue> CSSCounterStyleRule::cssValueFromText(CSSPropertyID propertyID, const String& valueText)
{
    auto tokenizer = CSSTokenizer(valueText);
    auto tokenRange = tokenizer.tokenRange();
    return CSSPropertyParser::parseCounterStyleDescriptor(propertyID, tokenRange, parserContext());
}

// https://drafts.csswg.org/css-counter-styles-3/#dom-csscounterstylerule-name
void CSSCounterStyleRule::setName(const String& text)
{
    auto tokenizer = CSSTokenizer(text);
    auto tokenRange = tokenizer.tokenRange();
    auto name = CSSPropertyParserHelpers::consumeCounterStyleNameInPrelude(tokenRange);
    if (!name)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setName(WTFMove(name));
}

void CSSCounterStyleRule::setSystem(const String& text)
{
    auto systemValue = cssValueFromText(CSSPropertySystem, text);
    if (!systemValue)
        return;
    auto system = toCounterStyleSystemEnum(systemValue.get());
    // If the attribute being set is `system`, and the new value would change the algorithm used, do nothing
    // and abort these steps.
    // (It's okay to change an aspect of the algorithm, like the first symbol value of a `fixed` system.)
    // https://www.w3.org/TR/css-counter-styles-3/#the-csscounterstylerule-interface
    auto systemData = extractSystemDataFromCSSValue(WTFMove(systemValue), system);
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setSystemData(WTFMove(systemData));
}

void CSSCounterStyleRule::setNegative(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertyNegative, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setNegative(negativeSymbolsFromCSSValue(newValue.releaseNonNull()));
}

void CSSCounterStyleRule::setPrefix(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertyPrefix, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setPrefix(symbolFromCSSValue(WTFMove(newValue)));
}

void CSSCounterStyleRule::setSuffix(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertySuffix, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setSuffix(symbolFromCSSValue(WTFMove(newValue)));
}

void CSSCounterStyleRule::setRange(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertyRange, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setRanges(rangeFromCSSValue(newValue.releaseNonNull()));
}

void CSSCounterStyleRule::setPad(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertyPad, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setPad(padFromCSSValue(newValue.releaseNonNull()));
}

void CSSCounterStyleRule::setFallback(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertyFallback, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setFallbackName(fallbackNameFromCSSValue(newValue.releaseNonNull()));
}

void CSSCounterStyleRule::setSymbols(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertySymbols, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setSymbols(symbolsFromCSSValue(newValue.releaseNonNull()));
}

void CSSCounterStyleRule::setAdditiveSymbols(const String& text)
{
    auto newValue = cssValueFromText(CSSPropertyAdditiveSymbols, text);
    if (!newValue)
        return;
    CSSStyleSheet::RuleMutationScope mutationScope(this);
    mutableDescriptors().setAdditiveSymbols(additiveSymbolsFromCSSValue(newValue.releaseNonNull()));
}

void CSSCounterStyleRule::setSpeakAs(const String&)
{
    // FIXME: @counter-style speak-as not supported (rdar://103019111).
}

} // namespace WebCore
