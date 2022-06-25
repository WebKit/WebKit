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

#if ENABLE(APPLE_PAY_COUPON_CODE)

#include "ApplePayDetailsUpdateBase.h"
#include "ApplePayError.h"
#include "ApplePayShippingMethod.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

struct ApplePayCouponCodeUpdate final : public ApplePayDetailsUpdateBase {
    Vector<RefPtr<ApplePayError>> errors;

    Vector<ApplePayShippingMethod> newShippingMethods;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplePayCouponCodeUpdate> decode(Decoder&);
};

template<class Encoder>
void ApplePayCouponCodeUpdate::encode(Encoder& encoder) const
{
    ApplePayDetailsUpdateBase::encode(encoder);
    encoder << errors;
    encoder << newShippingMethods;
}

template<class Decoder>
std::optional<ApplePayCouponCodeUpdate> ApplePayCouponCodeUpdate::decode(Decoder& decoder)
{
    ApplePayCouponCodeUpdate result;

    if (!result.decodeBase(decoder))
        return std::nullopt;

    std::optional<Vector<RefPtr<ApplePayError>>> errors;
    decoder >> errors;
    if (!errors)
        return std::nullopt;
    result.errors = WTFMove(*errors);

    std::optional<Vector<ApplePayShippingMethod>> newShippingMethods;
    decoder >> newShippingMethods;
    if (!newShippingMethods)
        return std::nullopt;
    result.newShippingMethods = WTFMove(*newShippingMethods);

    return result;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY_COUPON_CODE)
