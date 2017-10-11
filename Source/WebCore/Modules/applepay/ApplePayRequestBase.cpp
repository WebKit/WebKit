/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

namespace WebCore {

static ExceptionOr<void> validate(unsigned version, const Vector<String>& supportedNetworks)
{
    if (supportedNetworks.isEmpty())
        return Exception { TypeError, "At least one supported network must be provided." };

    for (auto& supportedNetwork : supportedNetworks) {
        if (!ApplePaySessionPaymentRequest::isValidSupportedNetwork(version, supportedNetwork))
            return Exception { TypeError, makeString("\"" + supportedNetwork, "\" is not a valid payment network.") };
    }

    return { };
}

ExceptionOr<ApplePaySessionPaymentRequest> convertAndValidate(unsigned version, ApplePayRequestBase& request)
{
    ApplePaySessionPaymentRequest result;
    result.setCountryCode(request.countryCode);

    auto merchantCapabilities = convertAndValidate(request.merchantCapabilities);
    if (merchantCapabilities.hasException())
        return merchantCapabilities.releaseException();
    result.setMerchantCapabilities(merchantCapabilities.releaseReturnValue());

    auto exception = validate(version, WTFMove(request.supportedNetworks));
    if (exception.hasException())
        return exception.releaseException();
    result.setSupportedNetworks(request.supportedNetworks);

    if (request.requiredBillingContactFields) {
        auto requiredBillingContactFields = convertAndValidate(version, *request.requiredBillingContactFields);
        if (requiredBillingContactFields.hasException())
            return requiredBillingContactFields.releaseException();
        result.setRequiredBillingContactFields(requiredBillingContactFields.releaseReturnValue());
    }

    if (request.billingContact)
        result.setBillingContact(PaymentContact::fromApplePayPaymentContact(version, *request.billingContact));

    result.setApplicationData(request.applicationData);

    if (version >= 3)
        result.setSupportedCountries(WTFMove(request.supportedCountries));

    return WTFMove(result);
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
