/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ApplePayPaymentMethodUpdate final : public ApplePayDetailsUpdateBase {
#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    Vector<RefPtr<ApplePayError>> errors;

    Vector<ApplePayShippingMethod> newShippingMethods;
#endif

#if ENABLE(APPLE_PAY_INSTALLMENTS)
    String installmentGroupIdentifier;
#endif // ENABLE(APPLE_PAY_INSTALLMENTS)

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplePayPaymentMethodUpdate> decode(Decoder&);
};

template<class Encoder>
void ApplePayPaymentMethodUpdate::encode(Encoder& encoder) const
{
    ApplePayDetailsUpdateBase::encode(encoder);
#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    encoder << errors;
    encoder << newShippingMethods;
#endif
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    encoder << installmentGroupIdentifier;
#endif // ENABLE(APPLE_PAY_INSTALLMENTS)
}

template<class Decoder>
std::optional<ApplePayPaymentMethodUpdate> ApplePayPaymentMethodUpdate::decode(Decoder& decoder)
{
    ApplePayPaymentMethodUpdate result;

    if (!result.decodeBase(decoder))
        return std::nullopt;

#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return std::nullopt; \
    result.name = WTFMove(*name); \

#if ENABLE(APPLE_PAY_UPDATE_SHIPPING_METHODS_WHEN_CHANGING_LINE_ITEMS)
    DECODE(errors, Vector<RefPtr<ApplePayError>>)
    DECODE(newShippingMethods, Vector<ApplePayShippingMethod>)
#endif

#if ENABLE(APPLE_PAY_INSTALLMENTS)
    DECODE(installmentGroupIdentifier, String)
#endif // ENABLE(APPLE_PAY_INSTALLMENTS)

#undef DECODE

    return result;
}

} // namespace WebCore

#endif
