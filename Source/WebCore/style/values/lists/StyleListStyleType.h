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

#include "StyleCounterStyle.h"
#include "StyleValueTypes.h"
#include <wtf/Variant.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Style {

// <'list-style-type'> = <counter-style> | <string> | none
// https://drafts.csswg.org/css-lists/#propdef-list-style-type
struct ListStyleType {
    ListStyleType(CSS::Keyword::None)
        : m_type { Type::None }
    {
    }

    ListStyleType(AtomString&& string)
        : m_type { Type::String }
        , m_identifier { WTFMove(string) }
    {
    }

    ListStyleType(CounterStyle&& counterStyle)
        : m_type { Type::CounterStyle }
        , m_identifier { WTFMove(counterStyle.identifier.value) }
    {
    }

    // <counter-style> specific constructors.

    ListStyleType(CSS::Keyword::Circle keyword)
        : ListStyleType { CounterStyle { { nameString(keyword.value) } } }
    {
    }

    ListStyleType(CSS::Keyword::Disc keyword)
        : ListStyleType { CounterStyle { { nameString(keyword.value) } } }
    {
    }

    ListStyleType(CSS::Keyword::Square keyword)
        : ListStyleType { CounterStyle { { nameString(keyword.value) } } }
    {
    }

    bool isNone() const { return m_type == Type::None; }
    bool isCounterStyle() const { return m_type == Type::CounterStyle; }
    bool isString() const { return m_type == Type::String; }

    // <counter-style> specific predicates.

    bool isCircle() const;
    bool isDisc() const;
    bool isSquare() const;

    std::optional<CounterStyle> tryCounterStyle() const
    {
        return isCounterStyle() ? std::make_optional(CounterStyle { { m_identifier } } ) : std::nullopt;
    }

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
            return visitor(m_identifier);
        case Type::CounterStyle:
            return visitor(CounterStyle { { m_identifier } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const ListStyleType&) const = default;

    // IPC-specific representation.
    struct NoneData { };
    struct StringData { AtomString identifier; };
    struct CounterStyleData { AtomString identifier; };
    using IPCData = Variant<NoneData, StringData, CounterStyleData>;

    WEBCORE_EXPORT ListStyleType(const IPCData&);
    WEBCORE_EXPORT IPCData ipcData() const;

private:
    enum class Type : uint8_t {
        CounterStyle,
        String,
        None
    };

    static Type type(const IPCData&);
    static AtomString identifier(const IPCData&);

    Type m_type { Type::None };
    // The identifier is the string when the type is String and is the @counter-style name when the type is CounterStyle.
    AtomString m_identifier { nullAtom() };
};

// MARK: - Conversion

template<> struct CSSValueConversion<ListStyleType> { auto operator()(BuilderState&, const CSSValue&) -> ListStyleType; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ListStyleType)
