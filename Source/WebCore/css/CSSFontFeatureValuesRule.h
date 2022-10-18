/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "CSSRule.h"

#include "StyleRule.h"

namespace WebCore {

class CSSFontFeatureValuesRule final : public CSSRule {
public:
    static Ref<CSSFontFeatureValuesRule> create(StyleRuleFontFeatureValues& rule, CSSStyleSheet* sheet) { return adoptRef(*new CSSFontFeatureValuesRule(rule, sheet)); }
    virtual ~CSSFontFeatureValuesRule() = default;

    const Vector<AtomString>& fontFamilies() const
    {
        return m_fontFeatureValuesRule->fontFamilies();
    }

    // Used by the CSSOM.
    AtomString fontFamily() const
    {
        StringBuilder builder;
        bool first = true;
        for (auto& family : m_fontFeatureValuesRule->fontFamilies()) {
            if (first)
                first = false;
            else
                builder.append(", ");
            
            builder.append(family);
        }
        return builder.toAtomString();
    }

private:
    CSSFontFeatureValuesRule(StyleRuleFontFeatureValues&, CSSStyleSheet* parent);

    StyleRuleType styleRuleType() const final { return StyleRuleType::FontFeatureValues; }
    String cssText() const final;
    void reattach(StyleRuleBase&) final;

    Ref<StyleRuleFontFeatureValues> m_fontFeatureValuesRule;
};

class CSSFontFeatureValuesBlockRule final : public CSSRule {
public:
    static Ref<CSSFontFeatureValuesBlockRule> create(StyleRuleFontFeatureValuesBlock& rule, CSSStyleSheet* sheet) { return adoptRef(*new CSSFontFeatureValuesBlockRule(rule, sheet)); }
    virtual ~CSSFontFeatureValuesBlockRule() = default;

private:
    CSSFontFeatureValuesBlockRule(StyleRuleFontFeatureValuesBlock&, CSSStyleSheet* parent);

    StyleRuleType styleRuleType() const final { return StyleRuleType::FontFeatureValuesBlock; }
    String cssText() const final;
    void reattach(StyleRuleBase&) final;

    Ref<StyleRuleFontFeatureValuesBlock> m_fontFeatureValuesBlockRule;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSFontFeatureValuesRule, StyleRuleType::FontFeatureValues)
SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSFontFeatureValuesBlockRule, StyleRuleType::FontFeatureValuesBlock)
