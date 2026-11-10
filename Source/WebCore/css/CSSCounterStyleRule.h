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

#pragma once

#include "CSSCounterStyleDescriptors.h"
#include "CSSRule.h"
#include "StyleProperties.h"
#include "StyleRule.h"
#include <wtf/text/AtomString.h>

namespace WebCore {
class StyleRuleCounterStyle final : public StyleRuleBase {
public:
    static Ref<StyleRuleCounterStyle> create(CSSCounterStyleDescriptors&&);
    ~StyleRuleCounterStyle();

    Ref<StyleRuleCounterStyle> copy() const { return adoptRef(*new StyleRuleCounterStyle(*this)); }

    const CSSCounterStyleDescriptors& descriptors() const LIFETIME_BOUND { return m_descriptors; };
    CSSCounterStyleDescriptors& mutableDescriptors() LIFETIME_BOUND { return m_descriptors; };

    const RefPtr<AtomStringImpl>& name() const LIFETIME_BOUND { return m_descriptors.m_name; }
    RefPtr<StringImpl> system() const { return m_descriptors.systemCSSText(); }
    RefPtr<StringImpl> negative() const { return m_descriptors.negativeCSSText(); }
    RefPtr<StringImpl> prefix() const { return m_descriptors.prefixCSSText(); }
    RefPtr<StringImpl> suffix() const { return m_descriptors.suffixCSSText(); }
    RefPtr<StringImpl> range() const { return { m_descriptors.rangesCSSText() }; }
    RefPtr<StringImpl> pad() const { return m_descriptors.padCSSText(); }
    RefPtr<StringImpl> fallback() const { return m_descriptors.fallbackCSSText(); }
    RefPtr<StringImpl> symbols() const { return m_descriptors.symbolsCSSText(); }
    RefPtr<StringImpl> additiveSymbols() const { return m_descriptors.additiveSymbolsCSSText(); }
    RefPtr<StringImpl> speakAs() const { return { }; }
    bool newValueInvalidOrEqual(CSSPropertyID, const RefPtr<CSSValue> newValue) const;

private:
    explicit StyleRuleCounterStyle(CSSCounterStyleDescriptors&&);
    StyleRuleCounterStyle(const StyleRuleCounterStyle&) = default;

    CSSCounterStyleDescriptors m_descriptors;
};

class CSSCounterStyleRule final : public CSSRule {
public:
    NODELETE static Ref<CSSCounterStyleRule> create(StyleRuleCounterStyle&, CSSStyleSheet*);
    virtual ~CSSCounterStyleRule();

    RefPtr<StringImpl> cssText() const final;
    void reattach(StyleRuleBase&) final;
    StyleRuleType styleRuleType() const final { return StyleRuleType::CounterStyle; }

    RefPtr<StringImpl> name() const { return m_counterStyleRule->name(); }
    RefPtr<StringImpl> system() const { return protect(m_counterStyleRule)->system(); }
    RefPtr<StringImpl> negative() const { return protect(m_counterStyleRule)->negative(); }
    RefPtr<StringImpl> prefix() const { return protect(m_counterStyleRule)->prefix(); }
    RefPtr<StringImpl> suffix() const { return protect(m_counterStyleRule)->suffix(); }
    RefPtr<StringImpl> range() const { return protect(m_counterStyleRule)->range(); }
    RefPtr<StringImpl> pad() const { return protect(m_counterStyleRule)->pad(); }
    RefPtr<StringImpl> fallback() const { return protect(m_counterStyleRule)->fallback(); }
    RefPtr<StringImpl> symbols() const { return protect(m_counterStyleRule)->symbols(); }
    RefPtr<StringImpl> additiveSymbols() const { return protect(m_counterStyleRule)->additiveSymbols(); }
    RefPtr<StringImpl> speakAs() const { return m_counterStyleRule->speakAs(); }

    void setName(StringImpl*);
    void setSystem(StringImpl*);
    void setNegative(StringImpl*);
    void setPrefix(StringImpl*);
    void setSuffix(StringImpl*);
    void setRange(StringImpl*);
    void setPad(StringImpl*);
    void setFallback(StringImpl*);
    void setSymbols(StringImpl*);
    void setAdditiveSymbols(StringImpl*);
    NODELETE void setSpeakAs(StringImpl*);

private:
    CSSCounterStyleRule(StyleRuleCounterStyle&, CSSStyleSheet* parent);

    bool setterInternal(CSSPropertyID, StringImpl*);
    RefPtr<CSSValue> cssValueFromText(CSSPropertyID, StringImpl*);
    const CSSCounterStyleDescriptors& descriptors() const LIFETIME_BOUND { return m_counterStyleRule->descriptors(); }
    CSSCounterStyleDescriptors& mutableDescriptors() LIFETIME_BOUND { return m_counterStyleRule->mutableDescriptors(); }

    Ref<StyleRuleCounterStyle> m_counterStyleRule;
};

CSSCounterStyleDescriptors::System toCounterStyleSystemEnum(const CSSValue*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSCounterStyleRule, StyleRuleType::CounterStyle)

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleCounterStyle)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isCounterStyleRule(); }
SPECIALIZE_TYPE_TRAITS_END()
