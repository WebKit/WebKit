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

#include "config.h"
#include "CSSFontFeatureValuesRule.h"

#include "CSSMarkup.h"

namespace WebCore {

CSSFontFeatureValuesRule::CSSFontFeatureValuesRule(StyleRuleFontFeatureValues& fontFeatureValuesRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_fontFeatureValuesRule(fontFeatureValuesRule)
{
}

String CSSFontFeatureValuesRule::cssText() const
{
    StringBuilder builder;
    builder.append("@font-feature-values ");
    auto joinFontFamiliesWithSeparator = [&builder] (const auto& elements, String separator) {
        bool first = true;
        for (auto element : elements) {
            if (!first)
                builder.append(separator);
            builder.append(serializeFontFamily(element));
            first = false;
        }
    };
    joinFontFamiliesWithSeparator(m_fontFeatureValuesRule->fontFamilies(), ", "_s);
    builder.append(" { ");
    const auto& value = m_fontFeatureValuesRule->value();
    
    auto addVariant = [&builder] (const String& variantName, const auto& tags) {
        if (!tags.isEmpty()) {
            builder.append("@", variantName, " { ");
            for (auto tag : tags) {
                serializeIdentifier(tag.key, builder);
                builder.append(':');
                for (auto integer : tag.value)
                    builder.append(' ', integer);
                builder.append("; ");
            }
            builder.append("} ");
        }
    };
    
    // WPT expects the order used in Servo.
    // https://searchfox.org/mozilla-central/source/servo/components/style/stylesheets/font_feature_values_rule.rs#430
    addVariant("swash"_s, value->swash());
    addVariant("stylistic"_s, value->stylistic());
    addVariant("ornaments"_s, value->ornaments());
    addVariant("annotation"_s, value->annotation());
    addVariant("character-variant"_s, value->characterVariant());
    addVariant("styleset"_s, value->styleset());
    
    builder.append('}');
    return builder.toString();
}

void CSSFontFeatureValuesRule::reattach(StyleRuleBase& rule)
{
    m_fontFeatureValuesRule = static_cast<StyleRuleFontFeatureValues&>(rule);
}

CSSFontFeatureValuesBlockRule::CSSFontFeatureValuesBlockRule(StyleRuleFontFeatureValuesBlock& block , CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_fontFeatureValuesBlockRule(block)
{
}

String CSSFontFeatureValuesBlockRule::cssText() const
{
    // This rule is always contained inside a FontFeatureValuesRule,
    // which is the only one we are expected to serialize to CSS.
    // We should never serialize a Block by itself.
    ASSERT_NOT_REACHED();
    return { };
}

void CSSFontFeatureValuesBlockRule::reattach(StyleRuleBase& rule)
{
    m_fontFeatureValuesBlockRule = static_cast<StyleRuleFontFeatureValuesBlock&>(rule);
}

} // namespace WebCore
