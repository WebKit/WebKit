/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "DeprecatedCSSOMValueList.h"

#include "CSSValueList.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

Ref<DeprecatedCSSOMValueList> DeprecatedCSSOMValueList::create(DeprecatedCSSOMValueListBuilder&& values, CSSValue::ValueSeparator separator, CSSStyleDeclaration& owner)
{
    return adoptRef(*new DeprecatedCSSOMValueList(WTF::move(values), separator, owner));
}

Ref<DeprecatedCSSOMValueList> DeprecatedCSSOMValueList::create(const CSSValueContainingVector& values, CSSStyleDeclaration& owner)
{
    return adoptRef(*new DeprecatedCSSOMValueList(WTF::map<4>(values, [&](auto& value) -> Ref<DeprecatedCSSOMValue> { return value.createDeprecatedCSSOMWrapper(owner); }), values.separator(), owner));
}

DeprecatedCSSOMValueList::DeprecatedCSSOMValueList(DeprecatedCSSOMValueListBuilder&& values, CSSValue::ValueSeparator separator, CSSStyleDeclaration& owner)
    : DeprecatedCSSOMValue(owner)
    , m_valueSeparator { separator }
    , m_values { WTF::move(values) }
{
}

String DeprecatedCSSOMValueList::cssText() const
{
    return makeString(interleave(m_values, [](auto& value) { return value.get().cssText(); }, CSSValueList::separatorCSSText(m_valueSeparator)));
}

unsigned short DeprecatedCSSOMValueList::cssValueType() const
{
    return CSS_VALUE_LIST;
}

} // namespace WebCore
