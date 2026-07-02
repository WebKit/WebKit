/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSValueTypes+DeprecatedCSSOMValueCreation.h"

#include "DeprecatedCSSOMCustomValue.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {
namespace CSS {

// MARK: - DeprecatedCSSOMValue Creation

Ref<DeprecatedCSSOMValue> makeCustomDeprecatedCSSOMValue(Function<WTF::String(const SerializationContext&)>&& functor, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMCustomValue::create(WTF::move(functor), owner);
}

Ref<DeprecatedCSSOMValue> makePrimitiveDeprecatedCSSOMValue(Function<WTF::String(const SerializationContext&)>&& functor, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMPrimitiveValue::create(WTF::move(functor), owner);
}

Ref<DeprecatedCSSOMValue> makePrimitiveDeprecatedCSSOMValue(CSSValueID value, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMPrimitiveValue::create(Keyword { value }, owner);
}

template<> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue<SerializationSeparatorType::Space>(DeprecatedCSSOMValueListBuilder&& builder, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMValueList::create(WTF::move(builder), CSSValue::ValueSeparator::SpaceSeparator, owner);
}

template<> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue<SerializationSeparatorType::Comma>(DeprecatedCSSOMValueListBuilder&& builder, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMValueList::create(WTF::move(builder), CSSValue::ValueSeparator::CommaSeparator, owner);
}

template<> Ref<DeprecatedCSSOMValue> makeListDeprecatedCSSOMValue<SerializationSeparatorType::Slash>(DeprecatedCSSOMValueListBuilder&& builder, CSSStyleDeclaration& owner)
{
    return DeprecatedCSSOMValueList::create(WTF::move(builder), CSSValue::ValueSeparator::SlashSeparator, owner);
}

} // namespace CSS
} // namespace WebCore
