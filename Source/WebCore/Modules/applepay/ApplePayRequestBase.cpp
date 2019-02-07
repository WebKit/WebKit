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

static ExceptionOr<Vector<String>> convertAndValidate(unsigned version, const Vector<String>& supportedNetworks, const PaymentCoordinator& paymentCoordinator)
{
    if (supportedNetworks.isEmpty())
        return Exception { TypeError, "At least one supported network must be provided." };

    Vector<String> result;
    result.reserveInitialCapacity(supportedNetworks.size());
    for (auto& supportedNetwork : supportedNetworks) {
        auto validatedNetwork = paymentCoordinator.validatedPaymentNetwork(version, supportedNetwork);
        if (!validatedNetwork)
            return Exception { TypeError, makeString("\"", supportedNetwork, "\" is not a valid payment network.") };
        result.uncheckedAppend(*validatedNetwork);
    }

    return WTFMove(result);
}

ExceptionOr<ApplePaySessionPaymentRequest> convertAndValidate(unsigned version, ApplePayRequestBase& request, const PaymentCoordinator& paymentCoordinator)
{
    if (!version || !paymentCoordinator.supportsVersion(version))
        return Exception { InvalidAccessError, makeString('"', version, "\" is not a supported version.") };

    ApplePaySessionPaymentRequest result;
    result.setVersion(version);
    result.setCountryCode(request.countryCode);

    auto merchantCapabilities = convertAndValidate(request.merchantCapabilities);
    if (merchantCapabilities.hasException())
        return merchantCapabilities.releaseException();
    result.setMerchantCapabilities(merchantCapabilities.releaseReturnValue());

    auto supportedNetworks = convertAndValidate(version, request.supportedNetworks, paymentCoordinator);
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
        result.setSupportedCountries(WTFMove(request.supportedCountries));

    return WTFMove(result);
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
