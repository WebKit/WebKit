/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

#include "config.h"
#include "ReferrerPolicy.h"

#include "HTTPParsers.h"

namespace WebCore {

enum class ShouldParseLegacyKeywords { No, Yes };

static Optional<ReferrerPolicy> parseReferrerPolicyToken(StringView policy, ShouldParseLegacyKeywords shouldParseLegacyKeywords)
{
    // "never" / "default" / "always" are legacy keywords that we support and still defined in the HTML specification:
    // https://html.spec.whatwg.org/#meta-referrer
    if (shouldParseLegacyKeywords == ShouldParseLegacyKeywords::Yes) {
        if (equalLettersIgnoringASCIICase(policy, "never"))
            return ReferrerPolicy::NoReferrer;
        if (equalLettersIgnoringASCIICase(policy, "always"))
            return ReferrerPolicy::UnsafeUrl;
        if (equalLettersIgnoringASCIICase(policy, "default"))
            return ReferrerPolicy::NoReferrerWhenDowngrade;
    }

    if (equalLettersIgnoringASCIICase(policy, "no-referrer"))
        return ReferrerPolicy::NoReferrer;
    if (equalLettersIgnoringASCIICase(policy, "unsafe-url"))
        return ReferrerPolicy::UnsafeUrl;
    if (equalLettersIgnoringASCIICase(policy, "origin"))
        return ReferrerPolicy::Origin;
    if (equalLettersIgnoringASCIICase(policy, "origin-when-cross-origin"))
        return ReferrerPolicy::OriginWhenCrossOrigin;
    if (equalLettersIgnoringASCIICase(policy, "same-origin"))
        return ReferrerPolicy::SameOrigin;
    if (equalLettersIgnoringASCIICase(policy, "strict-origin"))
        return ReferrerPolicy::StrictOrigin;
    if (equalLettersIgnoringASCIICase(policy, "strict-origin-when-cross-origin"))
        return ReferrerPolicy::StrictOriginWhenCrossOrigin;
    if (equalLettersIgnoringASCIICase(policy, "no-referrer-when-downgrade"))
        return ReferrerPolicy::NoReferrerWhenDowngrade;
    if (!policy.isNull() && policy.isEmpty())
        return ReferrerPolicy::EmptyString;

    return WTF::nullopt;
}
    
Optional<ReferrerPolicy> parseReferrerPolicy(StringView policyString, ReferrerPolicySource source)
{
    switch (source) {
    case ReferrerPolicySource::HTTPHeader: {
        // Implementing https://www.w3.org/TR/2017/CR-referrer-policy-20170126/#parse-referrer-policy-from-header.
        Optional<ReferrerPolicy> result;
        for (auto tokenView : policyString.split(',')) {
            auto token = parseReferrerPolicyToken(stripLeadingAndTrailingHTTPSpaces(tokenView), ShouldParseLegacyKeywords::No);
            if (token && token.value() != ReferrerPolicy::EmptyString)
                result = token.value();
        }
        return result;
    }
    case ReferrerPolicySource::MetaTag:
        return parseReferrerPolicyToken(policyString, ShouldParseLegacyKeywords::Yes);
    case ReferrerPolicySource::ReferrerPolicyAttribute:
        return parseReferrerPolicyToken(policyString, ShouldParseLegacyKeywords::No);
    }
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

} // namespace WebCore
