/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY)

#include "ApplePayShippingMethodData.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ApplePayShippingMethod final : public ApplePayShippingMethodData {
    String label;
    String detail;
    String amount;
    String identifier;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ApplePayShippingMethod> decode(Decoder&);
};

template<class Encoder>
void ApplePayShippingMethod::encode(Encoder& encoder) const
{
    ApplePayShippingMethodData::encode(encoder);
    encoder << label;
    encoder << detail;
    encoder << amount;
    encoder << identifier;
}

template<class Decoder>
Optional<ApplePayShippingMethod> ApplePayShippingMethod::decode(Decoder& decoder)
{
    ApplePayShippingMethod result;

    if (!result.decodeData(decoder))
        return WTF::nullopt;

#define DECODE(name, type) \
    Optional<type> name; \
    decoder >> name; \
    if (!name) \
        return WTF::nullopt; \
    result.name = WTFMove(*name); \

    DECODE(label, String)
    DECODE(detail, String)
    DECODE(amount, String)
    DECODE(identifier, String)

#undef DECODE

    return result;
}

} // namespace WebCore

#endif
