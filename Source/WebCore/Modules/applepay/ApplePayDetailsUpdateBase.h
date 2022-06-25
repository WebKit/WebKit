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

#if ENABLE(APPLE_PAY)

#include "ApplePayAutomaticReloadPaymentRequest.h"
#include "ApplePayLineItem.h"
#include "ApplePayPaymentTokenContext.h"
#include "ApplePayRecurringPaymentRequest.h"
#include <wtf/Vector.h>

namespace WebCore {

struct ApplePayDetailsUpdateBase {
    ApplePayLineItem newTotal;
    Vector<ApplePayLineItem> newLineItems;

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    std::optional<ApplePayRecurringPaymentRequest> newRecurringPaymentRequest;
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    std::optional<ApplePayAutomaticReloadPaymentRequest> newAutomaticReloadPaymentRequest;
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    std::optional<Vector<ApplePayPaymentTokenContext>> newMultiTokenContexts;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplePayDetailsUpdateBase> decode(Decoder&);

    template<class Decoder> WARN_UNUSED_RETURN bool decodeBase(Decoder&);
};

template<class Encoder>
void ApplePayDetailsUpdateBase::encode(Encoder& encoder) const
{
    encoder << newTotal;
    encoder << newLineItems;
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    encoder << newRecurringPaymentRequest;
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    encoder << newAutomaticReloadPaymentRequest;
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    encoder << newMultiTokenContexts;
#endif
}

template<class Decoder>
std::optional<ApplePayDetailsUpdateBase> ApplePayDetailsUpdateBase::decode(Decoder& decoder)
{
    ApplePayDetailsUpdateBase result;
    if (!result.decodeBase(decoder))
        return std::nullopt;
    return result;
}

template<class Decoder>
bool ApplePayDetailsUpdateBase::decodeBase(Decoder& decoder)
{
#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return false; \
    this->name = WTFMove(*name); \

    DECODE(newTotal, ApplePayLineItem)
    DECODE(newLineItems, Vector<ApplePayLineItem>)
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    DECODE(newRecurringPaymentRequest, std::optional<ApplePayRecurringPaymentRequest>);
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    DECODE(newAutomaticReloadPaymentRequest, std::optional<ApplePayAutomaticReloadPaymentRequest>);
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    DECODE(newMultiTokenContexts, std::optional<Vector<ApplePayPaymentTokenContext>>);
#endif

#undef DECODE

    return true;
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
