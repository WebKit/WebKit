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

#include "ApplePayLineItem.h"
#include "ApplePayPaymentContact.h"
#include "ApplePayShippingMethod.h"
#include "PaymentRequest.h"

namespace WebCore {

struct ApplePayPaymentRequest {
    enum class MerchantCapability { Supports3DS, SupportsEMV, SupportsCredit, SupportsDebit };
    enum class ContactField { Email, Name, Phone, PostalAddress };

    using ShippingType = PaymentRequest::ShippingType;

    Vector<MerchantCapability> merchantCapabilities;
    Vector<String> supportedNetworks;
    String countryCode;
    String currencyCode;

    std::optional<Vector<ContactField>> requiredBillingContactFields;
    std::optional<ApplePayPaymentContact> billingContact;

    std::optional<Vector<ContactField>>  requiredShippingContactFields;
    std::optional<ApplePayPaymentContact> shippingContact;

    ShippingType shippingType { ShippingType::Shipping };
    std::optional<Vector<ApplePayShippingMethod>> shippingMethods;

    ApplePayLineItem total;
    std::optional<Vector<ApplePayLineItem>> lineItems;

    String applicationData;
    Vector<String> supportedCountries;
};

}

#endif
