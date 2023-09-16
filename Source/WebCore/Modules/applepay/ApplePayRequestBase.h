/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "ApplePayContactField.h"
#include "ApplePayFeature.h"
#include "ApplePayInstallmentConfigurationWebCore.h"
#include "ApplePayMerchantCapability.h"
#include "ApplePayPaymentContact.h"
#include "ApplePayShippingContactEditingMode.h"

namespace WebCore {

class Document;
class PaymentCoordinator;

struct ApplePayRequestBase {
    std::optional<Vector<ApplePayFeature>> features;

    Vector<ApplePayMerchantCapability> merchantCapabilities;
    Vector<String> supportedNetworks;
    String countryCode;

    std::optional<Vector<ApplePayContactField>> requiredBillingContactFields;
    std::optional<ApplePayPaymentContact> billingContact;

    std::optional<Vector<ApplePayContactField>> requiredShippingContactFields;
    std::optional<ApplePayPaymentContact> shippingContact;

    String applicationData;
    Vector<String> supportedCountries;

#if ENABLE(APPLE_PAY_INSTALLMENTS)
    std::optional<ApplePayInstallmentConfiguration> installmentConfiguration;
#endif

#if ENABLE(APPLE_PAY_COUPON_CODE)
    std::optional<bool> supportsCouponCode;
    String couponCode;
#endif

#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    std::optional<ApplePayShippingContactEditingMode> shippingContactEditingMode;
#endif

#if ENABLE(APPLE_PAY_LATER_AVAILABILITY)
    std::optional<ApplePayLaterAvailability> applePayLaterAvailability;
#endif
};

ExceptionOr<ApplePaySessionPaymentRequest> convertAndValidate(Document&, unsigned version, const ApplePayRequestBase&, const PaymentCoordinator&);

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
