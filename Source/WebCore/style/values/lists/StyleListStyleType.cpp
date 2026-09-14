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

#include "CSSFunctionValue.h"
#include "CSSKeywordValue.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"
#include "CSSRegisteredCounterStyle.h"
#include "CSSStringValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

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
            m_symbolsFunction = { CounterStyle::SymbolsParameters { symbolsFunctionData.system, Vector<WTF::String> { symbolsFunctionData.symbols } } };
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
    case Type::Symbols:
        return IPCData { SymbolsFunctionData { m_symbolsFunction->system, m_symbolsFunction->symbols.value } };
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

RefPtr<CSSRegisteredCounterStyle> ListStyleType::trySymbolsFunctionCounterStyle() const
{
    if (!isSymbolsFunction())
        return nullptr;
    return CSSRegisteredCounterStyle::createForSymbolsFunction(m_symbolsFunction->system, m_symbolsFunction->symbols.value);
}

// MARK: - Conversion

static ListStyleType listStyleTypeFromSymbolsFunction(const CSSFunctionValue& function)
{
    ASSERT(function.name() == CSSValueSymbols);

    auto system = CSSCounterStyleDescriptors::System::Symbolic;
    unsigned index = 0;
    if (function.length()) {
        if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(function[0])) {
            if (auto systemFromKeyword = systemFromSymbolsTypeKeyword(keywordValue->valueID())) {
                system = *systemFromKeyword;
                index = 1;
            }
        }
    }

    Vector<WTF::String> symbols;
    symbols.reserveInitialCapacity(function.length() - index);
    for (; index < function.length(); ++index) {
        if (RefPtr stringValue = dynamicDowncast<CSSStringValue>(function[index]))
            symbols.append(stringValue->string().value);
    }

    return { ListStyleType::SymbolsFunction { CounterStyle::SymbolsParameters { system, WTF::move(symbols) } } };
}

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

    if (RefPtr functionValue = dynamicDowncast<CSSFunctionValue>(value); functionValue && functionValue->name() == CSSValueSymbols)
        return listStyleTypeFromSymbolsFunction(*functionValue);

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
