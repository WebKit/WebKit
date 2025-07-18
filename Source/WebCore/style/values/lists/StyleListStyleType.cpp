/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleListStyleType.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

ListStyleType::Type ListStyleType::type(const IPCData& data)
{
    static_assert(WTF::VariantSizeV<IPCData> == 3);
    switch (data.index()) {
    case WTF::alternativeIndexV<NoneData, IPCData>:
        return Type::None;
    case WTF::alternativeIndexV<StringData, IPCData>:
        return Type::String;
    case WTF::alternativeIndexV<CounterStyleData, IPCData>:
        return Type::CounterStyle;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

AtomString ListStyleType::identifier(const IPCData& data)
{
    return WTF::switchOn(data,
        [](const NoneData&) -> AtomString {
            return nullAtom();
        },
        [](const StringData& data) -> AtomString {
            return data.identifier;
        },
        [](const CounterStyleData& data) -> AtomString {
            return data.identifier;
        }
    );
}

ListStyleType::ListStyleType(const IPCData& data)
    : m_type { ListStyleType::type(data) }
    , m_identifier { ListStyleType::identifier(data) }
{
}

ListStyleType::IPCData ListStyleType::ipcData() const
{
    switch (m_type) {
    case Type::None:
        return IPCData { NoneData { } };
    case Type::String:
        return IPCData { StringData { m_identifier } };
    case Type::CounterStyle:
        return IPCData { CounterStyleData { m_identifier } };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool ListStyleType::isCircle() const
{
    return m_type == Type::CounterStyle && m_identifier == nameString(CSSValueCircle);
}

bool ListStyleType::isDisc() const
{
    return m_type == Type::CounterStyle && m_identifier == nameString(CSSValueDisc);
}

bool ListStyleType::isSquare() const
{
    return m_type == Type::CounterStyle && m_identifier == nameString(CSSValueSquare);
}

// MARK: - Conversion

auto CSSValueConversion<ListStyleType>::operator()(BuilderState& state, const CSSValue& value) -> ListStyleType
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::None { };

    if (primitiveValue->isValueID()) {
        if (primitiveValue->valueID() == CSSValueNone)
            return CSS::Keyword::None { };
        return CounterStyle { { AtomString { primitiveValue->stringValue() } } };
    }
    if (primitiveValue->isCustomIdent())
        return CounterStyle { { AtomString { primitiveValue->stringValue() } } };

    return AtomString { primitiveValue->stringValue() };
}

} // namespace Style
} // namespace WebCore
