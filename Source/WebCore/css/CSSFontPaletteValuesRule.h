/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

namespace WebCore {

class CSSStyleDeclaration;
class DOMMapAdapter;
class StyleRuleCSSStyleDeclaration;
class StyleRuleFontPaletteValues;

class CSSFontPaletteValuesRule final : public CSSRule {
public:
    static Ref<CSSFontPaletteValuesRule> create(StyleRuleFontPaletteValues& rule, CSSStyleSheet* sheet) { return adoptRef(*new CSSFontPaletteValuesRule(rule, sheet)); }

    virtual ~CSSFontPaletteValuesRule();

    String name() const;
    String fontFamily() const;
    String basePalette() const;
    String overrideColors() const;

private:
    CSSFontPaletteValuesRule(StyleRuleFontPaletteValues&, CSSStyleSheet* parent);

    StyleRuleType styleRuleType() const final { return StyleRuleType::FontPaletteValues; }
    String cssText() const final;
    void reattach(StyleRuleBase&) final;

    Ref<StyleRuleFontPaletteValues> m_fontPaletteValuesRule;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSFontPaletteValuesRule, StyleRuleType::FontPaletteValues)
