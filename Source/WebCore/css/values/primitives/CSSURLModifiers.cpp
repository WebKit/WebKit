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

#include "config.h"
#include "CSSURLModifiers.h"

#include "ResourceLoaderOptions.h"
#include <wtf/Hasher.h>

namespace WebCore {
namespace CSS {

void applyModifiersToLoaderOptions(const URLModifiers& modifiers, ResourceLoaderOptions& loaderOptions)
{
    if (modifiers.crossOrigin) {
        // https://www.w3.org/TR/css-values-5/#typedef-request-url-modifier-cross-origin-modifier
        //
        // The URL request modifier steps for this modifier given request req are:
        // 1. Set request's mode to "cors".
        loaderOptions.mode = FetchOptions::Mode::Cors;

        // 2. If the given value is use-credentials, set request's credentials mode to "include"
        if (std::holds_alternative<Keyword::UseCredentials>(modifiers.crossOrigin->parameters))
            loaderOptions.credentials = FetchOptions::Credentials::Include;
        else
            loaderOptions.credentials = FetchOptions::Credentials::SameOrigin;
    }

    if (modifiers.integrity) {
        // https://www.w3.org/TR/css-values-5/#typedef-request-url-modifier-integrity-modifier
        //
        // The URL request modifier steps for this modifier given request req are to set
        // request's integrity metadata to the given <string>.
        loaderOptions.integrity = modifiers.integrity->parameters.value;
    }

    if (modifiers.referrerPolicy) {
        // https://www.w3.org/TR/css-values-5/#typedef-request-url-modifier-referrer-policy-modifier
        //
        // The URL request modifier steps for this modifier given request req are to set
        // request's referrer policy to the ReferrerPolicy that matches the given value.
        loaderOptions.referrerPolicy = WTF::switchOn(modifiers.referrerPolicy->parameters,
            [](Keyword::NoReferrer) {
                return ReferrerPolicy::NoReferrer;
            },
            [](Keyword::NoReferrerWhenDowngrade) {
                return ReferrerPolicy::NoReferrerWhenDowngrade;
            },
            [](Keyword::SameOrigin) {
                return ReferrerPolicy::SameOrigin;
            },
            [](Keyword::Origin) {
                return ReferrerPolicy::Origin;
            },
            [](Keyword::StrictOrigin) {
                return ReferrerPolicy::StrictOrigin;
            },
            [](Keyword::OriginWhenCrossOrigin) {
                return ReferrerPolicy::OriginWhenCrossOrigin;
            },
            [](Keyword::StrictOriginWhenCrossOrigin) {
                return ReferrerPolicy::StrictOriginWhenCrossOrigin;
            },
            [](Keyword::UnsafeUrl) {
                return ReferrerPolicy::UnsafeUrl;
            }
        );
    }

    loaderOptions.loadedFromOpaqueSource = modifiers.loadedFromOpaqueSource;
}

// MARK: - Hashing

void add(Hasher& hasher, const URLModifiers& modifiers)
{
    add(hasher, modifiers.crossOrigin.has_value());
    if (modifiers.crossOrigin)
        add(hasher, modifiers.crossOrigin->parameters.index());

    add(hasher, modifiers.integrity.has_value());
    if (modifiers.integrity)
        add(hasher, modifiers.integrity->parameters);

    add(hasher, modifiers.referrerPolicy.has_value());
    if (modifiers.referrerPolicy)
        add(hasher, modifiers.referrerPolicy->parameters.index());

    add(hasher, modifiers.linkParameters.size());
    for (auto& parameter : modifiers.linkParameters)
        add(hasher, parameter.parameters);

    add(hasher, modifiers.loadedFromOpaqueSource);
}

} // namespace CSS
} // namespace WebCore
