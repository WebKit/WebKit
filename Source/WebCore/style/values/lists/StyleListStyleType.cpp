/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSKeywordValue.h"
#include "CSSStringValue.h"
#include "CSSSymbolsFunctionValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

ListStyleType::ListStyleType(const IPCData& data)
{
    WTF::switchOn(data,
        [&](const NoneData&) {
            m_type = Type::None;
        },
        [&](const StringData& stringData) {
            m_type = Type::String;
            m_identifier = stringData.identifier;
        },
        [&](const CounterStyleData& counterStyleData) {
            m_type = Type::CounterStyle;
            m_identifier = counterStyleData.identifier;
        },
        [&](const SymbolsFunctionData& symbolsFunctionData) {
            m_type = Type::Symbols;
            auto symbols = symbolsFunctionData.symbols.map([](auto& symbol) { return String { symbol }; });
            m_symbolsFunction = { SymbolsParameters { keywordFromSymbolsSystemForSerialization(symbolsFunctionData.system), WTF::move(symbols) } };
        }
    );
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
    case Type::Symbols: {
        auto symbols = m_symbolsFunction->symbols.value.map([](auto& symbol) { return symbol.value; });
        return IPCData { SymbolsFunctionData { symbolsSystemFromKeyword(m_symbolsFunction->system), WTF::move(symbols) } };
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool ListStyleType::isCircle() const
{
    return m_type == Type::CounterStyle && m_identifier == nameString(CSSValueCircle);
}

bool ListStyleType::isDecimal() const
{
    return m_type == Type::CounterStyle && m_identifier == nameString(CSSValueDecimal);
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
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (auto valueID = keywordValue->valueID(); valueID) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        default:
            return CounterStyle { CustomIdent { nameStringForSerialization(valueID) } };
        }
    }

    if (RefPtr stringValue = dynamicDowncast<CSSStringValue>(value))
        return toStyleFromCSSValue<String>(state, *stringValue);

    if (RefPtr symbolsFunctionValue = dynamicDowncast<CSSSymbolsFunctionValue>(value))
        return { toStyle(symbolsFunctionValue->symbolsFunction(), state) };

    return CounterStyle { toStyleFromCSSValue<CustomIdent>(state, value) };
}

auto CSSValueCreation<ListStyleType>::operator()(CSSValuePool& pool, const Style::ComputedStyle& style, const ListStyleType& value) -> Ref<CSSValue>
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::None& none) { return Style::createCSSValue(pool, style, none); },
        [&](const CounterStyle& counterStyle) { return Style::createCSSValue(pool, style, counterStyle); },
        [&](const String& string) { return Style::createCSSValue(pool, style, string); },
        [&](const ListStyleType::SymbolsFunction& symbolsFunction) { return Style::createCSSValue(pool, style, symbolsFunction); }
    );
}

} // namespace Style
} // namespace WebCore
