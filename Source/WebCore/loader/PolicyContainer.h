/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "CrossOriginEmbedderPolicy.h"
#include "CrossOriginOpenerPolicy.h"

namespace WebCore {

// https://html.spec.whatwg.org/multipage/origin.html#policy-container
struct PolicyContainer {
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    CrossOriginOpenerPolicy crossOriginOpenerPolicy;
    // FIXME: CSP list and referrer policy should be part of the PolicyContainer.

    PolicyContainer isolatedCopy() const & { return { crossOriginEmbedderPolicy.isolatedCopy(), crossOriginOpenerPolicy.isolatedCopy() }; }
    PolicyContainer isolatedCopy() && { return { WTFMove(crossOriginEmbedderPolicy).isolatedCopy(), WTFMove(crossOriginOpenerPolicy).isolatedCopy() }; }
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PolicyContainer> decode(Decoder&);
};

inline bool operator==(const PolicyContainer& a, const PolicyContainer& b)
{
    return a.crossOriginEmbedderPolicy == b.crossOriginEmbedderPolicy && a.crossOriginOpenerPolicy == b.crossOriginOpenerPolicy;
}

template<class Encoder>
void PolicyContainer::encode(Encoder& encoder) const
{
    encoder << crossOriginEmbedderPolicy << crossOriginOpenerPolicy;
}

template<class Decoder>
std::optional<PolicyContainer> PolicyContainer::decode(Decoder& decoder)
{
    std::optional<CrossOriginEmbedderPolicy> crossOriginEmbedderPolicy;
    decoder >> crossOriginEmbedderPolicy;
    if (!crossOriginEmbedderPolicy)
        return std::nullopt;

    std::optional<CrossOriginOpenerPolicy> crossOriginOpenerPolicy;
    decoder >> crossOriginOpenerPolicy;
    if (!crossOriginOpenerPolicy)
        return std::nullopt;

    return {{
        WTFMove(*crossOriginEmbedderPolicy),
        WTFMove(*crossOriginOpenerPolicy)
    }};
}

} // namespace WebCore
