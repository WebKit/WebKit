/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSContent.h"

#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"
#include "DeprecatedCSSOMPrimitiveValue.h"

namespace WebCore {
namespace CSS {

// MARK: - Serialization

static std::optional<CounterStyle> counterStyleForSerialization(const CounterStyle& style)
{
    return WTF::switchOn(style.identifier,
        [&](const CSS::Keyword& predefinedKeyword) -> std::optional<CounterStyle> {
            return predefinedKeyword.value != CSSValueDecimal ? std::make_optional(style) : std::nullopt;
        },
        [&](const CSS::CustomIdent&) -> std::optional<CounterStyle> {
            return style;
        }
    );
};

void Serialize<ContentCounterFunctionParameters>::operator()(StringBuilder& builder, const SerializationContext& context, const ContentCounterFunctionParameters& value)
{
    serializationForCSS(builder, context, CommaSeparatedTuple<CustomIdent, std::optional<CounterStyle>> {
        value.identifier,
        counterStyleForSerialization(value.style)
    });
}

void Serialize<ContentCountersFunctionParameters>::operator()(StringBuilder& builder, const SerializationContext& context, const ContentCountersFunctionParameters& value)
{
    serializationForCSS(builder, context, CommaSeparatedTuple<CustomIdent, String, std::optional<CounterStyle>> {
        value.identifier,
        value.separator,
        counterStyleForSerialization(value.style)
    });
}

// MARK: - DeprecatedCSSOMValue Creation

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<ContentCounterFunction>::operator()(CSSValuePool&, CSSStyleDeclaration& owner, const ContentCounterFunction& value)
{
    return DeprecatedCSSOMPrimitiveValue::create(ContentCounterFunctionWrapper { value }, owner);
}

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<ContentCountersFunction>::operator()(CSSValuePool&, CSSStyleDeclaration& owner, const ContentCountersFunction& value)
{
    return DeprecatedCSSOMPrimitiveValue::create(ContentCountersFunctionWrapper { value }, owner);
}

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<ContentLegacyAttrFunction>::operator()(CSSValuePool&, CSSStyleDeclaration& owner, const ContentLegacyAttrFunction& value)
{
    return DeprecatedCSSOMPrimitiveValue::create(ContentLegacyAttrFunctionWrapper { value }, owner);
}

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<Content::Data>::operator()(CSSValuePool& pool, CSSStyleDeclaration& owner, const Content::Data& value)
{
    if (!value.alt)
        return createDeprecatedCSSOMValue(pool, owner, value.visible);

    return makeListDeprecatedCSSOMValue<SerializationSeparator<Content::Data>>(
        DeprecatedCSSOMValueListBuilder {
            createDeprecatedCSSOMValue(pool, owner, value.visible),
            createDeprecatedCSSOMValue(pool, owner, *value.alt),
        },
        owner
    );
}

} // namespace CSS
} // namespace WebCore
