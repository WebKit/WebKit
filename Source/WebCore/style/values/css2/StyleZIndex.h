/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {

class RenderStyle;
class StyleBoxData;

namespace Style {

// <'z-index'> = auto | <integer>
// https://drafts.csswg.org/css2/#propdef-z-index
struct ZIndex {
    using Value = Integer<>;

    constexpr ZIndex(CSS::Keyword::Auto)
    {
    }

    constexpr ZIndex(Value value)
        : m_isAuto { false }
        , m_value { value }
    {
    }

    constexpr ZIndex(Value::ResolvedValueType value)
        : m_isAuto { false }
        , m_value { value }
    {
    }

    constexpr ZIndex(CSS::ValueLiteral<CSS::IntegerUnit::Integer> literal)
        : m_isAuto { false }
        , m_value { literal }
    {
    }

    constexpr bool isAuto() const { return m_isAuto; }
    constexpr bool isValue() const { return !m_isAuto; }
    constexpr std::optional<Value> tryValue() const { return isValue() ? std::make_optional(m_value) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (m_isAuto)
            return visitor(CSS::Keyword::Auto { });
        return visitor(m_value);
    }

    constexpr bool operator==(const ZIndex&) const = default;

private:
    // NOTE: This type is stored represented using an explicit `bool` and `Value`, rather than a `Variant`, to allow compact
    // storage and efficient construction in `StyleBoxData`. It is not using a `ValueOrKeyword` to preserve the entire `int`
    // value range for `z-index`. If we determine its ok for `z-index` to only have `MAX_INT - 1` values, we can switch this
    // out for `ValueOrKeyword` with a custom `MarkableTraits`.
    friend class WebCore::RenderStyle;
    friend class WebCore::RenderStyleProperties;
    friend class WebCore::StyleBoxData;

    constexpr ZIndex(bool isAuto, Value value)
        : m_isAuto { isAuto }
        , m_value { value }
    {
    }

    bool m_isAuto { true };
    Value m_value { 0 };
};

// MARK: - Conversion

template<> struct CSSValueConversion<ZIndex> { auto operator()(BuilderState&, const CSSValue&) -> ZIndex; };

// MARK: - Blending

template<> struct Blending<ZIndex> {
    auto canBlend(const ZIndex&, const ZIndex&) -> bool;
    auto blend(const ZIndex&, const ZIndex&, const BlendingContext&) -> ZIndex;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ZIndex)
