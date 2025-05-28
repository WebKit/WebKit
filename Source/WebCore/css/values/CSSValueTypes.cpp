/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CSSValueTypes.h"

#include "CSSFunctionValue.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"

namespace WebCore {
namespace CSS {

void Serialize<CustomIdentifier>::operator()(StringBuilder& builder, const SerializationContext&, const CustomIdentifier& value)
{
    serializeIdentifier(value.value, builder);
}

void Serialize<WTF::AtomString>::operator()(StringBuilder& builder, const SerializationContext&, const WTF::AtomString& value)
{
    serializeString(value, builder);
}

void Serialize<WTF::String>::operator()(StringBuilder& builder, const SerializationContext&, const WTF::String& value)
{
    serializeString(value, builder);
}

Ref<CSSValue> makePrimitiveCSSValue(CSSValueID value)
{
    return CSSPrimitiveValue::create(value);
}

Ref<CSSValue> makePrimitiveCSSValue(const CustomIdentifier& value)
{
    return CSSPrimitiveValue::createCustomIdent(value.value);
}

Ref<CSSValue> makePrimitiveCSSValue(const WTF::AtomString& value)
{
    return CSSPrimitiveValue::create(value);
}

Ref<CSSValue> makePrimitiveCSSValue(const WTF::String& value)
{
    return CSSPrimitiveValue::create(value);
}

Ref<CSSValue> makeFunctionCSSValue(CSSValueID name, Ref<CSSValue>&& value)
{
    return CSSFunctionValue::create(name, WTFMove(value));
}

Ref<CSSValue> makeSpaceSeparatedCoalescingPairCSSValue(Ref<CSSValue>&& first, Ref<CSSValue>&& second)
{
    return CSSValuePair::create(WTFMove(first), WTFMove(second));
}

Ref<CSSValue> makeSpaceSeparatedListCSSValue(CSSValueListBuilder&& builder)
{
    return CSSValueList::createSpaceSeparated(WTFMove(builder));
}

Ref<CSSValue> makeCommaSeparatedListCSSValue(CSSValueListBuilder&& builder)
{
    return CSSValueList::createCommaSeparated(WTFMove(builder));
}

Ref<CSSValue> makeSlashSeparatedListCSSValue(CSSValueListBuilder&& builder)
{
    return CSSValueList::createSlashSeparated(WTFMove(builder));
}

} // namespace CSS
} // namespace WebCore
