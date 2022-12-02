/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "StylePropertyMapReadOnly.h"

#include "CSSCustomPropertyValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSStyleImageValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnitValue.h"
#include "CSSUnparsedValue.h"
#include "CSSValueList.h"
#include "Document.h"

namespace WebCore {

RefPtr<CSSStyleValue> StylePropertyMapReadOnly::reifyValue(RefPtr<CSSValue>&& value, Document& document)
{
    if (!value)
        return nullptr;
    auto result = CSSStyleValueFactory::reifyValue(value.releaseNonNull(), &document);
    return (result.hasException() ? nullptr : RefPtr<CSSStyleValue> { result.releaseReturnValue() });
}

Vector<RefPtr<CSSStyleValue>> StylePropertyMapReadOnly::reifyValueToVector(RefPtr<CSSValue>&& value, Document& document)
{
    if (!value)
        return { };

    if (!is<CSSValueList>(*value))
        return { StylePropertyMapReadOnly::reifyValue(WTFMove(value), document) };

    auto& valueList = downcast<CSSValueList>(*value);
    Vector<RefPtr<CSSStyleValue>> result;
    result.reserveInitialCapacity(valueList.length());
    for (const auto& cssValue : valueList)
        result.uncheckedAppend(StylePropertyMapReadOnly::reifyValue(cssValue.copyRef(), document));
    return result;
}

StylePropertyMapReadOnly::Iterator::Iterator(StylePropertyMapReadOnly& map, ScriptExecutionContext* context)
    : m_values(map.entries(context))
{
}

std::optional<StylePropertyMapReadOnly::StylePropertyMapEntry> StylePropertyMapReadOnly::Iterator::next()
{
    if (m_index >= m_values.size())
        return std::nullopt;

    return m_values[m_index++];
}

} // namespace WebCore
