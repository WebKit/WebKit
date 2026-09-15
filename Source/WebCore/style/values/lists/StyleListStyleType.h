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

#pragma once

#include <WebCore/CSSCounterStyleDescriptors.h>
#include <WebCore/StyleCounterStyle.h>
#include <WebCore/StyleString.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class CSSRegisteredCounterStyle;

namespace Style {

// <'list-style-type'> = <counter-style> | <string> | none
// https://drafts.csswg.org/css-lists/#propdef-list-style-type
struct ListStyleType {
    using SymbolsFunction = CounterStyle::SymbolsFunction;

    ListStyleType(CSS::Keyword::None)
        : m_type { Type::None }
    {
    }

    ListStyleType(String&& string)
        : m_type { Type::String }
        , m_identifier { WTF::move(string.value) }
    {
    }

    ListStyleType(CounterStyle&& counterStyle)
    {
        if (auto symbolsFunction = counterStyle.trySymbolsFunction()) {
            m_type = Type::Symbols;
            m_symbolsFunction = WTF::move(*symbolsFunction);
            return;
        }
        m_type = Type::CounterStyle;
        if (auto name = counterStyle.tryName())
            m_identifier = WTF::move(name->value);
    }

    ListStyleType(SymbolsFunction&& symbolsFunction)
        : m_type { Type::Symbols }
        , m_symbolsFunction { WTF::move(symbolsFunction) }
    {
    }

    // <counter-style> specific constructors.

    ListStyleType(CSS::Keyword::Circle keyword)
        : ListStyleType { CounterStyle { CustomIdent { nameString(keyword.value) } } }
    {
    }

    ListStyleType(CSS::Keyword::Decimal keyword)
        : ListStyleType { CounterStyle { CustomIdent { nameString(keyword.value) } } }
    {
    }

    ListStyleType(CSS::Keyword::Disc keyword)
        : ListStyleType { CounterStyle { CustomIdent { nameString(keyword.value) } } }
    {
    }

    ListStyleType(CSS::Keyword::Square keyword)
        : ListStyleType { CounterStyle { CustomIdent { nameString(keyword.value) } } }
    {
    }

    bool isNone() const { return m_type == Type::None; }
    bool isCounterStyle() const { return m_type == Type::CounterStyle; }
    bool isString() const { return m_type == Type::String; }
    bool isSymbolsFunction() const { return m_type == Type::Symbols; }

    // <counter-style> specific predicates.

    bool isCircle() const;
    bool isDisc() const;
    bool isDecimal() const;
    bool isSquare() const;

    std::optional<CounterStyle> tryCounterStyle() const
    {
        return isCounterStyle() ? std::make_optional(CounterStyle { CustomIdent { m_identifier } }) : std::nullopt;
    }

    std::optional<SymbolsFunction> trySymbolsFunction() const
    {
        return isSymbolsFunction() ? std::make_optional(m_symbolsFunction) : std::nullopt;
    }

    // Builds the anonymous runtime counter style used to render the list marker for the `symbols()`
    // function. Declared here, but defined out-of-line since `CSSRegisteredCounterStyle` is only
    // forward-declared in this header.
    WEBCORE_EXPORT RefPtr<CSSRegisteredCounterStyle> trySymbolsFunctionCounterStyle() const;

    std::optional<AtomString> tryString() const
    {
        return isString() ? std::make_optional(m_identifier) : std::nullopt;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case Type::None:
            return visitor(CSS::Keyword::None { });
        case Type::String:
            return visitor(String { m_identifier });
        case Type::CounterStyle:
            return visitor(CounterStyle { CustomIdent { m_identifier } });
        case Type::Symbols:
            return visitor(m_symbolsFunction);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const ListStyleType&) const = default;

    // IPC-specific representation.
    struct NoneData { };
    struct StringData { AtomString identifier; };
    struct CounterStyleData { AtomString identifier; };
    struct SymbolsFunctionData {
        CSSCounterStyleDescriptors::System system { CSSCounterStyleDescriptors::System::Symbolic };
        Vector<WTF::String> symbols;
        friend bool operator==(const SymbolsFunctionData&, const SymbolsFunctionData&) = default;
    };
    using IPCData = Variant<NoneData, StringData, CounterStyleData, SymbolsFunctionData>;

    WEBCORE_EXPORT ListStyleType(const IPCData&);
    WEBCORE_EXPORT IPCData ipcData() const;

private:
    enum class Type : uint8_t {
        CounterStyle,
        String,
        None,
        Symbols
    };

    Type m_type { Type::None };
    // The identifier is the string when the type is String and is the @counter-style name when the type is CounterStyle.
    AtomString m_identifier { nullAtom() };
    // Only engaged when the type is Symbols.
    SymbolsFunction m_symbolsFunction;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ListStyleType> { auto operator()(BuilderState&, const CSSValue&) -> ListStyleType; };

template<> struct CSSValueCreation<ListStyleType> { Ref<CSSValue> operator()(CSSValuePool&, const Style::ComputedStyle&, const ListStyleType&); };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ListStyleType)
