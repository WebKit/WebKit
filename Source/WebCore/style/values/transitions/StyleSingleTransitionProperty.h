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

#include <WebCore/CSSPropertyParser.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/WebAnimationTypes.h>
#include <WebCore/WebAnimationUtilities.h>

namespace WebCore {
namespace Style {

// <single-transition-property> = all | <custom-ident>
// https://www.w3.org/TR/css-transitions-1/#single-transition-property
struct SingleTransitionProperty {
    struct UnknownProperty {
        CustomIdentifier value;

        bool operator==(const UnknownProperty&) const = default;
    };
    struct SingleProperty {
        AnimatableCSSProperty value;

        template<typename... F> decltype(auto) switchOn(F&&... f) const
        {
            auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
            return visitor(CustomIdentifier { animatablePropertyAsString(value) });
        }

        bool operator==(const SingleProperty&) const = default;
    };

    SingleTransitionProperty(CSS::Keyword::All keyword)
        : m_value { keyword }
    {
    }

    SingleTransitionProperty(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    SingleTransitionProperty(CustomIdentifier&& identifier)
        : m_value { fromCustomIdentifier(WTFMove(identifier)) }
    {
    }

    SingleTransitionProperty(CSSPropertyID propertyID)
        : m_value { SingleProperty { .value = propertyID } }
    {
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const SingleTransitionProperty&) const = default;

private:
    using Kind = Variant<CSS::Keyword::All, CSS::Keyword::None, UnknownProperty, SingleProperty>;

    static Kind fromCustomIdentifier(CustomIdentifier&& identifier)
    {
        if (isCustomPropertyName(identifier.value))
            return Kind { SingleProperty { .value = WTFMove(identifier.value) } };
        return Kind { UnknownProperty { .value = WTFMove(identifier) } };
    }

    Kind m_value;
};

DEFINE_TYPE_WRAPPER_GET(SingleTransitionProperty::UnknownProperty, value);

// MARK: - Conversion

template<> struct CSSValueConversion<SingleTransitionProperty> { auto operator()(BuilderState&, const CSSValue&) -> SingleTransitionProperty; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::SingleTransitionProperty::UnknownProperty)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleTransitionProperty::SingleProperty)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SingleTransitionProperty)
