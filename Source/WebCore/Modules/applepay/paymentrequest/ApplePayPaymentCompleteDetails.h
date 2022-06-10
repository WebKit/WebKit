/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)

#include "ApplePayPaymentOrderDetails.h"
#include <optional>

namespace WebCore {

struct ApplePayPaymentCompleteDetails {
#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
    std::optional<ApplePayPaymentOrderDetails> orderDetails;
#endif

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<ApplePayPaymentCompleteDetails> decode(Decoder&);
};

template<typename Encoder>
void ApplePayPaymentCompleteDetails::encode(Encoder& encoder) const
{
    UNUSED_PARAM(encoder);
#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
    encoder << orderDetails;
#endif
}

template<typename Decoder>
std::optional<ApplePayPaymentCompleteDetails> ApplePayPaymentCompleteDetails::decode(Decoder& decoder)
{
#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return std::nullopt; \

    UNUSED_PARAM(decoder);
#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
    DECODE(orderDetails, std::optional<ApplePayPaymentOrderDetails>)
#endif

#undef DECODE

    return { {
#if ENABLE(APPLE_PAY_PAYMENT_ORDER_DETAILS)
        WTFMove(*orderDetails),
#endif
    } };
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY) && ENABLE(PAYMENT_REQUEST)
