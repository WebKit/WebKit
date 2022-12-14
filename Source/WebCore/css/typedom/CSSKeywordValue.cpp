/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSKeywordValue.h"

#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSKeywordValue);

Ref<CSSKeywordValue> CSSKeywordValue::rectifyKeywordish(CSSKeywordish&& keywordish)
{
    // https://drafts.css-houdini.org/css-typed-om/#rectify-a-keywordish-value
    return WTF::switchOn(WTFMove(keywordish), [] (String string) {
        return adoptRef(*new CSSKeywordValue(string));
    }, [] (RefPtr<CSSKeywordValue> value) {
        RELEASE_ASSERT(value);
        return value.releaseNonNull();
    });
}

ExceptionOr<Ref<CSSKeywordValue>> CSSKeywordValue::create(const String& value)
{
    if (value.isEmpty())
        return Exception { TypeError };
    
    return adoptRef(*new CSSKeywordValue(value));
}

ExceptionOr<void> CSSKeywordValue::setValue(const String& value)
{
    if (value.isEmpty())
        return Exception { TypeError };
    
    m_value = value;
    return { };
}

void CSSKeywordValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    // https://drafts.css-houdini.org/css-typed-om/#keywordvalue-serialization
    serializeIdentifier(m_value, builder);
}

RefPtr<CSSValue> CSSKeywordValue::toCSSValue() const
{
    auto keyword = cssValueKeywordID(m_value);
    if (keyword == CSSValueInvalid)
        return CSSPrimitiveValue::create(m_value, CSSUnitType::CustomIdent);
    return CSSPrimitiveValue::createIdentifier(keyword);
}

} // namespace WebCore
