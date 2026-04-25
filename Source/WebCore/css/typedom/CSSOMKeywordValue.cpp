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
#include "CSSOMKeywordValue.h"

#include "CSSCustomIdentValue.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CSSValuePool.h"
#include "ExceptionOr.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSOMKeywordValue);

Ref<CSSOMKeywordValue> CSSOMKeywordValue::rectifyKeywordish(CSSOMKeywordish&& keywordish)
{
    // https://drafts.css-houdini.org/css-typed-om/#rectify-a-keywordish-value
    return WTF::switchOn(WTF::move(keywordish),
        [](String&& string) {
            return adoptRef(*new CSSOMKeywordValue(string));
        },
        [](Ref<CSSOMKeywordValue>&& value) {
            return value;
        }
    );
}

ExceptionOr<Ref<CSSOMKeywordValue>> CSSOMKeywordValue::create(const String& value)
{
    if (value.isEmpty())
        return Exception { ExceptionCode::TypeError };

    return adoptRef(*new CSSOMKeywordValue(value));
}

ExceptionOr<void> CSSOMKeywordValue::setValue(const String& value)
{
    if (value.isEmpty())
        return Exception { ExceptionCode::TypeError };

    m_value = value;
    return { };
}

void CSSOMKeywordValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments>) const
{
    // https://drafts.css-houdini.org/css-typed-om/#keywordvalue-serialization
    serializeIdentifier(builder, m_value);
}

RefPtr<CSSValue> CSSOMKeywordValue::toCSSValue() const
{
    auto keyword = cssValueKeywordID(m_value);
    if (keyword == CSSValueInvalid)
        return CSSCustomIdentValue::create(CSS::CustomIdent { AtomString { m_value } });
    return CSSKeywordValue::create(CSS::Keyword { keyword });
}

} // namespace WebCore
