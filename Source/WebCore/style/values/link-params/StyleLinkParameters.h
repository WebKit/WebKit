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

#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleDeclarationValue.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// The arguments of param(): a name and the value it sets, serialized comma separated.
// The comma is always present, so the value is not coalesced away when it is empty.
struct LinkParameter {
    CustomIdent name;
    DeclarationValue value;

    bool operator==(const LinkParameter&) const = default;
};

template<size_t I> const auto& get(const LinkParameter& parameter)
{
    if constexpr (!I)
        return parameter.name;
    else if constexpr (I == 1)
        return parameter.value;
}

// <param()> = param( <dashed-ident> , <declaration-value>? )
// https://drafts.csswg.org/css-link-params/#funcdef-param
using ParamFunction = FunctionNotation<CSSValueParam, LinkParameter>;

// <param()>#
using LinkParameterList = CommaSeparatedFixedVector<ParamFunction>;

// <'link-parameters'> = none | <param()>#
// https://drafts.csswg.org/css-link-params/#propdef-link-parameters
struct LinkParameters : ListOrNone<LinkParameterList> {
    using ListOrNone<LinkParameterList>::ListOrNone;
};

// MARK: - Conversion

template<> struct CSSValueConversion<LinkParameter> {
    auto operator()(BuilderState&, const CSSValue&) -> LinkParameter;
};

template<> struct CSSValueCreation<LinkParameter> {
    auto operator()(CSSValuePool&, const ComputedStyle&, const LinkParameter&) -> Ref<CSSValue>;
};

} // namespace Style
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::LinkParameter, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::LinkParameters)
