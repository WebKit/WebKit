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

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)

#include <WebCore/ApplePayLineItem.h>
#include <optional>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ApplePayAutomaticReloadPaymentRequest final {
    String paymentDescription; // required
    ApplePayLineItem automaticReloadBilling; // required, must be `paymentTiming: "automaticReload"`
    String billingAgreement;
    String managementURL; // required
    String tokenNotificationURL;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ApplePayAutomaticReloadPaymentRequest> decode(Decoder&);
};

template<class Encoder>
void ApplePayAutomaticReloadPaymentRequest::encode(Encoder& encoder) const
{
    encoder << paymentDescription;
    encoder << automaticReloadBilling;
    encoder << billingAgreement;
    encoder << managementURL;
    encoder << tokenNotificationURL;
}

template<class Decoder>
std::optional<ApplePayAutomaticReloadPaymentRequest> ApplePayAutomaticReloadPaymentRequest::decode(Decoder& decoder)
{
#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return std::nullopt; \

    DECODE(paymentDescription, String)
    DECODE(automaticReloadBilling, ApplePayLineItem)
    DECODE(billingAgreement, String)
    DECODE(managementURL, String)
    DECODE(tokenNotificationURL, String)

#undef DECODE

    return { {
        WTFMove(*paymentDescription),
        WTFMove(*automaticReloadBilling),
        WTFMove(*billingAgreement),
        WTFMove(*managementURL),
        WTFMove(*tokenNotificationURL),
    } };
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
