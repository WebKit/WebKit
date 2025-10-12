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

#include <WebCore/CSSValueTypes.h>
#include <WebCore/LoadedFromOpaqueSource.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ResourceLoaderOptions;

namespace CSS {

// <cross-origin-modifier> = cross-origin( anonymous | use-credentials )
// https://drafts.csswg.org/css-values-5/#typedef-request-url-modifier-cross-origin-modifier
using URLCrossOriginParameters = Variant<
    Keyword::Anonymous,
    Keyword::UseCredentials
>;
using URLCrossOriginFunction = FunctionNotation<CSSValueCrossOrigin, URLCrossOriginParameters>;

// <integrity-modifier> = integrity( <string> )
// https://drafts.csswg.org/css-values-5/#typedef-request-url-modifier-integrity-modifier
using URLIntegrityParameters = String;
using URLIntegrityFunction = FunctionNotation<CSSValueIntegrity, URLIntegrityParameters>;

// <referrer-policy-modifier> = referrer-policy( no-referrer | no-referrer-when-downgrade | same-origin | origin | strict-origin | origin-when-cross-origin | strict-origin-when-cross-origin | unsafe-url )
// https://drafts.csswg.org/css-values-5/#typedef-request-url-modifier-referrer-policy-modifier
using URLReferrerPolicyParameters = Variant<
    Keyword::NoReferrer,
    Keyword::NoReferrerWhenDowngrade,
    Keyword::SameOrigin,
    Keyword::Origin,
    Keyword::StrictOrigin,
    Keyword::OriginWhenCrossOrigin,
    Keyword::StrictOriginWhenCrossOrigin,
    Keyword::UnsafeUrl
>;
using URLReferrerPolicyFunction = FunctionNotation<CSSValueReferrerPolicy, URLReferrerPolicyParameters>;

// https://drafts.csswg.org/css-values-5/#typedef-request-url-modifier
// <request-url-modifier> = <cross-origin-modifier> | <integrity-modifier> | <referrer-policy-modifier>
struct URLModifiers {
    std::optional<URLCrossOriginFunction> crossOrigin { };
    std::optional<URLIntegrityFunction> integrity { };
    std::optional<URLReferrerPolicyFunction> referrerPolicy { };

    // This is not a parsed value, but is implicit from context the modifiers were parsed with.
    LoadedFromOpaqueSource loadedFromOpaqueSource { LoadedFromOpaqueSource::No };

    bool operator==(const URLModifiers&) const = default;
};

template<size_t I> const auto& get(const URLModifiers& value)
{
    if constexpr (I == 0)
        return value.crossOrigin;
    else if constexpr (I == 1)
        return value.integrity;
    else if constexpr (I == 2)
        return value.referrerPolicy;
}

// Applies `URLModifiers` to `ResourceLoaderOptions`.
void applyModifiersToLoaderOptions(const URLModifiers&, ResourceLoaderOptions&);

} // namespace CSS
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::URLModifiers, 3)
