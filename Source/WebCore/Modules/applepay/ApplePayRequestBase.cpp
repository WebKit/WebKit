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

#include "config.h"
#include "ApplePayRequestBase.h"

#if ENABLE(APPLE_PAY)

#include "PaymentCoordinator.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

static bool requiresSupportedNetworks(unsigned version, const ApplePayRequestBase& request)
{
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    return version < 8 || !request.installmentConfiguration;
#else
    UNUSED_PARAM(version);
    UNUSED_PARAM(request);
    return true;
#endif
}

static ExceptionOr<Vector<String>> convertAndValidate(Document& document, unsigned version, const Vector<String>& supportedNetworks, const PaymentCoordinator& paymentCoordinator)
{
    Vector<String> result;
    result.reserveInitialCapacity(supportedNetworks.size());
    for (auto& supportedNetwork : supportedNetworks) {
        auto validatedNetwork = paymentCoordinator.validatedPaymentNetwork(document, version, supportedNetwork);
        if (!validatedNetwork)
            return Exception { ExceptionCode::TypeError, makeString("\"", supportedNetwork, "\" is not a valid payment network.") };
        result.append(*validatedNetwork);
    }

    return WTFMove(result);
}

ExceptionOr<ApplePaySessionPaymentRequest> convertAndValidate(Document& document, unsigned version, const ApplePayRequestBase& request, const PaymentCoordinator& paymentCoordinator)
{
    if (!version || !paymentCoordinator.supportsVersion(document, version))
        return Exception { ExceptionCode::InvalidAccessError, makeString('"', version, "\" is not a supported version.") };

    ApplePaySessionPaymentRequest result;
    result.setVersion(version);
    result.setCountryCode(request.countryCode);

    auto merchantCapabilities = convertAndValidate(request.merchantCapabilities);
    if (merchantCapabilities.hasException())
        return merchantCapabilities.releaseException();
    result.setMerchantCapabilities(merchantCapabilities.releaseReturnValue());

    if (requiresSupportedNetworks(version, request) && request.supportedNetworks.isEmpty())
        return Exception { ExceptionCode::TypeError, "At least one supported network must be provided."_s };

    auto supportedNetworks = convertAndValidate(document, version, request.supportedNetworks, paymentCoordinator);
    if (supportedNetworks.hasException())
        return supportedNetworks.releaseException();
    result.setSupportedNetworks(supportedNetworks.releaseReturnValue());

    if (request.requiredBillingContactFields) {
        auto requiredBillingContactFields = convertAndValidate(version, *request.requiredBillingContactFields);
        if (requiredBillingContactFields.hasException())
            return requiredBillingContactFields.releaseException();
        result.setRequiredBillingContactFields(requiredBillingContactFields.releaseReturnValue());
    }

    if (request.billingContact)
        result.setBillingContact(PaymentContact::fromApplePayPaymentContact(version, *request.billingContact));

    if (request.requiredShippingContactFields) {
        auto requiredShippingContactFields = convertAndValidate(version, *request.requiredShippingContactFields);
        if (requiredShippingContactFields.hasException())
            return requiredShippingContactFields.releaseException();
        result.setRequiredShippingContactFields(requiredShippingContactFields.releaseReturnValue());
    }

    if (request.shippingContact)
        result.setShippingContact(PaymentContact::fromApplePayPaymentContact(version, *request.shippingContact));

    result.setApplicationData(request.applicationData);

    if (version >= 3)
        result.setSupportedCountries(Vector { request.supportedCountries });

#if ENABLE(APPLE_PAY_INSTALLMENTS)
    if (request.installmentConfiguration) {
        auto installmentConfiguration = PaymentInstallmentConfiguration::create(*request.installmentConfiguration);
        if (installmentConfiguration.hasException())
            return installmentConfiguration.releaseException();
        result.setInstallmentConfiguration(installmentConfiguration.releaseReturnValue());
    }
#endif

#if ENABLE(APPLE_PAY_COUPON_CODE)
    result.setSupportsCouponCode(request.supportsCouponCode);
    result.setCouponCode(request.couponCode);
#endif

#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    result.setShippingContactEditingMode(request.shippingContactEditingMode);
#endif

#if ENABLE(APPLE_PAY_LATER_AVAILABILITY)
    result.setApplePayLaterAvailability(request.applePayLaterAvailability);
#endif

    return WTFMove(result);
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
