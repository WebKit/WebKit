/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'white-space-trim'> = none | discard-before || discard-after || discard-inner
// https://drafts.csswg.org/css-text-4/#white-space-trim

enum class WhiteSpaceTrimValue : uint8_t {
    DiscardBefore,
    DiscardAfter,
    DiscardInner,
};

using WhiteSpaceTrimValueEnumSet = SpaceSeparatedEnumSet<WhiteSpaceTrimValue>;

struct WhiteSpaceTrim {
    using EnumSet = WhiteSpaceTrimValueEnumSet;
    using value_type = WhiteSpaceTrimValueEnumSet::value_type;

    constexpr WhiteSpaceTrim(CSS::Keyword::None) : m_value { } { }
    constexpr WhiteSpaceTrim(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr WhiteSpaceTrim(value_type value) : WhiteSpaceTrim { EnumSet { value } } { }
    constexpr WhiteSpaceTrim(std::initializer_list<value_type> initializerList) : WhiteSpaceTrim { EnumSet { initializerList } } { }

    static constexpr WhiteSpaceTrim fromRaw(EnumSet::StorageType rawValue) { return EnumSet::fromRaw(rawValue); }
    constexpr EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(WhiteSpaceTrimValue e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }

    constexpr bool isNone() const { return m_value.isEmpty(); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(m_value);
    }

    constexpr bool operator==(const WhiteSpaceTrim&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<WhiteSpaceTrim> {
    auto operator()(BuilderState&, const CSSValue&) -> WhiteSpaceTrim;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WhiteSpaceTrim)
