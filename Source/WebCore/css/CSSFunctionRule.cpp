/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSFunctionRule.h"

#include "CSSMarkup.h"
#include "StyleRuleFunction.h"

namespace WebCore {

Ref<CSSFunctionRule> CSSFunctionRule::create(StyleRuleFunction& rule, CSSStyleSheet* parent)
{
    return adoptRef(*new CSSFunctionRule(rule, parent));
}

CSSFunctionRule::CSSFunctionRule(StyleRuleFunction& rule, CSSStyleSheet* parent)
    : CSSGroupingRule(rule, parent)
{
}

String CSSFunctionRule::name() const
{
    return styleRuleFunction().name();
}

auto CSSFunctionRule::getParameters() const -> Vector<FunctionParameter>
{
    return WTF::map(styleRuleFunction().parameters(), [](const auto& parameter) {
        RefPtr defaultValue = parameter.defaultValue;
        return FunctionParameter {
            .name = parameter.name,
            .type = "*"_s, // FIXME: Implement.
            // FIXME: The spec says:
            //   "The default value of the function parameter, or `null` if the argument does not have a default".
            // But WPT tests currently expect the value to missing/undefined, not `null`, so we are using
            // `std::nullopt` instead of `nullString()`. See https://github.com/w3c/csswg-drafts/issues/13394.
            .defaultValue = defaultValue ? std::make_optional(defaultValue->serialize()) : std::nullopt
        };
    });
}

String CSSFunctionRule::returnType() const
{
    return "*"_s;
}

String CSSFunctionRule::cssText() const
{
    StringBuilder builder;
    builder.append("@function "_s);
    serializeIdentifier(name(), builder);
    builder.append('(');

    auto separator = ""_s;
    for (auto& parameter : styleRuleFunction().parameters()) {
        builder.append(separator);
        serializeIdentifier(parameter.name, builder);
        // FIXME: Serialize the type.

        if (RefPtr defaultValue = parameter.defaultValue)
            builder.append(": "_s, defaultValue->serialize());
        separator = ", "_s;
    }

    builder.append(") { "_s);

    for (unsigned index = 0; index < length(); ++index) {
        Ref rule = *item(index);
        auto ruleText = rule->cssText();
        builder.append(ruleText, ' ');
    }
    builder.append('}');

    return builder.toString();

}

const StyleRuleFunction& CSSFunctionRule::styleRuleFunction() const
{
    return downcast<StyleRuleFunction>(groupRule());
}

}
