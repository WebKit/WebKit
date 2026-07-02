/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

struct BorderImageWidthValue {
    using LengthPercentage = CSS::LengthPercentage<CSS::Nonnegative, float>;
    using Number = CSS::Number<CSS::Nonnegative, float>;

    BorderImageWidthValue(Keyword::Auto value) : m_value { value } { }
    BorderImageWidthValue(LengthPercentage&& value) : m_value { WTF::move(value) } { }
    BorderImageWidthValue(Number&& value) : m_value { WTF::move(value) } { }

    bool isLength() const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const BorderImageWidthValue&) const = default;

    bool operator==(Keyword::Auto) const
    {
        return WTF::holdsAlternative<Keyword::Auto>(m_value);
    }

    template<LengthPercentage::UnitType unitValue>
    bool operator==(const ValueLiteral<unitValue>& other) const
    {
        if (auto* lengthPercentage = std::get_if<LengthPercentage>(&m_value))
            return *lengthPercentage == other;
        return false;
    }

    template<NestedUnitEnumOf<LengthPercentage::UnitType> E, E unitValue>
    bool operator==(const ValueLiteral<unitValue>& other) const
    {
        if (auto* lengthPercentage = std::get_if<LengthPercentage>(&m_value))
            return *lengthPercentage == other;
        return false;
    }

    bool operator==(const ValueLiteral<NumberUnit::Number>& other) const
    {
        if (auto* number = std::get_if<Number>(&m_value))
            return *number == other;
        return false;
    }

private:
    Variant<Keyword::Auto, LengthPercentage, Number> m_value;
};

// <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width
struct BorderImageWidth {
    using Value = BorderImageWidthValue;
    using Edges = MinimallySerializingSpaceSeparatedRectEdges<Value>;

    Edges values;
    bool legacyWebkitBorderImage;

    bool overridesBorderWidths() const { return legacyWebkitBorderImage; }

    bool operator==(const BorderImageWidth&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(BorderImageWidth, values);

// MARK: - Conversion

template<> struct CSSValueCreation<BorderImageWidth> { auto operator()(CSSValuePool&, const BorderImageWidth&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<BorderImageWidth> { void operator()(StringBuilder&, const SerializationContext&, const BorderImageWidth&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::BorderImageWidth)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::BorderImageWidth::Value)
