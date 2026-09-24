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

#include <WebCore/CSSLinkParameter.h>
#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleDeclarationValue.h>
#include <WebCore/StyleTypeSpecifier.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

namespace Style {

// <param-spec> = color | accent-color | [ <dashed-ident> <css-type>? ]
struct ParamSpec {
    // The <dashed-ident> alternative, whose default comes from the same-named custom
    // property on the element. The name and type are space separated.
    struct Custom {
        CustomIdent name;
        std::optional<TypeSpecifier> type;

        bool operator==(const Custom&) const = default;
    };

    Variant<CSS::Keyword::Color, CSS::Keyword::AccentColor, Custom> value;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const ParamSpec&) const = default;

    // The link parameter's name, which for the keyword alternatives is the keyword itself.
    const AtomString& name() const;
};

template<size_t I> const auto& get(const ParamSpec::Custom& custom)
{
    if constexpr (!I)
        return custom.name;
    else if constexpr (I == 1)
        return custom.type;
}

// The arguments of param(): a spec and the value it sets, serialized comma separated.
// The comma is always present, so the value is not coalesced away when it is empty.
struct LinkParameter {
    // https://drafts.csswg.org/css-link-params/#param
    // "Other values will be left unresolved without a type, so the specified value is
    // passed on unchanged."
    struct Unresolved {
        bool operator==(const Unresolved&) const = default;
    };

    // "If it fails to parse as the given type, the link parameter is invalid and ignored."
    struct Invalid {
        bool operator==(const Invalid&) const = default;
    };

    // A typed value resolves against the element it is specified on, since the resource
    // cannot see that element's style.
    using ResolvedValue = Variant<Unresolved, Invalid, Ref<CSSVariableData>>;

    ParamSpec spec;
    DeclarationValue value;
    ResolvedValue resolved { Unresolved { } };

    bool operator==(const LinkParameter&) const = default;
};

template<size_t I> const auto& get(const LinkParameter& parameter)
{
    if constexpr (!I)
        return parameter.spec;
    else if constexpr (I == 1)
        return parameter.value;
}

// <param()> = param( <param-spec> , <declaration-value>? )
// https://drafts.csswg.org/css-link-params/#funcdef-param
using ParamFunction = FunctionNotation<CSSValueParam, LinkParameter>;

// <param()>#
using LinkParameterList = CommaSeparatedFixedVector<ParamFunction>;

// <'link-parameters'> = none | <param()>#
// https://drafts.csswg.org/css-link-params/#propdef-link-parameters
struct LinkParameters : ListOrNone<LinkParameterList> {
    using ListOrNone<LinkParameterList>::ListOrNone;
};

// Appends the param() modifiers from a resource's url() to the parameters set by the
// link-parameters property on the element referencing it.
// https://drafts.csswg.org/css-link-params/#setting
LinkParameters linkParametersForResource(const LinkParameters& fromProperty, const Vector<CSS::ParamFunction>& fromURL);

// MARK: - Conversion

template<> struct CSSValueConversion<LinkParameter> {
    auto operator()(BuilderState&, const CSSValue&) -> LinkParameter;
};

template<> struct CSSValueCreation<LinkParameter> {
    auto operator()(CSSValuePool&, const ComputedStyle&, const LinkParameter&) -> Ref<CSSValue>;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ParamSpec::Custom, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ParamSpec)
DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::LinkParameter, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::LinkParameters)
