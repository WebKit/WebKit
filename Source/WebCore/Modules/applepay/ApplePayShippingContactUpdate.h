/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ApplePayDetailsUpdateBase.h"
#include "ApplePayError.h"
#include "ApplePayShippingMethod.h"
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

struct ApplePayShippingContactUpdate final : public ApplePayDetailsUpdateBase {
    Vector<RefPtr<ApplePayError>> errors;

    Vector<ApplePayShippingMethod> newShippingMethods;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ApplePayShippingContactUpdate> decode(Decoder&);
};

template<class Encoder>
void ApplePayShippingContactUpdate::encode(Encoder& encoder) const
{
    ApplePayDetailsUpdateBase::encode(encoder);
    encoder << errors;
    encoder << newShippingMethods;
}

template<class Decoder>
Optional<ApplePayShippingContactUpdate> ApplePayShippingContactUpdate::decode(Decoder& decoder)
{
    ApplePayShippingContactUpdate result;

    if (!result.decodeBase(decoder))
        return WTF::nullopt;

#define DECODE(name, type) \
    Optional<type> name; \
    decoder >> name; \
    if (!name) \
        return WTF::nullopt; \
    result.name = WTFMove(*name); \

    DECODE(errors, Vector<RefPtr<ApplePayError>>)
    DECODE(newShippingMethods, Vector<ApplePayShippingMethod>)

#undef DECODE

    return result;
}

} // namespace WebCore

#endif
